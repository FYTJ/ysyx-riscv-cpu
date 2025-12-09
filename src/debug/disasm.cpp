#include "disasm.h"
#include "debug.h"
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <elf.h>
#include <vector>
#include <cstring>
#include <elf.h>

static std::unordered_map<uint32_t, std::string> g_disasm_lines;
static bool g_inited = false;
static bool g_use_abs_addr = false;
static uint32_t g_text_base = 0;
static std::unordered_map<uint32_t, std::string> g_symbols;
static std::vector<uint32_t> g_symbol_addrs;

bool disasm_load_from_txt(const char* path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    g_disasm_lines.clear();
    std::string line;
    std::regex re_addr(R"(^\s*([0-9a-fA-F]+):\s*(.*)$)");
    std::regex re_sym(R"(^\s*([0-9a-fA-F]+)\s+<([^>]+)>:\s*$)");
    uint32_t min_addr = 0xFFFFFFFFu;
    uint32_t max_addr = 0;
    size_t abs_cnt = 0;
    while (std::getline(in, line)) {
        std::smatch m;
        if (std::regex_match(line, m, re_addr)) {
            std::string addr_hex = m[1].str();
            uint32_t addr = (uint32_t)strtoul(addr_hex.c_str(), nullptr, 16);
            g_disasm_lines[addr] = line;
            if (addr < min_addr) min_addr = addr;
            if (addr > max_addr) max_addr = addr;
            if (addr >= MEM_BASE) abs_cnt++;
        } else if (std::regex_match(line, m, re_sym)) {
            std::string addr_hex = m[1].str();
            uint32_t addr = (uint32_t)strtoul(addr_hex.c_str(), nullptr, 16);
            std::string name = m[2].str();
            g_symbols[addr] = name;
            g_symbol_addrs.push_back(addr);
        }
    }
    g_inited = !g_disasm_lines.empty();
    if (g_inited) {
        if (abs_cnt > 0) g_use_abs_addr = true;
        else g_use_abs_addr = false;
    }
    return g_inited;
}

static std::string replace_suffix(const std::string& s, const std::string& old_suf, const std::string& new_suf) {
    if (s.size() >= old_suf.size() && s.substr(s.size() - old_suf.size()) == old_suf) {
        return s.substr(0, s.size() - old_suf.size()) + new_suf;
    }
    return std::string();
}

bool disasm_try_init_from_env() {
    if (g_inited) return true;
    const char* txt = getenv("NPC_DISASM_TXT");
    if (txt && disasm_load_from_txt(txt)) return true;
    const char* bin = getenv("NPC_PROGRAM_PATH");
    if (bin) {
        std::string txt_guess = replace_suffix(std::string(bin), ".bin", ".txt");
        if (!txt_guess.empty()) {
            if (disasm_load_from_txt(txt_guess.c_str())) return true;
        }
    }
    const char* elf = getenv("NPC_ELF_PATH");
    if (elf) {
        std::string txt_guess = replace_suffix(std::string(elf), ".elf", ".txt");
        if (!txt_guess.empty()) {
            if (disasm_load_from_txt(txt_guess.c_str())) return true;
        }
        disasm_load_symbols_from_elf(elf);
        return g_inited || !g_symbol_addrs.empty();
    }
    return false;
}

bool disasm_initialized() { return g_inited; }

static inline uint32_t map_addr(uint32_t addr) {
    if (g_use_abs_addr) return addr;
    if (addr >= MEM_BASE) return addr - MEM_BASE;
    return addr;
}

bool disasm_has_addr(uint32_t addr) {
    auto it = g_disasm_lines.find(map_addr(addr));
    return it != g_disasm_lines.end();
}

std::string disasm_line_for_addr(uint32_t addr) {
    auto it = g_disasm_lines.find(map_addr(addr));
    if (it != g_disasm_lines.end()) return it->second;
    std::ostringstream os;
    os << std::hex << std::setw(8) << std::setfill('0') << addr << ":\t" << "(no objdump)" << std::dec;
    return os.str();
}

std::string disasm_func_for_addr(uint32_t addr) {
    uint32_t a = map_addr(addr);
    if (g_symbols.empty()) return std::string();
    // find nearest symbol <= a
    uint32_t best = 0;
    bool found = false;
    for (uint32_t sa : g_symbol_addrs) {
        if (sa <= a) {
            if (!found || sa > best) { best = sa; found = true; }
        }
    }
    if (found) {
        auto it = g_symbols.find(best);
        if (it != g_symbols.end()) return it->second;
    }
    return std::string();
}


