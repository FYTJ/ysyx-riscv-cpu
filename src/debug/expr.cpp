#include "expr.h"
#include "debug.h"
#include "../minirv/cpp/register.h"
#include "../minirv/cpp/memory.h"
#include <iostream>
#include <string>

enum class TKTYPE{
  TK_NOTYPE = 256, TK_EQ, TK_NUM, TK_NEQ, TK_AND, TK_HEX, TK_REG, TK_DEREF, TK_ADD, TK_SUB, TK_MUL, TK_DIV, TK_LPAREN, TK_RPAREN
};

struct Rule {
    regex re;
    TKTYPE token_type;
};


static vector<Rule> rules = {
    {regex(R"( +)", regex_constants::extended), TKTYPE::TK_NOTYPE},
    {regex(R"(0x[0-9a-fA-F]+)", regex_constants::extended), TKTYPE::TK_HEX},
    {regex(R"(\$[a-zA-Z][a-zA-Z0-9]*)", regex_constants::extended), TKTYPE::TK_REG},
    {regex(R"([0-9]+)", regex_constants::extended), TKTYPE::TK_NUM},
    {regex(R"(\+)", regex_constants::extended), TKTYPE::TK_ADD},
    {regex(R"(-)", regex_constants::extended), TKTYPE::TK_SUB},
    {regex(R"(\*)", regex_constants::extended), TKTYPE::TK_MUL},
    {regex(R"(/)", regex_constants::extended), TKTYPE::TK_DIV},
    {regex(R"(\()", regex_constants::extended), TKTYPE::TK_LPAREN},
    {regex(R"(\))", regex_constants::extended), TKTYPE::TK_RPAREN},
    {regex(R"(==)", regex_constants::extended), TKTYPE::TK_EQ},
    {regex(R"(!=)", regex_constants::extended), TKTYPE::TK_NEQ},
    {regex(R"(&&)", regex_constants::extended), TKTYPE::TK_AND},
};

const size_t NR_REGEX = rules.size();
static vector<regex> re;

typedef struct token {
  TKTYPE type;
  string_view str;
} Token;

static Token tokens[1024] __attribute__((used)) = {};

int nr_token = 0;

static uint32_t* g_regs = nullptr;
static uint8_t* g_mem = nullptr;
static uint64_t g_pc = 0;
static uint32_t g_mem_base = MEM_BASE;
static uint32_t g_mem_size = MEM_SIZE;

void expr_set_context(uint32_t* regs, uint8_t* mem, uint64_t pc, uint32_t mem_base, uint32_t mem_size) {
    g_regs = regs;
    g_mem = mem;
    g_pc = pc;
    g_mem_base = mem_base;
    g_mem_size = mem_size;
}

