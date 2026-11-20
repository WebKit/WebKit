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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "bmalloc_prefault_supply.h"

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap_inlines.h"
#include "pas_dyld_state.h"
#include "pas_heap_lock.h"
#include "pas_large_map.h"
#include "pas_page_malloc.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_system_heap.h"
#include "pas_thread.h"
#if PAS_OS(WINDOWS)
#include "pas_monotonic_time.h"
#else
#include <sys/time.h>
#endif
#if PAS_OS(DARWIN)
#include <sys/qos.h>
#endif

PAS_BEGIN_EXTERN_C;

unsigned bmalloc_prefault_supply_target;
double bmalloc_prefault_supply_idle_timeout_in_milliseconds = 10. * 1000.;
bool bmalloc_prefault_supply_allocation_should_fail_for_testing;

static pthread_mutex_t supply_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t supply_cond = PTHREAD_COND_INITIALIZER;

/* Read on every take and written only while configuring, so it is kept clear of the line the take
   path writes: otherwise recording demand evicts it from every other core on every take. */
static PAS_ALIGNED(64) size_t supply_block_size;

/* On its own cache line, so that a taker claiming a slot does not also take away the line holding
   the counters the filler reads on every pass. */
static PAS_ALIGNED(64) void* slots[BMALLOC_PREFAULT_SUPPLY_MAX_BLOCKS];

/* One struct rather than three objects because the layout is the point: num_blocks and demand_count
   are both written by every take and want one line between them, and neither may share a line with
   supply_block_size. Separate objects cannot express that, since the compiler groups them by
   alignment rather than by which thread touches them. thread_is_running and take_found_supply_low
   ride along, being touched only on the slow path, under supply_lock. */
static PAS_ALIGNED(64) struct {
    unsigned num_blocks;
    /* Counted rather than flagged: a flag has to be cleared, and a take landing next to the clear
       would be erased rather than merely late. Only ever compared against a snapshot taken one
       interval earlier, so it may wrap freely. */
    unsigned demand_count;
    bool thread_is_running;
    /* Set by a take that found the supply nearly empty, and cleared by the thread the next time it
       looks, which resumes refilling unless the thread is backing off. Both happen under
       supply_lock, so unlike a snapshot of demand_count it cannot miss a take that lands while the
       thread is starting or busy faulting in. */
    bool take_found_supply_low;
} supply_state;

/* Advice only, so a fault-in that does not fit pushes out the oldest one, which is the one most
   likely to have been written already. */
#define MAX_FAULT_IN_REQUESTS 16

/* How much the thread hands the OS at once. Between chunks it checks for a newer fault-in, for a
   cancellation, and for a take that found the block supply low, so this bounds how long any of
   them waits. */
#define FAULT_IN_CHUNK_SIZE ((uintptr_t)1 << 20)

/* A cancellation runs on the free path of every large object, so it takes no lock unless it has
   to wait for a chunk. That works because the allocation it cancels cannot be queued again while it
   is being freed, so its slot can only go from holding it to free; and because a slot never moves,
   a scan cannot step over it. Everything else that writes a slot holds supply_lock. */
typedef struct {
    /* Zero while the slot is free. Cleared by a cancellation without supply_lock, so every access
       is atomic. The rest of the slot is touched only under supply_lock. */
    uintptr_t allocation;
    uintptr_t begin;
    uintptr_t end;
    /* Order of arrival, which newest-first serving and oldest-first eviction go by. */
    uint64_t sequence;
} fault_in_request;

static fault_in_request fault_in_requests[MAX_FAULT_IN_REQUESTS];
/* Guarded by supply_lock. */
static uint64_t next_fault_in_sequence;
/* Compared against a snapshot like demand_count, so it may wrap. Guarded by supply_lock. */
static unsigned fault_in_request_count;

/* Written by the thread on every chunk, so kept together, and apart from the read-mostly state the
   allocation and free paths load. */
struct PAS_ALIGNED(64) fault_in_chunk_state {
    /* The allocation whose chunk the thread is faulting in, or zero. Written only by the thread,
       while it holds chunk_lock. */
    uintptr_t allocation_being_faulted_in;

