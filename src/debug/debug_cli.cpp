#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <charconv>
#include <iomanip>
#include "debug.h"
#include "debug_cli.h"
#include "watchpoint.h"
#include "breakpoint.h"
#include "expr.h"
#include "disasm.h"
#include "trace.h"
#include "../minirv/cpp/signal.h"
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

DebugCli::DebugCli() {
    debug_info.regs = new uint32_t[32];
    debug_info.mem = new uint8_t[128 * 1024 * 1024];
}

pair<string_view, string_view> DebugCli::split_first_token(string_view s) {
    auto first = s.find_first_not_of(" \t\r\n\v\f");
    if (first == string_view::npos){
        return {"", ""};
    }

    s.remove_prefix(first);
    auto sp = s.find(' ');
    if (sp == string_view::npos) {
        return {s, ""};
    }
    
    auto cmd = s.substr(0, sp);
    auto args = s.substr(sp + 1);
    auto p = args.find_first_not_of(" \t\r\n\v\f");
    if (p != string_view::npos) {
        args.remove_prefix(p);
    }
    else {
        args = {};
    }
    return {cmd, args};
}

static const CmdEntry cmd_table[] = {
    { "help", "Display information about all supported commands", &DebugCli::cmd_help },
    { "c", "Continue the execution of the program", &DebugCli::cmd_c },
    { "continue", "Continue the execution of the program", &DebugCli::cmd_c },
    { "q", "Exit Debugger", &DebugCli::cmd_q },
    { "quit", "Exit Debugger", &DebugCli::cmd_q },
    { "si", "Execute the next N instructions and then pause; if N is omitted, the default is 1", &DebugCli::cmd_si },
    { "stepi", "Execute the next N instructions and then pause; if N is omitted, the default is 1", &DebugCli::cmd_si },
    { "info", "Display information about the program", &DebugCli::cmd_info},
    { "i", "Display information about the program", &DebugCli::cmd_info},
    { "info s", "Display signals: info s [FILTER]", &DebugCli::cmd_info},
    { "x", "Examine memory: x/N{fmt}{size} EXPR (fmt: x|d|u|t, size: b|h|w)", &DebugCli::cmd_x},
    { "examine", "Examine memory: x/N{fmt}{size} EXPR (fmt: x|d|u|t, size: b|h|w)", &DebugCli::cmd_x},
    { "p", "Print expression: p/[fmt][size] EXPR (fmt: x|d|u|t)", &DebugCli::cmd_p},
    { "print", "Print expression: p/[fmt][size] EXPR (fmt: x|d|u|t)", &DebugCli::cmd_p},
    { "w", "Set a watchpoint on the memory address EXPR", &DebugCli::cmd_w},
    { "watch", "Set a watchpoint on the memory address EXPR", &DebugCli::cmd_w},
    { "d", "Delete the watchpoint with the specified number", &DebugCli::cmd_d},
    { "delete", "Delete breakpoint/watchpoint by ID", &DebugCli::cmd_d},
    { "b", "Set breakpoint: b *ADDR [if EXPR] or b if EXPR", &DebugCli::cmd_b},
    { "break", "Set breakpoint: b *ADDR [if EXPR] or b if EXPR", &DebugCli::cmd_b},
    { "disas", "Disassemble via objdump: disas [ADDR] [N], defaults to $pc,10", &DebugCli::cmd_disas},
    { "disassemble", "Disassemble via objdump: disassemble [ADDR] [N]", &DebugCli::cmd_disas},
    { "until", "Run until address: until *ADDR", &DebugCli::cmd_until},
    { "trace", "Trace control: trace on|off; trace itrace on [--size N|--size=N] [--file PATH|--file=PATH]", &DebugCli::cmd_trace}
};

static const size_t NR_CMD = sizeof(cmd_table) / sizeof(cmd_table[0]);

pair<bool, uint64_t> DebugCli::get_r(std::string_view reg) {
    if (reg == "pc") {
        return {true, static_cast<uint64_t>(debug_info.pc)};
    }
    for (int i = 0; i < 32; i++) {
        if (reg == regs[i]) {
            return {true, static_cast<uint64_t>(debug_info.regs[i])};
        }
    }
    return {false, 0};
}

