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

#ifndef PAS_TREE_DIRECTION_H
#define PAS_TREE_DIRECTION_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_tree_direction {
    pas_tree_direction_left,
    pas_tree_direction_right
};

typedef enum pas_tree_direction pas_tree_direction;

static inline const char* pas_tree_direction_get_string(pas_tree_direction direction)
{
    switch (direction) {
    case pas_tree_direction_left:
        return "left";
    case pas_tree_direction_right:
        return "right";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static inline pas_tree_direction pas_tree_direction_invert(pas_tree_direction direction)
{
    switch (direction) {
    case pas_tree_direction_left:
        return pas_tree_direction_right;
    case pas_tree_direction_right:
        return pas_tree_direction_left;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_tree_direction_left;
}

static inline int
pas_tree_direction_invert_comparison_result_if_right(
    pas_tree_direction direction,
    int comparison_result)
{
    switch (direction) {
    case pas_tree_direction_left:
        return comparison_result;
    case pas_tree_direction_right:
        return -comparison_result;
    }
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

PAS_END_EXTERN_C;

#endif /* PAS_TREE_DIRECTION_H */

