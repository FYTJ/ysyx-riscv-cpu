#ifndef REGISTER_H
#define REGISTER_H

#include <cstdint>
#include <vector>

using namespace std;

class Register {
private:
    vector<uint32_t> reg_file;
    uint32_t pc;

public: 
    Register();
    void get(int i, uint32_t reg_val);
    void snapshot(uint32_t* out) const;
};

void reg_read_all(uint32_t* out);

#endif