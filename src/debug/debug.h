#ifndef DEBUG_H
#define DEBUG_H

#include <vector>
#include <cstdint>
#include <string_view>
#include "../minirv/cpp/register.h"
#include "../minirv/cpp/memory.h"

using namespace std;

#define NR_WP 10

const string_view regs[] = {
    "x0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

enum class Status {
    RUNNING,
    PAUSE,
    QUIT
};

typedef struct DebugInfo {
    uint64_t pc;
    uint32_t *regs;
    uint8_t *mem;
} DebugInfo;

#endif