/*
 * Copyright (C) 2003, 2004, 2005, 2014, 2015 Filip Pizlo. All rights reserved.
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

/*
 * Region memory management API specifically for complex objects that consist of lots of
 * small chunks of memory. This code will compile as both C and C++.
 */

#ifndef FP_TSF_REGION_H
#define FP_TSF_REGION_H

#include "tsf_config.h"
#include "tsf_types.h"

#include <string.h>
#include <stdlib.h>

/* private stuff */
#define TSF_ALIGN(size) \
    ((size + TSF_NATIVE_ALIGNMENT - 1) & (~(TSF_NATIVE_ALIGNMENT - 1)))

/* the following invariant must hold: TSF_REGION_SIZE_BITS < 16 */
#define TSF_REGION_SIZE_BITS 10
#define TSF_REGION_SIZE_UNIT (1 << TSF_REGION_SIZE_BITS)

/* only pass an aligned size */
#define TSF_SIZE_FOR_REGION(size_needed) \
    ((size_needed + TSF_REGION_SIZE_UNIT - 1) & ~(TSF_REGION_SIZE_UNIT - 1))
#define TSF_INITIAL_SIZE_FOR_REGION(size_needed) \
    TSF_SIZE_FOR_REGION(size_needed)

struct tsf_region;
struct tsf_region_cback_node;
typedef struct tsf_region tsf_region_t;
typedef struct tsf_region_cback_node tsf_region_cback_node_t;

typedef void (*tsf_region_cback_t)(void *arg);

struct tsf_region_cback_node {
    tsf_region_cback_t cback;
    void *arg;
    tsf_region_cback_node_t *next;
};

struct tsf_region {
    tsf_region_t *next;
    uint8_t *alloc;
    size_t size;

    tsf_region_cback_node_t *cback_head;

    /* wanna make this structure native aligned. */
#if (((NATIVE_ALIGNMENT -                       \
       (SIZEOF_VOID_P * 3 + SIZEOF_SIZE_T)) &   \
      (NATIVE_ALIGNMENT - 1)) != 0)
    char padding[(TSF_NATIVE_ALIGNMENT -
                  (TSF_SIZEOF_VOID_P * 3 + TSF_SIZEOF_SIZE_T))
                 & (TSF_NATIVE_ALIGNMENT - 1)];
#endif
};

static TSF_inline void *tsf_region_create(size_t size) {
    size_t region_size;
    tsf_region_t *region;
    
    size = TSF_ALIGN(size);
    region_size = TSF_INITIAL_SIZE_FOR_REGION(size);
    
    /* start a new region and allocate the root object */
    
    region = (tsf_region_t*)
        malloc(sizeof(tsf_region_t) + region_size);
    if (region == NULL) {
        return NULL;
    }
    
    region->next = region;
    region->alloc = ((uint8_t*)(region + 1)) + size;
    region->size = region_size;
    region->cback_head = NULL;
    
    return region + 1;
}

static TSF_inline void *tsf_region_alloc(void *root,
                                         size_t size) {
    tsf_region_t *region;
    tsf_region_t *next;
    void *result;

    size = TSF_ALIGN(size);
    
    region = ((tsf_region_t*)root) - 1;
    next = region->next;
    
    /* allocate new object */
    
    /* we do the comparison this way to avoid overflow! */
    if (next->alloc - ((uint8_t*)next) - sizeof(tsf_region_t) + size > next->size) {
        /* alloc new chunk */
        
        tsf_region_t *new_chunk = (tsf_region_t*)
            malloc(sizeof(tsf_region_t) + TSF_SIZE_FOR_REGION(size));
        
        if (new_chunk == NULL) {
            return NULL;
        }
        
        new_chunk->next = next;
        new_chunk->alloc = (uint8_t*)(new_chunk + 1);
        new_chunk->size = TSF_SIZE_FOR_REGION(size);
        next=region->next = new_chunk;
    }
    
    result = next->alloc;
    next->alloc += size;
    return result;
}