static bool make_token(string_view e) {
    size_t pos = 0;
    nr_token = 0;
    while (pos < e.size()) {
        bool matched = false;
        const char *cur = e.data() + pos;
        cmatch m;
        for (size_t i = 0; i < rules.size(); i++) {
            if (regex_search(cur, m, rules[i].re) && m.position() == 0) {
                int len = static_cast<int>(m.length());
                TKTYPE token_type = rules[i].token_type;
                if (token_type == TKTYPE::TK_NOTYPE) {
                    pos += len;
                    matched = true;
                    break;
                }
                if (token_type == TKTYPE::TK_MUL) {
                    if (nr_token != 0 && (tokens[nr_token - 1].type == TKTYPE::TK_NUM || tokens[nr_token - 1].type == TKTYPE::TK_HEX || tokens[nr_token - 1].type == TKTYPE::TK_REG || tokens[nr_token - 1].type == TKTYPE::TK_RPAREN)) {
                        token_type = TKTYPE::TK_MUL;
                    }
                    else {
                        token_type = TKTYPE::TK_DEREF;
                    }
                    tokens[nr_token].type = token_type;
                    tokens[nr_token].str = e.substr(pos, len);
                    nr_token++;
                    pos += len;
                    matched = true;
                    break;
                }
                else {
                    tokens[nr_token].type = token_type;
                    tokens[nr_token].str = e.substr(pos, len);
                    nr_token++;
                    pos += len;
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) {
            std::printf("no match at position %zu\n%.*s\n%*s^\n", pos, (int)e.size(), e.data(), (int)pos, "");
            return false;
        }
    }
    return true;
}

static bool check_parentheses(int start, int end) {
    int cnt = 0;
    if (tokens[start].type != TKTYPE::TK_LPAREN || tokens[end].type != TKTYPE::TK_RPAREN)
        return false;
    for (int i = start + 1; i <= end - 1; i++) {
        if (tokens[i].type == TKTYPE::TK_LPAREN) {
        cnt++;
        continue;
        }
        if (tokens[i].type == TKTYPE::TK_RPAREN) {
        cnt--;
        if (cnt < 0) {
            return false;
        }
        }
    }
    if (cnt == 0) {
        return true;
    }
    return false;
}

long long eval(int start, int end, bool *success) {
    if ((start > end) || *success == false) {
        *success = false;
        return 0;
    }
    *success = true;
    if (start == end) {
        if (tokens[start].type == TKTYPE::TK_NUM) {
            return (long long)stoi((string)tokens[start].str);
        }
        if (tokens[start].type == TKTYPE::TK_HEX) {
            return stoll((string)tokens[start].str, nullptr, 16);
        }
        if (tokens[start].type == TKTYPE::TK_REG) {
            if (tokens[start].str == "$pc") {
                return (long long)g_pc;
            }
            string_view name_sv(tokens[start].str);
            name_sv.remove_prefix(1);
            if (g_regs == nullptr) { *success = false; return 0; }
            for (int i = 0; i < 32; ++i) {
                if (name_sv == regs[i]) {
                    return (long long)g_regs[i];
                }
            }
            *success = false;
            return 0;
        }
    }
    if (check_parentheses(start, end) == true) {
        return eval(start + 1, end - 1, success);
    }
    else {
        int op = start;
        int depth = 0;
        int find = 0;
        for (int i = end; i >= start; i--) {
        if (tokens[i].type == TKTYPE::TK_RPAREN) {
            depth++;
        }
        if (tokens[i].type == TKTYPE::TK_LPAREN) {
            depth--;
        }
        if (depth == 0) {
            if (tokens[i].type == TKTYPE::TK_ADD || tokens[i].type == TKTYPE::TK_SUB) {
            op = i;
            find = 1;
            break;
            }
        }
        }
        for (int i = end; i >= start && find == 0; i--) {
        if (tokens[i].type == TKTYPE::TK_RPAREN) {
            depth++;
        }
        if (tokens[i].type == TKTYPE::TK_LPAREN) {
            depth--;
        }
        if (depth == 0) {
            if (tokens[i].type == TKTYPE::TK_MUL || tokens[i].type == TKTYPE::TK_DIV) {
            op = i;
            find = 1;
            break;
            }
        }
        }
        for (int i = end; i >= start && find == 0; i--) {
            if (tokens[i].type == TKTYPE::TK_RPAREN) {
                depth++;
            }
            if (tokens[i].type == TKTYPE::TK_LPAREN) {
                depth--;
            }
            if (depth == 0) {
                if (tokens[i].type == TKTYPE::TK_EQ || tokens[i].type == TKTYPE::TK_NEQ || tokens[i].type == TKTYPE::TK_AND) {
                op = i;
                find = 1;
                break;
                }
            }
        }
        if (op == start || op == end) {
            if (tokens[start].type == TKTYPE::TK_DEREF) {
                long long addr = eval(start + 1, end, success);
                if (g_mem != nullptr && addr >= g_mem_base && addr < g_mem_base + g_mem_size) {
                    return (long long)g_mem[(uint32_t)addr - g_mem_base];
                }
            }
            *success = false;
            return 0;
        }
        long long val1 = eval(start, op - 1, success);
        long long val2 = eval(op + 1, end, success);
        switch (tokens[op].type)
        {
            case TKTYPE::TK_ADD:
                return val1 + val2;
            case TKTYPE::TK_SUB:
                return val1 - val2;
            case TKTYPE::TK_MUL:
                return val1 * val2;
            case TKTYPE::TK_DIV:
                if (val2 == 0) {
                *success = false;
                cout << "ZeroDivisionError: divide by zero" << endl;
                return 0;
                }
                return val1 / val2;
            case TKTYPE::TK_EQ:
                return val1 == val2;
            case TKTYPE::TK_NEQ:
                return val1 != val2;
            case TKTYPE::TK_AND:
                return val1 && val2;
            default: {
                *success = false;
                cout << "Unknown operator: " << tokens[op].str << endl;
                return 0;
            }
        }
    }
}


uint32_t expr(string_view e, bool *success) {
    if (!make_token(e)) {
        *success = false;
        return 0;
    }
    if (g_regs == nullptr || g_mem == nullptr) {
        *success = false;
        return 0;
    }
    uint32_t result = (uint32_t)eval(0, nr_token - 1, success);
    if (*success == false) {
        return 0;
    }
    return result;
}
