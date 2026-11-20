/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#ifndef BMALLOC_PREFAULT_SUPPLY_H
#define BMALLOC_PREFAULT_SUPPLY_H

#include "pas_config.h"

#if PAS_ENABLE_BMALLOC

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* A supply of ordinary bmalloc blocks that a helper thread has already written to, so that the
   thread taking one does not have to fault the memory in.

   This exists for JavaScriptCore's MarkedBlock: a heap that is ramping up asks for thousands of
   fresh blocks and pays a write fault for each one on the mutator thread. The blocks are ordinary
   bmalloc allocations and are freed with the ordinary bmalloc entrypoint, so nothing about how the
   memory is shared with the rest of the process changes; the only difference is which thread takes
   the fault.

   No blocks are kept ready until a client sets the block size and a nonzero target.

   Except for bmalloc_prefault_supply_cancel_fault_in, nothing here may be called with the heap lock
   held, since each entrypoint sits on a path that allocates or frees. */

#define BMALLOC_PREFAULT_SUPPLY_MAX_BLOCKS 64

/* How many blocks to keep ready, silently capped at BMALLOC_PREFAULT_SUPPLY_MAX_BLOCKS, or zero to
   disable the supply. Must be set after the block size and before the first take, and must not
   change afterwards. */
PAS_API extern unsigned bmalloc_prefault_supply_target;

/* How long the supply goes without anyone wanting a block before it hands the memory back. The
   thread exits once a whole interval also passes without a fault-in (see below), and a later take
   or fault-in starts a new one. */
PAS_API extern double bmalloc_prefault_supply_idle_timeout_in_milliseconds;

/* The one size served. Must be set before the target, and must not change afterwards. */
PAS_API void bmalloc_prefault_supply_set_block_size(size_t block_size);

/* A block of the configured size: prefaulted if the supply had one ready, allocated the ordinary
   way if it did not. NULL means the allocation failed, or that no block size has been set. Free it
   with the ordinary bmalloc entrypoint, exactly like a block that never came from here. */
PAS_API void* bmalloc_prefault_supply_try_allocate(void);

/* Frees everything being held. */
PAS_API void bmalloc_prefault_supply_scavenge(void);

/* How many blocks are ready right now. For tests. */
PAS_API unsigned bmalloc_prefault_supply_block_count(void);

/* Makes the filling thread behave as if the heap were exhausted, so that a test can reach the
   standing-down path without running the machine out of memory. */
PAS_API extern bool bmalloc_prefault_supply_allocation_should_fail_for_testing;

/* The same thread also faults in large allocations that have already been handed out, for a client
   that is about to write one from start to end: the client gets the memory at once and writes it
   while the thread faults the pages in ahead of it. Unlike the supply above, the pages are faulted
   in without being written, so the client may write the memory at any time.

   Allocations smaller than this are not faulted in, since waking the thread costs more than their
   faults. Every allocation at least this large is a large object, which every way of freeing or
   reallocating it reaches through pas_try_deallocate_known_large or pas_try_deallocate_pgm_large.
   Both withdraw the allocation's request before the memory goes back to the heap, because the thread
   reads the pages it faults in, and faulting in a range the heap has since decommitted would make it
   resident again behind the heap's back. So the client frees the allocation like any other, at any
   time. */
#define BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE ((size_t)1 << 20)

/* Whether allocations ask for a fault-in at all. Set by a client that would rather keep the
   footprint down; read on every such allocation. */
PAS_API extern bool bmalloc_prefault_supply_fault_in_is_enabled;

/* Has the thread fault in allocation, which the caller has just allocated from any bmalloc heap and
   is about to write, if it is at least BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE. */
PAS_API void bmalloc_prefault_supply_request_fault_in(void* allocation, size_t size);

/* How many fault-ins are queued, plus one while the thread is faulting in a chunk. Read without a
   lock so that freeing a large object stays cheap when nothing is outstanding. The thread writes it
   on every chunk, so it is padded to a cache line of its own: otherwise each chunk would also evict
   whatever the linker happened to place next to it, from every core reading that. */
struct PAS_ALIGNED(64) bmalloc_prefault_supply_active_fault_in_count {
    unsigned value;
};
typedef struct bmalloc_prefault_supply_active_fault_in_count bmalloc_prefault_supply_active_fault_in_count;

PAS_API extern bmalloc_prefault_supply_active_fault_in_count bmalloc_prefault_supply_num_active_fault_ins;

PAS_API void bmalloc_prefault_supply_cancel_fault_in_slow(void* allocation);

/* Withdraws the fault-in of allocation. Once this returns, the thread is not touching the
   allocation and never will. Takes no lock unless the thread is faulting in a chunk of it, and then
   waits for that chunk. The thread takes no other lock while finishing a chunk, so this may be called
   with the heap lock held, at the cost of holding it through the wait. */
static inline void bmalloc_prefault_supply_cancel_fault_in(void* allocation)
{
    /* Pairs with the release decrement the thread makes once a chunk is done, so that seeing
       nothing active also means seeing the thread's last chunk as finished. */
    if (__atomic_load_n(&bmalloc_prefault_supply_num_active_fault_ins.value, __ATOMIC_ACQUIRE))
        bmalloc_prefault_supply_cancel_fault_in_slow(allocation);
}

/* Queues a fault-in of [base, base + size), which need not be a heap allocation. The caller cancels
   it with bmalloc_prefault_supply_cancel_fault_in before unmapping the memory. */
PAS_API void bmalloc_prefault_supply_request_fault_in_for_testing(void* base, size_t size);
/* How many fault-ins are waiting for the thread, which a cancellation brings to zero for its
   allocation before it returns. For tests. */
PAS_API unsigned bmalloc_prefault_supply_queued_fault_in_count(void);
/* Whether a fault-in of allocation is waiting for the thread. For tests. */
PAS_API bool bmalloc_prefault_supply_fault_in_is_queued_for_testing(void* allocation);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* BMALLOC_PREFAULT_SUPPLY_H */
