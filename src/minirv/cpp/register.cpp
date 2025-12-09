#include "register.h"

static Register* g_register = nullptr;

Register::Register() : reg_file(32, 0) {}

void Register::get(int i, uint32_t reg_val) {
    this->reg_file[i] = reg_val;
}

void Register::snapshot(uint32_t* out) const {
    for (int i = 0; i < 32; ++i) {
        out[i] = this->reg_file[i];
    }
}

void reg_read_all(uint32_t* out) {
    if (g_register == nullptr) {
        for (int i = 0; i < 32; ++i) out[i] = 0;
        return;
    }
    g_register->snapshot(out);
}

extern "C" void reg_get(int i, uint32_t reg_val) {
    if (g_register == nullptr) {
        g_register = new Register();
    }
    g_register->get(i, reg_val);
}
