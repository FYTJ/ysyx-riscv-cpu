#ifndef EXPR_H
#define EXPR_H

#include <vector>
#include <string>
#include <regex>
#include <cstdint>
#include <string_view>
#include "debug.h"

using namespace std;

void expr_set_context(uint32_t* regs, uint8_t* mem, uint64_t pc, uint32_t mem_base, uint32_t mem_size);
uint32_t expr(string_view expr, bool* success);

#endif