#ifndef DISASM_H
#define DISASM_H

#include <string>
#include <cstdint>

bool disasm_try_init_from_env();
bool disasm_load_from_txt(const char* path);
bool disasm_load_symbols_from_elf(const char* path);
bool disasm_initialized();
bool disasm_has_addr(uint32_t addr);
std::string disasm_line_for_addr(uint32_t addr);
std::string disasm_func_for_addr(uint32_t addr);

#endif
