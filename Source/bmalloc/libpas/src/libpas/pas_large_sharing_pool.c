/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#include "pas_large_sharing_pool.h"

#include "pas_bootstrap_free_heap.h"
#include "pas_debug_spectrum.h"
#include "pas_deferred_decommit_log.h"
#include "pas_epoch.h"
#include "pas_heap_lock.h"
#include "pas_large_free_heap_deferred_commit_log.h"
#include "pas_log.h"
#include "pas_page_malloc.h"
#include "pas_page_sharing_pool.h"
#include "pas_physical_memory_transaction.h"
#include "pas_stream.h"
#include "pas_utility_heap.h"

bool pas_large_sharing_pool_enabled = true;
pas_red_black_tree pas_large_sharing_tree = PAS_RED_BLACK_TREE_INITIALIZER;
pas_red_black_tree_jettisoned_nodes pas_large_sharing_tree_jettisoned_nodes;
pas_large_sharing_min_heap pas_large_sharing_min_heap_instance =
    PAS_MIN_HEAP_INITIALIZER(pas_large_sharing_min_heap);
pas_page_sharing_participant_payload_with_use_epoch pas_large_sharing_participant_payload;
bool pas_large_sharing_pool_aggressive_asserts = false;
bool pas_large_sharing_pool_validate_each_splat = false;
pas_large_sharing_pool_epoch_update_mode pas_large_sharing_pool_epoch_update_mode_setting = pas_large_sharing_pool_forward_min_epoch;

static const bool verbose = false;

static int node_compare_callback(pas_red_black_tree_node* left_node,
                                 pas_red_black_tree_node* right_node)
{
    pas_large_sharing_node* left;
    pas_large_sharing_node* right;
    
    left = (pas_large_sharing_node*)left_node;
    right = (pas_large_sharing_node*)right_node;
    
    /* NOTE: it's tempting to assert that the ranges don't overlap - but in the middle of
       mutating the tree, we may temporarily create nodes that overlap and then fix them up. */
    
    if (left->range.begin < right->range.begin)
        return -1;
    if (left->range.begin == right->range.begin)
        return 0;
    return 1;
}

static int inner_key_compare_callback(pas_red_black_tree_node* left_node,
                                      void* raw_right)
{
    pas_large_sharing_node* left;
    uintptr_t right_inner;
    
    left = (pas_large_sharing_node*)left_node;
    right_inner = (uintptr_t)raw_right;

    if (pas_range_contains(left->range, right_inner))
        return 0;
    if (left->range.begin < right_inner)
        return -1;
    return 1;
}

static void update_min_epoch(void)
{
    pas_large_sharing_node* min_node;
    
    min_node = pas_large_sharing_min_heap_get_min(
        &pas_large_sharing_min_heap_instance);

    switch (pas_large_sharing_pool_epoch_update_mode_setting) {
    case pas_large_sharing_pool_forward_min_epoch:
        if (min_node)
            pas_large_sharing_participant_payload.use_epoch = min_node->use_epoch;
        break;
    case pas_large_sharing_pool_combined_use_epoch:
        break;
    }
}

static void validate_min_heap(void)
{
    size_t index;
    
    if (verbose)
        pas_log("min_heap:");
    
    for (index = 1; index <= pas_large_sharing_min_heap_instance.size; ++index) {
        pas_large_sharing_node* node;

        node = *pas_large_sharing_min_heap_get_ptr_by_index(
            &pas_large_sharing_min_heap_instance, index);
        
        if (verbose) {
            pas_log(" %d:%p:%lu-%lu:%llu",
                    node->index_in_min_heap,
                    node, node->range.begin, node->range.end,
                    (unsigned long long)node->use_epoch);
        }
    }
    
    if (verbose)
        pas_log("\n");

    for (index = 1; index <= pas_large_sharing_min_heap_instance.size; ++index) {
        pas_large_sharing_node* node;

        node = *pas_large_sharing_min_heap_get_ptr_by_index(
            &pas_large_sharing_min_heap_instance, index);
        
        PAS_ASSERT(node->index_in_min_heap == index);
        PAS_ASSERT(pas_large_sharing_min_heap_is_index_still_ordered(
                       &pas_large_sharing_min_heap_instance, index, node));
    }
}

static void validate_node(pas_large_sharing_node* node)
{
    size_t page_size;

    page_size = pas_page_malloc_alignment();

    PAS_ASSERT(node->range.end > node->range.begin);
    PAS_ASSERT(node->num_live_bytes <= pas_range_size(node->range));
    PAS_ASSERT(pas_is_aligned(node->range.begin, page_size));
    PAS_ASSERT(pas_is_aligned(node->range.end, page_size));
    PAS_ASSERT(!node->num_live_bytes
               || pas_range_size(node->range) == pas_page_malloc_alignment()
               || node->num_live_bytes == pas_range_size(node->range));
}

static void validate_node_if_asserting_aggressively(pas_large_sharing_node* node)
{
    if (pas_large_sharing_pool_aggressive_asserts)
        validate_node(node);
}

