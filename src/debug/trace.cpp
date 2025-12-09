#include "trace.h"
#include "disasm.h"
#include "debug.h"
#include "../minirv/cpp/register.h"
#include <deque>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

static bool g_itrace_en = false;
static std::string g_itrace_path = "itrace.log";
static std::ofstream g_itrace_out;
static const char* DEFAULT_TRACE_DIR = "/mnt/hgfs/zhuyanbo/Desktop/风云天际/一生一芯/ysyx-workbench/npc/build/trace";
static int g_indent_spaces = 2;
static std::deque<std::string> g_itrace_buf;
static size_t g_itrace_cap = 64;

extern "C" int mem_read_word(int addr);

static inline void open_itrace_file() {
    if (g_itrace_out.is_open()) return;
    std::filesystem::path p(g_itrace_path);
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    g_itrace_out.open(g_itrace_path, std::ios::out | std::ios::trunc);
}

void trace_init() {
    const char* elf_env = getenv("NPC_ELF_PATH");
    const char* bin_env = getenv("NPC_PROGRAM_PATH");
    const char* txt_env = getenv("NPC_DISASM_TXT");
    bool has_elf = (elf_env && elf_env[0] != '\0');
    bool has_bin = (bin_env && bin_env[0] != '\0');
    bool has_txt = (txt_env && txt_env[0] != '\0');

    std::string build_dir = "/mnt/hgfs/zhuyanbo/Desktop/风云天际/一生一芯/ysyx-workbench/am-kernels/tests/cpu-tests/build";
    std::string found_bin, found_elf, found_txt;
    if (std::filesystem::exists(build_dir)) {
        for (auto& p : std::filesystem::directory_iterator(build_dir)) {
            if (!p.is_regular_file()) continue;
            auto path = p.path().string();
            if (path.find("-minirv-npc.bin") != std::string::npos) found_bin = path;
            else if (path.find("-minirv-npc.elf") != std::string::npos) found_elf = path;
            else if (path.find("-minirv-npc.txt") != std::string::npos) found_txt = path;
        }
    }

    if (!has_bin && !found_bin.empty()) setenv("NPC_PROGRAM_PATH", found_bin.c_str(), 1);
    if (!has_elf) {
        std::string candidate_elf;
        const char* cur_bin = getenv("NPC_PROGRAM_PATH");
        if (cur_bin && cur_bin[0] != '\0') {
            std::string bin_path(cur_bin);
            if (bin_path.size() >= 4 && bin_path.substr(bin_path.size()-4) == ".bin") {
                candidate_elf = bin_path.substr(0, bin_path.size()-4) + ".elf";
            }
        }
        if (!candidate_elf.empty() && std::filesystem::exists(candidate_elf)) {
            setenv("NPC_ELF_PATH", candidate_elf.c_str(), 1);
        } else if (!found_elf.empty()) {
            setenv("NPC_ELF_PATH", found_elf.c_str(), 1);
        }
    }
    if (!has_txt && !found_txt.empty()) setenv("NPC_DISASM_TXT", found_txt.c_str(), 1);

    disasm_try_init_from_env();
    g_itrace_path = std::string(DEFAULT_TRACE_DIR) + "/itrace.log";
}

void itrace_enable(bool en) {
    g_itrace_en = en;
    if (en && !g_itrace_out.is_open()) open_itrace_file();
}

void itrace_open_file(const std::string& path) {
    g_itrace_path = path;
    if (g_itrace_out.is_open()) g_itrace_out.close();
    open_itrace_file();
}

void itrace_close_file() {
    if (g_itrace_out.is_open()) g_itrace_out.close();
}

void itrace_set_capacity(size_t n) {
    if (n == 0) n = 1;
    g_itrace_cap = n;
    while (g_itrace_buf.size() > g_itrace_cap) g_itrace_buf.pop_front();
}

static inline uint32_t decode_opcode(uint32_t inst) { return inst & 0x7Fu; }

static void record_itrace(uint32_t pc) {
    std::string line = disasm_line_for_addr(pc);
    if (g_itrace_buf.size() >= g_itrace_cap) g_itrace_buf.pop_front();
    g_itrace_buf.push_back(line);
}

void trace_step(uint32_t pc) {
    if (g_itrace_en) record_itrace(pc);
}

std::vector<std::string> itrace_recent() {
    return std::vector<std::string>(g_itrace_buf.begin(), g_itrace_buf.end());
}

void trace_flush() {
    if (g_itrace_en) {
        if (!g_itrace_out.is_open()) open_itrace_file();
        if (g_itrace_out.is_open()) {
            for (auto &line : g_itrace_buf) {
                g_itrace_out << line << "\n";
            }
            g_itrace_out.flush();
        }
    }
}

std::string itrace_get_file_path() { return g_itrace_path; }
size_t itrace_get_capacity() { return g_itrace_cap; }
