#include "watchpoint.h"
#include "expr.h"
#include "debug_ids.h"
#include <iostream>

vector<WatchPoint> watch_points;

WatchPoint::WatchPoint(std::string expression) {
    this->expression = std::move(expression);
    this->id = alloc_debug_id();
    this->prev_val = 0;
    this->cur_val = 0;
}

WatchPoint::~WatchPoint() {
}

void WatchPoint::add_watchpoint() {
    bool success = true;
    uint32_t v = expr(string_view(this->expression), &success);
    if (!success) {
        cout << "Invalid expression: " << this->expression << endl;
        return;
    }
    this->prev_val = v;
    this->cur_val = v;
    cout << "Watchpoint " << this->id << " set on " << this->expression << endl;
}

bool WatchPoint::update_watchpoint() {
    bool success = true;
    uint32_t v = expr(string_view(this->expression), &success);
    if (!success) return false;
    if (this->cur_val != v) {
        this->prev_val = this->cur_val;
        this->cur_val = v;
        cout << "Watchpoint " << this->id << " hit on " << this->expression << endl;
        this->print_watchpoint();
        return true;
    }
    return false;
}

void WatchPoint::print_watchpoint() {
    cout << "Watchpoint " << this->id << ": prev: " << this->prev_val << " cur: " << this->cur_val << endl;
}

void WatchPoint::print_unset() {
    cout << "Watchpoint " << this->id << " unset on " << this->expression << endl;
}

Status watchpoint_update_all() {
    bool hit = false;
    for (auto& wp : watch_points) {
        if (wp.update_watchpoint()) {
            hit = true;
        }
    }
    return hit ? Status::PAUSE : Status::RUNNING;
}
