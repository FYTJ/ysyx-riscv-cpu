#ifndef WATCHPOINT_H
#define WATCHPOINT_H

#include <string>
#include <vector>
#include <cstdint>
#include "debug.h"

using namespace std;

enum class Status;

class WatchPoint {
private:
    int id;
    uint32_t prev_val;
    uint32_t cur_val;
    std::string expression;
public:
    WatchPoint(std::string expression);
    ~WatchPoint();
    void add_watchpoint();
    bool update_watchpoint();
    void print_watchpoint();
    void print_unset();
    int get_id() const { return id; }
};

extern vector<WatchPoint> watch_points;

extern Status watchpoint_update_all();

#endif
