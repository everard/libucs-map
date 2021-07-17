// Copyright Nezametdinov E. Ildus 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
//
#include "map.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define key_eq_(k, node) \
    (map->key_cmp_fn((k), map->key_get_fn((node)->mem)) == 0)

#define key_lt_(k, node) \
    (map->key_cmp_fn((k), map->key_get_fn((node)->mem)) < 0)

#define key_gt_(k, node) \
    (map->key_cmp_fn((k), map->key_get_fn((node)->mem)) > 0)

#define child_idx_(node)                                                   \
    ((((node)->parent == NULL) || ((node)->parent->children[0] == (node))) \
         ? 0                                                               \
         : 1)

////////////////////////////////////////////////////////////////////////////////
// Map data types.
////////////////////////////////////////////////////////////////////////////////

typedef struct ucs_map_node {
    char* mem;
    struct ucs_map_node* parent;
    struct ucs_map_node* children[2];
    signed char balance;
} ucs_map_node;

struct ucs_map {
    ucs_allocator_object_storage allocator_storage;
    ucs_allocator allocator;

    ucs_map_node* root;
    size_t element_mem_offset;

    ucs_map_key_set_fn key_set_fn;
    ucs_map_key_get_fn key_get_fn;
    ucs_map_key_cmp_fn key_cmp_fn;
};

static_assert(alignof(struct ucs_map) <= ucs_map_object_alignment, "");
static_assert(sizeof(struct ucs_map) <= ucs_map_object_size, "");

////////////////////////////////////////////////////////////////////////////////
// Utility functions.
////////////////////////////////////////////////////////////////////////////////

// Memory management.

static ucs_map_node*
ucs_map_node_alloc(ucs_map map) {
    char* mem = ucs_allocator_alloc(map->allocator);

    if(mem != NULL) {
        *((ucs_map_node*)(mem)) =
            (ucs_map_node){.mem = (mem + map->element_mem_offset)};
    }

    return (ucs_map_node*)(mem);
}

static void
ucs_map_node_free(ucs_map map, ucs_map_node* node) {
    ucs_allocator_free(map->allocator, node);
}

// Node linking (parent to child).

static void
ucs_map_node_link(ucs_map_node* parent, ucs_map_node* child,
                  ptrdiff_t child_i) {
    if(child != NULL) {
        child->parent = parent;
    }

    if(parent != NULL) {
        parent->children[child_i] = child;
    }
}

// Node rotation.

static ucs_map_node*
ucs_map_node_rotate(ucs_map_node* x) {
    // Precondition: (x != NULL) && (x->balance != 0).

    ptrdiff_t const a_i = ((x->balance < 0) ? 0 : 1), b_i = ((a_i + 1) % 2),
                    c_i = child_idx_(x);

    ucs_map_node* y = x->children[a_i];
    ucs_map_node* z = y->children[b_i];

    ucs_map_node_link(x->parent, y, c_i);
    ucs_map_node_link(x, z, a_i);
    ucs_map_node_link(y, x, b_i);

    return y;
}

// Node rebalancing.

static ucs_map_node*
ucs_map_node_rebalance(ucs_map_node* x) {
    // Precondition: (x != NULL) && (|x->balance| > 1).

    ucs_map_node* y = x->children[(x->balance < 0) ? 0 : 1];
    bool need_double_rotation = ((x->balance < 0) && (y->balance > 0)) ||
                                ((x->balance > 0) && (y->balance < 0));

    if(need_double_rotation) {
        ucs_map_node* z = ucs_map_node_rotate(y);
        ucs_map_node_rotate(x);

        switch(z->balance) {
            case 0:
                x->balance = y->balance = 0;
                break;

            case -1:
                if(x->balance < 0) {
                    y->balance = z->balance = 0;
                    x->balance = +1;
                } else {
                    x->balance = z->balance = 0;
                    y->balance = +1;
                }
                break;

            case +1:
                if(x->balance < 0) {
                    x->balance = z->balance = 0;
                    y->balance = -1;
                } else {
                    y->balance = z->balance = 0;
                    x->balance = -1;
                }
                break;
        }

        return z;
    } else {
        switch(ucs_map_node_rotate(x)->balance) {
            case -1:
                // fall-through
            case +1:
                x->balance = y->balance = 0;
                break;

            case 0:
                if(x->balance < 0) {
                    x->balance = -1;
                    y->balance = +1;
                } else {
                    x->balance = +1;
                    y->balance = -1;
                }
                break;
        }

        return y;
    }
}

// Map rebalancing.

