/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_CARTESIAN_TREE_H
#define PAS_CARTESIAN_TREE_H

#include "pas_compact_cartesian_tree_node_ptr.h"
#include "pas_log.h"
#include "pas_tree_direction.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_cartesian_tree;
struct pas_cartesian_tree_config;
struct pas_cartesian_tree_node;

typedef struct pas_cartesian_tree pas_cartesian_tree;
typedef struct pas_cartesian_tree_config pas_cartesian_tree_config;
typedef struct pas_cartesian_tree_node pas_cartesian_tree_node;

struct pas_cartesian_tree_node {
    pas_compact_cartesian_tree_node_ptr parent;
    pas_compact_cartesian_tree_node_ptr left;
    pas_compact_cartesian_tree_node_ptr right;
};

struct pas_cartesian_tree {
    pas_compact_cartesian_tree_node_ptr root;
    pas_compact_cartesian_tree_node_ptr minimum;
};

#define PAS_CARTESIAN_TREE_INITIALIZER { \
        .root = PAS_COMPACT_PTR_INITIALIZER, \
        .minimum = PAS_COMPACT_PTR_INITIALIZER \
    }

static inline void pas_cartesian_tree_construct(pas_cartesian_tree* tree)
{
    pas_compact_cartesian_tree_node_ptr_store(&tree->root, NULL);
    pas_compact_cartesian_tree_node_ptr_store(&tree->minimum, NULL);
}

typedef void* (*pas_cartesian_tree_get_key_callback)(pas_cartesian_tree_node* node);
typedef int (*pas_cartesian_tree_compare_callback)(void* a_key, void* b_key);

struct pas_cartesian_tree_config {
    pas_cartesian_tree_get_key_callback get_x_key;
    pas_cartesian_tree_get_key_callback get_y_key;
    pas_cartesian_tree_compare_callback x_compare;
    pas_cartesian_tree_compare_callback y_compare;
};