    /* Held by the thread while the OS faults in a chunk, so that a cancellation waiting for the
       chunk waits on a lock, which on Darwin lends the waiter's priority to the thread. Taken after
       supply_lock, never before it: the thread holds both while it claims a chunk. Nothing else is
       taken while it is held, which is what lets a cancellation wait for it with the heap lock held. */
    pas_lock chunk_lock;
};

static struct fault_in_chunk_state fault_in_chunk_state = { .allocation_being_faulted_in = 0, .chunk_lock = PAS_LOCK_INITIALIZER };

bool bmalloc_prefault_supply_fault_in_is_enabled = true;
bmalloc_prefault_supply_active_fault_in_count bmalloc_prefault_supply_num_active_fault_ins;

static bool fault_in_is_unsupported;

/* Far longer than any client could mean, and short of what would overflow the deadline. */
#define MAX_IDLE_TIMEOUT_IN_MILLISECONDS (24. * 60. * 60. * 1000.)

/* A floor, so that a client asking for no wait at all cannot turn the filling thread into a spin. */
#define MIN_IDLE_TIMEOUT_IN_MILLISECONDS 1.

static unsigned supply_target(void)
{
    if (!supply_block_size)
        return 0;
    return PAS_MIN(bmalloc_prefault_supply_target, (unsigned)BMALLOC_PREFAULT_SUPPLY_MAX_BLOCKS);
}

void bmalloc_prefault_supply_set_block_size(size_t block_size)
{
    PAS_ASSERT(pas_is_power_of_2(block_size));
    PAS_ASSERT(!supply_block_size || supply_block_size == block_size);
    supply_block_size = block_size;
}

unsigned bmalloc_prefault_supply_block_count(void)
{
    return __atomic_load_n(&supply_state.num_blocks, __ATOMIC_RELAXED);
}

void bmalloc_prefault_supply_scavenge(void)
{
    unsigned index;

    pas_heap_lock_testing_assert_not_held();

    for (index = 0; index < BMALLOC_PREFAULT_SUPPLY_MAX_BLOCKS; ++index) {
        void* block = __atomic_exchange_n(&slots[index], (void*)NULL, __ATOMIC_ACQUIRE);
        if (!block)
            continue;
        __atomic_fetch_sub(&supply_state.num_blocks, 1, __ATOMIC_RELAXED);

        /* An ordinary bmalloc block, so this hands the memory back the way the client would. */
        bmalloc_deallocate_inline(block);
    }
}

/* Fills every empty slot, taking no lock at all. Returns false if the heap could not satisfy a
   request, which is the caller's cue to back off instead of asking again straight away. */
static bool refill(void)
{
    unsigned target;
    unsigned index;
    bool should_fail_for_testing;

    should_fail_for_testing = __atomic_load_n(&bmalloc_prefault_supply_allocation_should_fail_for_testing, __ATOMIC_RELAXED);

    target = supply_target();
    for (index = 0; index < target; ++index) {
        void* block;
        void* previous;

        if (__atomic_load_n(&slots[index], __ATOMIC_RELAXED))
            continue;

        if (PAS_UNLIKELY(should_fail_for_testing))
            return false;

        block = bmalloc_try_allocate_auxiliary_with_alignment_inline(
            &bmalloc_compact_primitive_heap_ref, supply_block_size, supply_block_size);
        if (!block)
            return false;

        pas_page_malloc_populate(block, supply_block_size);

        /* Counted before it is reachable, so that a taker's decrement can never be ordered ahead
           of this increment and wrap the count below zero. */
        __atomic_fetch_add(&supply_state.num_blocks, 1, __ATOMIC_RELAXED);
        /* Only this thread fills, so the slot seen empty above is still empty. */
        previous = __atomic_exchange_n(&slots[index], block, __ATOMIC_RELEASE);
        PAS_ASSERT(!previous);
    }
    return true;
}

static bool supply_needs_refill(void)
{
    unsigned target;

    target = supply_target();
    return target && __atomic_load_n(&supply_state.num_blocks, __ATOMIC_RELAXED) < target;
}

/* Frees the slot if it still holds allocation. Returns false if somebody else freed it first. A zero
   allocation is rejected rather than compared, since swapping a free slot's zero for zero would
   succeed and uncount a fault-in that is not there. */
