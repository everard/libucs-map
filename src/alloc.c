// Copyright Nezametdinov E. Ildus 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
//
#include "alloc.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
// Allocator data types.
////////////////////////////////////////////////////////////////////////////////

typedef struct ucs_allocated_block {
    struct ucs_allocated_block* prev;
    struct ucs_allocated_block* next;
    void* free_ptrs[];
} ucs_allocated_block;

struct ucs_allocator {
    size_t block_size, element_size, element_mem_offset;
    size_t alignment, allocation_size, free_idx;
    ucs_allocated_block *head, *tail, *free_list_head;
};

static_assert(alignof(struct ucs_allocator) <= ucs_allocator_object_alignment,
              "");
static_assert(sizeof(struct ucs_allocator) <= ucs_allocator_object_size, "");

////////////////////////////////////////////////////////////////////////////////
// Helper functions.
////////////////////////////////////////////////////////////////////////////////

static ucs_allocated_block*
ucs_allocator_append_block(ucs_allocator allocator) {
    char* mem = aligned_alloc(allocator->alignment, allocator->allocation_size);
    ucs_allocated_block* block = (ucs_allocated_block*)(mem);

    if(block != NULL) {
        mem += allocator->element_mem_offset;

        for(size_t i = 0; i != allocator->block_size;
            ++i, mem += allocator->element_size) {
            block->free_ptrs[i] = mem;
        }

        block->prev = allocator->tail;
        block->next = NULL;

        if(block->prev != NULL) {
            block->prev->next = block;
        }
    }

    return block;
}

////////////////////////////////////////////////////////////////////////////////
// Allocator creation/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

ucs_allocator
ucs_allocator_create_in_place(ucs_allocator_config cfg, char* mem) {
    if((cfg.block_size == 0) || (cfg.element_size == 0)) {
        return NULL;
    }

#define max_(x, y) (((x) > (y)) ? (x) : (y))
#define is_pot_(x) (((x) & ((x)-1)) == 0)

    size_t alignment =
        max_(cfg.element_alignment, alignof(ucs_allocated_block));

    if(!is_pot_(alignment)) {
        return NULL;
    }

#undef is_pot_
#undef max_

    size_t free_ptrs_array_size = cfg.block_size * sizeof(void*);
    if((free_ptrs_array_size / cfg.block_size) != sizeof(void*)) {
        return NULL;
    }

    size_t elements_array_size = cfg.block_size * cfg.element_size;
    if((elements_array_size / cfg.block_size) != cfg.element_size) {
        return NULL;
    }

#define pad_(size, aignment)                                        \
    if(true) {                                                      \
        size_t d = (size) % (alignment);                            \
        if((d != 0) && (((size) += (alignment)-d) < (alignment))) { \
            return NULL;                                            \
        }                                                           \
    }

#define add_(x, y)           \
    if(((x) += (y)) < (y)) { \
        return NULL;         \
    }

    size_t allocation_size = sizeof(ucs_allocated_block);
    add_(allocation_size, free_ptrs_array_size);
    pad_(allocation_size, alignment);

    size_t element_mem_offset = allocation_size;

    add_(allocation_size, elements_array_size);
    pad_(allocation_size, alignment);

#undef add_
#undef pad_

    ucs_allocator allocator = (ucs_allocator)(mem);
    if(allocator != NULL) {
        *allocator =
            (struct ucs_allocator){.block_size = cfg.block_size,
                                   .element_size = cfg.element_size,
                                   .element_mem_offset = element_mem_offset,
                                   .alignment = alignment,
                                   .allocation_size = allocation_size};
    }

    return allocator;
}

ucs_allocator
ucs_allocator_create(ucs_allocator_config cfg) {
    char* mem = aligned_alloc(
        ucs_allocator_object_alignment, ucs_allocator_object_size);

    if(ucs_allocator_create_in_place(cfg, mem) == NULL) {
        free(mem);
        mem = NULL;
    }

    return (ucs_allocator)(mem);
}

void
ucs_allocator_destroy_in_place(ucs_allocator allocator) {
    if(allocator == NULL) {
        return;
    }

    for(ucs_allocated_block* block = allocator->head; block != NULL;) {
        ucs_allocated_block* x = block;
        block = block->next;
        free(x);
    }
}

void
ucs_allocator_destroy(ucs_allocator allocator) {
    ucs_allocator_destroy_in_place(allocator);
    free(allocator);
}

////////////////////////////////////////////////////////////////////////////////
// Allocator memory management interface implementation.
////////////////////////////////////////////////////////////////////////////////

void*
ucs_allocator_alloc(ucs_allocator allocator) {
    if(allocator->head == NULL) {
        allocator->tail = allocator->free_list_head = allocator->head =
            ucs_allocator_append_block(allocator);

        if(allocator->head == NULL) {
            return NULL;
        }
    }

    if(allocator->free_idx == allocator->block_size) {
        if(allocator->free_list_head->next != NULL) {
            allocator->free_list_head = allocator->free_list_head->next;
            allocator->free_idx = 0;
        } else {
            ucs_allocated_block* block = ucs_allocator_append_block(allocator);

            if(block == NULL) {
                return NULL;
            }

            allocator->tail = allocator->free_list_head = block;
            allocator->free_idx = 0;
        }
    }

    return allocator->free_list_head->free_ptrs[allocator->free_idx++];
}

void
ucs_allocator_free(ucs_allocator allocator, void* mem) {
    if(mem == NULL) {
        return;
    }

    if(allocator->free_idx == 0) {
        allocator->free_list_head = allocator->free_list_head->prev;
        allocator->free_idx = allocator->block_size;
    }

    allocator->free_list_head->free_ptrs[--allocator->free_idx] = mem;
}

void
ucs_allocator_free_all(ucs_allocator allocator) {
    allocator->free_idx = 0;
    allocator->free_list_head = allocator->head;

    for(ucs_allocated_block* block = allocator->head; block != NULL;) {
        char* mem = ((char*)(block)) + allocator->element_mem_offset;

        for(size_t i = 0; i != allocator->block_size;
            ++i, mem += allocator->element_size) {
            block->free_ptrs[i] = mem;
        }

        block = block->next;
    }
}