pair<bool, uint32_t> DebugCli::get_m(uint32_t addr) {
    if (addr < MEM_BASE || addr + 4 > MEM_BASE + MEM_SIZE) return {false, 0};
    size_t idx = addr - MEM_BASE;
    uint32_t b0 = static_cast<uint32_t>(debug_info.mem[idx + 0]);
    uint32_t b1 = static_cast<uint32_t>(debug_info.mem[idx + 1]);
    uint32_t b2 = static_cast<uint32_t>(debug_info.mem[idx + 2]);
    uint32_t b3 = static_cast<uint32_t>(debug_info.mem[idx + 3]);
    uint32_t word = (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
    return {true, word};
}

pair<bool, uint16_t> DebugCli::get_mh(uint32_t addr) {
    if (addr < MEM_BASE || addr + 2 > MEM_BASE + MEM_SIZE) return {false, 0};
    size_t idx = addr - MEM_BASE;
    uint16_t b0 = static_cast<uint16_t>(debug_info.mem[idx + 0]);
    uint16_t b1 = static_cast<uint16_t>(debug_info.mem[idx + 1]);
    uint16_t half = (b0 | (b1 << 8));
    return {true, half};
}

pair<bool, uint8_t> DebugCli::get_mb(uint32_t addr) {
    if (addr < MEM_BASE || addr + 1 > MEM_BASE + MEM_SIZE) return {false, 0};
    size_t idx = addr - MEM_BASE;
    uint8_t b0 = static_cast<uint8_t>(debug_info.mem[idx + 0]);
    return {true, b0};
}

void DebugCli::print_wp() {
    for (auto& wp : watch_points) {
        wp.print_watchpoint();
    }
}

void DebugCli::print_bp() {
    for (auto& bp : break_points) {
        bp.print_breakpoint();
    }
}

string DebugCli::read_line() {
    termios orig;
    tcgetattr(STDIN_FILENO, &orig);
    termios raw = orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    string line;
    size_t cursor = 0;
    cout << "(debugger) " << flush;
    while (true) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig); return string(); }
        if (c == '\t') {
            size_t first_non = line.find_first_not_of(" \t\r\n\v\f");
            if (first_non != std::string::npos && cursor >= first_non) {
                size_t first_space = line.find(' ', first_non);
                size_t second_non = (first_space == std::string::npos ? std::string::npos : line.find_first_not_of(" \t\r\n\v\f", first_space + 1));
                bool in_first_token = (first_space == std::string::npos) || (second_non == std::string::npos) || (cursor <= first_space);
                if (in_first_token) {
                    size_t token_end = (first_space == std::string::npos ? cursor : std::min(cursor, first_space));
                    std::string prefix = line.substr(first_non, token_end - first_non);
                    if (!prefix.empty()) {
                        using HandlerT = std::pair<Status,int> (DebugCli::*)(std::string_view);
                        std::vector<std::pair<HandlerT, std::string>> groups;
                        for (size_t i = 0; i < NR_CMD; ++i) {
                            auto h = cmd_table[i].handler;
                            bool found = false;
                            for (auto &g : groups) {
                                if (g.first == h) {
                                    if (std::strlen(cmd_table[i].name) < g.second.size() ||
                                        (std::strlen(cmd_table[i].name) == g.second.size() && std::string(cmd_table[i].name) < g.second)) {
                                        g.second = std::string(cmd_table[i].name);
                                    }
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                groups.emplace_back(h, std::string(cmd_table[i].name));
                            }
                        }
                        std::vector<std::string> suggs;
                        for (auto &g : groups) {
                            std::vector<std::string> matches;
                            for (size_t j = 0; j < NR_CMD; ++j) {
                                if (cmd_table[j].handler == g.first) {
                                    std::string nm(cmd_table[j].name);
                                    if (nm.find(' ') == std::string::npos && nm.rfind(prefix, 0) == 0) matches.push_back(nm);
                                }
                            }
                            if (matches.empty()) continue;
                            std::string best = matches[0];
                            for (size_t k = 1; k < matches.size(); ++k) {
                                if (matches[k].size() < best.size() || (matches[k].size() == best.size() && matches[k] < best)) {
                                    best = matches[k];
                                }
                            }
                            suggs.push_back(best);
                        }
                        if (suggs.size() == 1) {
                            std::string new_line = line.substr(0, first_non) + suggs[0] + line.substr(token_end);
                            line.swap(new_line);
                            cursor = first_non + suggs[0].size();
                            cout << "\r(debugger) " << line << "\x1b[K";
                            size_t tail = line.size() - cursor;
                            if (tail > 0) cout << "\x1b[" << tail << "D";
                            cout << flush;
                        } else if (suggs.size() > 1) {
                            cout << "\n";
                            winsize ws{}; int width = 80; if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) width = ws.ws_col;
                            int colw = 30; int cols = std::max(1, width / colw);
                            for (size_t i = 0; i < suggs.size(); ++i) {
                                cout << std::left << std::setw(colw) << suggs[i];
                                if ((int)((i + 1) % cols) == 0) cout << "\n";
                            }
                            if ((suggs.size() % cols) != 0) cout << "\n";
                            cout << "(debugger) " << line << "\x1b[K";
                            size_t tail = line.size() - cursor;
                            if (tail > 0) cout << "\x1b[" << tail << "D";
                            cout << flush;
                        }
                    }
                } else {
                    size_t ts = cursor;
                    while (ts > 0) {
                        char cc = line[ts - 1];
                        if (!((cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z') || (cc >= '0' && cc <= '9') || cc == '_' || cc == '.')) break;
                        ts--;
                    }
                    std::string prefix = line.substr(ts, cursor - ts);
                    if (prefix.empty()) { continue; }
                    size_t first_non2 = line.find_first_not_of(" \t\r\n\v\f");
                    size_t first_space2 = line.find(' ', first_non2);
                    std::string base_cmd;
                    if (first_non2 != std::string::npos && first_space2 != std::string::npos) {
                        base_cmd = line.substr(first_non2, first_space2 - first_non2);
                    }
                    std::string sec_tok;
                    if (!base_cmd.empty()) {
                        size_t sec_start = line.find_first_not_of(" \t\r\n\v\f", first_space2 + 1);
                        if (sec_start != std::string::npos) {
                            size_t sec_end = line.find(' ', sec_start);
                            if (sec_end == std::string::npos) sec_end = line.size();
                            sec_tok = line.substr(sec_start, sec_end - sec_start);
                        }
                    }
                    std::vector<std::pair<std::string, uint32_t>> list;
                    bool use_regs = false;
                    if ((base_cmd == "info" || base_cmd == "i") && sec_tok == "r") {
                        use_regs = true;
                    } else if ((base_cmd == "info" || base_cmd == "i") && sec_tok == "s") {
                        use_regs = false;
                    } else if ((base_cmd == "info" || base_cmd == "i")) {
                        continue;
                    }
                    if (use_regs) {
                        std::vector<std::pair<std::string, uint32_t>> tmp;
                        std::vector<std::string> names;
                        names.push_back("pc");
                        for (int i = 0; i < 32; ++i) names.push_back(std::string(regs[i]));
                        std::sort(names.begin(), names.end());
                        for (auto &n : names) { if (n.rfind(prefix, 0) == 0) tmp.emplace_back(n, 0u); }
                        list.swap(tmp);
                    } else {
                        list = signal_list(prefix);
                    }
                    if (!list.empty()) {
                        if (list.size() == 1) {
                            std::string comp = list[0].first;
                            std::string new_line = line.substr(0, ts) + comp + line.substr(cursor);
                            line.swap(new_line);
                            cursor = ts + comp.size();
                            cout << "\r(debugger) " << line << "\x1b[K";
                            size_t tail = line.size() - cursor;
                            if (tail > 0) cout << "\x1b[" << tail << "D";
                            cout << flush;
                        } else if (list.size() <= 10) {
                            cout << "\n";
                            winsize ws{}; int width = 80; if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) width = ws.ws_col;
                            int colw = 30; int cols = std::max(1, width / colw);
                            for (size_t i = 0; i < list.size(); ++i) {
                                cout << std::left << std::setw(colw) << list[i].first;
                                if ((int)((i + 1) % cols) == 0) cout << "\n";
                            }
                            if ((list.size() % cols) != 0) cout << "\n";
                            cout << "(debugger) " << line << "\x1b[K";
                            size_t tail = line.size() - cursor;
                            if (tail > 0) cout << "\x1b[" << tail << "D";
                            cout << flush;
                        } else {
                            cout << "\n";
                            cout << "do you wish to see all " << list.size() << " possibilities?(y/n)" << flush;
                            char ans = 0;
                            if (read(STDIN_FILENO, &ans, 1) > 0) {
                                if (ans == 'y' || ans == 'Y') {
                                    cout << "\n";
                                    winsize ws{}; int width = 80; if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) width = ws.ws_col;
                                    int colw = 30; int cols = std::max(1, width / colw);
                                    for (size_t i = 0; i < list.size(); ++i) {
                                        cout << std::left << std::setw(colw) << list[i].first;
                                        if ((int)((i + 1) % cols) == 0) cout << "\n";
                                    }
                                    if ((list.size() % cols) != 0) cout << "\n";
                                } else {
                                    cout << "\n";
                                }
                            } else {
                                cout << "\n";
                            }
                            cout << "(debugger) " << line << "\x1b[K";
                            size_t tail = line.size() - cursor;
                            if (tail > 0) cout << "\x1b[" << tail << "D";
                            cout << flush;
                        }
                    }
                }
            }
            continue;
        }
        if (c == '\r' || c == '\n') {
            cout << "\r(debugger) " << line << "\x1b[K" << endl;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
            return line;
        }
        if (c == 0x7f || c == 0x08) {
            if (cursor > 0) {
                line.erase(cursor - 1, 1);
                cursor--;
                cout << "\r(debugger) " << line << "\x1b[K";
                size_t tail = line.size() - cursor;
                if (tail > 0) cout << "\x1b[" << tail << "D";
                cout << flush;
            }
            continue;
        }
        if (c == 27) {
            char seq1;
            if (read(STDIN_FILENO, &seq1, 1) <= 0) continue;
            if (seq1 == '[') {
                char seq2;
                if (read(STDIN_FILENO, &seq2, 1) <= 0) continue;
                if (seq2 == 'A') {
                    if (!history.empty()) {
                        if (history_index < 0) history_index = (int)history.size() - 1;
                        else if (history_index > 0) history_index--;
                        line = history[history_index];
                        cursor = line.size();
                        cout << "\r(debugger) " << line << "\x1b[K" << flush;
                    }
                } else if (seq2 == 'B') {
                    if (history_index >= 0) {
                        if (history_index + 1 < (int)history.size()) {
                            history_index++;
                            line = history[history_index];
                        } else {
                            history_index = -1;
                            line.clear();
                        }
                        cursor = line.size();
                        cout << "\r(debugger) " << line << "\x1b[K" << flush;
                    }
                } else if (seq2 == 'C') {
                    if (cursor < line.size()) { cout << "\x1b[1C" << flush; cursor++; }
                } else if (seq2 == 'D') {
                    if (cursor > 0) { cout << "\x1b[1D" << flush; cursor--; }
                } else if (seq2 == '3') {
                    char t;
                    if (read(STDIN_FILENO, &t, 1) > 0 && t == '~') {
                        if (cursor < line.size()) {
                            line.erase(cursor, 1);
                            cout << "\r(debugger) " << line << "\x1b[K";
                            size_t tail = line.size() - cursor;
                            if (tail > 0) cout << "\x1b[" << tail << "D";
                            cout << flush;
                        }
                    }
                }
            }
            continue;
        }
        if ((unsigned char)c >= 32) {
            line.insert(line.begin() + cursor, c);
            cursor++;
            cout << "\r(debugger) " << line << "\x1b[K";
            size_t tail = line.size() - cursor;
            if (tail > 0) cout << "\x1b[" << tail << "D";
            cout << flush;
            continue;
        }
    }
}

