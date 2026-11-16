/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This code is based on WebKit's wtf/RedBlackTree.h. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_red_black_tree.h"

#include <stdio.h>

static const bool verbose = false;

#if PAS_ENABLE_TESTING
void (*pas_red_black_tree_validate_enumerable_callback)(void);
#endif /* PAS_ENABLE_TESTING */

static void tree_insert(pas_red_black_tree* tree,
                        pas_red_black_tree_node* z,
                        pas_red_black_tree_node_compare_callback compare_callback)
{
    pas_red_black_tree_node* x;
    pas_red_black_tree_node* y;
    
    PAS_ASSERT(!pas_red_black_tree_node_get_left(z));
    PAS_ASSERT(!pas_red_black_tree_node_get_right(z));
    PAS_ASSERT(!pas_red_black_tree_node_get_parent(z));
    PAS_ASSERT(pas_red_black_tree_node_get_color(z) == pas_red_black_tree_color_red);
    
    y = NULL;
    x = pas_red_black_tree_get_root(tree);
    
    while (x) {
        y = x;
        if (compare_callback(z, x) < 0)
            x = pas_red_black_tree_node_get_left(x);
        else
            x = pas_red_black_tree_node_get_right(x);
    }
    
    pas_red_black_tree_node_set_parent(z, y);
    
    if (!y)
        pas_red_black_tree_set_root(tree, z);
    else {
        if (compare_callback(z, y) < 0)
            pas_red_black_tree_node_set_left(y, z);
        else
            pas_red_black_tree_node_set_right(y, z);
    }
}

static pas_red_black_tree_node* left_rotate(pas_red_black_tree* tree,
                                            pas_red_black_tree_node* x,
                                            pas_red_black_tree_jettisoned_nodes* jettisoned_nodes)
{
    pas_red_black_tree_node* y;
    pas_red_black_tree_node* y_left;
    pas_red_black_tree_node* x_parent;
    
    y = pas_red_black_tree_node_get_right(x);
    
    jettisoned_nodes->first_rotate_jettisoned = x;
    jettisoned_nodes->second_rotate_jettisoned = y;
    
    y_left = pas_red_black_tree_node_get_left(y);
    pas_red_black_tree_node_set_right(x, y_left);
    if (y_left)
        pas_red_black_tree_node_set_parent(y_left, x);

    x_parent = pas_red_black_tree_node_get_parent(x);
    pas_red_black_tree_node_set_parent(y, x_parent);
    if (!x_parent)
        pas_red_black_tree_set_root(tree, y);
    else {
        if (x == pas_red_black_tree_node_get_left(x_parent))
            pas_red_black_tree_node_set_left(x_parent, y);
        else
            pas_red_black_tree_node_set_right(x_parent, y);
    }
    
    pas_red_black_tree_node_set_left(y, x);
    pas_red_black_tree_node_set_parent(x, y);

    jettisoned_nodes->first_rotate_jettisoned = NULL;
    jettisoned_nodes->second_rotate_jettisoned = NULL;
    
    return y;
}

static pas_red_black_tree_node* right_rotate(pas_red_black_tree* tree,
                                             pas_red_black_tree_node* y,
                                             pas_red_black_tree_jettisoned_nodes* jettisoned_nodes)
{
    pas_red_black_tree_node* x;
    pas_red_black_tree_node* x_right;
    pas_red_black_tree_node* y_parent;
    
    x = pas_red_black_tree_node_get_left(y);
    
    jettisoned_nodes->first_rotate_jettisoned = y;
    jettisoned_nodes->second_rotate_jettisoned = x;
    pas_compiler_fence();
    
    x_right = pas_red_black_tree_node_get_right(x);
    pas_red_black_tree_node_set_left(y, x_right);
    if (x_right)
        pas_red_black_tree_node_set_parent(x_right, y);

    y_parent = pas_red_black_tree_node_get_parent(y);
    pas_red_black_tree_node_set_parent(x, y_parent);
    if (!y_parent)
        pas_red_black_tree_set_root(tree, x);
    else {
        if (y == pas_red_black_tree_node_get_left(y_parent))
            pas_red_black_tree_node_set_left(y_parent, x);
        else
            pas_red_black_tree_node_set_right(y_parent, x);
    }
    
    pas_red_black_tree_node_set_right(x, y);
    pas_red_black_tree_node_set_parent(y, x);

    pas_compiler_fence();
    jettisoned_nodes->first_rotate_jettisoned = NULL;
    jettisoned_nodes->second_rotate_jettisoned = NULL;
    
    return x;
}

