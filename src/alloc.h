// Copyright Nezametdinov E. Ildus 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
//
#ifndef H_F17DB8136C8748449DEFB0C8DC3633BD
#define H_F17DB8136C8748449DEFB0C8DC3633BD

#include <stdalign.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// Opaque types.
////////////////////////////////////////////////////////////////////////////////

struct ucs_allocator;
typedef struct ucs_allocator* ucs_allocator;

struct ucs_allocator_private {
    size_t m00_, m01_, m02_, m03_, m04_, m05_;
    void *m06_, *m07_, *m08_;
};

////////////////////////////////////////////////////////////////////////////////
// Constants.
////////////////////////////////////////////////////////////////////////////////

enum {
    ucs_allocator_object_alignment = alignof(struct ucs_allocator_private),
    ucs_allocator_object_size = sizeof(struct ucs_allocator_private)
};

////////////////////////////////////////////////////////////////////////////////
// Allocator object storage type.
////////////////////////////////////////////////////////////////////////////////

typedef struct ucs_allocator_object_storage {
    char alignas(ucs_allocator_object_alignment) mem[ucs_allocator_object_size];
} ucs_allocator_object_storage;

////////////////////////////////////////////////////////////////////////////////
// Allocator configuration.
////////////////////////////////////////////////////////////////////////////////

typedef struct ucs_allocator_config {
    size_t block_size, element_alignment, element_size;
} ucs_allocator_config;

////////////////////////////////////////////////////////////////////////////////
// Allocator creation/destruction interface.
////////////////////////////////////////////////////////////////////////////////

// Requires: if {mem} is not NULL, then it must point to a storage of size
// {ucs_allocator_object_size} aligned to {ucs_allocator_object_alignment}.
ucs_allocator
ucs_allocator_create_in_place(ucs_allocator_config cfg, char* mem);

ucs_allocator
ucs_allocator_create(ucs_allocator_config cfg);

void
ucs_allocator_destroy_in_place(ucs_allocator allocator);

void
ucs_allocator_destroy(ucs_allocator allocator);

////////////////////////////////////////////////////////////////////////////////
// Allocator memory management interface.
////////////////////////////////////////////////////////////////////////////////

void*
ucs_allocator_alloc(ucs_allocator allocator);

void
ucs_allocator_free(ucs_allocator allocator, void* mem);

void
ucs_allocator_free_all(ucs_allocator allocator);

#endif // H_F17DB8136C8748449DEFB0C8DC3633BD
