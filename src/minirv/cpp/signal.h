#ifndef SIGNAL_H
#define SIGNAL_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct SignalInfo {
    std::string name;
    int width;
    uint32_t value;
};

extern "C" void npc_signal_register(const char* name, int width, int id);
extern "C" void npc_signal_update(int id, int value);
bool signal_get(std::string_view name, uint32_t &value);
std::vector<std::pair<std::string, uint32_t>> signal_list(std::string_view prefix);

#endif