static void remove_fixup(pas_red_black_tree* tree,
                         pas_red_black_tree_node* x,
                         pas_red_black_tree_node* x_parent,
                         pas_red_black_tree_jettisoned_nodes* jettisoned_nodes)
{
    pas_red_black_tree_node* w;
    pas_red_black_tree_node* w_left;
    pas_red_black_tree_node* w_right;
    
    while (x != pas_red_black_tree_get_root(tree)
           && (!x || pas_red_black_tree_node_get_color(x) == pas_red_black_tree_color_black)) {
        if (x == pas_red_black_tree_node_get_left(x_parent)) {
            w = pas_red_black_tree_node_get_right(x_parent);
            PAS_ASSERT(w);
            if (pas_red_black_tree_node_get_color(w) == pas_red_black_tree_color_red) {
                pas_red_black_tree_node_set_color(w, pas_red_black_tree_color_black);
                pas_red_black_tree_node_set_color(x_parent, pas_red_black_tree_color_red);
                left_rotate(tree, x_parent, jettisoned_nodes);
                w = pas_red_black_tree_node_get_right(x_parent);
            }
            w_left = pas_red_black_tree_node_get_left(w);
            w_right = pas_red_black_tree_node_get_right(w);
            if ((!w_left
                 || pas_red_black_tree_node_get_color(w_left) == pas_red_black_tree_color_black) &&
                (!w_right
                 || pas_red_black_tree_node_get_color(w_right) == pas_red_black_tree_color_black)) {
                pas_red_black_tree_node_set_color(w, pas_red_black_tree_color_red);
                x = x_parent;
                x_parent = pas_red_black_tree_node_get_parent(x);
            } else {
                if (!w_right
                    || pas_red_black_tree_node_get_color(w_right) == pas_red_black_tree_color_black) {
                    pas_red_black_tree_node_set_color(w_left, pas_red_black_tree_color_black);
                    pas_red_black_tree_node_set_color(w, pas_red_black_tree_color_red);
                    right_rotate(tree, w, jettisoned_nodes);
                    w = pas_red_black_tree_node_get_right(x_parent);
                }
                pas_red_black_tree_node_set_color(w, pas_red_black_tree_node_get_color(x_parent));
                pas_red_black_tree_node_set_color(x_parent, pas_red_black_tree_color_black);
                w_right = pas_red_black_tree_node_get_right(w);
                if (w_right)
                    pas_red_black_tree_node_set_color(w_right, pas_red_black_tree_color_black);
                left_rotate(tree, x_parent, jettisoned_nodes);
                x = pas_red_black_tree_get_root(tree);
                x_parent = pas_red_black_tree_node_get_parent(x);
            }
        } else {
            w = pas_red_black_tree_node_get_left(x_parent);
            PAS_ASSERT(w);
            if (pas_red_black_tree_node_get_color(w) == pas_red_black_tree_color_red) {
                pas_red_black_tree_node_set_color(w, pas_red_black_tree_color_black);
                pas_red_black_tree_node_set_color(x_parent, pas_red_black_tree_color_red);
                right_rotate(tree, x_parent, jettisoned_nodes);
                w = pas_red_black_tree_node_get_left(x_parent);
            }
            w_left = pas_red_black_tree_node_get_left(w);
            w_right = pas_red_black_tree_node_get_right(w);
            if ((!w_right
                 || pas_red_black_tree_node_get_color(w_right) == pas_red_black_tree_color_black) &&
                (!w_left
                 || pas_red_black_tree_node_get_color(w_left) == pas_red_black_tree_color_black)) {
                pas_red_black_tree_node_set_color(w, pas_red_black_tree_color_red);
                x = x_parent;
                x_parent = pas_red_black_tree_node_get_parent(x);
            } else {
                if (!w_left
                    || pas_red_black_tree_node_get_color(w_left) == pas_red_black_tree_color_black) {
                    pas_red_black_tree_node_set_color(w_right, pas_red_black_tree_color_black);
                    pas_red_black_tree_node_set_color(w, pas_red_black_tree_color_red);
                    left_rotate(tree, w, jettisoned_nodes);
                    w = pas_red_black_tree_node_get_left(x_parent);
                }
                pas_red_black_tree_node_set_color(w, pas_red_black_tree_node_get_color(x_parent));
                pas_red_black_tree_node_set_color(x_parent, pas_red_black_tree_color_black);
                w_left = pas_red_black_tree_node_get_left(w);
                if (w_left)
                    pas_red_black_tree_node_set_color(w_left, pas_red_black_tree_color_black);
                right_rotate(tree, x_parent, jettisoned_nodes);
                x = pas_red_black_tree_get_root(tree);
                x_parent = pas_red_black_tree_node_get_parent(x);
            }
        }
    }
    
    if (x)
        pas_red_black_tree_node_set_color(x, pas_red_black_tree_color_black);
}

