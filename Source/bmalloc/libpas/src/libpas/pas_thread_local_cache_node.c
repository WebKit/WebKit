/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_thread_local_cache_node.h"

#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_log.h"

pas_thread_local_cache_node* pas_thread_local_cache_node_first_free;
pas_thread_local_cache_node* pas_thread_local_cache_node_first;

static unsigned num_nodes;

pas_thread_local_cache_node* pas_thread_local_cache_node_allocate(void)
{
    static const bool verbose = false;
    
    pas_thread_local_cache_node* result;
    
    pas_heap_lock_assert_held();

    result = pas_thread_local_cache_node_first_free;
    if (!result) {
        const size_t interference_padding = 64;
        
        size_t size;

        size = PAS_MAX(
            interference_padding,
            pas_round_up_to_next_power_of_2(sizeof(pas_thread_local_cache_node)));

        if (verbose)
            pas_log("Allocating TLC node with size/alignment = %zu.\n", size);
        
        result = pas_immortal_heap_allocate_with_alignment(
            size, size,
            "pas_thread_local_cache_node",
            pas_object_allocation);
        
        result->next_free = NULL;
        
        result->next = pas_thread_local_cache_node_first;

        pas_lock_construct(&result->page_lock);
        pas_lock_construct(&result->scavenger_lock);

        if (verbose)
            pas_log("TLC page lock: %p\n", &result->page_lock);
        
        result->cache = NULL; /* This is set by the caller. */

        pas_fence(); /* This means we can iterate the list of caches without holding locks. */
        
        pas_thread_local_cache_node_first = result;

        if (verbose)
            pas_log("Allocated new node %p (num live = %u)\n", result, ++num_nodes);
        return result;
    }
    
    pas_thread_local_cache_node_first_free = result->next_free;
    result->next_free = NULL;
    result->cache = NULL;
    if (verbose)
        pas_log("Allocated existing node %p (num live = %u)\n", result, ++num_nodes);
    
    return result;
}

void pas_thread_local_cache_node_deallocate(pas_thread_local_cache_node* node)
{
    static const bool verbose = false;
    
    PAS_ASSERT(!node->next_free);
    pas_heap_lock_assert_held();

    if (verbose)
        pas_log("Deallocating node %p (num live = %u)\n", node, --num_nodes);
    
    node->cache = NULL;
    pas_compiler_fence();
    
    node->next_free = pas_thread_local_cache_node_first_free;
    pas_thread_local_cache_node_first_free = node;
}

#endif /* LIBPAS_ENABLED */