static pas_large_sharing_node* create_node(
    pas_range range,
    uint64_t use_epoch,
    pas_commit_mode is_committed,
    size_t num_live_bytes,
    pas_physical_memory_synchronization_style synchronization_style,
    pas_mmap_capability mmap_capability)
{
    pas_large_sharing_node* result;
    
    result = pas_utility_heap_allocate(
        sizeof(pas_large_sharing_node),
        "pas_large_sharing_node");
    
    /* NOTE: We don't have to initialize the tree node part. But we zero the whole node to
       make things sane. */
    pas_zero_memory(result, sizeof(pas_large_sharing_node));
    
    result->range = range;
    result->use_epoch = use_epoch;
    result->index_in_min_heap = 0;
    result->is_committed = is_committed;
    result->num_live_bytes = num_live_bytes;
    result->synchronization_style = synchronization_style;
    result->mmap_capability = mmap_capability;

    validate_node_if_asserting_aggressively(result);
    
    return result;
}

static pas_large_sharing_node* create_and_insert(
    pas_range range,
    uint64_t use_epoch,
    pas_commit_mode is_committed,
    size_t num_live_bytes,
    pas_physical_memory_synchronization_style synchronization_style,
    pas_mmap_capability mmap_capability)
{
    pas_large_sharing_node* result;
    
    result = create_node(range, use_epoch, is_committed, num_live_bytes, synchronization_style,
                         mmap_capability);
    
    pas_red_black_tree_insert(
        &pas_large_sharing_tree, &result->tree_node, node_compare_callback,
        &pas_large_sharing_tree_jettisoned_nodes);
    
    return result;
}

static void boot_tree(void)
{
    pas_range range;
    
    pas_heap_lock_assert_held();
    
    if (!pas_red_black_tree_is_empty(&pas_large_sharing_tree))
        return;
    
    pas_page_sharing_participant_payload_with_use_epoch_construct(
        &pas_large_sharing_participant_payload);
    
    pas_page_sharing_pool_add(
        &pas_physical_page_sharing_pool,
        pas_page_sharing_participant_create(
            NULL, pas_page_sharing_participant_large_sharing_pool));
    
    /* Create the One True Node for all of VA.
     
       ROFL: This is totally borked on 32-bit. */
    range = pas_range_create(0, (uintptr_t)1 << PAS_ADDRESS_BITS);
    
    create_and_insert(
        range,
        0,
        pas_committed,
        pas_range_size(range),
        pas_physical_memory_is_locked_by_virtual_range_common_lock,
        pas_may_mmap);
}

static void destroy_node(pas_large_sharing_node* node)
{
    pas_utility_heap_deallocate(node);
}

static void remove_from_min_heap(pas_large_sharing_node* node)
{
    if (!node->index_in_min_heap)
        return;
    
    if (verbose)
        pas_log("removing %p from min_heap.\n", node);
    
    pas_large_sharing_min_heap_remove(
        &pas_large_sharing_min_heap_instance, node);
    update_min_epoch();

    if (pas_large_sharing_pool_validate_each_splat)
        validate_min_heap();
}

static void remove_and_destroy(pas_large_sharing_node* node)
{
    remove_from_min_heap(node);
    pas_red_black_tree_remove(&pas_large_sharing_tree, &node->tree_node,
                              &pas_large_sharing_tree_jettisoned_nodes);
    destroy_node(node);
}

static pas_large_sharing_node*
predecessor(pas_large_sharing_node* node)
{
    return (pas_large_sharing_node*)pas_red_black_tree_node_predecessor(&node->tree_node);
}

static pas_large_sharing_node*
successor(pas_large_sharing_node* node)
{
    return (pas_large_sharing_node*)pas_red_black_tree_node_successor(&node->tree_node);
}

static bool states_match(pas_large_sharing_node* left,
                         pas_large_sharing_node* right)
{
    bool both_empty;
    bool both_full;
    
    if (left->is_committed != right->is_committed)
        return false;

    if (left->synchronization_style != right->synchronization_style)
        return false;

    if (left->mmap_capability != right->mmap_capability)
        return false;

    both_empty =
        !left->num_live_bytes &&
        !right->num_live_bytes;
    
    both_full =
        pas_range_size(left->range) == left->num_live_bytes &&
        pas_range_size(right->range) == right->num_live_bytes;

    if (!both_empty && !both_full)
        return false;

    /* Right now: both sides have identical commit states and identical synchronization styes. And
       either both sides are empty or both sides are full.
    
       The only reason why we wouldn't want to coalesce is if epochs didn't match. But that only
       matters when the memory is free and committed. */

    if (!left->is_committed)
        return true;

    if (both_full)
        return true;

    return left->use_epoch == right->use_epoch;
}

static bool is_eligible(pas_large_sharing_node* node)
{
    return node->is_committed && !node->num_live_bytes;
}

static bool belongs_in_min_heap(pas_large_sharing_node* node)
{
    return is_eligible(node);
}