static bool clear_fault_in_request(fault_in_request* request, uintptr_t allocation)
{
    if (!allocation)
        return false;
    if (pas_compare_and_swap_uintptr_strong(&request->allocation, allocation, 0) != allocation)
        return false;
    __atomic_fetch_sub(&bmalloc_prefault_supply_num_active_fault_ins.value, 1, __ATOMIC_RELEASE);
    return true;
}

/* Caller holds supply_lock. */
static fault_in_request* newest_fault_in_request(void)
{
    fault_in_request* result;
    unsigned index;

    result = NULL;
    for (index = 0; index < MAX_FAULT_IN_REQUESTS; ++index) {
        fault_in_request* request = fault_in_requests + index;
        if (!__atomic_load_n(&request->allocation, __ATOMIC_ACQUIRE))
            continue;
        if (!result || request->sequence > result->sequence)
            result = request;
    }
    return result;
}

/* Caller holds supply_lock. Returns a free slot, making one by evicting the oldest fault-in if
   every slot is taken. */
static fault_in_request* free_fault_in_request(void)
{
    fault_in_request* oldest;
    uintptr_t oldest_allocation;
    unsigned index;

    oldest = NULL;
    oldest_allocation = 0;
    for (index = 0; index < MAX_FAULT_IN_REQUESTS; ++index) {
        fault_in_request* request = fault_in_requests + index;
        uintptr_t allocation = __atomic_load_n(&request->allocation, __ATOMIC_ACQUIRE);
        if (!allocation)
            return request;
        if (!oldest || request->sequence < oldest->sequence) {
            oldest = request;
            oldest_allocation = allocation;
        }
    }
    /* Failing just means a cancellation freed it first. The allocation seen by the scan is the one
       to clear: nothing can queue another into the slot while supply_lock is held. */
    clear_fault_in_request(oldest, oldest_allocation);
    return oldest;
}

/* Caller holds supply_lock. */
static void clear_all_fault_in_requests(void)
{
    unsigned index;

    for (index = 0; index < MAX_FAULT_IN_REQUESTS; ++index) {
        fault_in_request* request = fault_in_requests + index;
        uintptr_t allocation = __atomic_load_n(&request->allocation, __ATOMIC_ACQUIRE);
        if (allocation)
            clear_fault_in_request(request, allocation);
    }
}

typedef enum {
    supply_blocks_mode_refilling,

    /* The heap refused to fill the supply, so the thread waits out the rest of the interval instead
       of asking again, which is what keeps an exhausted heap from turning this into a spin. */
    supply_blocks_mode_backing_off,

    /* An interval passed without a take, so the blocks were given back, and fault-ins alone keep
       the thread until a take finds the supply low. */
    supply_blocks_mode_released,
} supply_blocks_mode;

typedef struct {
    uint64_t deadline;
    unsigned demand_at_start;
    unsigned fault_in_requests_at_start;
    supply_blocks_mode blocks_mode;
} supply_thread_state;

/* Caller holds supply_lock. */
static void resume_refilling_if_a_take_found_supply_low(supply_thread_state* state)
{
    if (!supply_state.take_found_supply_low)
        return;
    supply_state.take_found_supply_low = false;
    if (state->blocks_mode == supply_blocks_mode_released)
        state->blocks_mode = supply_blocks_mode_refilling;
}

static void refill_if_needed(supply_thread_state* state)
{
    if (state->blocks_mode != supply_blocks_mode_refilling || !supply_needs_refill())
        return;
    if (!refill())
        state->blocks_mode = supply_blocks_mode_backing_off;
}

/* Caller holds supply_lock. */
static bool blocks_were_wanted_this_interval(const supply_thread_state* state, unsigned demand)
{
    return demand != state->demand_at_start || supply_state.take_found_supply_low;
}

/* Serves one chunk of the newest fault-in. The newest range is the one its client is about to
   write, while an older one may already have been written, so a fault-in arriving while an older
   one is being served takes over at the next chunk. Returns false if there was nothing to do. */
