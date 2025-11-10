#include "memory.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <time.h>
#include <sys/sysinfo.h>

static Memory* g_memory = nullptr;

extern "C" void mem_init() {
    if (g_memory == nullptr) {
        g_memory = new Memory();
        g_memory->load_program();
        std::cout << "Memory DPI initialized" << std::endl;
    }
}

void Memory::load_program() {
    const char *program_path = std::getenv("NPC_PROGRAM_PATH");
    if (program_path == nullptr) {
        std::cerr << "NPC_PROGRAM_PATH environment variable not set" << std::endl;
        return;
    }
    std::ifstream file(program_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open program file: " << program_path << std::endl;
        return;
    }
    
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (file_size > MEM_SIZE) {
        std::cerr << "Program file too large: " << file_size << " bytes, memory size: " << MEM_SIZE << " bytes" << std::endl;
        file.close();
        return;
    }
    
    uint32_t load_addr = MEM_BASE;
    size_t mem_offset = load_addr - MEM_BASE;
    
    if (file.read(reinterpret_cast<char*>(&mem[mem_offset]), file_size)) {
        std::cout << "Successfully loaded " << file_size << " bytes from " << program_path 
                  << " to memory address 0x" << std::hex << load_addr << std::dec << std::endl;
    } else {
        std::cerr << "Failed to read program file: " << program_path << std::endl;
    }
    file.close();
    // uint32_t ebreak_addr = 0x224;
    // uint32_t ebreak_inst = 0x00100073;
    // size_t idx = ebreak_addr - MEM_BASE;
    // mem[idx + 0] = static_cast<uint8_t>((ebreak_inst >> 0)  & 0xFF);
    // mem[idx + 1] = static_cast<uint8_t>((ebreak_inst >> 8)  & 0xFF);
    // mem[idx + 2] = static_cast<uint8_t>((ebreak_inst >> 16) & 0xFF);
    // mem[idx + 3] = static_cast<uint8_t>((ebreak_inst >> 24) & 0xFF);
    // std::cout << "Overwrote address 0x" << std::hex << ebreak_addr << " with ebreak instruction 0x" << ebreak_inst << std::dec << std::endl;
}

Memory::Memory() : mem(MEM_SIZE, 0) {}

uint32_t Memory::read_word(uint32_t addr) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t rt = (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
    if (addr == RTC_ADDR) {
        return rt & 0xFFFFFFFFull;
    }
    else if (addr == RTC_ADDR + 4) {
        return (rt >> 32) & 0xFFFFFFFFull;
    }

    size_t idx = addr - MEM_BASE;
    uint32_t b0 = static_cast<uint32_t>(mem[idx + 0]);
    uint32_t b1 = static_cast<uint32_t>(mem[idx + 1]);
    uint32_t b2 = static_cast<uint32_t>(mem[idx + 2]);
    uint32_t b3 = static_cast<uint32_t>(mem[idx + 3]);
    return (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
}

void Memory::write_word(uint32_t addr, uint32_t data, uint8_t strb) {
    if (addr == SERIAL_PORT) {
        switch (strb)
        {
        case 0x1:
            putchar(data & 0xFF);
            break;
        case 0x2:
            putchar((data >> 8) & 0xFF);
            break;
        case 0x4:
            putchar((data >> 16) & 0xFF);
            break;
        case 0x8:
            putchar((data >> 24) & 0xFF);
            break;
        default:
            break;
        }
        return;
    }
    size_t idx = addr - MEM_BASE;
    if (strb & 0x1) mem[idx + 0] = static_cast<uint8_t>((data >> 0)  & 0xFF);
    if (strb & 0x2) mem[idx + 1] = static_cast<uint8_t>((data >> 8)  & 0xFF);
    if (strb & 0x4) mem[idx + 2] = static_cast<uint8_t>((data >> 16) & 0xFF);
    if (strb & 0x8) mem[idx + 3] = static_cast<uint8_t>((data >> 24) & 0xFF);
}

extern "C" int mem_read_word(int addr) {
    if (g_memory == nullptr) {
        std::cerr << "Memory not initialized!" << std::endl;
        return 0;
    }
    return g_memory->read_word(static_cast<uint32_t>(addr));
}

extern "C" unsigned char mem_write_word(int addr, int data, unsigned char strb) {
    if (g_memory == nullptr) {
        std::cerr << "Memory not initialized!" << std::endl;
        return (unsigned char)1;
    }
    g_memory->write_word(static_cast<uint32_t>(addr),
                         static_cast<uint32_t>(data),
                         static_cast<uint8_t>(strb));
    return (unsigned char)1;
}

extern "C" void mem_cleanup() {
    if (g_memory != nullptr) {
        delete g_memory;
        g_memory = nullptr;
        std::cout << "Memory DPI cleaned up" << std::endl;
    }
}