#include <iostream>
#include <memory>
#include <verilated.h>
#include <verilated_fst_c.h>
#include "VSimTop.h"
#include <fstream>
#include <sstream>
#include <string>

#define HAS_TRACE false

static uint64_t sim_time = 0;
static const uint64_t MAX_SIM_TIME = 500000;

struct TraceEntry { uint32_t pc; uint32_t waddr; uint32_t wdata; };
class TraceFile {
    public:
    explicit TraceFile(const std::string &path) : ifs(path) {}
    bool read_next(TraceEntry &e) {
        std::string line;
        while (std::getline(ifs, line)) {
        auto trim = [](const std::string &s) {
            size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) return std::string();
            size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        };
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string pc_s, waddr_s, wdata_s;
        if (!(ss >> pc_s >> waddr_s >> wdata_s)) continue;
        e.pc = parse_val(pc_s);
        e.waddr = parse_val(waddr_s);
        e.wdata = parse_val(wdata_s);
        return true;
        }
        return false;
    }
    private:
    std::ifstream ifs;
    static uint32_t parse_val(const std::string &s) {
        std::string t = s;
        if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        t = t.substr(2);
        }
        return static_cast<uint32_t>(std::stoul(t, nullptr, 16));
    }
};

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    
    auto top = std::make_unique<VSimTop>();
    
    auto tfp = std::make_unique<VerilatedFstC>();
    top->trace(tfp.get(), 99);
    tfp->open("dump.fst");
    
    std::cout << "=== Starting Simulation ===" << std::endl;
    
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

        if (HAS_TRACE) {
            TraceFile trace("/Users/zhuyanbo/Desktop/风云天际/一生一芯/ysyx-workbench/npc/playground/test/trace/mem.txt");
            if (top->io_debug_rf_wen && top->io_debug_rf_waddr != 0) {
                TraceEntry ref;
                if (!trace.read_next(ref)) {
                    std::cout << "Trace 文件耗尽或格式错误，终止仿真" << std::endl;
                    sim_ret = 1;
                    break;
                }
                uint32_t pc = top->io_debug_pc;
                uint32_t waddr = top->io_debug_rf_waddr;
                uint32_t wdata = top->io_debug_rf_wdata;
                if (pc != ref.pc || waddr != ref.waddr || wdata != ref.wdata) {
                    std::cout << std::hex;
                    std::cout << "reference: pc = 0x" << ref.pc
                            << ", waddr = " << std::dec << ref.waddr << std::hex
                            << ", wdata = 0x" << ref.wdata << std::dec << std::endl;
                    std::cout << "yours:     pc = 0x" << std::hex << pc
                            << ", waddr = " << std::dec << waddr << std::hex
                            << ", wdata = 0x" << wdata << std::dec << std::endl;
                    sim_ret = 1;
                    // 修改：对比失败后再运行两个周期后停止
                    for (int hold = 0; hold < 2; ++hold) {
                    if (sim_time >= MAX_SIM_TIME || top->io_exit) break;
                    top->clock = 0; top->eval(); tfp->dump(sim_time++);
                    top->clock = 1; top->eval(); tfp->dump(sim_time++);
                    }
                    break;
                }
            }
            
            if (top->io_exit) {
                std::cout << "Simulation exit detected at cycle " << sim_time/2 << std::endl;
                std::cout << "Exit code: " << top->io_exit_code << "\n" << std::endl;
                break;
            }
        }
    }
    
    if (sim_time >= MAX_SIM_TIME) {
        std::cout << "Simulation timeout at cycle " << MAX_SIM_TIME/2 << std::endl;
    }
    
    uint32_t r10_value = top->io_debug_r10;
    if (r10_value == 0) {
        std::cout << "Hit Good Trap" << "\n" << std::endl;
    } else {
        std::cout << "Hit Bad Trap" << "\n" << std::endl;
    }
    
    tfp->close();
    top->final();
    
    std::cout << "=== Simulation Completed ===" << std::endl;
    std::cout << "Total cycles: " << sim_time/2 << std::endl;
    std::cout << "R10 register value: " << r10_value << std::endl;
    std::cout << "Waveform saved to: dump.fst\n" << std::endl;
    
    return sim_ret ? sim_ret : top->io_exit_code;
}