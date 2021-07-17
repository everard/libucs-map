// Copyright Nezametdinov E. Ildus 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
//
#include <stdalign.h>
#include <stdbool.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "../src/map.h"

////////////////////////////////////////////////////////////////////////////////
// Map key and element types.
////////////////////////////////////////////////////////////////////////////////

typedef unsigned map_key;

typedef struct {
    map_key k;
} map_element;

////////////////////////////////////////////////////////////////////////////////
// Map key setter/getter/comparison functions. Pointers to these functions are
// passed to the map creation function.
////////////////////////////////////////////////////////////////////////////////

static void
map_key_set(ucs_map_key k, char* mem) {
    memcpy(&(((map_element*)(mem))->k), k, sizeof(map_key));
}

static ucs_map_key
map_key_get(char* mem) {
    return &(((map_element*)(mem))->k);
}

static int
map_key_cmp(ucs_map_key k0, ucs_map_key k1) {
    if(*((map_key*)(k0)) < *((map_key*)(k1))) {
        return -1;
    } else if(*((map_key*)(k0)) > *((map_key*)(k1))) {
        return +1;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Deterministic number generation.
////////////////////////////////////////////////////////////////////////////////

static unsigned
key_rand() {
    static uint64_t x = 17;
    uint64_t const a = 4294967279, b = 29;

    x *= b;
    x += a;
    return (unsigned)(x);
}

////////////////////////////////////////////////////////////////////////////////
// Program entry point.
////////////////////////////////////////////////////////////////////////////////

int
main() {
    ucs_map_object_storage map_storage = {};
    ucs_map map = ucs_map_create_in_place(
        (ucs_map_config){.element_alignment = alignof(map_element),
                         .element_size = sizeof(map_element),
                         .key_set_fn = map_key_set,
                         .key_get_fn = map_key_get,
                         .key_cmp_fn = map_key_cmp},
        map_storage.mem);

    if(map == NULL) {
        printf("error: failed to create map\n");
        return EXIT_FAILURE;
    }

    printf("inserting keys:\n");
    for(unsigned i = 0; i < 2048; ++i) {
        map_key k = key_rand() % 8192;
        printf("%4d ", k);
        ucs_map_insert(map, &k);
    }

    printf("\n\n");

    printf("map:\n");
    unsigned map_size = 0;
    for(ucs_map_iterator i = ucs_map_lower(map); i != NULL;
        i = ucs_map_iterator_next(i), ++map_size) {
        map_element* elem = (map_element*)(ucs_map_iterator_mem(i));
        printf("%4d ", elem->k);
    }

    printf("\n\nmap (reversed):\n");
    for(ucs_map_iterator i = ucs_map_upper(map); i != NULL;
        i = ucs_map_iterator_prev(i)) {
        map_element* elem = (map_element*)(ucs_map_iterator_mem(i));
        printf("%4d ", elem->k);
    }

    printf("\n\nmap size: %d\n\n", map_size);

    printf("removing half of the map keys\n");
    for(unsigned k = 0; k < (map_size / 2); ++k) {
        unsigned l = key_rand() % (map_size - k);

        ucs_map_iterator i = ucs_map_lower(map);
        for(; l != 0; --l, i = ucs_map_iterator_next(i)) {
        }

        map_element* elem = (map_element*)(ucs_map_iterator_mem(i));
        printf("%4d ", elem->k);
        if(!ucs_map_remove(map, &(elem->k))) {
            printf("failed to remove key %4d", elem->k);
            return EXIT_FAILURE;
        }
    }

    printf("\n\n");

    printf("inserting half of the map keys again\n\n");
    for(unsigned i = 0; i < (map_size / 2); ++i) {
        map_key k = key_rand() % 8192;
        printf("%4d ", k);
        ucs_map_insert(map, &k);
    }

    printf("\n\n");

    printf("map:\n");
    map_size = 0;
    for(ucs_map_iterator i = ucs_map_lower(map); i != NULL;
        i = ucs_map_iterator_next(i), ++map_size) {
        map_element* elem = (map_element*)(ucs_map_iterator_mem(i));
        printf("%4d ", elem->k);
    }

    printf("\n\nmap (reversed):\n");
    for(ucs_map_iterator i = ucs_map_upper(map); i != NULL;
        i = ucs_map_iterator_prev(i)) {
        map_element* elem = (map_element*)(ucs_map_iterator_mem(i));
        printf("%4d ", elem->k);
    }

    printf("\n\nmap size: %d\n\n", map_size);

    printf("lower bound:\n");
    if(true) {
        map_key keys[] = {5656, 2227, 6031, 893};

        for(size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
            map_key k = keys[i];
            ucs_map_iterator i = ucs_map_lower_bound(map, &k);
            if(i == NULL) {
                printf("failed to find lower bound for key %4d", k);
            } else {
                map_element* elem = (map_element*)(ucs_map_iterator_mem(i));
                printf("k : %4d, e : %4d\n", k, elem->k);
            }
        }

        if(true) {
            map_key k = 8191;
            ucs_map_iterator i = ucs_map_lower_bound(map, &k);
            if(i != NULL) {
                printf("error: found bound for key %4d\n", k);
            } else {
                printf("no lower bound for key %4d\n", k);
            }
        }
    }

    ucs_map_destroy_in_place(map);
    return EXIT_SUCCESS;
}