void pas_red_black_tree_insert(pas_red_black_tree* tree,
                               pas_red_black_tree_node* x,
                               pas_red_black_tree_node_compare_callback compare_callback,
                               pas_red_black_tree_jettisoned_nodes* jettisoned_nodes)
{
    pas_red_black_tree_node* y;
    
    if (verbose)
        printf("Inserting %p!\n", x);
    
    pas_red_black_tree_node_reset(x);
    tree_insert(tree, x, compare_callback);
    pas_red_black_tree_node_set_color(x, pas_red_black_tree_color_red);
    
    while (x != pas_red_black_tree_get_root(tree) &&
           pas_red_black_tree_node_get_color(
               pas_red_black_tree_node_get_parent(x)) == pas_red_black_tree_color_red) {
        pas_red_black_tree_node* x_parent;
        pas_red_black_tree_node* x_parent_parent;
        x_parent = pas_red_black_tree_node_get_parent(x);
        x_parent_parent = pas_red_black_tree_node_get_parent(x_parent);
        if (x_parent == pas_red_black_tree_node_get_left(x_parent_parent)) {
            y = pas_red_black_tree_node_get_right(x_parent_parent);
            if (y && pas_red_black_tree_node_get_color(y) == pas_red_black_tree_color_red) {
                pas_red_black_tree_node_set_color(x_parent, pas_red_black_tree_color_black);
                pas_red_black_tree_node_set_color(y, pas_red_black_tree_color_black);
                pas_red_black_tree_node_set_color(x_parent_parent, pas_red_black_tree_color_red);
                x = x_parent_parent;
            } else {
                if (x == pas_red_black_tree_node_get_right(x_parent)) {
                    x = x_parent;
                    left_rotate(tree, x, jettisoned_nodes);
                    x_parent = pas_red_black_tree_node_get_parent(x);
                    x_parent_parent = pas_red_black_tree_node_get_parent(x_parent);
                }
                pas_red_black_tree_node_set_color(x_parent, pas_red_black_tree_color_black);
                pas_red_black_tree_node_set_color(x_parent_parent, pas_red_black_tree_color_red);
                right_rotate(tree, x_parent_parent, jettisoned_nodes);
            }
        } else {
            y = pas_red_black_tree_node_get_left(x_parent_parent);
            if (y && pas_red_black_tree_node_get_color(y) == pas_red_black_tree_color_red) {
                pas_red_black_tree_node_set_color(x_parent, pas_red_black_tree_color_black);
                pas_red_black_tree_node_set_color(y, pas_red_black_tree_color_black);
                pas_red_black_tree_node_set_color(x_parent_parent, pas_red_black_tree_color_red);
                x = x_parent_parent;
            } else {
                if (x == pas_red_black_tree_node_get_left(x_parent)) {
                    x = x_parent;
                    right_rotate(tree, x, jettisoned_nodes);
                    x_parent = pas_red_black_tree_node_get_parent(x);
                    x_parent_parent = pas_red_black_tree_node_get_parent(x_parent);
                }
                pas_red_black_tree_node_set_color(x_parent, pas_red_black_tree_color_black);
                pas_red_black_tree_node_set_color(x_parent_parent, pas_red_black_tree_color_red);
                left_rotate(tree, x_parent_parent, jettisoned_nodes);
            }
        }
    }

    pas_red_black_tree_node_set_color(pas_red_black_tree_get_root(tree),
                                      pas_red_black_tree_color_black);
}