static void update_min_heap(pas_large_sharing_node* node)
{
    pas_allocation_config allocation_config;
    
    /* NOTE: We could assert that the node was already in the min_heap. The upside is that this
       tolerates the min_heap being totally messed up while you're in the middle of a splat. It's
       all fine so long as you call update_min_heap on all affected nodes once you're done. */
    
    if (verbose)
        pas_log("updating node in min_heap = %p:%lu-%lu\n", node, node->range.begin, node->range.end);
    
    PAS_ASSERT(!node->index_in_min_heap);
    
    if (!belongs_in_min_heap(node))
        return;
    
    if (verbose)
        pas_log("belongs!\n");
    
    pas_bootstrap_free_heap_allocation_config_construct(&allocation_config, pas_lock_is_held);
    
    pas_large_sharing_min_heap_add(
        &pas_large_sharing_min_heap_instance, node, &allocation_config);
    
    update_min_epoch();
    
    if (pas_large_sharing_pool_validate_each_splat)
        validate_min_heap();
}

static pas_large_sharing_node*
split_node_and_get_right_impl(pas_large_sharing_node* node,
                              uintptr_t split_point)
{
    pas_large_sharing_node* right_node;

    PAS_ASSERT(pas_is_aligned(split_point, pas_page_malloc_alignment()));

    validate_node_if_asserting_aggressively(node);
    
    if (verbose)
        pas_log("Splitting %p:%lu-%lu at %lu\n", node, node->range.begin, node->range.end, split_point);

    remove_from_min_heap(node);
    
    right_node = create_and_insert(
        pas_range_create(split_point, node->range.end),
        node->use_epoch,
        node->is_committed,
        node->num_live_bytes ? node->range.end - split_point : 0,
        node->synchronization_style,
        node->mmap_capability);
    
    node->range.end = split_point;
    node->num_live_bytes = node->num_live_bytes ? split_point - node->range.begin : 0;
    
    update_min_heap(node);
    update_min_heap(right_node);

    validate_node_if_asserting_aggressively(node);
    validate_node_if_asserting_aggressively(right_node);
    
    if (verbose) {
        pas_log("Now have %p:%lu-%lu and %p:%lu-%lu\n",
                node, node->range.begin, node->range.end,
                right_node, right_node->range.begin, right_node->range.end);
    }

    return right_node;
}

static pas_large_sharing_node*
split_node_and_get_right(pas_large_sharing_node* node,
                         uintptr_t split_point)
{
    if (split_point <= node->range.begin) {
        PAS_ASSERT(split_point == node->range.begin);
        return node;
    }
    if (split_point >= node->range.end) {
        PAS_ASSERT(split_point == node->range.end);
        return successor(node);
    }

    return split_node_and_get_right_impl(node, split_point);
}

static pas_large_sharing_node*
split_node_and_get_left(pas_large_sharing_node* node,
                        uintptr_t split_point)
{
    if (split_point <= node->range.begin) {
        PAS_ASSERT(split_point == node->range.begin);
        return predecessor(node);
    }
    if (split_point >= node->range.end) {
        PAS_ASSERT(split_point == node->range.end);
        return node;
    }

    split_node_and_get_right_impl(node, split_point);
    return node;
}

/* Must be careful about merging with right!
 
   If we merge with right, we must make sure that the right node is the one that survives
   and subsumes us. */
static pas_large_sharing_node*
merge_if_possible(pas_large_sharing_node* node)
{
    pas_large_sharing_node* left;
    pas_large_sharing_node* right;
    pas_large_sharing_node* result;
    
    left = predecessor(node);
    right = successor(node);
    result = right;

    if (left) {
        PAS_ASSERT(left->range.begin < left->range.end);
        PAS_ASSERT(left->range.end == node->range.begin);
    }
    
    if (right) {
        PAS_ASSERT(right->range.begin < right->range.end);
        PAS_ASSERT(right->range.begin == node->range.end);
    }
    
    /* When merging node with left, we kill left.
       
       When merging node with right, we kill node.
    
       Our caller expects this! */
    
    if (left && states_match(node, left)) {
        remove_from_min_heap(left);
        remove_from_min_heap(node);
        if (left->num_live_bytes) {
            PAS_ASSERT(pas_range_size(left->range) == left->num_live_bytes);
            PAS_ASSERT(pas_range_size(node->range) == node->num_live_bytes);
            node->num_live_bytes += left->num_live_bytes;
        }
        node->range.begin = left->range.begin;
        node->use_epoch = PAS_MAX(node->use_epoch, left->use_epoch);
        remove_and_destroy(left);
        update_min_heap(node);
        validate_node_if_asserting_aggressively(node);
    }
    
    if (right && states_match(node, right)) {
        remove_from_min_heap(node);
        remove_from_min_heap(right);
        if (right->num_live_bytes) {
            PAS_ASSERT(pas_range_size(right->range) == right->num_live_bytes);
            PAS_ASSERT(pas_range_size(node->range) == node->num_live_bytes);
            right->num_live_bytes += node->num_live_bytes;
        }
        right->range.begin = node->range.begin;
        right->use_epoch = PAS_MAX(node->use_epoch, right->use_epoch);
        remove_and_destroy(node);
        update_min_heap(right);
        validate_node_if_asserting_aggressively(right);
    }
    
    return result;
}

static pas_large_sharing_node* node_containing(uintptr_t address)
{
    return (pas_large_sharing_node*)
        pas_red_black_tree_find_exact(
            &pas_large_sharing_tree,
            (void*)address,
            inner_key_compare_callback);
}

