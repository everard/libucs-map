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
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define array_size_(x) (sizeof(x) / sizeof((x)[0]))
#define iter_value_(x) (*((map_element*)(ucs_map_iterator_mem(i))))

////////////////////////////////////////////////////////////////////////////////
// Map's key and element types.
////////////////////////////////////////////////////////////////////////////////

typedef unsigned map_key;

typedef struct {
    map_key k;
} map_element;

////////////////////////////////////////////////////////////////////////////////
// Map key setter/getter/comparison functions. Pointers to these functions are
// passed to the map creation function.
////////////////////////////////////////////////////////////////////////////////

// This function interprets {mem} as a pointer to an object of type
// {map_element}. Let's call this object {x}. Then, value pointed to by {k} is
// copied to {x.k}.
static void
map_key_set(ucs_map_key k, char* mem) {
    memcpy(&(((map_element*)(mem))->k), k, sizeof(map_key));
}

// This function interprets {mem} as a pointer to an object of type
// {map_element}. Let's call this object {x}. Then, this function returns a
// pointer to {x.k}.
static ucs_map_key
map_key_get(char* mem) {
    return &(((map_element*)(mem))->k);
}

// This function compares two map keys pointed by {k0} and {k1}. It returns -1
// when the first key is less than the second key, 1 - when the first key is
// greater that the second, and 0 - when these two keys are equal.
static int
map_key_cmp(ucs_map_key k0, ucs_map_key k1) {
#define key_(k) (*((map_key*)(k)))

    if(key_(k0) < key_(k1)) {
        return -1;
    } else if(key_(k0) > key_(k1)) {
        return +1;
    }

#undef key_

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Utility functions.
////////////////////////////////////////////////////////////////////////////////

enum { key_array_size = 2048 };

// Deterministically generates random numbers.
static unsigned
key_rand() {
    static uint64_t x = 17;
    uint64_t const a = 4294967279, b = 29;

    x *= b;
    x += a;
    return (unsigned)(x);
}

// Sorts the given array, removes duplicates, and returns its size without
// duplicates.
static unsigned
key_array_sort_and_remove_duplicates(map_key keys[static key_array_size]) {
    qsort(keys, key_array_size, sizeof(map_key), map_key_cmp);

    map_key keys_unique[key_array_size] = {keys[0]};
    unsigned size = 1;

    for(ptrdiff_t i = 0, j = 1; j != key_array_size; ++j) {
        if(keys_unique[i] == keys[j]) {
            continue;
        } else {
            keys_unique[++i] = keys[j];
            ++size;
        }
    }

    memcpy(keys, keys_unique, sizeof(keys_unique));
    return size;
}

////////////////////////////////////////////////////////////////////////////////
// Map validation function.
////////////////////////////////////////////////////////////////////////////////

static bool
map_validate_and_print(ucs_map map, map_key keys[static key_array_size],
                       unsigned map_size_expected) {
    // Iterate through the map and check if it holds its elements in the right
    // order. Calculate map's size.
    printf("map:\n");
    unsigned map_size = 0;
    for(ucs_map_iterator i = ucs_map_lower(map); i != NULL;
        i = ucs_map_iterator_next(i), ++map_size) {
        printf("%4d ", iter_value_(i).k);

        map_key k_expected = keys[(ptrdiff_t)map_size];
        if(iter_value_(i).k != k_expected) {
            printf("\nerror: map failed to insert key %d\n", k_expected);
            return false;
        }
    }

    printf("\n\nmap (reversed):\n");
    for(ucs_map_iterator i = ucs_map_upper(map); i != NULL;
        i = ucs_map_iterator_prev(i)) {
        printf("%4d ", iter_value_(i).k);
    }

    // Validate the size of the map.
    printf("\n\nmap size: %d\n\n", map_size);

    if(map_size != map_size_expected) {
        printf("error: map's size is wrong, expected: %d\n", map_size_expected);
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Program entry point.
////////////////////////////////////////////////////////////////////////////////

int
main() {
    // Allocate storage for a map object on stack. {map_storage.mem} has right
    // alignment and size to store an object of map type.
    ucs_map_object_storage map_storage = {};

    // Create a new map in allocated storage. This map will contain objects of
    // the {map_element} type.
    ucs_map map = ucs_map_create_in_place(
        // Specify map's element alignment and size. Also specify functions
        // which set/get/compare map's keys.
        (ucs_map_config){.element_alignment = alignof(map_element),
                         .element_size = sizeof(map_element),
                         .key_set_fn = map_key_set,
                         .key_get_fn = map_key_get,
                         .key_cmp_fn = map_key_cmp},
        // Pass a pointer to the allocated storage for the map.
        map_storage.mem);

    if(map == NULL) {
        printf("error: failed to create map\n");
        return EXIT_FAILURE;
    }

    int result = EXIT_SUCCESS;
    unsigned map_size_expected = 0;
    map_key keys[key_array_size] = {};

    // Insert elements to the map.
    printf("inserting elements:\n");
    if(true) {
        map_key* k = keys;
        for(unsigned i = 0; i != key_array_size; ++i, ++k) {
            printf("%4d ", (*k = key_rand() % 8192));
            ucs_map_insert(map, k);
        }

        map_size_expected = key_array_sort_and_remove_duplicates(keys);
    }

    printf("\n\n");

    if(!map_validate_and_print(map, keys, map_size_expected)) {
        result = EXIT_FAILURE;
        goto cleanup;
    }

    // Remove half of the map's elements.
    printf("removing half of the map's elements\n");
    for(unsigned k = 0; k < (map_size_expected / 2); ++k) {
        unsigned l = key_rand() % (map_size_expected - k);

        ucs_map_iterator i = ucs_map_lower(map);
        for(; l != 0; --l, i = ucs_map_iterator_next(i)) {
        }

        printf("%4d ", iter_value_(i).k);
        if(!ucs_map_remove(map, &(iter_value_(i).k))) {
            printf("error: failed to remove element with key %d\n",
                   iter_value_(i).k);

            result = EXIT_FAILURE;
            goto cleanup;
        }
    }

    if(true) {
        ptrdiff_t j = 0;
        for(ucs_map_iterator i = ucs_map_lower(map); i != NULL;
            i = ucs_map_iterator_next(i)) {
            keys[j++] = iter_value_(i).k;
        }
    }

    printf("\n\n");

    // Insert new elements to the map.
    printf("inserting half of the map's elements again:\n");
    if(true) {
        map_key* k = &keys[(ptrdiff_t)(map_size_expected / 2)];
        for(unsigned i = 0; i < (map_size_expected / 2); ++i, ++k) {
            printf("%4d ", (*k = key_rand() % 8192));
            ucs_map_insert(map, k);
        }

        map_size_expected = key_array_sort_and_remove_duplicates(keys);
    }

    printf("\n\n");

    if(!map_validate_and_print(map, keys, map_size_expected)) {
        result = EXIT_FAILURE;
        goto cleanup;
    }

    // Test lower bounds.
    printf("testing lower bounds:\n");
    if(true) {
        map_key bounds[] = {5656, 2227, 6031, 893};
        map_key expected[] = {5660, 2228, 6031, 896};

        for(size_t j = 0; j < array_size_(bounds); ++j) {
            map_key k = bounds[j];
            ucs_map_iterator i = ucs_map_lower_bound(map, &k);

            if(i == NULL) {
                printf("error: failed to find lower bound for %d\n", k);

                result = EXIT_FAILURE;
                goto cleanup;
            } else {
                if(iter_value_(i).k != expected[j]) {
                    printf(
                        "error: wrong lower bound for %d (expected: %d, got: "
                        "%d)\n",
                        k, expected[j], iter_value_(i).k);

                    result = EXIT_FAILURE;
                    goto cleanup;
                }

                printf("k : %4d, e : %4d\n", k, iter_value_(i).k);
            }
        }

        if(true) {
            map_key k = 8191;
            ucs_map_iterator i = ucs_map_lower_bound(map, &k);

            if(i != NULL) {
                printf(
                    "error: found lower bound for %d when none is expected\n",
                    k);

                result = EXIT_FAILURE;
                goto cleanup;
            } else {
                printf("k : %4d, e : none\n", k);
            }
        }
    }

    printf("\nsuccess\n");

cleanup:

    ucs_map_destroy_in_place(map);
    return result;
}
