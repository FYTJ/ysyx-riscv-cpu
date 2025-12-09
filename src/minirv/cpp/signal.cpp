#include "signal.h"
#include <mutex>
#include <algorithm>

static std::unordered_map<int, SignalInfo> g_by_id;
static std::unordered_map<std::string, int> g_name_to_id;
static std::mutex g_mtx;

extern "C" void npc_signal_register(const char* name, int width, int id) {
    std::lock_guard<std::mutex> lk(g_mtx);
    std::string n(name ? name : "");
    auto it = g_by_id.find(id);
    if (it == g_by_id.end()) {
        SignalInfo si{n, width, 0u};
        g_by_id.emplace(id, si);
        g_name_to_id[n] = id;
    } else {
        it->second.name = n;
        it->second.width = width;
        g_name_to_id[n] = id;
    }
}

extern "C" void npc_signal_update(int id, int value) {
    std::lock_guard<std::mutex> lk(g_mtx);
    auto it = g_by_id.find(id);
    if (it != g_by_id.end()) {
        it->second.value = static_cast<uint32_t>(value);
    }
}

bool signal_get(std::string_view name, uint32_t &value) {
    std::lock_guard<std::mutex> lk(g_mtx);
    auto itn = g_name_to_id.find(std::string(name));
    if (itn == g_name_to_id.end()) return false;
    auto iti = g_by_id.find(itn->second);
    if (iti == g_by_id.end()) return false;
    value = iti->second.value;
    return true;
}

std::vector<std::pair<std::string, uint32_t>> signal_list(std::string_view prefix) {
    std::lock_guard<std::mutex> lk(g_mtx);
    std::vector<std::pair<std::string, uint32_t>> res;
    std::string p(prefix);
    for (const auto &kv : g_by_id) {
        const auto &si = kv.second;
        if (p.empty() || si.name.rfind(p, 0) == 0) {
            res.emplace_back(si.name, si.value);
        }
    }
    std::sort(res.begin(), res.end(), [](auto &a, auto &b){ return a.first < b.first; });
    return res;
}
