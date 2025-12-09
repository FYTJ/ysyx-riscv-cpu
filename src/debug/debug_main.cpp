#include <iostream>
#include <memory>
#include <verilated.h>
#include <verilated_fst_c.h>
#include "VSimTop.h"
#include <fstream>
#include <sstream>
#include <string>
#include "debug.h"
#include "watchpoint.h"
#include "breakpoint.h"
#include "debug_cli.h"
#include "expr.h"
#include "trace.h"

using namespace std;

static uint64_t sim_time = 0;
static const uint64_t MAX_SIM_TIME = 10000000000;

DebugCli debug_cli;

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    auto status = Status::PAUSE;
    trace_init();
    
    auto top = make_unique<VSimTop>();
    
    auto tfp = make_unique<VerilatedFstC>();
    top->trace(tfp.get(), 99);
    tfp->open("dump.fst");
    
    cout << "=== Starting Simulation ===" << endl;
    
    top->reset = 1;
    for (int i = 0; i < 10; i++) {
        top->clock = 0;
        top->eval();
        tfp->dump(sim_time++);
        
        top->clock = 1;
        top->eval();
        tfp->dump(sim_time++);
    }
    top->reset = 0;

    int sim_ret = 0;
    int forward = -1;
    DebugInfo debug_info;
    debug_info.regs = new uint32_t[32];
    debug_info.mem  = new uint8_t[128 * 1024 * 1024];
    while (sim_time < MAX_SIM_TIME && !top->io_exit) {
        uint32_t live_regs[32];
        reg_read_all(live_regs);
        expr_set_context(live_regs, debug_info.mem, top->io_debug_pc, MEM_BASE, MEM_SIZE);
        if (has_temp_breakpoint() && check_temp_breakpoint(top->io_debug_pc)) {
            cout << "until reached 0x" << hex << top->io_debug_pc << dec << endl;
            clear_temp_breakpoint();
            status = Status::PAUSE;
        } else {
            Status bp_status = breakpoint_update_all(top->io_debug_pc);
            if (bp_status == Status::PAUSE) {
                if (status == Status::RUNNING) {
                    cout << "Breakpoint hit at 0x" << hex << top->io_debug_pc << dec << endl;
                    for (size_t i = 0; i < break_points.size(); ++i) {
                        auto &bp = break_points[i];
                        if (bp.has_address()) {
                            if (bp.address() == (uint32_t)top->io_debug_pc && bp.hit()) {
                                bp.print_breakpoint();
                                if (bp.is_temporary()) { break_points.erase(break_points.begin() + i); }
                                break;
                            }
                        } else {
                            if (bp.hit()) {
                                bp.print_breakpoint();
                                if (bp.is_temporary()) { break_points.erase(break_points.begin() + i); }
                                break;
                            }
                        }
                    }
                }
                status = Status::PAUSE;
            }
        }
        Status watch_status = watchpoint_update_all();
        if (watch_status == Status::PAUSE) {
            cout << "watchpoint hit" << endl;
            status = Status::PAUSE;
        }

        if (status == Status::PAUSE || forward == 0) {
            debug_info.pc = top->io_debug_pc;
            reg_read_all(debug_info.regs);
            mem_read_all(debug_info.mem);
            try{
                auto [new_status, ret_forward] = debug_cli.mainloop(&debug_info);
                status = new_status;
                if (status == Status::RUNNING) {
                    forward = (ret_forward == 0 ? -1 : ret_forward);
                }
                if (status == Status::QUIT) {
                    break;
                }
            }
            catch(...) {
                cout << "Invalid Command" << endl;
                status = Status::PAUSE;
            }
            
        }
        if (status == Status::RUNNING) {
            top->clock = 0;
            top->eval();
            tfp->dump(sim_time++);

            top->clock = 1;
            top->eval();
            tfp->dump(sim_time++);

            trace_step((uint32_t)top->io_debug_pc);

            if (forward > 0) {
                forward--;
            }
        }
    }
    
    if (sim_time >= MAX_SIM_TIME) {
        cout << "Simulation timeout at cycle " << MAX_SIM_TIME/2 << endl;
    }
    
    uint32_t r10_value = top->io_debug_r10;
    if (r10_value == 0) {
        cout << "Hit Good Trap" << "\n" << endl;
    } else {
        cout << "Hit Bad Trap" << "\n" << endl;
    }
    
    tfp->close();
    top->final();
    trace_flush();
    
    cout << "=== Simulation Completed ===" << endl;
    cout << "Trace outputs:" << endl;
    cout << "  itrace: " << itrace_get_file_path() << " (last " << itrace_get_capacity() << ")" << endl;
    cout << "Total cycles: " << sim_time / 2 << endl;
    cout << "R10 register value: " << r10_value << endl;
    cout << "Waveform saved to: dump.fst\n" << endl;

    delete[] debug_info.regs;
    delete[] debug_info.mem;
    
    return sim_ret ? sim_ret : top->io_exit_code;
}
