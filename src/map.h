// Copyright Nezametdinov E. Ildus 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_90988947122C4A99B7ED48C2EC268033
#define H_90988947122C4A99B7ED48C2EC268033

#include "alloc.h"

#include <stdbool.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// Opaque types.
////////////////////////////////////////////////////////////////////////////////

struct ucs_map;
typedef struct ucs_map* ucs_map;

typedef void const* ucs_map_key;
typedef void* ucs_map_iterator;

////////////////////////////////////////////////////////////////////////////////
// Function pointer types.
////////////////////////////////////////////////////////////////////////////////

typedef void (*ucs_map_key_set_fn)(ucs_map_key, char* mem);
typedef ucs_map_key (*ucs_map_key_get_fn)(char* mem);
typedef int (*ucs_map_key_cmp_fn)(ucs_map_key, ucs_map_key);

////////////////////////////////////////////////////////////////////////////////
// Map's private structure.
////////////////////////////////////////////////////////////////////////////////

struct ucs_map_private {
    ucs_allocator_object_storage m00_;
    ucs_allocator m01_;

    void* m02_;
    size_t m03_;

    ucs_map_key_set_fn m04_;
    ucs_map_key_get_fn m05_;
    ucs_map_key_cmp_fn m06_;
};

////////////////////////////////////////////////////////////////////////////////
// Constants.
////////////////////////////////////////////////////////////////////////////////

enum {
    ucs_map_object_alignment = alignof(struct ucs_map_private),
    ucs_map_object_size = sizeof(struct ucs_map_private)
};

////////////////////////////////////////////////////////////////////////////////
// Map object storage type.
////////////////////////////////////////////////////////////////////////////////

typedef struct ucs_map_object_storage {
    char alignas(ucs_map_object_alignment) mem[ucs_map_object_size];
} ucs_map_object_storage;

////////////////////////////////////////////////////////////////////////////////
// Map configuration.
////////////////////////////////////////////////////////////////////////////////

typedef struct ucs_map_config {
    size_t element_alignment, element_size;

    ucs_map_key_set_fn key_set_fn;
    ucs_map_key_get_fn key_get_fn;
    ucs_map_key_cmp_fn key_cmp_fn;
} ucs_map_config;

////////////////////////////////////////////////////////////////////////////////
// Map creation/destruction interface.
////////////////////////////////////////////////////////////////////////////////

// Requires: if {mem} is not NULL, then it must point to a storage of size
// {ucs_map_object_size} aligned to {ucs_map_object_alignment}.
ucs_map
ucs_map_create_in_place(ucs_map_config cfg, char* mem);

ucs_map
ucs_map_create(ucs_map_config cfg);

void
ucs_map_destroy_in_place(ucs_map map);

void
ucs_map_destroy(ucs_map map);

////////////////////////////////////////////////////////////////////////////////
// Map update interface.
////////////////////////////////////////////////////////////////////////////////

void
ucs_map_clear(ucs_map map);

ucs_map_iterator
ucs_map_insert(ucs_map map, ucs_map_key k);

bool
ucs_map_remove(ucs_map map, ucs_map_key k);

bool
ucs_map_remove_by_iterator(ucs_map map, ucs_map_iterator i);

////////////////////////////////////////////////////////////////////////////////
// Map search interface.
////////////////////////////////////////////////////////////////////////////////

ucs_map_iterator
ucs_map_find(ucs_map map, ucs_map_key k);

ucs_map_iterator
ucs_map_lower_bound(ucs_map map, ucs_map_key k);

////////////////////////////////////////////////////////////////////////////////
// Map iteration interface.
////////////////////////////////////////////////////////////////////////////////

ucs_map_iterator
ucs_map_lower(ucs_map map);

ucs_map_iterator
ucs_map_upper(ucs_map map);

ucs_map_iterator
ucs_map_iterator_next(ucs_map_iterator i);

ucs_map_iterator
ucs_map_iterator_prev(ucs_map_iterator i);

char*
ucs_map_iterator_mem(ucs_map_iterator i);

#endif // H_90988947122C4A99B7ED48C2EC268033