static pas_large_sharing_node* min_node_for_range(pas_range range)
{
    return node_containing(range.begin);
}

static pas_large_sharing_node* max_node_for_range(pas_range range)
{
    return node_containing(range.end - 1);
}

static void splat_live_bytes(pas_large_sharing_node* node,
                             size_t delta,
                             pas_free_mode free_mode)
{
    bool did_overflow;
    switch (free_mode) {
    case pas_free:
        did_overflow = __builtin_usubl_overflow(node->num_live_bytes, delta, &node->num_live_bytes);
        PAS_ASSERT(!did_overflow);
        break;
    case pas_allocated:
        did_overflow = __builtin_uaddl_overflow(node->num_live_bytes, delta, &node->num_live_bytes);
        PAS_ASSERT(!did_overflow);
        PAS_ASSERT(node->num_live_bytes <= pas_range_size(node->range));
        break;
    }
    validate_node_if_asserting_aggressively(node);
}

static bool should_do_commit_stuff_to(pas_large_sharing_node* node,
                                      pas_range range,
                                      pas_commit_mode desired_commit_mode)
{
    return node->is_committed != desired_commit_mode
        && (desired_commit_mode == pas_committed
            || pas_range_subsumes(range, node->range));
}

enum splat_command {
    splat_decommit,
    splat_allocate_and_commit,
    splat_free,
    splat_boot_free
};

typedef enum splat_command splat_command;

