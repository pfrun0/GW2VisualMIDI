#include <stdlib.h>
#include <stdint.h>
#include "allocator.h"

//#define FINE_TRACKING

typedef struct allocations{
    void *ptr;
    uint64_t size;
} allocations;

typedef struct allocator_o
{
    #ifdef FINE_TRACKING
    uint64_t total_alloc;
    uint64_t capacity;
    uint64_t alloc_count;
    allocations *allocs;
    #else
    uint64_t total_alloc;
    #endif
}allocator_o;

#ifdef FINE_TRACKING
static allocator_o simple_allocator_state = { 0, 0, 0, NULL };
#else
static allocator_o simple_allocator_state = {0};
#endif

static void track_alloc(allocator_o *inst, void *ptr, uint64_t size) {
    if (!inst || !ptr || size == 0) return;
    
    #ifdef FINE_TRACKING
    if (inst->alloc_count >= inst->capacity) {
        uint64_t new_capacity = (inst->capacity == 0) ? 16 : inst->capacity * 2;
        allocations *new_entries = realloc(inst->allocs, new_capacity * sizeof(allocations));
        if (!new_entries) return; 

        inst->allocs = new_entries;
        inst->capacity = new_capacity;
    }

    inst->allocs[inst->alloc_count].ptr = ptr;
    inst->allocs[inst->alloc_count].size = size;
    inst->alloc_count++;
    inst->total_alloc += size;
    #else
    inst->total_alloc += size;
    #endif
}

static void remove_alloc(allocator_o *inst, void *ptr, uint64_t size) {
    if (!inst || !ptr) return;
    #ifdef FINE_TRACKING
    for (size_t i = 0; i < inst->alloc_count; i++) {
        if (inst->allocs[i].ptr == ptr) {
            inst->total_alloc -= inst->allocs[i].size;

            inst->allocs[i] = inst->allocs[inst->alloc_count - 1];
            inst->alloc_count--;
            return;
        }
    }
    #else
    inst->total_alloc -= size;
    #endif
}

static void update_alloc(allocator_o *inst, void *old_ptr, void *new_ptr, uint64_t new_size, uint64_t old_size) {
    if (!inst || !old_ptr || !new_ptr) return;
    #ifdef FINE_TRACKING
    for (size_t i = 0; i < inst->alloc_count; i++) {
        if (inst->allocs[i].ptr == old_ptr) {
            inst->total_alloc -= inst->allocs[i].size;
            inst->allocs[i].ptr = new_ptr;
            inst->allocs[i].size = new_size;
            inst->total_alloc -= old_size;
            inst->total_alloc += new_size;
            return;
        }
    }
    #else
    inst->total_alloc -= old_size;
    inst->total_alloc += new_size;
    #endif
}

static void *simple_realloc(struct allocator_o *inst, void *ptr, uint64_t new_size, uint64_t old_size)
{
    if(new_size == 0){
        remove_alloc(inst, ptr, old_size);
        free(ptr);
        return NULL;
    }
    if (ptr == NULL) {
        void *new_ptr = malloc((size_t)new_size);
        if (new_ptr) track_alloc(inst, new_ptr, new_size);
        return new_ptr;
    }

    void *new_ptr = realloc(ptr, (size_t)new_size);
    if (new_ptr) update_alloc(inst, ptr, new_ptr, new_size,old_size);

    return new_ptr;
}

static uint64_t get_allocated(struct allocator_o *inst)
{
    return inst->total_alloc;
}

struct allocator_i simple_allocator = 
{
    &simple_allocator_state,
    simple_realloc,
    get_allocated
};

allocator_i *get_simple_allocator(void)
{
    return &simple_allocator;
}