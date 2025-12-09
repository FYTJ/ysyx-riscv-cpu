#include "debug_ids.h"

static std::atomic<int> g_next_debug_id{1};

int alloc_debug_id() {
    return g_next_debug_id.fetch_add(1);
}

