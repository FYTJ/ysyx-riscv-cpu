#ifndef DEBUG_CLI_H
#define DEBUG_CLI_H

#include "debug.h"
#include "breakpoint.h"
#include "disasm.h"
#include <vector>
#include <cstdint>
#include <string_view>

using namespace std;

class DebugCli {
private:
    DebugInfo debug_info;
    inline pair<string_view, string_view> split_first_token(string_view s);
    pair<bool, uint64_t> get_r(string_view reg);
    pair<bool, uint32_t> get_m(uint32_t addr);
    pair<bool, uint16_t> get_mh(uint32_t addr);
    pair<bool, uint8_t> get_mb(uint32_t addr);
    void print_wp();
    void print_bp();
    string read_line();
    vector<string> history;
    int history_index = -1;
public:
    DebugCli();
    pair<Status, int> mainloop(DebugInfo* debug_info);
    pair<Status, int> cmd_c(string_view args);
    pair<Status, int> cmd_q(string_view args);
    pair<Status, int> cmd_help(string_view args);
    pair<Status, int> cmd_si(string_view args);
    pair<Status, int> cmd_info(string_view args);
    pair<Status, int> cmd_x(string_view args);
    pair<Status, int> cmd_p(string_view args);
    pair<Status, int> cmd_w(string_view args);
    pair<Status, int> cmd_d(string_view args);
    pair<Status, int> cmd_b(string_view args);
    pair<Status, int> cmd_delete(string_view args);
    pair<Status, int> cmd_disas(string_view args);
    pair<Status, int> cmd_until(string_view args);
    pair<Status, int> cmd_trace(string_view args);
};

struct CmdEntry {
    const char *name;
    const char *description;
    pair<Status, int> (DebugCli::*handler) (string_view);
};

#endif