static TSF_inline char *tsf_region_strdup(void *root,
                                          const char *str) {
    size_t size;
    char *result;
    
    size = strlen(str) + 1;
    result = (char*)tsf_region_alloc(root, size);
    if (result == NULL) {
        return NULL;
    }
    
    memcpy(result, str, size);
    
    return result;
}

static TSF_inline void *tsf_region_realloc(void *root,
                                           void *obj,
                                           size_t size) {
    tsf_region_t *region;
    tsf_region_t *next;

    size = TSF_ALIGN(size);
    
    region = ((tsf_region_t*)root) - 1;
    next = region->next;
    
    /* resize an existing object */
    
    /* we do the comparison this way to avoid overflow! */
    if (((uint8_t*)obj) - ((uint8_t*)next) - sizeof(tsf_region_t) + size >
        next->size) {
        
        /* check if this is the only object in the chunk. */
        if (next + 1 == obj) {
            /* if so, simply realloc the chunk.  we do this to prevent
             * O(n^2) memory usage in the case of repeated reallocs. */
            
            next = (tsf_region_t*)
                realloc(next,
                        sizeof(tsf_region_t) + TSF_SIZE_FOR_REGION(size));
            
            if (next == NULL) {
                return NULL;
            }
        } else {
            /* alloc new chunk */
            
            tsf_region_t *new_chunk = (tsf_region_t*)
                malloc(sizeof(tsf_region_t) + TSF_SIZE_FOR_REGION(size));
            
            if (new_chunk == NULL) {
                return NULL;
            }
            
            new_chunk->next = next;
            new_chunk->size = TSF_SIZE_FOR_REGION(size);
            
            memcpy(new_chunk + 1, obj, next->alloc - ((uint8_t*)obj));
        
            next = new_chunk;
        }
        
        region->next = next;
        obj = next + 1;
    }
    
    next->alloc = ((uint8_t*)obj) + size;
    return obj;
}

/* allocate either in a region or using straight malloc.  will use
 * malloc if region is NULL, otherwise will call tsf_region_alloc.()
 * this cannot create a new region.  this is useful as a convenience
 * function for implementing data structures that need to be used both
 * in a region and with malloc & free. */
static TSF_inline void *tsf_cond_alloc(void *root, size_t size) {
    if (root == NULL) {
        return malloc(size);
    }
    return tsf_region_alloc(root, size);
}

/* reallocate either in a region or using straight malloc. */
static TSF_inline void *tsf_cond_realloc(void *root, void *ptr, size_t size) {
    if (root == NULL) {
        return realloc(ptr, size);
    }
    return tsf_region_realloc(root, ptr, size);
}

static TSF_inline tsf_bool_t tsf_region_add_cback(void *root,
                                                  tsf_region_cback_t cback,
                                                  void *arg) {
    tsf_region_t *region = ((tsf_region_t*)root) - 1;

    tsf_region_cback_node_t *node = (tsf_region_cback_node_t*)
        tsf_region_alloc(root, sizeof(tsf_region_cback_node_t));
    if (node == NULL) {
        return tsf_false;
    }
    
    node->next = region->cback_head;
    node->cback = cback;
    node->arg = arg;
    
    region->cback_head = node;
    
    return tsf_true;
}

/* delete a region rooted at the given object. */
static TSF_inline void tsf_region_free(void *root) {
    tsf_region_t *cur = ((tsf_region_t*)root) - 1;
    tsf_region_cback_node_t *cback_node = cur->cback_head;
    while (cback_node != NULL) {
        cback_node->cback(cback_node->arg);
        cback_node = cback_node->next;
    }
    do {
        tsf_region_t *to_free = cur;
        cur = cur->next;
        free(to_free);
    } while (cur+1 != root);
}

/* get the total allocatable space occupied by this region */
static TSF_inline size_t tsf_region_size(void *root) {
    tsf_region_t *cur = ((tsf_region_t*)root) - 1;
    size_t result = 0;
    do {
        result += cur->size;
        cur = cur->next;
    } while (cur + 1 != root);
    return result;
}

#endif