pas_red_black_tree_node*
pas_red_black_tree_remove(pas_red_black_tree* tree,
                          pas_red_black_tree_node* z,
                          pas_red_black_tree_jettisoned_nodes* jettisoned_nodes)
{
    pas_red_black_tree_node* x;
    pas_red_black_tree_node* y;
    pas_red_black_tree_node* x_parent;
    pas_red_black_tree_node* y_left;
    pas_red_black_tree_node* root;
    
    if (verbose)
        printf("Removing %p!\n", z);
    
    PAS_ASSERT(z);
    PAS_ASSERT(pas_red_black_tree_node_get_parent(z)
               || z == pas_red_black_tree_get_root(tree));

    if (!pas_red_black_tree_node_get_left(z) || !pas_red_black_tree_node_get_right(z))
        y = z;
    else
        y = pas_red_black_tree_node_successor(z);

    y_left = pas_red_black_tree_node_get_left(y);
    if (y_left)
        x = y_left;
    else
        x = pas_red_black_tree_node_get_right(y);

    /* x is a child of y, preferably the left child. */
    
    if (x) {
        x_parent = pas_red_black_tree_node_get_parent(y);
        pas_red_black_tree_node_set_parent(x, x_parent);
    } else
        x_parent = pas_red_black_tree_node_get_parent(y);

    /* x_parent was the parent of y, but will be the parent of x. */
    jettisoned_nodes->remove_jettisoned = y;
    
    if (!x_parent)
        pas_red_black_tree_set_root(tree, x);
    else {
        if (y == pas_red_black_tree_node_get_left(x_parent))
            pas_red_black_tree_node_set_left(x_parent, x);
        else
            pas_red_black_tree_node_set_right(x_parent, x);
    }

    /* At this point, y is removed from the tree. That's weird, since we're being asked to remove z. So,
       if y and z are not the same node, then we swap in y into z's position, so that we can remove z. */
    
    if (y != z) {
        pas_red_black_tree_node* z_left;
        pas_red_black_tree_node* z_right;
        pas_red_black_tree_node* z_parent;
        
        if (pas_red_black_tree_node_get_color(y) == pas_red_black_tree_color_black)
            remove_fixup(tree, x, x_parent, jettisoned_nodes);

        pas_red_black_tree_node_set_left(y, pas_red_black_tree_node_get_left(z));
        pas_compiler_fence();
        pas_red_black_tree_node_set_right(y, pas_red_black_tree_node_get_right(z));
        pas_compiler_fence(); /* Really important that the compiler doesn't "optimize" the copying of
                                 left and right. */
        pas_red_black_tree_node_set_parent(y, pas_red_black_tree_node_get_parent(z));
        pas_red_black_tree_node_set_color(y, pas_red_black_tree_node_get_color(z));

        z_left = pas_red_black_tree_node_get_left(z);
        if (z_left)
            pas_red_black_tree_node_set_parent(z_left, y);
        z_right = pas_red_black_tree_node_get_right(z);
        if (z_right)
            pas_red_black_tree_node_set_parent(z_right, y);
        z_parent = pas_red_black_tree_node_get_parent(z);
        if (z_parent) {
            if (pas_red_black_tree_node_get_left(z_parent) == z)
                pas_red_black_tree_node_set_left(z_parent, y);
            else
                pas_red_black_tree_node_set_right(z_parent, y);
        } else {
            PAS_ASSERT(pas_red_black_tree_get_root(tree) == z);
            pas_red_black_tree_set_root(tree, y);
        }
    } else if (pas_red_black_tree_node_get_color(y) == pas_red_black_tree_color_black)
        remove_fixup(tree, x, x_parent, jettisoned_nodes);

    jettisoned_nodes->remove_jettisoned = NULL;

    root = pas_red_black_tree_get_root(tree);
    if (root)
        PAS_ASSERT(pas_red_black_tree_node_get_color(root) == pas_red_black_tree_color_black);
    
    return z;
}

size_t pas_red_black_tree_size(pas_red_black_tree* tree)
{
    size_t result;
    pas_red_black_tree_node* current;

    result = 0;
    
    for (current = pas_red_black_tree_minimum(tree);
         current;
         current = pas_red_black_tree_node_successor(current))
        result++;
    
    return result;
}

#endif /* LIBPAS_ENABLED */
