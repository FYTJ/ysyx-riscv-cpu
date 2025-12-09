#include "breakpoint.h"
#include "expr.h"
#include "debug_ids.h"
#include <iostream>

vector<BreakPoint> break_points;

static bool g_has_temp_bp = false;
static uint32_t g_temp_bp_addr = 0;

BreakPoint::BreakPoint(uint32_t a, std::string cond) {
    this->id = alloc_debug_id();
    this->addr = a;
    this->has_addr = true;
    if (!cond.empty()) {
        this->has_cond = true;
        this->cond_expr = std::move(cond);
    } else {
        this->has_cond = false;
        this->cond_expr.clear();
    }
    this->temporary = false;
}

BreakPoint::BreakPoint(std::string cond) {
    this->id = alloc_debug_id();
    this->addr = 0;
    this->has_addr = false;
    if (!cond.empty()) {
        this->has_cond = true;
        this->cond_expr = std::move(cond);
    } else {
        this->has_cond = false;
        this->cond_expr.clear();
    }
    this->temporary = false;
}

BreakPoint::~BreakPoint() {}

void BreakPoint::add_breakpoint() {
    if (has_addr) {
        std::cout << "Breakpoint " << this->id << " set at 0x" << std::hex << this->addr << std::dec;
        if (has_cond) {
            std::cout << " if " << this->cond_expr;
        }
        std::cout << std::endl;
    } else {
        std::cout << "Breakpoint " << this->id << " set with condition " << this->cond_expr << std::endl;
    }
}

uint32_t BreakPoint::address() const { return addr; }

bool BreakPoint::hit() {
    if (!has_cond) return true;
    bool success = true;
    uint32_t v = expr(string_view(this->cond_expr), &success);
    if (!success) return false;
    return v != 0;
}

void BreakPoint::print_breakpoint() const {
    if (has_addr) {
        std::cout << "Breakpoint " << this->id << ": 0x" << std::hex << this->addr << std::dec;
        if (has_cond) {
            std::cout << " if " << this->cond_expr;
        }
        std::cout << std::endl;
    } else {
        std::cout << "Breakpoint " << this->id << ": if " << this->cond_expr << std::endl;
    }
}

Status breakpoint_update_all(uint64_t pc) {
    for (auto &bp : break_points) {
        if (bp.has_address()) {
            if (bp.address() == (uint32_t)pc) {
                if (bp.hit()) {
                    return Status::PAUSE;
                }
            }
        } else {
            if (bp.hit()) {
                return Status::PAUSE;
            }
        }
    }
    return Status::RUNNING;
}

void set_temp_breakpoint(uint32_t addr) {
    g_has_temp_bp = true;
    g_temp_bp_addr = addr;
}

bool has_temp_breakpoint() { return g_has_temp_bp; }

bool check_temp_breakpoint(uint64_t pc) {
    return g_has_temp_bp && (uint32_t)pc == g_temp_bp_addr;
}

void clear_temp_breakpoint() { g_has_temp_bp = false; }
