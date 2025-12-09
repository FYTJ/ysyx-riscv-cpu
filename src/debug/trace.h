#ifndef TRACE_H
#define TRACE_H

#include <string>
#include <vector>
#include <cstdint>

void trace_init();
void itrace_enable(bool en);
void itrace_open_file(const std::string& path);
void itrace_close_file();
void itrace_set_capacity(size_t n);
void trace_step(uint32_t pc);
std::vector<std::string> itrace_recent();
void trace_flush();
std::string itrace_get_file_path();
size_t itrace_get_capacity();

#endif
