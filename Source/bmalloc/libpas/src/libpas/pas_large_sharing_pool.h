/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_LARGE_SHARING_POOL_H
#define PAS_LARGE_SHARING_POOL_H

#include "pas_bootstrap_free_heap.h"
#include "pas_commit_mode.h"
#include "pas_config.h"
#include "pas_free_mode.h"
#include "pas_heap_summary.h"
#include "pas_large_sharing_pool_epoch_update_mode.h"
#include "pas_min_heap.h"
#include "pas_page_sharing_participant.h"
#include "pas_physical_memory_synchronization_style.h"
#include "pas_range.h"
#include "pas_red_black_tree.h"

PAS_BEGIN_EXTERN_C;

struct pas_deferred_decommit_log;
struct pas_large_sharing_node;
struct pas_physical_memory_transaction;
typedef struct pas_deferred_decommit_log pas_deferred_decommit_log;
typedef struct pas_large_sharing_node pas_large_sharing_node;
typedef struct pas_physical_memory_transaction pas_physical_memory_transaction;

struct pas_large_sharing_node {
    pas_red_black_tree_node tree_node;
    
    pas_commit_mode is_committed : 1;

    pas_physical_memory_synchronization_style synchronization_style : 1;

    unsigned index_in_min_heap : 30; /* it's a one-based index; it's zero to indicate that we're not in
                                        the min_heap. */
    
    pas_range range; /* Has to be a page range. */
    
    uint64_t use_epoch;
    
    size_t num_live_bytes; /* This could easily be compressed to a 32-bit word if we said that ranges
                              larger than page_size don't use this. */
};

static inline int
pas_large_sharing_node_heap_compare(
    pas_large_sharing_node** left_ptr,
    pas_large_sharing_node** right_ptr)
{
    pas_large_sharing_node* left;
    pas_large_sharing_node* right;
    
    left = *left_ptr;
    right = *right_ptr;
    
    if (left->use_epoch < right->use_epoch)
        return -1;
    if (left->use_epoch == right->use_epoch)
        return 0;
    return 1;
}

static inline size_t
pas_large_sharing_node_heap_get_index(
    pas_large_sharing_node** node_ptr)
{
    pas_large_sharing_node* node;
    
    node = *node_ptr;

    return node->index_in_min_heap;
}

static inline void
pas_large_sharing_node_heap_set_index(
    pas_large_sharing_node** node_ptr,
    size_t index)
{
    pas_large_sharing_node* node;
    
    node = *node_ptr;

    node->index_in_min_heap = (unsigned)index;
    PAS_ASSERT((size_t)node->index_in_min_heap == index);
}

PAS_CREATE_MIN_HEAP(
    pas_large_sharing_min_heap,
    pas_large_sharing_node*,
    4,
    .compare = pas_large_sharing_node_heap_compare,
    .get_index = pas_large_sharing_node_heap_get_index,
    .set_index = pas_large_sharing_node_heap_set_index);

/* The globals are exposed to make testing and debugging easier. */

PAS_API extern bool pas_large_sharing_pool_enabled;

/* This contains a node for every possible VA page. By default, it assumes that memory is
   allocated and committed. This has two natural implications:
   
   1) It means that we keep our dirty paws off memory we were never told about, since we
      naturally keep our dirty paws off allocated memory.
   
   2) We correctly assume that newly reserved memory is also committed. That happens to be
      true since we reserve memory in a way that causes the OS to give us memory that is
      also committed (at least by our definition of committing). */
PAS_API extern pas_red_black_tree pas_large_sharing_tree;

PAS_API extern pas_red_black_tree_jettisoned_nodes pas_large_sharing_tree_jettisoned_nodes;

/* This min_heap contains nodes the are committed and totally free. */
PAS_API extern pas_large_sharing_min_heap pas_large_sharing_min_heap_instance;

PAS_API extern pas_page_sharing_participant_payload_with_use_epoch pas_large_sharing_participant_payload;

PAS_API extern bool pas_large_sharing_pool_aggressive_asserts;
PAS_API extern bool pas_large_sharing_pool_validate_each_splat;

PAS_API extern pas_large_sharing_pool_epoch_update_mode pas_large_sharing_pool_epoch_update_mode_setting;

/* This makes the memory free and also bumps the epoch. */
PAS_API void pas_large_sharing_pool_boot_free(
    pas_range range,
    pas_physical_memory_synchronization_style synchronization_style);

PAS_API void pas_large_sharing_pool_free(
    pas_range range,
    pas_physical_memory_synchronization_style synchronization_style);

PAS_API bool pas_large_sharing_pool_allocate_and_commit(
    pas_range range,
    pas_physical_memory_transaction* transaction,
    pas_physical_memory_synchronization_style synchronization_style);

/* This doesn't actually decommit the memory. It just tells you about what memory to decommit
   using the decommit_log. It's your job to decommit everything in that log, which you can
   do by calling pas_deferred_decommit_log_decommit_all(). */
PAS_API pas_page_sharing_pool_take_result
pas_large_sharing_pool_decommit_least_recently_used(
    pas_deferred_decommit_log* decommit_log);

PAS_API void pas_large_sharing_pool_validate(void);

typedef enum {
    /* This works if using a page-aligned range. */
    pas_large_sharing_pool_compute_summary_unknown_allocation_state,

    pas_large_sharing_pool_compute_summary_known_allocated,
    pas_large_sharing_pool_compute_summary_known_free,
} pas_large_sharing_pool_compute_summary_allocation_state;

PAS_API pas_heap_summary
pas_large_sharing_pool_compute_summary(
    pas_range range,
    pas_large_sharing_pool_compute_summary_allocation_state allocation_state,
    pas_lock_hold_mode heap_lock_hold_mode);

typedef bool (*pas_large_sharing_pool_visitor)(
    pas_large_sharing_node* node,
    void* arg);

PAS_API bool pas_large_sharing_pool_for_each(
    pas_large_sharing_pool_visitor visitor,
    void* arg,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_SHARING_POOL_H */

