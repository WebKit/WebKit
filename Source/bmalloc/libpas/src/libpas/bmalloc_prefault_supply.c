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
#include "pas_page_malloc.h"
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
   alignment rather than by which thread touches them. thread_is_running rides along, being touched
   only on the slow path, under supply_lock. */
static PAS_ALIGNED(64) struct {
    unsigned num_blocks;
    /* Counted rather than flagged: a flag has to be cleared, and a take landing next to the clear
       would be erased rather than merely late. Only ever compared against a snapshot taken one
       interval earlier, so it may wrap freely. */
    unsigned demand_count;
    bool thread_is_running;
} supply_state;

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

        block = bmalloc_try_allocate_with_alignment_inline(supply_block_size, supply_block_size, pas_always_compact_allocation_mode);
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

static void compute_idle_deadline(struct timespec* deadline)
{
    double milliseconds;
    uint64_t nanoseconds;

    /* Clamped rather than trusted: an interval that lands outside what a timespec can hold makes
       pthread_cond_timedwait fail rather than wait, which would spin the filling thread. */
    milliseconds = bmalloc_prefault_supply_idle_timeout_in_milliseconds;
    if (!(milliseconds > MIN_IDLE_TIMEOUT_IN_MILLISECONDS))
        milliseconds = MIN_IDLE_TIMEOUT_IN_MILLISECONDS;
    else if (milliseconds > MAX_IDLE_TIMEOUT_IN_MILLISECONDS)
        milliseconds = MAX_IDLE_TIMEOUT_IN_MILLISECONDS;

    nanoseconds = now_in_nanoseconds() + (uint64_t)(milliseconds * 1000000.);
    deadline->tv_sec = (time_t)(nanoseconds / 1000000000ULL);
    deadline->tv_nsec = (long)(nanoseconds % 1000000000ULL);
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

    for (;;) {
        struct timespec deadline;
        unsigned demand_at_start;
        bool is_full;
        bool should_exit;

        /* Snapshotted before the refill, so that a take landing while this pass refills, or while
           it waits to acquire the lock, still counts towards this interval. */
        demand_at_start = __atomic_load_n(&supply_state.demand_count, __ATOMIC_RELAXED);

        is_full = refill();

        pthread_mutex_lock(&supply_lock);

        /* Computed with the lock held, so the interval measures the wait rather than the wait plus
           however long acquiring the lock took. */
        compute_idle_deadline(&deadline);

        /* A supply the heap refused to fill waits out the whole interval instead of asking again,
           which is what keeps an exhausted heap from turning this into a spin. Any error from the
           wait is treated as the interval being over for the same reason. */
        while (supply_target() && (!is_full || __atomic_load_n(&supply_state.num_blocks, __ATOMIC_RELAXED) >= supply_target())) {
            if (pthread_cond_timedwait(&supply_cond, &supply_lock, &deadline))
                break;
        }

        /* An interval with no demand at all means the memory is being held on the chance that
           somebody will want it. Give it back and let the thread go; a later take starts a new
           one. */
        should_exit = !supply_target()
            || __atomic_load_n(&supply_state.demand_count, __ATOMIC_RELAXED) == demand_at_start;
        pthread_mutex_unlock(&supply_lock);

        if (!should_exit)
            continue;

        bmalloc_prefault_supply_scavenge();

        pthread_mutex_lock(&supply_lock);
        /* thread_is_running stays set until the blocks are gone, so that a replacement filler
           cannot install a block this one is about to free. Demand that arrived in the meantime
           therefore started nobody, and has to be picked up here. */
        should_exit = __atomic_load_n(&supply_state.demand_count, __ATOMIC_RELAXED) == demand_at_start;
        if (should_exit)
            supply_state.thread_is_running = false;
        pthread_mutex_unlock(&supply_lock);

        if (should_exit)
            return SUPPLY_THREAD_RESULT_NULL;
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
            should_create = claim_filling_thread();
            pthread_cond_broadcast(&supply_cond);
            pthread_mutex_unlock(&supply_lock);

            if (should_create)
                create_filling_thread();
        }
    }

    if (result)
        return result;

    return bmalloc_try_allocate_with_alignment_inline(supply_block_size, supply_block_size, pas_always_compact_allocation_mode);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