static PAS_ALWAYS_INLINE pas_compact_cartesian_tree_node_ptr* pas_cartesian_tree_node_child_ptr(
    pas_cartesian_tree_node* node,
    pas_tree_direction direction)
{
    switch (direction) {
    case pas_tree_direction_left:
        return &node->left;
    case pas_tree_direction_right:
        return &node->right;
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static PAS_ALWAYS_INLINE bool
pas_cartesian_tree_node_is_null_constrained(pas_cartesian_tree_node* node,
                                            void* y_key,
                                            pas_cartesian_tree_config* config)
{
    return !node || config->y_compare(config->get_y_key(node), y_key) < 0;
}

static inline pas_cartesian_tree_node*
pas_cartesian_tree_node_minimum(pas_cartesian_tree_node* node)
{
    for (;;) {
        pas_cartesian_tree_node* left;
        left = pas_compact_cartesian_tree_node_ptr_load(&node->left);
        if (!left)
            return node;
        node = left;
    }
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_node_minimum_constrained(pas_cartesian_tree_node* node,
                                            void* y_key,
                                            pas_cartesian_tree_config* config)
{
    for (;;) {
        pas_cartesian_tree_node* left;
        left = pas_compact_cartesian_tree_node_ptr_load(&node->left);
        if (pas_cartesian_tree_node_is_null_constrained(left, y_key, config))
            return node;
        node = left;
    }
}

static inline pas_cartesian_tree_node*
pas_cartesian_tree_node_maximum(pas_cartesian_tree_node* node)
{
    for (;;) {
        pas_cartesian_tree_node* right;
        right = pas_compact_cartesian_tree_node_ptr_load(&node->right);
        if (!right)
            return node;
        node = right;
    }
}

static inline pas_cartesian_tree_node*
pas_cartesian_tree_node_maximum_constrained(pas_cartesian_tree_node* node,
                                            void* y_key,
                                            pas_cartesian_tree_config* config)
{
    for (;;) {
        pas_cartesian_tree_node* right;
        right = pas_compact_cartesian_tree_node_ptr_load(&node->right);
        if (pas_cartesian_tree_node_is_null_constrained(right, y_key, config))
            return node;
        node = right;
    }
}

static inline pas_cartesian_tree_node*
pas_cartesian_tree_node_successor(pas_cartesian_tree_node* node)
{
    static const bool verbose = false;
    
    pas_cartesian_tree_node* x;
    pas_cartesian_tree_node* y;
    pas_cartesian_tree_node* x_right;
    
    x = node;
    x_right = pas_compact_cartesian_tree_node_ptr_load(&x->right);

    if (verbose)
        pas_log("x = %p, x_right = %p.\n", x, x_right);
    
    if (x_right)
        return pas_cartesian_tree_node_minimum(x_right);
    
    y = pas_compact_cartesian_tree_node_ptr_load(&x->parent);

    if (verbose)
        pas_log("y = %p.\n", y);
    
    while (y && x == pas_compact_cartesian_tree_node_ptr_load(&y->right)) {
        x = y;
        y = pas_compact_cartesian_tree_node_ptr_load(&y->parent);
        if (verbose)
            pas_log("in the loop, now x = %p, y = %p.\n", x, y);
    }
    
    if (verbose)
        pas_log("returning y = %p.\n", y);
    
    return y;
}

static inline pas_cartesian_tree_node*
pas_cartesian_tree_node_successor_constrained(pas_cartesian_tree_node* node,
                                              void* y_key,
                                              pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* x;
    pas_cartesian_tree_node* y;
    pas_cartesian_tree_node* x_right;
    
    x = node;
    x_right = pas_compact_cartesian_tree_node_ptr_load(&x->right);
    if (!pas_cartesian_tree_node_is_null_constrained(x_right, y_key, config))
        return pas_cartesian_tree_node_minimum_constrained(x_right, y_key, config);
    
    y = pas_compact_cartesian_tree_node_ptr_load(&x->parent);
    while (!pas_cartesian_tree_node_is_null_constrained(y, y_key, config)
           && x == pas_compact_cartesian_tree_node_ptr_load(&y->right)) {
        x = y;
        y = pas_compact_cartesian_tree_node_ptr_load(&y->parent);
    }
    return pas_cartesian_tree_node_is_null_constrained(y, y_key, config) ? NULL : y;
}

static inline pas_cartesian_tree_node*
pas_cartesian_tree_node_predecessor(pas_cartesian_tree_node* node)
{
    pas_cartesian_tree_node* x;
    pas_cartesian_tree_node* y;
    pas_cartesian_tree_node* x_left;
    
    x = node;
    x_left = pas_compact_cartesian_tree_node_ptr_load(&x->left);
    if (x_left)
        return pas_cartesian_tree_node_maximum(x_left);
    
    y = pas_compact_cartesian_tree_node_ptr_load(&x->parent);
    while (y && x == pas_compact_cartesian_tree_node_ptr_load(&y->left)) {
        x = y;
        y = pas_compact_cartesian_tree_node_ptr_load(&y->parent);
    }
    return y;
}

static inline pas_cartesian_tree_node*
pas_cartesian_tree_node_predecessor_constrained(pas_cartesian_tree_node* node,
                                                void* y_key,
                                                pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* x;
    pas_cartesian_tree_node* y;
    pas_cartesian_tree_node* x_left;
    
    x = node;
    x_left = pas_compact_cartesian_tree_node_ptr_load(&x->left);
    if (!pas_cartesian_tree_node_is_null_constrained(x_left, y_key, config))
        return pas_cartesian_tree_node_maximum_constrained(x_left, y_key, config);
    
    y = pas_compact_cartesian_tree_node_ptr_load(&x->parent);
    while (!pas_cartesian_tree_node_is_null_constrained(y, y_key, config)
           && x == pas_compact_cartesian_tree_node_ptr_load(&y->left)) {
        x = y;
        y = pas_compact_cartesian_tree_node_ptr_load(&y->parent);
    }
    return pas_cartesian_tree_node_is_null_constrained(y, y_key, config) ? NULL : y;
}

static inline void pas_cartesian_tree_node_reset(pas_cartesian_tree_node* node)
{
    pas_compact_cartesian_tree_node_ptr_store(&node->left, NULL);
    pas_compact_cartesian_tree_node_ptr_store(&node->right, NULL);
    pas_compact_cartesian_tree_node_ptr_store(&node->parent, NULL);
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_node_find_exact(pas_cartesian_tree_node* root,
                                   void* x_key,
                                   pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* current;
    for (current = root; current;) {
        int compare_result;
        
        compare_result = config->x_compare(config->get_x_key(current), x_key);
        
        if (compare_result == 0)
            return current;
        if (compare_result > 0)
            current = pas_compact_cartesian_tree_node_ptr_load(&current->left);
        else
            current = pas_compact_cartesian_tree_node_ptr_load(&current->right);
    }
    return NULL;
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_find_exact(pas_cartesian_tree* tree,
                              void* x_key,
                              pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* root;
    root = pas_compact_cartesian_tree_node_ptr_load(&tree->root);
    if (!root)
        return NULL;
    return pas_cartesian_tree_node_find_exact(root, x_key, config);
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_node_find_least_greater_than_or_equal(
    pas_cartesian_tree_node* root,
    void* x_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* current;
    pas_cartesian_tree_node* best;
    
    best = NULL;
    
    for (current = root; current;) {
        int compare_result;
        
        compare_result = config->x_compare(config->get_x_key(current), x_key);
        
        if (compare_result == 0)
            return current;
        
        if (compare_result < 0)
            current = pas_compact_cartesian_tree_node_ptr_load(&current->right);
        else {
            best = current;
            current = pas_compact_cartesian_tree_node_ptr_load(&current->left);
        }
    }
    
    return best;
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_find_least_greater_than_or_equal(
    pas_cartesian_tree* tree,
    void* x_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* root;
    root = pas_compact_cartesian_tree_node_ptr_load(&tree->root);
    if (!root)
        return NULL;
    return pas_cartesian_tree_node_find_least_greater_than_or_equal(root, x_key, config);
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_node_find_least_greater_than(
    pas_cartesian_tree_node* root,
    void* x_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* current;
    pas_cartesian_tree_node* best;
    
    best = NULL;
    
    for (current = root; current;) {
        int compare_result;
        
        compare_result = config->x_compare(config->get_x_key(current), x_key);
        
        if (compare_result <= 0)
            current = pas_compact_cartesian_tree_node_ptr_load(&current->right);
        else {
            best = current;
            current = pas_compact_cartesian_tree_node_ptr_load(&current->left);
        }
    }
    
    return best;
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_find_least_greater_than(
    pas_cartesian_tree* tree,
    void* x_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* root;
    root = pas_compact_cartesian_tree_node_ptr_load(&tree->root);
    if (!root)
        return NULL;
    return pas_cartesian_tree_node_find_least_greater_than(root, x_key, config);
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_node_find_greatest_less_than_or_equal(
    pas_cartesian_tree_node* root,
    void* x_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* current;
    pas_cartesian_tree_node* best;
    
    best = NULL;
    
    for (current = root; current;) {
        int compare_result;
        
        compare_result = config->x_compare(config->get_x_key(current), x_key);
        
        if (compare_result == 0)
            return current;
        
        if (compare_result > 0)
            current = pas_compact_cartesian_tree_node_ptr_load(&current->left);
        else {
            best = current;
            current = pas_compact_cartesian_tree_node_ptr_load(&current->right);
        }
    }
    
    return best;
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_find_greatest_less_than_or_equal(
    pas_cartesian_tree* tree,
    void* x_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* root;
    root = pas_compact_cartesian_tree_node_ptr_load(&tree->root);
    if (!root)
        return NULL;
    return pas_cartesian_tree_node_find_greatest_less_than_or_equal(root, x_key, config);
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_node_find_greatest_less_than(
    pas_cartesian_tree_node* root,
    void* x_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* current;
    pas_cartesian_tree_node* best;
    
    best = NULL;
    
    for (current = root; current;) {
        int compare_result;
        
        compare_result = config->x_compare(config->get_x_key(current), x_key);
        
        if (compare_result >= 0)
            current = pas_compact_cartesian_tree_node_ptr_load(&current->left);
        else {
            best = current;
            current = pas_compact_cartesian_tree_node_ptr_load(&current->right);
        }
    }
    
    return best;
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node*
pas_cartesian_tree_find_greatest_less_than(
    pas_cartesian_tree* tree,
    void* x_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* root;
    root = pas_compact_cartesian_tree_node_ptr_load(&tree->root);
    if (!root)
        return NULL;
    return pas_cartesian_tree_node_find_greatest_less_than(root, x_key, config);
}

static inline pas_cartesian_tree_node* pas_cartesian_tree_minimum(pas_cartesian_tree* tree)
{
    return pas_compact_cartesian_tree_node_ptr_load(&tree->minimum);
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node* pas_cartesian_tree_minimum_constrained(
    pas_cartesian_tree* tree,
    void* y_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* result;

    result = pas_compact_cartesian_tree_node_ptr_load(&tree->minimum);

    for (;;) {
        if (!result)
            return NULL;

        if (config->y_compare(config->get_y_key(result), y_key) >= 0)
            return result;

        result = pas_compact_cartesian_tree_node_ptr_load(&result->parent);
    }
}

static inline pas_cartesian_tree_node* pas_cartesian_tree_maximum(pas_cartesian_tree* tree)
{
    pas_cartesian_tree_node* root;
    root = pas_compact_cartesian_tree_node_ptr_load(&tree->root);
    if (!root)
        return NULL;
    return pas_cartesian_tree_node_maximum(root);
}

static PAS_ALWAYS_INLINE pas_cartesian_tree_node* pas_cartesian_tree_maximum_constrained(
    pas_cartesian_tree* tree,
    void* y_key,
    pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* root;
    root = pas_compact_cartesian_tree_node_ptr_load(&tree->root);
    if (!root)
        return NULL;
    return pas_cartesian_tree_node_maximum_constrained(root, y_key, config);
}

static inline bool pas_cartesian_tree_is_empty(pas_cartesian_tree* tree)
{
    return !pas_compact_cartesian_tree_node_ptr_load(&tree->root);
}

static PAS_ALWAYS_INLINE void
pas_cartesian_tree_insert(pas_cartesian_tree* tree,
                          void* x_key,
                          void* y_key,
                          pas_cartesian_tree_node* node,
                          pas_cartesian_tree_config* config)
{
    static const bool verbose = false;
    
    pas_cartesian_tree_node* last_node;
    pas_cartesian_tree_node* current;
    pas_compact_cartesian_tree_node_ptr* child_ptr;
    bool should_update_minimum;

    pas_cartesian_tree_node_reset(node);

    child_ptr = &tree->root;
    current = pas_compact_cartesian_tree_node_ptr_load(child_ptr);
    last_node = NULL;

    should_update_minimum = true;

    if (verbose)
        pas_log("x_key = %p, y_key = %p, node = %p\n", x_key, y_key, node);

    for (;;) {
        pas_tree_direction direction;

        if (verbose) {
            pas_log("1/ current = %p, child_ptr = %p, last_node = %p.\n",
                    current, child_ptr, last_node);
        }
        
        if (!current) {
            /* This is the super easy case. */

            if (verbose)
                pas_log("easy insertion.\n");
            
            pas_compact_cartesian_tree_node_ptr_store(&node->parent, last_node);
            pas_compact_cartesian_tree_node_ptr_store(child_ptr, node);
            goto update_minimum_if_necessary_and_return;
        }

        /* We have to insert above this node if y_key > current. Otherwise favor inserting
           below. */
        if (config->y_compare(config->get_y_key(current), y_key) < 0) {
            pas_compact_cartesian_tree_node_ptr* other_child_ptr;
            pas_cartesian_tree_node* other_last_node;

            if (verbose)
                pas_log("hard insertion.\n");

            /* Select the direction in which current will hang off node. */
            if (config->x_compare(config->get_x_key(current), x_key) < 0) {
                direction = pas_tree_direction_left;
                should_update_minimum = false;
            } else
                direction = pas_tree_direction_right;

            if (verbose)
                pas_log("direction = %s\n", pas_tree_direction_get_string(direction));

            /* Begin to attach things. */
            pas_compact_cartesian_tree_node_ptr_store(&node->parent, last_node);
            pas_compact_cartesian_tree_node_ptr_store(child_ptr, node);
            pas_compact_cartesian_tree_node_ptr_store(
                pas_cartesian_tree_node_child_ptr(node, direction), current);
            pas_compact_cartesian_tree_node_ptr_store(&current->parent, node);
            
            other_child_ptr = pas_cartesian_tree_node_child_ptr(
                node, pas_tree_direction_invert(direction));
            other_last_node = node;

            do {
                if (verbose) {
                    pas_log("2/ current = %p, child_ptr = %p, last_node = %p, "
                            "other_child_ptr = %p, other_last_node = %p.\n",
                            current, child_ptr, last_node, other_child_ptr, other_last_node);
                }
                
                child_ptr = pas_cartesian_tree_node_child_ptr(
                    current, pas_tree_direction_invert(direction));
                last_node = current;

                current = pas_compact_cartesian_tree_node_ptr_load(child_ptr);

                if (!current) {
                    pas_compact_cartesian_tree_node_ptr_store(other_child_ptr, NULL);
                    if (verbose)
                        pas_log("bailing early.\n");
                    break;
                }

                if (pas_tree_direction_invert_comparison_result_if_right(
                        direction,
                        config->x_compare(config->get_x_key(current), x_key)) <= 0) {
                    if (verbose)
                        pas_log("key too small, continuing.\n");
                    continue;
                }

                should_update_minimum = false;
                pas_compact_cartesian_tree_node_ptr_store(other_child_ptr, current);
                pas_compact_cartesian_tree_node_ptr_store(&current->parent, other_last_node);

                for (;;) {
                    if (verbose) {
                        pas_log("3/ current = %p, child_ptr = %p, last_node = %p, "
                                "other_child_ptr = %p, other_last_node = %p.\n",
                                current, child_ptr, last_node, other_child_ptr, other_last_node);
                    }
                
                    other_child_ptr = pas_cartesian_tree_node_child_ptr(current, direction);
                    other_last_node = current;

                    current = pas_compact_cartesian_tree_node_ptr_load(other_child_ptr);

                    if (!current) {
                        pas_compact_cartesian_tree_node_ptr_store(child_ptr, NULL);
                        break;
                    }

                    if (pas_tree_direction_invert_comparison_result_if_right(
                            direction,
                            config->x_compare(config->get_x_key(current), x_key)) > 0)
                        continue;

                    pas_compact_cartesian_tree_node_ptr_store(child_ptr, current);
                    pas_compact_cartesian_tree_node_ptr_store(&current->parent, last_node);
                    break;
                }
            } while (current);
            
            goto update_minimum_if_necessary_and_return;
        }

        if (config->x_compare(config->get_x_key(current), x_key) > 0)
            direction = pas_tree_direction_left;
        else {
            should_update_minimum = false;
            direction = pas_tree_direction_right;
        }

        last_node = current;
        child_ptr = pas_cartesian_tree_node_child_ptr(current, direction);
        current = pas_compact_cartesian_tree_node_ptr_load(child_ptr);
    }

    PAS_ASSERT(!"Should not be reached");

update_minimum_if_necessary_and_return:
    if (should_update_minimum)
        pas_compact_cartesian_tree_node_ptr_store(&tree->minimum, node);
}

static PAS_ALWAYS_INLINE void
pas_cartesian_tree_remove(pas_cartesian_tree* tree,
                          pas_cartesian_tree_node* node,
                          pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* parent;
    pas_compact_cartesian_tree_node_ptr* child_ptr;
    pas_cartesian_tree_node* left;
    pas_cartesian_tree_node* right;
    void* left_y_key;
    void* right_y_key;
    pas_tree_direction direction;
    pas_cartesian_tree_node* replacement;
    pas_cartesian_tree_node* hanging_chad;
    void* hanging_chad_key;
    void* replacement_key;

    if (node == pas_compact_cartesian_tree_node_ptr_load(&tree->minimum)) {
        pas_compact_cartesian_tree_node_ptr_store(
            &tree->minimum, pas_cartesian_tree_node_successor(node));
    }

    parent = pas_compact_cartesian_tree_node_ptr_load(&node->parent);
    left = pas_compact_cartesian_tree_node_ptr_load(&node->left);
    right = pas_compact_cartesian_tree_node_ptr_load(&node->right);

    if (!parent)
        child_ptr = &tree->root;
    else if (pas_compact_cartesian_tree_node_ptr_load(&parent->left) == node)
        child_ptr = &parent->left;
    else
        child_ptr = &parent->right;

    if (!left) {
        pas_compact_cartesian_tree_node_ptr_store(child_ptr, right);
        if (right)
            pas_compact_cartesian_tree_node_ptr_store(&right->parent, parent);
        return;
    }

    if (!right) {
        pas_compact_cartesian_tree_node_ptr_store(child_ptr, left);
        pas_compact_cartesian_tree_node_ptr_store(&left->parent, parent);
        return;
    }

    left_y_key = config->get_y_key(left);
    right_y_key = config->get_y_key(right);

    /* If possible, we want the tree to have more stuff on the right.
       
       If we pick the right child, then we will end up putting the former left subtree onto the
       left side of the right subtree, creating a longer left arm. That's the opposite of what
       we want.
       
       So in case of a tie, we pick the left child. That way we will put the right subtree on the
       right side of the left subtree, creating a longer right arm. */
    if (config->y_compare(left_y_key, right_y_key) >= 0) {
        direction = pas_tree_direction_left;
        replacement = left;
        hanging_chad = right;
        hanging_chad_key = right_y_key;
    } else {
        direction = pas_tree_direction_right;
        replacement = right;
        hanging_chad = left;
        hanging_chad_key = left_y_key;
    }

    pas_compact_cartesian_tree_node_ptr_store(child_ptr, replacement);
    pas_compact_cartesian_tree_node_ptr_store(&replacement->parent, parent);

    for (;;) {
        PAS_ASSERT(replacement);
        PAS_ASSERT(hanging_chad);
        
        parent = replacement;

        child_ptr = pas_cartesian_tree_node_child_ptr(
            replacement, pas_tree_direction_invert(direction));

        replacement = pas_compact_cartesian_tree_node_ptr_load(child_ptr);

        if (!replacement) {
            pas_compact_cartesian_tree_node_ptr_store(child_ptr, hanging_chad);
            pas_compact_cartesian_tree_node_ptr_store(&hanging_chad->parent, parent);
            return;
        }

        replacement_key = config->get_y_key(replacement);

        if (config->y_compare(replacement_key, hanging_chad_key) < 0) {
            pas_compact_cartesian_tree_node_ptr_store(child_ptr, hanging_chad);
            pas_compact_cartesian_tree_node_ptr_store(&hanging_chad->parent, parent);

            for (;;) {
                parent = hanging_chad;
                child_ptr = pas_cartesian_tree_node_child_ptr(hanging_chad, direction);
                hanging_chad = pas_compact_cartesian_tree_node_ptr_load(child_ptr);

                if (!hanging_chad) {
                    pas_compact_cartesian_tree_node_ptr_store(child_ptr, replacement);
                    pas_compact_cartesian_tree_node_ptr_store(&replacement->parent, parent);
                    return;
                }

                hanging_chad_key = config->get_y_key(hanging_chad);
                
                if (config->y_compare(replacement_key, hanging_chad_key) > 0) {
                    pas_compact_cartesian_tree_node_ptr_store(child_ptr, replacement);
                    pas_compact_cartesian_tree_node_ptr_store(&replacement->parent, parent);
                    break;
                }
            }
        }
    }
}

static inline void pas_cartesian_tree_validate_recurse(pas_cartesian_tree_node* node,
                                                       pas_cartesian_tree_config* config,
                                                       pas_cartesian_tree_node* left_bound,
                                                       pas_cartesian_tree_node* right_bound)
{
    static const bool verbose = false;
    
    pas_cartesian_tree_node* parent;
    pas_cartesian_tree_node* left;
    pas_cartesian_tree_node* right;

    if (verbose)
        pas_log("Validating node = %p.\n", node);
    
    if (left_bound) {
        PAS_ASSERT(config->x_compare(config->get_x_key(node),
                                     config->get_x_key(left_bound)) >= 0);
    }

    if (right_bound) {
        PAS_ASSERT(config->x_compare(config->get_x_key(node),
                                     config->get_x_key(right_bound)) <= 0);
    }

    parent = pas_compact_cartesian_tree_node_ptr_load(&node->parent);
    left = pas_compact_cartesian_tree_node_ptr_load(&node->left);
    right = pas_compact_cartesian_tree_node_ptr_load(&node->right);

    if (verbose)
        pas_log("have parent = %p, left = %p, right = %p.\n", parent, left, right);

    if (parent) {
        PAS_ASSERT(config->y_compare(config->get_y_key(parent),
                                     config->get_y_key(node)) >= 0);
    }

    if (left) {
        PAS_ASSERT(pas_compact_cartesian_tree_node_ptr_load(&left->parent) == node);
        pas_cartesian_tree_validate_recurse(left, config, left_bound, node);
    }

    if (right) {
        PAS_ASSERT(pas_compact_cartesian_tree_node_ptr_load(&right->parent) == node);
        pas_cartesian_tree_validate_recurse(right, config, node, right_bound);
    }
}

static inline void pas_cartesian_tree_validate(pas_cartesian_tree* tree,
                                               pas_cartesian_tree_config* config)
{
    pas_cartesian_tree_node* node;

    node = pas_compact_cartesian_tree_node_ptr_load(&tree->root);
    if (node) {
        PAS_ASSERT(!pas_compact_cartesian_tree_node_ptr_load(&node->parent));
        pas_cartesian_tree_validate_recurse(node, config, NULL, NULL);
        
        PAS_ASSERT(pas_compact_cartesian_tree_node_ptr_load(&tree->minimum)
                   == pas_cartesian_tree_node_minimum(node));
    } else
        PAS_ASSERT(!pas_compact_cartesian_tree_node_ptr_load(&tree->minimum));
}

static inline size_t pas_cartesian_tree_size(pas_cartesian_tree* tree)
{
    static const bool verbose = false;
    
    pas_cartesian_tree_node* node;
    size_t result;

    result = 0;

    for (node = pas_cartesian_tree_minimum(tree);
         node;
         node = pas_cartesian_tree_node_successor(node)) {
        if (verbose) {
            pas_log("size seeing node = %p, successor = %p.\n",
                    node, pas_cartesian_tree_node_successor(node));
        }
        result++;
    }

    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_CARTESIAN_TREE_H */

