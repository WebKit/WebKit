/*
 * Copyright (C) 2003, 2004, 2005 Filip Pizlo. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "gpc_internal.h"
#include "tsf_format.h"

struct worklist {
    worklist_item_t *array;
    worklist_item_t *cur;
    size_t size;
};

typedef struct worklist worklist_t;

static inline tsf_bool_t worklist_init(worklist_t *worklist) {
    worklist->array = NULL;
    worklist->cur = NULL;
    worklist->size = 0;
    return tsf_true;
}

static inline void worklist_done(worklist_t *worklist) {
    if (worklist->array != NULL) {
        free(worklist->array);
    }
}

static inline worklist_item_t *worklist_push(worklist_t *worklist) {
    worklist_item_t *ret;
    
    if (worklist->cur == worklist->array+worklist->size) {
        if (worklist->array == NULL) {
            worklist->array = malloc(sizeof(worklist_item_t));
            if (worklist->array == NULL) {
                tsf_set_errno("Could not malloc worklist array");
                return NULL;
            }
            worklist->cur = worklist->array;
        } else {
            worklist_item_t *new_array = realloc(worklist->array,
                                                 sizeof(worklist_item_t) *
                                                 (1 + worklist->size));
            if (new_array == NULL) {
                tsf_set_errno("Could not realloc worklist array");
                return NULL;
            }
            worklist->cur += new_array-worklist->array;
            worklist->array = new_array;
        }
        worklist->size++;
    }
    
    ret = worklist->cur;
    worklist->cur++;
    return ret;
}

static inline worklist_item_t *worklist_pop(worklist_t *worklist) {
    if (worklist->cur == worklist->array) {
        return NULL;
    }

    return --worklist->cur;
}


