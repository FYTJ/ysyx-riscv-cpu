#include <iostream>
#include <memory>
#include <verilated.h>
#include <verilated_fst_c.h>
#include "VSimTop.h"
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

static uint64_t sim_time = 0;
static const uint64_t MAX_SIM_TIME = 10000000000;

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    
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
    while (sim_time < MAX_SIM_TIME && !top->io_exit) {
        top->clock = 0;
        top->eval();
        tfp->dump(sim_time++);
        
        top->clock = 1;
        top->eval();
        tfp->dump(sim_time++);
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
    
    cout << "=== Simulation Completed ===" << endl;
    cout << "Total cycles: " << sim_time/2 << endl;
    cout << "R10 register value: " << r10_value << endl;
    cout << "Waveform saved to: dump.fst\n" << endl;
    
    return sim_ret ? sim_ret : top->io_exit_code;
}