static const char* splat_command_get_string(splat_command command)
{
    switch (command) {
    case splat_decommit:
        return "decommit";
    case splat_allocate_and_commit:
        return "allocate_and_commit";
    case splat_free:
        return "free";
    case splat_boot_free:
        return "boot_free";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static pas_free_mode splat_command_get_free_mode(splat_command command)
{
    switch (command) {
    case splat_decommit:
        PAS_ASSERT(!"Invalid command");
        return pas_free;
    case splat_allocate_and_commit:
        return pas_allocated;
    case splat_free:
    case splat_boot_free:
        return pas_free;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_free;
}

static void dump_large_commit(pas_stream* stream, void* arg)
{
    PAS_UNUSED_PARAM(arg);
    pas_stream_printf(stream, "large synchronous");
}

/* Must pass non-NULL commit_log or decommit_log if you want to commit or decommit.
 
   Splat works differently for committed_bit and free_bit. For committed_bit, it's totally valid to
   commit committed memory or decommit decommitted memory. Nothing will change if you do that; that
   part of the splat will have no effect. For free_bit, it's only valid to free allocated memory and
   allocate freed memory. There are assertions that will catch obvious cases of this being wrong,
   but the algorithm can't quite all detect all cases of this, so you just have to get it right. */
static bool try_splat_impl(pas_range range,
                           splat_command command,
                           uint64_t epoch,
                           pas_large_free_heap_deferred_commit_log* commit_log,
                           pas_deferred_decommit_log* decommit_log,
                           pas_physical_memory_transaction* transaction,
                           pas_physical_memory_synchronization_style synchronization_style,
                           pas_mmap_capability mmap_capability)
{
    pas_large_sharing_node* min_node;
    pas_large_sharing_node* max_node;
    pas_large_sharing_node* begin_node;
    pas_large_sharing_node* end_node;
    pas_large_sharing_node* node;
    bool do_commit_stuff;
    size_t page_size;
    bool was_eligible;
    uint64_t old_epoch;
    pas_commit_mode desired_commit_mode;
    
    if (verbose) {
        pas_log("Doing splat in range %p-%p, command = %s, epoch = %llu, "
                "synchronization_style = %s, mmap_capability = %s\n",
                (void*)range.begin, (void*)range.end, splat_command_get_string(command), (unsigned long long)epoch,
                pas_physical_memory_synchronization_style_get_string(synchronization_style),
                pas_mmap_capability_get_string(mmap_capability));
    }
    
    pas_heap_lock_assert_held();
    
    PAS_ASSERT(range.end >= range.begin);
    if (pas_range_is_empty(range))
        return true; /* We return true because we in fact did succeed at doing nothing. */
    
    boot_tree();
    
    page_size = pas_page_malloc_alignment();
    
    old_epoch = pas_large_sharing_participant_payload.use_epoch;
    
    was_eligible = !!pas_large_sharing_min_heap_instance.size;

    min_node = min_node_for_range(range);
    
    PAS_ASSERT(min_node);

    if ((command == splat_free
         || (command == splat_allocate_and_commit
             && min_node->is_committed))
        && pas_range_size(range) < page_size
        && pas_range_size(min_node->range) == page_size) {
        /* This is a fast path to make this algorithm not suck too badly for small allocations. We may
           use this for small allocations when the heap hasn't yet tiered up to using the segregated
           heap. Note that this leaves nodes uncoalesced in the hope that future small allocations and
           deallocations that involve these nodes can go fast too. */

        if (min_node->range.end >= range.end) {
            /* We hope that this is the most common case. */
            remove_from_min_heap(min_node);
            min_node->use_epoch = PAS_MAX(min_node->use_epoch, epoch);
            splat_live_bytes(min_node, pas_range_size(range), splat_command_get_free_mode(command));
            update_min_heap(min_node);
            goto done;
        }

        max_node = successor(min_node);
        if (pas_range_size(max_node->range) == page_size
            && (command == splat_free || command == splat_boot_free || max_node->is_committed)) {
            PAS_ASSERT(max_node->range.begin < range.end);
            PAS_ASSERT(max_node->range.end >= range.end);
            remove_from_min_heap(min_node);
            remove_from_min_heap(max_node);
            min_node->use_epoch = PAS_MAX(min_node->use_epoch, epoch);
            max_node->use_epoch = PAS_MAX(max_node->use_epoch, epoch);
            splat_live_bytes(
                min_node,
                pas_range_size(pas_range_create_intersection(min_node->range, range)),
                splat_command_get_free_mode(command));
            splat_live_bytes(
                max_node,
                pas_range_size(pas_range_create_intersection(max_node->range, range)),
                splat_command_get_free_mode(command));
            update_min_heap(min_node);
            update_min_heap(max_node);
            goto done;
        }
    }
    
    /* We need to do some splitting:
           
       - For sure, we need a split at the page boundaries that most tightly cover our range.
           
       - Then if the range is inside pages, we need to split those pages off. */

    if (min_node->range.begin < range.begin) {
        if (pas_is_aligned(range.begin, page_size))
            min_node = split_node_and_get_right(min_node, range.begin);
        else {
            min_node = split_node_and_get_left(
                split_node_and_get_right(
                    min_node, pas_round_down_to_power_of_2(range.begin, page_size)),
                pas_round_up_to_power_of_2(range.begin, page_size));
        }
    }

    /* At this point, max_node may actually point to the left of the real max_node, if both
       min and max started out the same. Doing it in this order makes it magically work. */
    
    max_node = max_node_for_range(range);

    if (max_node->range.end > range.end) {
        if (pas_is_aligned(range.end, page_size))
            max_node = split_node_and_get_left(max_node, range.end);
        else {
            max_node = split_node_and_get_right(
                split_node_and_get_left(
                    max_node, pas_round_up_to_power_of_2(range.end, page_size)),
                pas_round_down_to_power_of_2(range.end, page_size));
        }
    }
    
    if (verbose)
        pas_log("min_node = %p:%lu-%lu, max_node = %p:%lu-%lu\n", min_node, min_node->range.begin, min_node->range.end, max_node, max_node->range.begin, max_node->range.end);
    
    begin_node = min_node;
    end_node = successor(max_node);
    
    if (pas_large_sharing_pool_validate_each_splat)
        validate_min_heap();

    desired_commit_mode = pas_committed; /* Make the compiler happy. */
    do_commit_stuff = false;
    
    switch (command) {
    case splat_decommit:
        desired_commit_mode = pas_decommitted;
        do_commit_stuff = true;
        break;
        
    case splat_allocate_and_commit:
        desired_commit_mode = pas_committed;
        do_commit_stuff = true;
        break;
        
    case splat_free:
    case splat_boot_free:
        break;
    }

    if (do_commit_stuff) {
        bool have_already_added_some_range;
        
        have_already_added_some_range = false;
        
        for (node = begin_node; node != end_node;) {
            pas_large_sharing_node* inner_node;
            uintptr_t affected_begin;
            uintptr_t affected_end;
            
            if (!should_do_commit_stuff_to(node, range, desired_commit_mode)) {
                node = successor(node);
                continue;
            }
            
            affected_begin = node->range.begin;
            affected_end = affected_begin; /* Give the compiler a chill pill. */
            
            PAS_ASSERT(pas_is_aligned(affected_begin, page_size));
            
            for (inner_node = node;
                 inner_node != end_node && should_do_commit_stuff_to(inner_node, range,
                                                                     desired_commit_mode);
                 inner_node = successor(inner_node)) {
                PAS_ASSERT(inner_node->synchronization_style == synchronization_style);
                PAS_ASSERT(inner_node->mmap_capability == mmap_capability);
                affected_end = inner_node->range.end;
            }
            
            PAS_ASSERT(inner_node != node);
            PAS_ASSERT(!inner_node || inner_node->range.begin == affected_end);
            node = inner_node;
            
            PAS_ASSERT(pas_is_aligned(affected_end, page_size));
            PAS_ASSERT(affected_end > affected_begin);

            switch (synchronization_style) {
            case pas_physical_memory_is_locked_by_heap_lock: {
                switch (desired_commit_mode) {
                case pas_decommitted:
                    pas_page_malloc_decommit((void*)affected_begin, affected_end - affected_begin,
                                             mmap_capability);
                    decommit_log->total += affected_end - affected_begin;
                    break;
                    
                case pas_committed:
                    pas_page_malloc_commit((void*)affected_begin, affected_end - affected_begin,
                                           mmap_capability);
                    if (PAS_DEBUG_SPECTRUM_USE_FOR_COMMIT) {
                        pas_debug_spectrum_add(
                            dump_large_commit, dump_large_commit, affected_end - affected_begin);
                    }
                    pas_physical_page_sharing_pool_take_later(affected_end - affected_begin);
                    break;
                }
                break;
            }

            case pas_physical_memory_is_locked_by_virtual_range_common_lock: {
                bool was_added;
                
                switch (desired_commit_mode) {
                case pas_decommitted:
                    was_added = pas_deferred_decommit_log_add(
                        decommit_log,
                        pas_virtual_range_create(affected_begin, affected_end,
                                                 &pas_virtual_range_common_lock,
                                                 mmap_capability),
                        pas_lock_is_held);
                    break;
                    
                case pas_committed:
                    was_added = pas_large_free_heap_deferred_commit_log_add(
                        commit_log,
                        pas_large_virtual_range_create(affected_begin, affected_end, mmap_capability),
                        transaction);
                    break;
                }
                
                if (!was_added) {
                    /* This _has_ to have been the first range we tried. */
                    PAS_ASSERT(!have_already_added_some_range);
                    
                    /* So the only thing we have to undo is the splitting. */
                    merge_if_possible(min_node);
                    if (max_node != min_node)
                        merge_if_possible(max_node);
                    return false;
                }
                break;
            } }
            
            have_already_added_some_range = true;
        }
    }
    
    /* Now change the nodes' states. This may cause us to coalesce. */
    
    if (pas_large_sharing_pool_validate_each_splat) {
        for (node = begin_node; node != end_node;) {
            pas_large_sharing_node* next_node;
            
            if (verbose)
                pas_log("Will see node = %p\n", node);
            
            next_node = successor(node);
            
            PAS_ASSERT(!next_node || next_node->range.begin > node->range.begin);
            PAS_ASSERT(!next_node || next_node->range.begin == node->range.end);
            PAS_ASSERT(node->range.begin >= pas_round_down_to_power_of_2(range.begin, page_size));
            PAS_ASSERT(node->range.end <= pas_round_up_to_power_of_2(range.end, page_size));
            
            node = next_node;
        }
    }
    
    if (verbose)
        pas_log("Setting states.\n");
    
    for (node = begin_node; node != end_node; node = successor(node)) {
        if (verbose)
            pas_log("Dealing with node = %p:%lu-%lu\n", node, node->range.begin, node->range.end);
        
        remove_from_min_heap(node);

        node->use_epoch = PAS_MAX(node->use_epoch, epoch);
        
        switch (command) {
        case splat_decommit:
            if (pas_range_subsumes(range, node->range))
                node->is_committed = pas_decommitted;
            break;

        case splat_allocate_and_commit:
            node->is_committed = pas_committed;
            splat_live_bytes(
                node,
                pas_range_size(pas_range_create_intersection(node->range, range)),
                pas_allocated);
            break;

        case splat_free:
        case splat_boot_free:
            splat_live_bytes(
                node,
                pas_range_size(pas_range_create_intersection(node->range, range)),
                pas_free);
            break;
        }

        switch (command) {
        case splat_decommit:
        case splat_allocate_and_commit:
        case splat_free:
            PAS_ASSERT(node->synchronization_style == synchronization_style);
            PAS_ASSERT(node->mmap_capability == mmap_capability);
            break;

        case splat_boot_free:
            PAS_ASSERT(
                node->synchronization_style
                == pas_physical_memory_is_locked_by_virtual_range_common_lock);
            PAS_ASSERT(
                node->mmap_capability
                == pas_may_mmap);
            node->synchronization_style = synchronization_style;
            node->mmap_capability = mmap_capability;
            break;
        }

        update_min_heap(node); /* merge_if_possible will call update_min_heap only if it changes
                                  a node. But we already changed this node, so we must call
                                  this. Then merge_if_possible may make more changes in which
                                  case it will update_min_heap a second time. */
    }

    for (node = begin_node; node && node->range.begin < range.end; node = merge_if_possible(node));

done:
    if (pas_large_sharing_pool_validate_each_splat) {
        begin_node = min_node_for_range(range);
        end_node = successor(max_node_for_range(range));
        
        PAS_ASSERT(begin_node);
        
        for (node = begin_node; node != end_node; node = successor(node)) {
            validate_node(node);
            switch (command) {
            case splat_decommit:
                PAS_ASSERT(!pas_range_subsumes(range, node->range)
                           || !node->is_committed);
                break;
            case splat_allocate_and_commit:
                PAS_ASSERT(node->is_committed);
                PAS_ASSERT(
                    node->num_live_bytes
                    >= pas_range_size(pas_range_create_intersection(node->range, range)));
                break;
            case splat_free:
            case splat_boot_free:
                PAS_ASSERT(
                    pas_range_size(node->range) - node->num_live_bytes
                    >= pas_range_size(pas_range_create_intersection(node->range, range)));
                break;
            }
        }
    }

    switch (pas_large_sharing_pool_epoch_update_mode_setting) {
    case pas_large_sharing_pool_forward_min_epoch:
        break;
    case pas_large_sharing_pool_combined_use_epoch:
        pas_large_sharing_participant_payload.use_epoch =
            PAS_MAX(old_epoch, epoch);
        break;
    }

    /* Finally check if our eligibility has changed. */
    if (old_epoch != pas_large_sharing_participant_payload.use_epoch
        || was_eligible != !!pas_large_sharing_min_heap_instance.size) {
        pas_page_sharing_pool_did_create_delta(
            &pas_physical_page_sharing_pool,
            pas_page_sharing_participant_create(
                NULL, pas_page_sharing_participant_large_sharing_pool));
    }
    
    return true;
}

static bool try_splat(pas_range range,
                      splat_command command,
                      uint64_t epoch,
                      pas_large_free_heap_deferred_commit_log* commit_log,
                      pas_deferred_decommit_log* decommit_log,
                      pas_physical_memory_transaction* transaction,
                      pas_physical_memory_synchronization_style synchronization_style,
                      pas_mmap_capability mmap_capability)
{
    bool result;
    
    result = try_splat_impl(
        range, command, epoch, commit_log, decommit_log, transaction, synchronization_style, mmap_capability);
    
    if (pas_large_sharing_pool_validate_each_splat)
        pas_large_sharing_pool_validate();
    
    return result;
}

static void splat(pas_range range,
                  splat_command command,
                  uint64_t epoch,
                  pas_large_free_heap_deferred_commit_log* commit_log,
                  pas_deferred_decommit_log* decommit_log,
                  pas_physical_memory_transaction* transaction,
                  pas_physical_memory_synchronization_style synchronization_style,
                  pas_mmap_capability mmap_capability)
{
    bool result;
    
    result = try_splat(
        range, command, epoch, commit_log, decommit_log, transaction, synchronization_style, mmap_capability);
    
    PAS_ASSERT(result);
}

void pas_large_sharing_pool_boot_free(
    pas_range range,
    pas_physical_memory_synchronization_style synchronization_style,
    pas_mmap_capability mmap_capability)
{
    uint64_t epoch;

    if (!pas_large_sharing_pool_enabled)
        return;
    
    epoch = pas_get_epoch();
    splat(range, splat_boot_free, epoch, NULL, NULL, NULL, synchronization_style, mmap_capability);
}

void pas_large_sharing_pool_free(pas_range range,
                                 pas_physical_memory_synchronization_style synchronization_style,
                                 pas_mmap_capability mmap_capability)
{
    uint64_t epoch;

    if (!pas_large_sharing_pool_enabled)
        return;
    
    epoch = pas_get_epoch();
    
    splat(range, splat_free, epoch, NULL, NULL, NULL, synchronization_style, mmap_capability);
}

bool pas_large_sharing_pool_allocate_and_commit(
    pas_range range,
    pas_physical_memory_transaction* transaction,
    pas_physical_memory_synchronization_style synchronization_style,
    pas_mmap_capability mmap_capability)
{
    static const bool verbose = false;
    
    pas_large_free_heap_deferred_commit_log commit_log;
    uint64_t epoch;
    
    if (verbose) {
        pas_log("Doing allocate and commit %p-%p.\n", (void*)range.begin, (void*)range.end);
        pas_log("Balance = %ld.\n", pas_physical_page_sharing_pool_balance);
    }
    
    if (!pas_large_sharing_pool_enabled) {
        if (verbose)
            pas_log("Giving up on allocate and commit because it's disabled.\n");
        return true;
    }
    
    epoch = pas_get_epoch();
    
    pas_large_free_heap_deferred_commit_log_construct(&commit_log);

    if (!try_splat(range, splat_allocate_and_commit, epoch,
                   &commit_log, NULL, transaction, synchronization_style, mmap_capability)) {
        pas_large_free_heap_deferred_commit_log_destruct(&commit_log);
        if (verbose)
            pas_log("Giving up on allocate and commit because the splat failed.\n");
        return false;
    }

    switch (synchronization_style) {
    case pas_physical_memory_is_locked_by_heap_lock:
        PAS_ASSERT(!commit_log.total);
        break;

    case pas_physical_memory_is_locked_by_virtual_range_common_lock:
        if (commit_log.total || pas_physical_page_sharing_pool_balance < 0) {
            enum { max_num_locks_held = 2 };
            
            pas_lock* locks_held[max_num_locks_held];
            size_t num_locks_held;
            
            if (verbose)
                pas_log("Allocate and commit needs to commit %zu bytes.\n", commit_log.total);
            
            num_locks_held = 0;
            if (transaction->lock_held)
                locks_held[num_locks_held++] = transaction->lock_held;
            if (commit_log.total && transaction->lock_held != &pas_virtual_range_common_lock)
                locks_held[num_locks_held++] = &pas_virtual_range_common_lock;
            
            PAS_ASSERT(num_locks_held <= max_num_locks_held);
            
            if (verbose)
                pas_log("Doing a take of %lu bytes.\n", commit_log.total);
            pas_physical_page_sharing_pool_take(
                commit_log.total, pas_lock_is_held, locks_held, num_locks_held);
            
            pas_large_free_heap_deferred_commit_log_commit_all(&commit_log, transaction);
        }
        break;
    }
    
    pas_large_free_heap_deferred_commit_log_destruct(&commit_log);
    
    if (verbose)
        pas_log("Done with allocate and commit %p-%p.\n", (void*)range.begin, (void*)range.end);

    return true;
}

pas_page_sharing_pool_take_result
pas_large_sharing_pool_decommit_least_recently_used(
    pas_deferred_decommit_log* decommit_log)
{
    pas_large_sharing_node* node;
    
    if (!pas_large_sharing_pool_enabled)
        return pas_page_sharing_pool_take_none_available;
    
    node = pas_large_sharing_min_heap_get_min(
        &pas_large_sharing_min_heap_instance);
    if (!node)
        return pas_page_sharing_pool_take_none_available;

    /* This has to actually take some memory. */
    PAS_ASSERT(!node->num_live_bytes);
    PAS_ASSERT(node->is_committed);

    validate_node(node);
    
    if (verbose) {
        pas_log("Going to decommit %lu to %lu with epoch %llu\n",
               node->range.begin, node->range.end, (unsigned long long)node->use_epoch);
    }
    
    if (try_splat(node->range, splat_decommit, 0, NULL, decommit_log, NULL,
                  node->synchronization_style, node->mmap_capability)) {
        if (verbose)
            pas_log("The splat worked.\n");
        return pas_page_sharing_pool_take_success;
    }
    return pas_page_sharing_pool_take_locks_unavailable;
}

void pas_large_sharing_pool_validate(void)
{
    pas_large_sharing_node* node;
    
    pas_heap_lock_assert_held();

    if (!pas_large_sharing_pool_enabled)
        return;
    
    for (node = (pas_large_sharing_node*)
             pas_red_black_tree_minimum(&pas_large_sharing_tree);
         node; node = successor(node)) {
        pas_large_sharing_node* next_node;
        
        validate_node(node);

        if (belongs_in_min_heap(node))
            PAS_ASSERT(node->index_in_min_heap);
        else
            PAS_ASSERT(!node->index_in_min_heap);
        
        if (node->index_in_min_heap) {
            PAS_ASSERT(*pas_large_sharing_min_heap_get_ptr_by_index(
                           &pas_large_sharing_min_heap_instance,
                           node->index_in_min_heap) == node);
            PAS_ASSERT(pas_large_sharing_min_heap_is_still_ordered(
                           &pas_large_sharing_min_heap_instance, node));
        }
        
        next_node = successor(node);
        
        if (next_node)
            PAS_ASSERT(next_node->range.begin == node->range.end);
    }
}

pas_heap_summary
pas_large_sharing_pool_compute_summary(
    pas_range range,
    pas_large_sharing_pool_compute_summary_allocation_state allocation_state,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_large_sharing_node* node;
    pas_heap_summary result;

    pas_zero_memory(&result, sizeof(result));
    
    if (!pas_large_sharing_pool_enabled) {
        result.allocated += pas_range_size(range);
        result.committed += pas_range_size(range);
        return result;
    }
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    
    pas_heap_lock_assert_held();
    
    node = min_node_for_range(range);
    
    PAS_ASSERT(node);

    for (; node && node->range.begin < range.end; node = successor(node)) {
        pas_range overlapping_range;
        size_t overlapping_size;

        overlapping_range = pas_range_create_intersection(node->range, range);
        overlapping_size = pas_range_size(overlapping_range);
        PAS_ASSERT(overlapping_size);
        
        if (node->is_committed)
            result.committed += overlapping_size;
        else
            result.decommitted += overlapping_size;
        if (!node->num_live_bytes) {
            result.free += overlapping_size;

            if (node->is_committed)
                result.free_eligible_for_decommit += overlapping_size;
            else
                result.free_decommitted += overlapping_size;
        } else {
            size_t node_allocated;
            size_t node_free;
            size_t allocated;
            size_t free;

            node_allocated = node->num_live_bytes;
            node_free = pas_range_size(node->range) - node->num_live_bytes;

            allocated = 0;
            free = 0;
            
            switch (allocation_state) {
            case pas_large_sharing_pool_compute_summary_unknown_allocation_state:
                PAS_ASSERT(pas_range_subsumes(range, node->range));
                allocated = node_allocated;
                free = node_free;
                break;
            case pas_large_sharing_pool_compute_summary_known_allocated:
                allocated = overlapping_size;
                break;
            case pas_large_sharing_pool_compute_summary_known_free:
                free = overlapping_size;
                break;
            }

            PAS_ASSERT(allocated <= node_allocated);
            PAS_ASSERT(free <= node_free);

            result.allocated += allocated;
            result.free += free;
            result.free_ineligible_for_decommit += free;
        }
    }
    
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    
    return result;
}

bool pas_large_sharing_pool_for_each(
    pas_large_sharing_pool_visitor visitor,
    void* arg,
    pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_large_sharing_node* node;
    bool result;

    if (!pas_large_sharing_pool_enabled)
        return true;
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    
    pas_heap_lock_assert_held();
    
    result = true;
    
    for (node = (pas_large_sharing_node*)
             pas_red_black_tree_minimum(&pas_large_sharing_tree);
         node; node = successor(node)) {
        if (!visitor(node, arg)) {
            result = false;
            break;
        }
    }
    
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    
    return result;
}

#endif /* LIBPAS_ENABLED */