static bool serve_fault_in_chunk(supply_thread_state* state)
{
    fault_in_request* request;
    uintptr_t allocation;
    uintptr_t begin;
    uintptr_t end;
    bool is_supported;

    pthread_mutex_lock(&supply_lock);
    resume_refilling_if_a_take_found_supply_low(state);
    request = newest_fault_in_request();
    if (!request) {
        pthread_mutex_unlock(&supply_lock);
        return false;
    }
    begin = request->begin;
    end = request->end - begin > FAULT_IN_CHUNK_SIZE ? begin + FAULT_IN_CHUNK_SIZE : request->end;
    allocation = __atomic_load_n(&request->allocation, __ATOMIC_ACQUIRE);

    /* Counted as active from before the check below until the chunk is done, whoever clears the
       slot meanwhile, so that a cancellation seeing nothing active also sees the chunk as
       finished. */
    pas_lock_lock(&fault_in_chunk_state.chunk_lock);
    __atomic_fetch_add(&bmalloc_prefault_supply_num_active_fault_ins.value, 1, __ATOMIC_RELAXED);

    /* A cancellation clears the slot and then looks at allocation_being_faulted_in, and this does
       the reverse, both sequentially consistent, so either this sees the slot cleared or the
       cancellation sees the chunk and waits for it. */
    __atomic_store_n(&fault_in_chunk_state.allocation_being_faulted_in, allocation, __ATOMIC_SEQ_CST);
    if (!allocation || __atomic_load_n(&request->allocation, __ATOMIC_SEQ_CST) != allocation) {
        __atomic_store_n(&fault_in_chunk_state.allocation_being_faulted_in, 0, __ATOMIC_RELEASE);
        __atomic_fetch_sub(&bmalloc_prefault_supply_num_active_fault_ins.value, 1, __ATOMIC_RELEASE);
        pas_lock_unlock(&fault_in_chunk_state.chunk_lock);
        pthread_mutex_unlock(&supply_lock);
        return true;
    }
    request->begin = end;
    if (end == request->end)
        clear_fault_in_request(request, allocation);
    pthread_mutex_unlock(&supply_lock);

    is_supported = pas_page_malloc_fault_in_without_writing((void*)begin, end - begin, bmalloc_heap_config.memory_flavor);

    __atomic_store_n(&fault_in_chunk_state.allocation_being_faulted_in, 0, __ATOMIC_RELEASE);
    __atomic_fetch_sub(&bmalloc_prefault_supply_num_active_fault_ins.value, 1, __ATOMIC_RELEASE);
    pas_lock_unlock(&fault_in_chunk_state.chunk_lock);

    if (!is_supported) {
        pthread_mutex_lock(&supply_lock);
        __atomic_store_n(&fault_in_is_unsupported, true, __ATOMIC_RELAXED);
        clear_all_fault_in_requests();
        pthread_mutex_unlock(&supply_lock);
    }
    return true;
}

static uint64_t now_in_nanoseconds(void)
{
#if PAS_OS(WINDOWS)
    /* The deadline pthread_cond_timedwait takes is measured against QueryPerformanceCounter there,
       which is the clock this reads. */
    return pas_get_current_monotonic_time_nanoseconds();
#else
    struct timeval now;

    /* pthread_cond_timedwait measures its deadline against the realtime clock. */
    gettimeofday(&now, NULL);
    return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_usec * 1000ULL;
#endif
}

static uint64_t compute_idle_deadline(void)
{
    double milliseconds;

    /* Clamped rather than trusted: an interval that lands outside what a timespec can hold makes
       pthread_cond_timedwait fail rather than wait, which would spin the filling thread. */
    milliseconds = bmalloc_prefault_supply_idle_timeout_in_milliseconds;
    if (!(milliseconds > MIN_IDLE_TIMEOUT_IN_MILLISECONDS))
        milliseconds = MIN_IDLE_TIMEOUT_IN_MILLISECONDS;
    else if (milliseconds > MAX_IDLE_TIMEOUT_IN_MILLISECONDS)
        milliseconds = MAX_IDLE_TIMEOUT_IN_MILLISECONDS;

    return now_in_nanoseconds() + (uint64_t)(milliseconds * 1000000.);
}

/* Caller holds supply_lock. */
static void start_idle_interval(supply_thread_state* state, unsigned demand)
{
    state->demand_at_start = demand;
    state->fault_in_requests_at_start = fault_in_request_count;
    state->deadline = compute_idle_deadline();
}