pair<Status, int> DebugCli::cmd_c(string_view args) {
    return {Status::RUNNING, -1};
}

pair<Status, int> DebugCli::cmd_q(string_view args) {
    return {Status::QUIT, 0};
}

pair<Status, int> DebugCli::cmd_info(string_view args) {
    auto [arg, reg] = this->split_first_token(args);

    if (arg == "r") {
        if (reg == "") {
            for (int i = 0; i < 32; i++) {
                cout << regs[i] << ": 0x" << hex << debug_info.regs[i] << dec << endl;
            }
        }
        else {
            auto [success, r] = this->get_r(reg.data());
            if (!success) {
                cout << "Invalid register: " << reg << endl;
                return {Status::PAUSE, 0};
            }
            cout << reg << ": 0x" << hex << r << dec << endl;
        }
        return {Status::PAUSE, 0};
    }
    else if (arg == "s") {
        std::string_view filter = reg;
        auto list = signal_list(filter);
        if (list.empty()) {
            if (filter.empty()) { cout << "No signals registered" << endl; }
            else { cout << "No signals matched: " << filter << endl; }
            return {Status::PAUSE, 0};
        }
        size_t count = list.size();
        size_t limit = 100;
        cout << "Signals (" << count << ")" << (filter.empty()?"":std::string(" prefix='") + std::string(filter) + "'") << ":" << endl;
        for (size_t i = 0; i < list.size() && i < limit; ++i) {
            cout << list[i].first << ": 0x" << hex << list[i].second << dec << endl;
        }
        if (list.size() > limit) {
            cout << "... (use 'info s " << filter << "' with narrower FILTER to see more)" << endl;
        }
        return {Status::PAUSE, 0};
    }
    else if (arg == "w") {
        this->print_wp();
        return {Status::PAUSE, 0};
    }
    else if (arg == "b") {
        this->print_bp();
        return {Status::PAUSE, 0};
    }
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::cmd_x(string_view args) {
    auto [first, rest] = this->split_first_token(args);
    if (first == "") {
        cout << "Invalid command" << endl;
        return {Status::PAUSE, 0};
    }

    int n = 1;
    char fmt = 'x';
    char size = 'w';
    string_view expression;

    if (first.size() > 0 && first[0] == '/') {
        size_t pos = 1;
        // parse count
        int tmpn = 0;
        while (pos < first.size() && first[pos] >= '0' && first[pos] <= '9') {
            tmpn = tmpn * 10 + (first[pos] - '0');
            pos++;
        }
        if (tmpn > 0) n = tmpn;
        if (pos < first.size()) {
            char c = first[pos];
            if (c == 'x' || c == 'd' || c == 'u' || c == 't') { fmt = c; pos++; }
        }
        if (pos < first.size()) {
            char c = first[pos];
            if (c == 'b' || c == 'h' || c == 'w') { size = c; pos++; }
        }
        expression = rest;
    } else {
        bool is_number = !first.empty();
        for (size_t i = 0; i < first.size(); ++i) {
            char c = first[i];
            if (c < '0' || c > '9') { is_number = false; break; }
        }
        if (is_number) {
            string num_s(first);
            char* endp = nullptr;
            long tmp = strtol(num_s.c_str(), &endp, 10);
            if (endp == num_s.c_str() + num_s.size()) {
                n = static_cast<int>(tmp);
                expression = rest;
            } else {
                expression = args;
            }
        } else {
            expression = args;
        }
    }

    if (expression == "") {
        cout << "Invalid command" << endl;
        return {Status::PAUSE, 0};
    }

    bool success = true;
    uint32_t addr = expr(expression, &success);
    if (success == false) {
        cout << "Invalid expression" << endl;
        return {Status::PAUSE, 0};
    }

    if (n <= 0) {
        cout << "Invalid number" << endl;
        return {Status::PAUSE, 0};
    }

    cout << "Memory:" << endl;
    int step = (size == 'b' ? 1 : (size == 'h' ? 2 : 4));
    for (int i = 0; i < n; i++) {
        uint32_t a = addr + i * step;
        if (size == 'b') {
            auto [ok, v] = this->get_mb(a);
            if (!ok) { cout << "Invalid address: 0x" << hex << a << dec << endl; return {Status::PAUSE, 0}; }
            cout << hex << a << ": ";
            switch (fmt) {
                case 'x': cout << "0x" << setw(2) << setfill('0') << hex << (unsigned)v << dec; break;
                case 'd': cout << (int8_t)v; break;
                case 'u': cout << (unsigned)v; break;
                case 't': { for (int b = 7; b >= 0; --b) cout << ((v >> b) & 1); } break;
                default: cout << (unsigned)v; break;
            }
            cout << endl;
        } else if (size == 'h') {
            auto [ok, v] = this->get_mh(a);
            if (!ok) { cout << "Invalid address: 0x" << hex << a << dec << endl; return {Status::PAUSE, 0}; }
            cout << hex << a << ": ";
            switch (fmt) {
                case 'x': cout << "0x" << setw(4) << setfill('0') << hex << (unsigned)v << dec; break;
                case 'd': cout << (int16_t)v; break;
                case 'u': cout << (unsigned)v; break;
                case 't': { for (int b = 15; b >= 0; --b) cout << ((v >> b) & 1); } break;
                default: cout << (unsigned)v; break;
            }
            cout << endl;
        } else {
            auto [ok, v] = this->get_m(a);
            if (!ok) { cout << "Invalid address: 0x" << hex << a << dec << endl; return {Status::PAUSE, 0}; }
            cout << hex << a << ": ";
            switch (fmt) {
                case 'x': cout << "0x" << setw(8) << setfill('0') << hex << v << dec; break;
                case 'd': cout << (int32_t)v; break;
                case 'u': cout << (uint32_t)v; break;
                case 't': { for (int b = 31; b >= 0; --b) cout << ((v >> b) & 1); } break;
                default: cout << v; break;
            }
            cout << endl;
        }
    }
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::cmd_disas(string_view args) {
    auto [first, rest] = this->split_first_token(args);
    if (!disasm_try_init_from_env()) {
        cout << "Disassembler not initialized. Set NPC_DISASM_TXT or NPC_ELF_PATH/NPC_PROGRAM_PATH to locate objdump output." << endl;
        return {Status::PAUSE, 0};
    }

    uint32_t addr = (uint32_t)debug_info.pc;
    int n = 10;

    if (first != "") {
        bool is_number = true;
        for (size_t i = 0; i < first.size(); ++i) {
            char c = first[i];
            if (c < '0' || c > '9') { is_number = false; break; }
        }
        if (is_number) {
            try { n = stoi(string(first)); } catch (...) {}
        } else {
            bool success = true;
            addr = expr(first, &success);
            if (!success) {
                cout << "Invalid address expression" << endl;
                return {Status::PAUSE, 0};
            }
            if (!rest.empty()) {
                try { n = stoi(string(rest)); } catch (...) {}
            }
        }
    }

    if (n <= 0) n = 1;
    if (!disasm_has_addr(addr)) {
        cout << "disas: no objdump entry at address 0x" << hex << addr << dec << endl;
        return {Status::PAUSE, 0};
    }
    for (int i = 0; i < n; ++i) {
        uint32_t a = addr + i * 4;
        cout << disasm_line_for_addr(a) << endl;
    }
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::cmd_p(string_view args) {
    auto [first, rest] = this->split_first_token(args);
    char fmt = 0;
    char size = 0;
    string_view expression = args;
    if (!first.empty() && first[0] == '/') {
        size_t pos = 1;
        if (pos < first.size()) {
            char c = first[pos];
            if (c == 'x' || c == 'd' || c == 'u' || c == 't') { fmt = c; pos++; }
        }
        if (pos < first.size()) {
            char c = first[pos];
            if (c == 'b' || c == 'h' || c == 'w') { size = c; pos++; }
        }
        expression = rest;
    }

    bool success = true;
    uint32_t result = expr(expression, &success);
    if (!success) {
        cout << "Invalid expression" << endl;
        return {Status::PAUSE, 0};
    }
    if (size == 'b') result &= 0xFFu;
    else if (size == 'h') result &= 0xFFFFu;
    else if (size == 'w' || size == 0) {}

    if (fmt == 'x') { cout << "0x" << hex << result << dec << endl; }
    else if (fmt == 'd') { cout << (int32_t)result << endl; }
    else if (fmt == 'u') { cout << (uint32_t)result << endl; }
    else if (fmt == 't') { for (int b = ((size=='b')?7: (size=='h'?15:31)); b>=0; --b) cout << ((result>>b)&1); cout << endl; }
    else { cout << result << endl; }
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::cmd_w(string_view args) {
    // trim full expression (support spaces inside expression)
    string_view expr_sv = args;
    auto first = expr_sv.find_first_not_of(" \t\r\n\v\f");
    if (first == string_view::npos) {
        cout << "Invalid command" << endl;
        return {Status::PAUSE, 0};
    }
    expr_sv.remove_prefix(first);
    auto last = expr_sv.find_last_not_of(" \t\r\n\v\f");
    if (last != string_view::npos && last + 1 < expr_sv.size()) {
        expr_sv.remove_suffix(expr_sv.size() - last - 1);
    }

    bool success = true;
    (void)expr(expr_sv, &success);
    if (!success) {
        cout << "Invalid expression: " << expr_sv << endl;
        return {Status::PAUSE, 0};
    }
    watch_points.emplace_back(std::string(expr_sv));
    watch_points.back().add_watchpoint();
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::cmd_d(string_view args) {
    auto [arg, _] = this->split_first_token(args);
    if (arg == "") {
        cout << "Invalid command" << endl;
        return {Status::PAUSE, 0};
    }
    int id = stoi(string(arg));
    // Try delete breakpoint first
    size_t idx_bp = break_points.size();
    for (size_t i = 0; i < break_points.size(); ++i) {
        if (break_points[i].get_id() == id) { idx_bp = i; break; }
    }
    if (idx_bp != break_points.size()) {
        break_points.erase(break_points.begin() + idx_bp);
        cout << "Breakpoint " << id << " deleted" << endl;
        return {Status::PAUSE, 0};
    }
    // Then try watchpoint
    size_t idx_wp = watch_points.size();
    for (size_t i = 0; i < watch_points.size(); ++i) {
        if (watch_points[i].get_id() == id) { idx_wp = i; break; }
    }
    if (idx_wp != watch_points.size()) {
        watch_points[idx_wp].print_unset();
        watch_points.erase(watch_points.begin() + idx_wp);
        return {Status::PAUSE, 0};
    }
    cout << "Invalid id: " << id << endl;
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::cmd_b(string_view args) {
    string_view full = args;
    auto first_pos = full.find_first_not_of(" \t\r\n\v\f");
    if (first_pos == string_view::npos) { cout << "Invalid command" << endl; return {Status::PAUSE, 0}; }
    full.remove_prefix(first_pos);
    if (!full.empty() && full[0] == '*') {
        full.remove_prefix(1);
        string cond;
        size_t if_pos = full.find(" if ");
        string_view addr_sv = full;
        if (if_pos != string_view::npos) {
            addr_sv = full.substr(0, if_pos);
            string_view cond_sv = full.substr(if_pos + 4);
            auto cs_first = cond_sv.find_first_not_of(" \t\r\n\v\f");
            if (cs_first != string_view::npos) cond_sv.remove_prefix(cs_first);
            auto cs_last = cond_sv.find_last_not_of(" \t\r\n\v\f");
            if (cs_last != string_view::npos && cs_last + 1 < cond_sv.size()) cond_sv.remove_suffix(cond_sv.size() - cs_last - 1);
            cond = std::string(cond_sv);
        }
        auto as_first = addr_sv.find_first_not_of(" \t\r\n\v\f");
        if (as_first != string_view::npos) addr_sv.remove_prefix(as_first);
        auto as_last = addr_sv.find_last_not_of(" \t\r\n\v\f");
        if (as_last != string_view::npos && as_last + 1 < addr_sv.size()) addr_sv.remove_suffix(addr_sv.size() - as_last - 1);
        bool success = true;
        uint32_t addr = expr(addr_sv, &success);
        if (!success) { cout << "Invalid address expression" << endl; return {Status::PAUSE, 0}; }
        break_points.emplace_back(addr, cond);
        break_points.back().add_breakpoint();
        return {Status::PAUSE, 0};
    } else {
        string_view sv = full;
        if (sv.substr(0, 3) == "if ") {
            string_view cond_sv = sv.substr(3);
            auto cs_first = cond_sv.find_first_not_of(" \t\r\n\v\f");
            if (cs_first != string_view::npos) cond_sv.remove_prefix(cs_first);
            auto cs_last = cond_sv.find_last_not_of(" \t\r\n\v\f");
            if (cs_last != string_view::npos && cs_last + 1 < cond_sv.size()) cond_sv.remove_suffix(cond_sv.size() - cs_last - 1);
            bool success = true;
            (void)expr(cond_sv, &success);
            if (!success) { cout << "Invalid condition expression" << endl; return {Status::PAUSE, 0}; }
            break_points.emplace_back(std::string(cond_sv));
            break_points.back().add_breakpoint();
            return {Status::PAUSE, 0};
        }
        cout << "Invalid command" << endl;
        return {Status::PAUSE, 0};
    }
}

pair<Status, int> DebugCli::cmd_delete(string_view args) {
    auto [arg, _] = this->split_first_token(args);
    if (arg == "") { cout << "Invalid command" << endl; return {Status::PAUSE, 0}; }
    int id = stoi(string(arg));
    size_t idx = break_points.size();
    for (size_t i = 0; i < break_points.size(); ++i) {
        if (break_points[i].get_id() == id) { idx = i; break; }
    }
    if (idx == break_points.size()) { cout << "Invalid breakpoint id: " << arg << endl; return {Status::PAUSE, 0}; }
    break_points.erase(break_points.begin() + idx);
    cout << "Breakpoint " << id << " deleted" << endl;
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::cmd_si(string_view args) {
    auto [num, _] = this->split_first_token(args);
    int n = 1;
    if (num != "") {
        try {
            n = stoi(string(num));
        } catch (...) {
            cout << "Invalid step count: " << num << endl;
            return {Status::PAUSE, 0};
        }
    }
    if (n <= 0) {
        cout << "Invalid step count: " << n << endl;
        return {Status::PAUSE, 0};
    }
    if (n > 1000000) {
        cout << "Step count too large, capped to 1000000" << endl;
        n = 1000000;
    }
    return {Status::RUNNING, n};
}

pair<Status, int> DebugCli::cmd_help(string_view args) {
    /* extract the first argument */
    auto [arg, _] = this->split_first_token(args);
    int i;
    
    if (arg == "") {
        /* no argument given */
        for (i = 0; i < NR_CMD; i ++) {
            cout << cmd_table[i].name << " - " << cmd_table[i].description << endl;
        }
    }
    else {
        for (i = 0; i < NR_CMD; i ++) {
            if (arg == cmd_table[i].name) {
                cout << cmd_table[i].name << " - " << cmd_table[i].description << endl;
                return {Status::PAUSE, 0};
            }
        }
        cout << "Unknown command '" << arg << "'" << endl;
    }
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::cmd_until(string_view args) {
    string_view full = args;
    auto first_pos = full.find_first_not_of(" \t\r\n\v\f");
    if (first_pos == string_view::npos) { cout << "Invalid command" << endl; return {Status::PAUSE, 0}; }
    full.remove_prefix(first_pos);
    if (!full.empty() && full[0] == '*') {
        full.remove_prefix(1);
    }
    auto last_pos = full.find_last_not_of(" \t\r\n\v\f");
    if (last_pos != string_view::npos && last_pos + 1 < full.size()) {
        full.remove_suffix(full.size() - last_pos - 1);
    }
    bool success = true;
    uint32_t addr = expr(full, &success);
    if (!success) { cout << "Invalid address expression" << endl; return {Status::PAUSE, 0}; }
    set_temp_breakpoint(addr);
    cout << "until set at 0x" << hex << addr << dec << endl;
    return {Status::RUNNING, -1};
}

pair<Status, int> DebugCli::cmd_trace(string_view args) {
    auto [first, rest] = this->split_first_token(args);
    if (first == "") {
        return {Status::PAUSE, 0};
    }
    if (first == "on") { 
        itrace_enable(true); 
        cout << "trace enabled" << endl;
        cout << "  itrace: size=" << itrace_get_capacity() << ", file=" << itrace_get_file_path() << endl;
        return {Status::PAUSE, 0}; 
    }
    if (first == "off") { itrace_enable(false); return {Status::PAUSE, 0}; }
    if (first == "file") { cout << "use 'trace itrace on --file PATH'" << endl; return {Status::PAUSE, 0}; }
    if (first == "itrace") {
        // parse: on/off and options; support options without 'on'
        auto [opt, tail] = this->split_first_token(rest);
        auto parse_itrace_opts = [&](std::string s){
            int set_size = 0;
            std::string set_file;
            std::vector<std::string> toks;
            {
                std::string cur;
                for (size_t i = 0; i < s.size(); ++i) { char c = s[i]; if (c == ' ') { if (!cur.empty()) { toks.push_back(cur); cur.clear(); } } else { cur.push_back(c); } }
                if (!cur.empty()) toks.push_back(cur);
            }
            bool changed = false;
            for (size_t i = 0; i < toks.size(); ++i) {
                std::string t = toks[i];
                if (t.rfind("--size=", 0) == 0) { try { set_size = stoi(t.substr(7)); } catch (...) { set_size = 0; } changed = true; }
                else if (t == "--size") { if (i + 1 < toks.size()) { try { set_size = stoi(toks[++i]); } catch (...) { set_size = 0; } changed = true; } }
                else if (t.rfind("--file=", 0) == 0) { set_file = t.substr(7); changed = true; }
                else if (t == "--file") { if (i + 1 < toks.size()) { set_file = toks[++i]; changed = true; } }
            }
            if (set_size > 0) { itrace_set_capacity((size_t)set_size); cout << "itrace capacity set to " << set_size << endl; }
            if (!set_file.empty()) { itrace_open_file(set_file); cout << "itrace file set to " << set_file << endl; }
            if (!changed) { cout << "Invalid trace command" << endl; }
        };
        if (opt == "on") { 
            itrace_enable(true); 
            if (!tail.empty()) parse_itrace_opts(std::string(tail));
            cout << "itrace enabled: size=" << itrace_get_capacity() << ", file=" << itrace_get_file_path() << endl;
            return {Status::PAUSE, 0}; 
        }
        if (opt == "off") { itrace_enable(false); return {Status::PAUSE, 0}; }
        // options without 'on'
        parse_itrace_opts(std::string(rest));
        return {Status::PAUSE, 0};
    }
    return {Status::PAUSE, 0};
}

pair<Status, int> DebugCli::mainloop(DebugInfo* debug_info) {
    this->debug_info = *debug_info;
    expr_set_context(this->debug_info.regs, this->debug_info.mem, this->debug_info.pc, MEM_BASE, MEM_SIZE);
    int forward = 0;
    Status status = Status::PAUSE;
    
    string line = read_line();
    if (line.size() > 0) { history.push_back(line); history_index = -1; }
    if (line.empty()) {
        return {Status::PAUSE, 0};
    }
    vector<char> buf(line.begin(), line.end());
    buf.push_back('\0');
    char* str = buf.data();
    char* str_end = str + strlen(str);

    char* cmd = strtok(str, " ");
    if (cmd == nullptr) { return {Status::PAUSE, 0}; }

    char* args = cmd + strlen(cmd) + 1;
    std::string_view args_sv;
    if (args >= str_end) { args_sv = {}; }
    else { args_sv = std::string_view(args, str_end - args); }

    // Support commands with inline options like x/8xw or p/x by splitting at '/'
    std::string base_cmd(cmd);
    size_t slash_pos = base_cmd.find('/');
    std::string combined_args;
    if (slash_pos != std::string::npos) {
        std::string tail = base_cmd.substr(slash_pos); // includes '/'
        base_cmd = base_cmd.substr(0, slash_pos);
        combined_args = tail;
        if (!args_sv.empty()) {
            combined_args.push_back(' ');
            combined_args.append(std::string(args_sv));
        }
    }
    std::string_view final_args_sv = args_sv;
    if (!combined_args.empty()) {
        final_args_sv = std::string_view(combined_args);
    }

    bool matched = false;
    for (size_t i = 0; i < NR_CMD; i++) {
        if (base_cmd == cmd_table[i].name) {
            tie(status, forward) = (this->*(cmd_table[i].handler))(final_args_sv);
            matched = true;
            break;
        }
    }
    if (!matched) {
        cout << "Unknown command '" << base_cmd << "'" << endl;
    }
    return {status, forward};
}
