#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <vector>

using namespace std;

static const uint32_t MEM_SIZE = 128 * 1024 * 1024; // 128MB
static const uint32_t MEM_BASE = 0x80000000;
static const uint32_t SERIAL_PORT = 0x10000000;
static const uint32_t RTC_ADDR = 0x10000048;

class Memory {
private:
    vector<uint8_t> mem;

public:
    Memory();
    void load_program();
    uint32_t read_word(uint32_t addr);
    uint16_t read_half(uint32_t addr);
    uint8_t read_byte(uint32_t addr);
    void write_word(uint32_t addr, uint32_t data, uint8_t strb);
    void write_half(uint32_t addr, uint16_t data);
    void write_byte(uint32_t addr, uint8_t data);
    void snapshot(uint8_t* out) const;
};

void mem_read_all(uint8_t* out);

#endif