#ifndef BREAKPOINT_H
#define BREAKPOINT_H

#include <string>
#include <vector>
#include <cstdint>
#include "debug.h"

using namespace std;

class BreakPoint {
private:
    int id;
    uint32_t addr;
    bool has_addr;
    bool has_cond;
    std::string cond_expr;
    bool temporary;
public:
    BreakPoint(uint32_t addr, std::string cond_expr);
    BreakPoint(std::string cond_expr);
    ~BreakPoint();
    void add_breakpoint();
    uint32_t address() const;
    int get_id() const { return id; }
    bool hit();
    void print_breakpoint() const;
    bool has_address() const { return has_addr; }
    void set_temporary(bool t) { temporary = t; }
    bool is_temporary() const { return temporary; }
};

extern vector<BreakPoint> break_points;
extern Status breakpoint_update_all(uint64_t pc);

extern void set_temp_breakpoint(uint32_t addr);
extern bool has_temp_breakpoint();
extern bool check_temp_breakpoint(uint64_t pc);
extern void clear_temp_breakpoint();

#endif