/* Caller holds supply_lock. */
static bool has_work(const supply_thread_state* state)
{
    if (newest_fault_in_request())
        return true;
    switch (state->blocks_mode) {
    case supply_blocks_mode_refilling:
        return supply_needs_refill();
    case supply_blocks_mode_backing_off:
        return false;
    case supply_blocks_mode_released:
        return supply_state.take_found_supply_low;
    }
    PAS_ASSERT_NOT_REACHED();
    return false;
}

/* Waits, with supply_lock held, until there is work or the interval is over. Returns whether the
   interval is over. Any error from the wait is treated as the interval being over, for the same
   reason as a failed refill. */
static bool wait_for_work(supply_thread_state* state)
{
    for (;;) {
        struct timespec deadline;

        /* Checked first, so that a thread kept busy by fault-ins still notices that the blocks
           have gone unwanted. */
        if (now_in_nanoseconds() >= state->deadline)
            return true;
        if (has_work(state))
            return false;

        deadline.tv_sec = (time_t)(state->deadline / 1000000000ULL);
        deadline.tv_nsec = (long)(state->deadline % 1000000000ULL);
        if (pthread_cond_timedwait(&supply_cond, &supply_lock, &deadline))
            return true;
    }
}

/* pthread_create takes a different entry point shape on Windows, where pthreads is the shim in
   pas_thread.h rather than the system's. */
#if PAS_OS(WINDOWS)
#define SUPPLY_THREAD_RESULT_NULL 0
static unsigned supply_thread_main(void* arg)
#else
#define SUPPLY_THREAD_RESULT_NULL NULL
static void* supply_thread_main(void* arg)
#endif
{
    supply_thread_state state;

    PAS_UNUSED_PARAM(arg);

#if PAS_OS(DARWIN) || PAS_PLATFORM(PLAYSTATION)
#if PAS_BMALLOC
    pthread_setname_np("JavaScriptCore libpas prefault");
#else
    pthread_setname_np("libpas prefault");
#endif
#endif

#if PAS_OS(DARWIN)
    /* Set rather than inherited: the thread is created by whichever mutator happened to find the
       supply empty, and keeping a ramping heap supplied is not background work. */
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
#endif

    /* Blocks are refilled only once a take has asked for them. A take that starts this thread, or
       races with its start, leaves take_found_supply_low behind, which brings them back below. */
    state.blocks_mode = supply_blocks_mode_released;

    /* Snapshotted with the lock, so that a take landing while this pass refills, or while it
       waits to acquire the lock, still counts towards this interval. */
    pthread_mutex_lock(&supply_lock);
    start_idle_interval(&state, __atomic_load_n(&supply_state.demand_count, __ATOMIC_RELAXED));
    resume_refilling_if_a_take_found_supply_low(&state);
    pthread_mutex_unlock(&supply_lock);

    for (;;) {
        unsigned demand;
        bool has_block_demand;
        bool has_fault_in_demand;

        refill_if_needed(&state);
        while (serve_fault_in_chunk(&state))
            refill_if_needed(&state);

        pthread_mutex_lock(&supply_lock);
        if (!wait_for_work(&state)) {
            resume_refilling_if_a_take_found_supply_low(&state);
            pthread_mutex_unlock(&supply_lock);
            continue;
        }
        has_block_demand = blocks_were_wanted_this_interval(&state, __atomic_load_n(&supply_state.demand_count, __ATOMIC_RELAXED));
        pthread_mutex_unlock(&supply_lock);

        /* An interval without a take means the blocks are being held on the chance that somebody
           will want them. Give them back; a later take asks for them again. */
        if (!has_block_demand && state.blocks_mode != supply_blocks_mode_released)
            bmalloc_prefault_supply_scavenge();

        pthread_mutex_lock(&supply_lock);
        /* thread_is_running stays set until the blocks are gone, so that a replacement filler
           cannot install a block this one is about to free. Demand that arrived in the meantime
           therefore started nobody, and has to be picked up here. */
        demand = __atomic_load_n(&supply_state.demand_count, __ATOMIC_RELAXED);
        has_block_demand = blocks_were_wanted_this_interval(&state, demand);
        has_fault_in_demand = newest_fault_in_request() || fault_in_request_count != state.fault_in_requests_at_start;
        if (!has_block_demand && !has_fault_in_demand) {
            supply_state.thread_is_running = false;
            pthread_mutex_unlock(&supply_lock);
            return SUPPLY_THREAD_RESULT_NULL;
        }
        state.blocks_mode = has_block_demand ? supply_blocks_mode_refilling : supply_blocks_mode_released;
        start_idle_interval(&state, demand);
        resume_refilling_if_a_take_found_supply_low(&state);
        pthread_mutex_unlock(&supply_lock);
    }
}