typedef enum {
    ucs_map_rebalance_insert,
    ucs_map_rebalance_remove
} ucs_map_rebalance_type;

static void
ucs_map_rebalance(ucs_map map, ucs_map_node* node, ptrdiff_t child_i,
                  ucs_map_rebalance_type type) {
    ucs_map_node* moved_node = NULL;

    while(node != NULL) {
        if(type == ucs_map_rebalance_insert) {
            node->balance += ((child_i == 0) ? -1 : +1);
            if(node->balance == 0) {
                break;
            }
        } else {
            node->balance += ((child_i == 0) ? +1 : -1);
            if((node->balance == -1) || (node->balance == +1)) {
                break;
            }
        }

        if((node->balance > 1) || (node->balance < -1)) {
            node = ucs_map_node_rebalance(moved_node = node);

            if(type == ucs_map_rebalance_insert) {
                break;
            } else {
                if(node->balance != 0) {
                    break;
                }
            }
        }

        child_i = child_idx_(node);
        node = node->parent;
    }

    if(map->root == moved_node) {
        map->root = map->root->parent;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Map creation/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

ucs_map
ucs_map_create_in_place(ucs_map_config cfg, char* mem) {
    if(cfg.element_size == 0) {
        return NULL;
    }

#define max_(x, y) (((x) > (y)) ? (x) : (y))
#define is_pot_(x) (((x) & ((x)-1)) == 0)

    size_t alignment = max_(cfg.element_alignment, alignof(ucs_map_node));
    if(!is_pot_(alignment)) {
        return NULL;
    }

#undef is_pot_
#undef max_

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

    size_t allocation_size = sizeof(ucs_map_node);
    pad_(allocation_size, alignment);

    size_t element_mem_offset = allocation_size;

    add_(allocation_size, cfg.element_size);
    pad_(allocation_size, alignment);

#undef add_
#undef pad_

    ucs_map m = (ucs_map)(mem);
    if(m != NULL) {
        *m = (struct ucs_map){.element_mem_offset = element_mem_offset,
                              .key_set_fn = cfg.key_set_fn,
                              .key_get_fn = cfg.key_get_fn,
                              .key_cmp_fn = cfg.key_cmp_fn};

        ucs_allocator_config alloc_cfg = {.block_size = 128,
                                          .element_alignment = alignment,
                                          .element_size = allocation_size};

        m->allocator =
            ucs_allocator_create_in_place(alloc_cfg, m->allocator_storage.mem);

        if(m->allocator == NULL) {
            m = NULL;
        }
    }

    return m;
}

ucs_map
ucs_map_create(ucs_map_config cfg) {
    char* mem = aligned_alloc(ucs_map_object_alignment, ucs_map_object_size);

    if(ucs_map_create_in_place(cfg, mem) == NULL) {
        free(mem);
        mem = NULL;
    }

    return (ucs_map)(mem);
}

void
ucs_map_destroy_in_place(ucs_map map) {
    if(map == NULL) {
        return;
    }

    ucs_allocator_destroy_in_place(map->allocator);
}

void
ucs_map_destroy(ucs_map map) {
    ucs_map_destroy_in_place(map);
    free(map);
}

////////////////////////////////////////////////////////////////////////////////
// Map update interface implementation.
////////////////////////////////////////////////////////////////////////////////

void
ucs_map_clear(ucs_map map) {
    ucs_allocator_free_all(map->allocator);
    map->root = NULL;
}

ucs_map_iterator
ucs_map_insert(ucs_map map, ucs_map_key k) {
    if(map->root == NULL) {
        map->root = ucs_map_node_alloc(map);

        if(map->root != NULL) {
            map->key_set_fn(k, map->root->mem);
            return map->root;
        }
    } else {
        // Find the closest node.
        ucs_map_node* node = map->root;
        ptrdiff_t child_i = 0;

        while(true) {
            if(key_eq_(k, node)) {
                return node;
            }

            child_i = key_gt_(k, node);
            if(node->children[child_i] == NULL) {
                break;
            }

            node = node->children[child_i];
        }

        // Insert new node.
        ucs_map_node* inserted_node = ucs_map_node_alloc(map);
        if(inserted_node != NULL) {
            map->key_set_fn(k, inserted_node->mem);

            ucs_map_node_link(node, inserted_node, child_i);
            ucs_map_rebalance(map, node, child_i, ucs_map_rebalance_insert);

            return inserted_node;
        }
    }

    return NULL;
}

bool
ucs_map_remove(ucs_map map, ucs_map_key k) {
    return ucs_map_remove_by_iterator(map, ucs_map_find(map, k));
}

bool
ucs_map_remove_by_iterator(ucs_map map, ucs_map_iterator i) {
    ucs_map_node* node = i;

    if(node == NULL) {
        return false;
    }

    ptrdiff_t child_i = child_idx_(node);
    if((node->children[0] == NULL) || (node->children[1] == NULL)) {
        // Node has at most one child.

        ucs_map_node* next = // Select non-null child (if any).
            ((node->children[0] != NULL) ? node->children[0]
                                         : node->children[1]);

        if(map->root == node) {
            if((map->root = next) != NULL) {
                next->parent = NULL;
            }
        } else {
            ucs_map_node_link(node->parent, next, child_i);
            ucs_map_rebalance(
                map, node->parent, child_i, ucs_map_rebalance_remove);
        }
    } else {
        // Node has two children.

        // Find in-order successor.
        ucs_map_node* next = node->children[1];
        for(; next->children[0] != NULL; next = next->children[0]) {
        }

        // Update map's root if needed.
        if(map->root == node) {
            map->root = next;
        }

        // Update in-order successor's links and rebalance the tree.
        ucs_map_node_link(next, node->children[0], 0);
        next->balance = node->balance;

        if(next->parent == node) {
            ucs_map_node_link(node->parent, next, child_i);
            ucs_map_rebalance(map, next, 1, ucs_map_rebalance_remove);
        } else {
            ucs_map_node* parent_next = next->parent;
            ptrdiff_t child_i_next = child_idx_(next);

            ucs_map_node_link(parent_next, next->children[1], child_i_next);
            ucs_map_node_link(node->parent, next, child_i);
            ucs_map_node_link(next, node->children[1], 1);
            ucs_map_rebalance(
                map, parent_next, child_i_next, ucs_map_rebalance_remove);
        }
    }

    ucs_map_node_free(map, node);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Map search interface implementation.
////////////////////////////////////////////////////////////////////////////////

ucs_map_iterator
ucs_map_find(ucs_map map, ucs_map_key k) {
    ucs_map_node* node = map->root;

    while(node != NULL) {
        if(key_eq_(k, node)) {
            break;
        }

        node = node->children[(ptrdiff_t)key_gt_(k, node)];
    }

    return node;
}

ucs_map_iterator
ucs_map_lower_bound(ucs_map map, ucs_map_key k) {
    ucs_map_node* node = map->root;
    ucs_map_node* prev = node;

    while(node != NULL) {
        if(key_eq_(k, node)) {
            return node;
        }

        prev = node;
        node = node->children[(ptrdiff_t)key_gt_(k, node)];
    }

    if(prev != NULL) {
        return (key_lt_(k, prev) ? prev : ucs_map_iterator_next(prev));
    }

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Map iteration interface implementation.
////////////////////////////////////////////////////////////////////////////////

ucs_map_iterator
ucs_map_lower(ucs_map map) {
    ucs_map_node* node = map->root;

    if(node != NULL) {
        for(; node->children[0] != NULL; node = node->children[0]) {
        }
    }

    return node;
}

ucs_map_iterator
ucs_map_upper(ucs_map map) {
    ucs_map_node* node = map->root;

    if(node != NULL) {
        for(; node->children[1] != NULL; node = node->children[1]) {
        }
    }

    return node;
}

ucs_map_iterator
ucs_map_iterator_next(ucs_map_iterator i) {
    ucs_map_node* node = (ucs_map_node*)(i);

    if(node != NULL) {
        if(node->children[1] != NULL) {
            for(node = node->children[1]; node->children[0] != NULL;
                node = node->children[0]) {
            }
        } else {
            ptrdiff_t child_i = 0;

            do {
                child_i = child_idx_(node);
                node = node->parent;
            } while((child_i != 0) && (node != NULL));
        }
    }

    return node;
}

ucs_map_iterator
ucs_map_iterator_prev(ucs_map_iterator i) {
    ucs_map_node* node = (ucs_map_node*)(i);

    if(node != NULL) {
        if(node->children[0] != NULL) {
            for(node = node->children[0]; node->children[1] != NULL;
                node = node->children[1]) {
            }
        } else {
            ptrdiff_t child_i = 0;

            do {
                child_i = child_idx_(node);
                node = node->parent;
            } while((child_i != 1) && (node != NULL));
        }
    }

    return node;
}

char*
ucs_map_iterator_mem(ucs_map_iterator i) {
    return ((ucs_map_node*)(i))->mem;
}
