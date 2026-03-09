#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


struct allocator_o;

typedef struct allocator_i {
    struct allocator_o *inst; 
    void *(*realloc)(struct allocator_o *inst, void *ptr, uint64_t size, uint64_t old_size);
    uint64_t (*get_total_allocated)(struct allocator_o* inst);
} allocator_i;

allocator_i *get_simple_allocator(void);


#ifdef __cplusplus
}
#endif