/* Reserves the right to be the one that creates the filling thread. Claiming and creating are split
   so that pthread_create runs with supply_lock dropped: it allocates, and holding a lock across an
   allocation is how a lock cycle gets built once this entrypoint sits on an allocation path.
   Caller holds supply_lock. */
static bool claim_filling_thread(void)
{
    if (supply_state.thread_is_running)
        return false;
    if (!pas_dyld_is_libsystem_initialized())
        return false;

    /* Marked as running before the thread exists, so that a second taker cannot claim it too. */
    supply_state.thread_is_running = true;
    return true;
}

/* Caller has claimed the thread and dropped supply_lock. */
static void create_filling_thread(void)
{
    pthread_t thread;

    if (pthread_create(&thread, NULL, supply_thread_main, NULL)) {
        pthread_mutex_lock(&supply_lock);
        supply_state.thread_is_running = false;
        clear_all_fault_in_requests();
        pthread_mutex_unlock(&supply_lock);
        return;
    }
    pthread_detach(thread);
}

void* bmalloc_prefault_supply_try_allocate(void)
{
    void* result;
    unsigned target;
    unsigned index;

    pas_heap_lock_testing_assert_not_held();

    /* Without a size there is nothing to hand back, not even an ordinary allocation. */
    if (!supply_block_size)
        return NULL;

    result = NULL;
    target = supply_target();
    for (index = 0; index < target; ++index) {
        /* Probed with a load rather than an exchange, so that walking past the slots earlier
           takers emptied does not take those lines away from the filler. */
        if (!__atomic_load_n(&slots[index], __ATOMIC_RELAXED))
            continue;

        result = __atomic_exchange_n(&slots[index], (void*)NULL, __ATOMIC_ACQUIRE);
        if (result) {
            __atomic_fetch_sub(&supply_state.num_blocks, 1, __ATOMIC_RELAXED);
            break;
        }
    }

    /* A disabled supply wants no thread and no record of demand, but the caller still wants its
       block, so the ordinary allocation below is what it gets. */
    if (target) {
        __atomic_fetch_add(&supply_state.demand_count, 1, __ATOMIC_RELAXED);

        /* Waking the filler on every take would cost a signal per block during a ramp, which is
           more than the faults being avoided, so it happens only once the supply is nearly gone. */
        if (!result || __atomic_load_n(&supply_state.num_blocks, __ATOMIC_RELAXED) * 4 < target) {
            bool should_create;

            pthread_mutex_lock(&supply_lock);
            supply_state.take_found_supply_low = true;
            should_create = claim_filling_thread();
            pthread_cond_broadcast(&supply_cond);
            pthread_mutex_unlock(&supply_lock);

            if (should_create)
                create_filling_thread();
        }
    }

    if (result)
        return result;

    return bmalloc_try_allocate_auxiliary_with_alignment_inline(
        &bmalloc_compact_primitive_heap_ref, supply_block_size, supply_block_size);
}

/* Queues the pages that lie wholly inside the allocation. */
static void request_fault_in(uintptr_t allocation, size_t size)
{
    uintptr_t page_size;
    uintptr_t begin;
    uintptr_t end;
    fault_in_request* request;
    bool should_create;

    if (!PAS_PAGE_MALLOC_CAN_FAULT_IN_WITHOUT_WRITING)
        return;
    if (__atomic_load_n(&fault_in_is_unsupported, __ATOMIC_RELAXED))
        return;
    /* No thread can be started to serve the request yet. */
    if (!pas_dyld_is_libsystem_initialized())
        return;

    page_size = pas_page_malloc_alignment();
    begin = pas_round_up_to_power_of_2(allocation, page_size);
    end = pas_round_down_to_power_of_2(allocation + size, page_size);
    if (end <= begin)
        return;

    pthread_mutex_lock(&supply_lock);
    request = free_fault_in_request();
    request->begin = begin;
    request->end = end;
    request->sequence = next_fault_in_sequence++;
    /* Counted before the slot is visible, so the count never falls short of the slots in use. */
    __atomic_fetch_add(&bmalloc_prefault_supply_num_active_fault_ins.value, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&request->allocation, allocation, __ATOMIC_RELEASE);
    fault_in_request_count++;
    should_create = claim_filling_thread();
    pthread_cond_broadcast(&supply_cond);
    pthread_mutex_unlock(&supply_lock);

    if (should_create)
        create_filling_thread();
}