bool disasm_load_symbols_from_elf(const char* path) {
    g_symbols.clear();
    g_symbol_addrs.clear();
    g_text_base = 0;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    // Read ELF header
    Elf64_Ehdr eh64{};
    in.read(reinterpret_cast<char*>(&eh64), sizeof(eh64));
    if (!in.good()) return false;
    if (eh64.e_ident[EI_MAG0] != ELFMAG0 || eh64.e_ident[EI_MAG1] != ELFMAG1 ||
        eh64.e_ident[EI_MAG2] != ELFMAG2 || eh64.e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }

    bool is64 = (eh64.e_ident[EI_CLASS] == ELFCLASS64);
    bool isLE = (eh64.e_ident[EI_DATA] == ELFDATA2LSB);
    if (!isLE) return false;

    auto read_section = [&](uint64_t offset, uint64_t size, std::vector<char>& buf) {
        buf.resize((size_t)size);
        in.seekg((std::streamoff)offset, std::ios::beg);
        in.read(buf.data(), (std::streamsize)size);
        return in.good();
    };

    
    
    
    
    
    // Load section headers and section header string table
    std::vector<char> shdrs;
    uint64_t shoff = is64 ? eh64.e_shoff : ((Elf32_Ehdr*)&eh64)->e_shoff;
    uint16_t shentsize = is64 ? eh64.e_shentsize : ((Elf32_Ehdr*)&eh64)->e_shentsize;
    uint16_t shnum = is64 ? eh64.e_shnum : ((Elf32_Ehdr*)&eh64)->e_shnum;
    uint16_t shstrndx = is64 ? eh64.e_shstrndx : ((Elf32_Ehdr*)&eh64)->e_shstrndx;
    if (!read_section(shoff, (uint64_t)shentsize * shnum, shdrs)) return false;

    auto get_shdr64 = [&](int i) -> const Elf64_Shdr* {
        return reinterpret_cast<const Elf64_Shdr*>(shdrs.data() + (size_t)i * shentsize);
    };
    auto get_shdr32 = [&](int i) -> const Elf32_Shdr* {
        return reinterpret_cast<const Elf32_Shdr*>(shdrs.data() + (size_t)i * shentsize);
    };

    // Read section names
    std::vector<char> shstrtab;
    uint64_t shstr_off = 0, shstr_size = 0;
    if (is64) {
        shstr_off = get_shdr64(shstrndx)->sh_offset;
        shstr_size = get_shdr64(shstrndx)->sh_size;
    } else {
        shstr_off = get_shdr32(shstrndx)->sh_offset;
        shstr_size = get_shdr32(shstrndx)->sh_size;
    }
    if (!read_section(shstr_off, shstr_size, shstrtab)) return false;

    auto sec_name = [&](uint32_t name_off) -> const char* {
        if (name_off >= shstrtab.size()) return "";
        return shstrtab.data() + name_off;
    };

    int sym_sec_idx = -1, str_sec_idx = -1;
    int text_sec_idx = -1;
    for (int i = 0; i < shnum; ++i) {
        uint32_t name = is64 ? get_shdr64(i)->sh_name : get_shdr32(i)->sh_name;
        const char* nm = sec_name(name);
        if (!nm) continue;
        if (std::strcmp(nm, ".symtab") == 0 || std::strcmp(nm, ".dynsym") == 0) {
            sym_sec_idx = i;
        }
        if (std::strcmp(nm, ".strtab") == 0 || std::strcmp(nm, ".dynstr") == 0) {
            str_sec_idx = i;
        }
        if (std::strcmp(nm, ".text") == 0) {
            text_sec_idx = i;
        }
    }
    if (sym_sec_idx < 0 || str_sec_idx < 0) return false;

    // Read symbol and string tables
    std::vector<char> symtab, strtab;
    uint64_t sym_off = is64 ? get_shdr64(sym_sec_idx)->sh_offset : get_shdr32(sym_sec_idx)->sh_offset;
    uint64_t sym_size = is64 ? get_shdr64(sym_sec_idx)->sh_size : get_shdr32(sym_sec_idx)->sh_size;
    uint64_t sym_entsize = is64 ? get_shdr64(sym_sec_idx)->sh_entsize : get_shdr32(sym_sec_idx)->sh_entsize;
    uint64_t str_off = is64 ? get_shdr64(str_sec_idx)->sh_offset : get_shdr32(str_sec_idx)->sh_offset;
    uint64_t str_size = is64 ? get_shdr64(str_sec_idx)->sh_size : get_shdr32(str_sec_idx)->sh_size;
    if (!read_section(sym_off, sym_size, symtab)) return false;
    if (!read_section(str_off, str_size, strtab)) return false;

    auto name_from_str = [&](uint32_t idx) -> const char* {
        if (idx >= strtab.size()) return "";
        return strtab.data() + idx;
    };

    size_t count = (size_t)(sym_size / sym_entsize);
    for (size_t i = 0; i < count; ++i) {
        if (is64) {
            const Elf64_Sym* s = reinterpret_cast<const Elf64_Sym*>(symtab.data() + i * sym_entsize);
            uint8_t type = ELF64_ST_TYPE(s->st_info);
            if (type == STT_FUNC || type == STT_NOTYPE) {
                const char* nm = name_from_str(s->st_name);
                if (nm && *nm) {
                    uint32_t addr = (uint32_t)s->st_value;
                    g_symbols[addr] = nm;
                    g_symbol_addrs.push_back(addr);
                }
            }
        } else {
            const Elf32_Sym* s = reinterpret_cast<const Elf32_Sym*>(symtab.data() + i * sym_entsize);
            uint8_t type = ELF32_ST_TYPE(s->st_info);
            if (type == STT_FUNC || type == STT_NOTYPE) {
                const char* nm = name_from_str(s->st_name);
                if (nm && *nm) {
                    uint32_t addr = (uint32_t)s->st_value;
                    g_symbols[addr] = nm;
                    g_symbol_addrs.push_back(addr);
                }
            }
        }
    }

    if (text_sec_idx >= 0) {
        uint64_t sh_addr = is64 ? get_shdr64(text_sec_idx)->sh_addr : get_shdr32(text_sec_idx)->sh_addr;
        g_text_base = (uint32_t)sh_addr;
    }
    bool has_abs = false;
    for (uint32_t a : g_symbol_addrs) { if (a >= MEM_BASE) { has_abs = true; break; } }
    if (!has_abs && g_text_base >= MEM_BASE) has_abs = true;
    g_use_abs_addr = has_abs;

    return !g_symbol_addrs.empty();
}