static bool fault_in_is_enabled(void)
{
    if (!__atomic_load_n(&bmalloc_prefault_supply_fault_in_is_enabled, __ATOMIC_RELAXED))
        return false;

    /* The system heap frees without reaching the cancellation in libpas's own free path. */
    return !pas_system_heap_should_supplant_bmalloc(pas_heap_config_kind_bmalloc);
}

void bmalloc_prefault_supply_request_fault_in(void* allocation, size_t size)
{
    pas_heap_lock_testing_assert_not_held();

    if (!allocation || size < BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE)
        return;
    if (!fault_in_is_enabled())
        return;

    if (PAS_ENABLE_TESTING) {
        bool is_large;

        pas_heap_lock_lock();
        is_large = !pas_large_map_entry_is_empty(pas_large_map_find((uintptr_t)allocation))
            || pas_probabilistic_guard_malloc_check_exists((uintptr_t)allocation);
        pas_heap_lock_unlock();
        PAS_ASSERT(is_large);
    }

    request_fault_in((uintptr_t)allocation, size);
}

void bmalloc_prefault_supply_request_fault_in_for_testing(void* base, size_t size)
{
    pas_heap_lock_testing_assert_not_held();
    request_fault_in((uintptr_t)base, size);
}

void bmalloc_prefault_supply_cancel_fault_in_slow(void* allocation)
{
    unsigned index;

    /* This function is called when freeing allocation. Thus assuming that we are not concurrently
       registering this allocation for prefault. We can just check slots without locking. */
    for (index = 0; index < MAX_FAULT_IN_REQUESTS; ++index) {
        fault_in_request* request = fault_in_requests + index;
        if (__atomic_load_n(&request->allocation, __ATOMIC_ACQUIRE) == (uintptr_t)allocation) {
            clear_fault_in_request(request, (uintptr_t)allocation);
            break;
        }
    }

    /* The thread published the allocation before checking its slot, and holds the lock until the
       chunk is done and the allocation is unpublished. */
    if (__atomic_load_n(&fault_in_chunk_state.allocation_being_faulted_in, __ATOMIC_SEQ_CST) == (uintptr_t)allocation) {
        pas_lock_lock(&fault_in_chunk_state.chunk_lock);
        pas_lock_unlock(&fault_in_chunk_state.chunk_lock);
    }
}

unsigned bmalloc_prefault_supply_queued_fault_in_count(void)
{
    unsigned result;
    unsigned index;

    result = 0;
    for (index = 0; index < MAX_FAULT_IN_REQUESTS; ++index)
        result += !!__atomic_load_n(&fault_in_requests[index].allocation, __ATOMIC_ACQUIRE);
    return result;
}

bool bmalloc_prefault_supply_fault_in_is_queued_for_testing(void* allocation)
{
    unsigned index;

    for (index = 0; index < MAX_FAULT_IN_REQUESTS; ++index) {
        if (__atomic_load_n(&fault_in_requests[index].allocation, __ATOMIC_ACQUIRE) == (uintptr_t)allocation)
            return true;
    }
    return false;
}

#if PAS_COMPILER(CLANG)
_Static_assert(PAS_MAX_BITFIT_OBJECT_SIZE_WITH_MIN_OBJECTS(PAS_MARGE_PAGE_DEFAULT_SIZE, PAS_MIN_MARGE_ALIGN_SHIFT,
                                                           BMALLOC_MARGE_BITFIT_MIN_OBJECTS_PER_PAGE)
               < BMALLOC_PREFAULT_SUPPLY_MIN_FAULT_IN_SIZE,
               "Marge bitfit objects, the largest that are not large objects, must be too small to fault in, "
               "since freeing them does not cancel a fault-in");
#endif

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
