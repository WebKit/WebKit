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

#include "pas_futex.h"

#include <errno.h>
#include <limits.h>
#include <time.h>

#if PAS_OS(DARWIN)
#include <os/os_sync_wait_on_address.h>
#elif PAS_OS(LINUX)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif PAS_OS(WINDOWS)
#include <synchapi.h>
#include <windows.h>
#elif PAS_OS(FREEBSD)
#include <sys/types.h>
#include <sys/umtx.h>
#else
#include <pthread.h>
#endif

/* Clamped so a far deadline never overflows a 32-bit time_t or the kernel's deadline arithmetic;
   waking early is harmless because pas_futex_wait_until rechecks the clock. */
#define PAS_FUTEX_MAX_RELATIVE_TIMEOUT_IN_NANOSECONDS (365ULL * 24ULL * 60ULL * 60ULL * 1000000000ULL)

#if PAS_OS(DARWIN)

/* CLOCK_UPTIME_RAW is mach_absolute_time in nanoseconds, the clock OS_CLOCK_MACH_ABSOLUTE_TIME
   timeouts run on. */
uint64_t pas_futex_get_time_in_nanoseconds(void)
{
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

/* ENOMEM and EFAULT can be transient under memory pressure. Like EINTR they only end the wait
   early, and the caller's recheck decides whether to wait again. */
static void pas_futex_check_system_wait_result(int result)
{
    PAS_ASSERT(result >= 0 || errno != EINVAL);
}

void pas_futex_wait(unsigned* address, unsigned expected_value)
{
    pas_futex_check_system_wait_result(
        os_sync_wait_on_address(address, expected_value, sizeof(unsigned), OS_SYNC_WAIT_ON_ADDRESS_NONE));
}

static void pas_futex_system_wait_for(unsigned* address, unsigned expected_value, uint64_t relative_timeout_in_nanoseconds)
{
    pas_futex_check_system_wait_result(
        os_sync_wait_on_address_with_timeout(
            address, expected_value, sizeof(unsigned), OS_SYNC_WAIT_ON_ADDRESS_NONE,
            OS_CLOCK_MACH_ABSOLUTE_TIME, relative_timeout_in_nanoseconds));
}

void pas_futex_wake_one(unsigned* address)
{
    int result = os_sync_wake_by_address_any(address, sizeof(unsigned), OS_SYNC_WAKE_BY_ADDRESS_NONE);
    PAS_ASSERT(!result || errno != EINVAL);
}

void pas_futex_wake_all(unsigned* address)
{
    int result = os_sync_wake_by_address_all(address, sizeof(unsigned), OS_SYNC_WAKE_BY_ADDRESS_NONE);
    PAS_ASSERT(!result || errno != EINVAL);
}

#elif PAS_OS(WINDOWS)

uint64_t pas_futex_get_time_in_nanoseconds(void)
{
    static uint64_t ticks_per_second;
    LARGE_INTEGER counter;
    uint64_t frequency;
    uint64_t ticks;
    BOOL succeeded;

    frequency = __atomic_load_n(&ticks_per_second, __ATOMIC_RELAXED);
    if (PAS_UNLIKELY(!frequency)) {
        LARGE_INTEGER queried_frequency;
        succeeded = QueryPerformanceFrequency(&queried_frequency);
        PAS_ASSERT(succeeded);
        frequency = (uint64_t)queried_frequency.QuadPart;
        __atomic_store_n(&ticks_per_second, frequency, __ATOMIC_RELAXED);
    }

    succeeded = QueryPerformanceCounter(&counter);
    PAS_ASSERT(succeeded);
    ticks = (uint64_t)counter.QuadPart;
    return (ticks / frequency) * 1000000000ULL + ((ticks % frequency) * 1000000000ULL) / frequency;
}

void pas_futex_wait(unsigned* address, unsigned expected_value)
{
    WaitOnAddress(address, &expected_value, sizeof(unsigned), INFINITE);
}

static void pas_futex_system_wait_for(unsigned* address, unsigned expected_value, uint64_t relative_timeout_in_nanoseconds)
{
    uint64_t timeout_in_milliseconds;

    /* Rounded up: WaitOnAddress takes whole milliseconds, and rounding down would wake before the
       deadline only to wait again. */
    timeout_in_milliseconds = relative_timeout_in_nanoseconds / 1000000ULL + !!(relative_timeout_in_nanoseconds % 1000000ULL);
    if (timeout_in_milliseconds >= (uint64_t)(INFINITE - 1))
        timeout_in_milliseconds = INFINITE - 1;
    WaitOnAddress(address, &expected_value, sizeof(unsigned), (DWORD)timeout_in_milliseconds);
}

void pas_futex_wake_one(unsigned* address)
{
    WakeByAddressSingle(address);
}

void pas_futex_wake_all(unsigned* address)
{
    WakeByAddressAll(address);
}

#else /* POSIX systems other than Darwin */

#if PAS_PLATFORM(PLAYSTATION)
#define PAS_FUTEX_CLOCK_GETTIME clock_gettime_np
#else
#define PAS_FUTEX_CLOCK_GETTIME clock_gettime
#endif

uint64_t pas_futex_get_time_in_nanoseconds(void)
{
    struct timespec now;
    PAS_FUTEX_CLOCK_GETTIME(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
}

#if PAS_OS(LINUX)

/* SYS_futex reads its timeout in the kernel's original layout of two longs, which differs from
   struct timespec on 32-bit targets built with a 64-bit time_t. Targets that only provide
   SYS_futex_time64 read a struct timespec instead, and so does x32, whose SYS_futex reads 64-bit
   fields even though its long is 32 bits. FUTEX_WAIT measures the timeout against
   CLOCK_MONOTONIC. */
#if !defined(SYS_futex) && defined(SYS_futex_time64)
#define SYS_futex SYS_futex_time64
typedef struct timespec pas_futex_timespec;
#elif defined(__x86_64__) && defined(__ILP32__)
typedef struct timespec pas_futex_timespec;
#else
struct pas_futex_timespec {
    long tv_sec;
    long tv_nsec;
};
typedef struct pas_futex_timespec pas_futex_timespec;
#endif

/* Every other error comes from invalid arguments, which waiting again cannot fix. */
static void pas_futex_check_system_wait_result(long result)
{
    PAS_ASSERT(result >= 0 || errno == EAGAIN || errno == EINTR || errno == ETIMEDOUT);
}

void pas_futex_wait(unsigned* address, unsigned expected_value)
{
    pas_futex_check_system_wait_result(
        syscall(SYS_futex, address, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, expected_value, NULL, NULL, 0));
}

static void pas_futex_system_wait_for(unsigned* address, unsigned expected_value, uint64_t relative_timeout_in_nanoseconds)
{
    pas_futex_timespec timeout;

    timeout.tv_sec = (long)(relative_timeout_in_nanoseconds / 1000000000ULL);
    timeout.tv_nsec = (long)(relative_timeout_in_nanoseconds % 1000000000ULL);
    pas_futex_check_system_wait_result(
        syscall(SYS_futex, address, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, expected_value, &timeout, NULL, 0));
}

void pas_futex_wake_one(unsigned* address)
{
    syscall(SYS_futex, address, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, NULL, NULL, 0);
}

void pas_futex_wake_all(unsigned* address)
{
    syscall(SYS_futex, address, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, INT_MAX, NULL, NULL, 0);
}

#elif PAS_OS(FREEBSD)

#if PAS_PLATFORM(PLAYSTATION)
#define PAS_FUTEX_UMTX_OP(address, operation, value, size, timeout) _umtx_op(address, operation, value, size, timeout, 0)
#else
#define PAS_FUTEX_UMTX_OP(address, operation, value, size, timeout) _umtx_op(address, operation, value, size, timeout)
#endif

void pas_futex_wait(unsigned* address, unsigned expected_value)
{
    PAS_FUTEX_UMTX_OP(address, UMTX_OP_WAIT_UINT_PRIVATE, expected_value, NULL, NULL);
}

static void pas_futex_system_wait_for(unsigned* address, unsigned expected_value, uint64_t relative_timeout_in_nanoseconds)
{
    struct _umtx_time timeout;

    /* A bare timespec would be measured against CLOCK_REALTIME. */
    timeout._timeout.tv_sec = (time_t)(relative_timeout_in_nanoseconds / 1000000000ULL);
    timeout._timeout.tv_nsec = (long)(relative_timeout_in_nanoseconds % 1000000000ULL);
    timeout._flags = 0;
    timeout._clockid = CLOCK_MONOTONIC;
    PAS_FUTEX_UMTX_OP(address, UMTX_OP_WAIT_UINT_PRIVATE, expected_value, (void*)(uintptr_t)sizeof(timeout), &timeout);
}

void pas_futex_wake_one(unsigned* address)
{
    PAS_FUTEX_UMTX_OP(address, UMTX_OP_WAKE_PRIVATE, 1, NULL, NULL);
}

void pas_futex_wake_all(unsigned* address)
{
    PAS_FUTEX_UMTX_OP(address, UMTX_OP_WAKE_PRIVATE, INT_MAX, NULL, NULL);
}

#else

/* Systems without a native wait-on-address primitive park waiters on a mutex and condition chosen by
   hashing the address. Waking checks nothing, so it wakes every waiter in the bucket; the spurious
   wakeups this causes for other addresses are allowed. */

#define PAS_FUTEX_NUM_BUCKETS 64

struct pas_futex_bucket {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
};
typedef struct pas_futex_bucket pas_futex_bucket;

static pas_futex_bucket pas_futex_buckets[PAS_FUTEX_NUM_BUCKETS] = {
    [0 ... PAS_FUTEX_NUM_BUCKETS - 1] = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER }
};
static pthread_once_t pas_futex_buckets_once = PTHREAD_ONCE_INIT;
static clockid_t pas_futex_buckets_clock = CLOCK_REALTIME;

/* The conditions are re-created on CLOCK_MONOTONIC where the system allows it, so a wall clock change
   cannot stretch a wait. Every bucket use goes through pthread_once, so none can see a condition
   while it is being re-created. */
static void pas_futex_initialize_buckets(void)
{
    pthread_condattr_t attributes;
    size_t index;

    pthread_condattr_init(&attributes);
    if (!pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC)) {
        for (index = 0; index < PAS_FUTEX_NUM_BUCKETS; ++index) {
            pthread_cond_destroy(&pas_futex_buckets[index].condition);
            pthread_cond_init(&pas_futex_buckets[index].condition, &attributes);
        }
        pas_futex_buckets_clock = CLOCK_MONOTONIC;
    }
    pthread_condattr_destroy(&attributes);
}

static pas_futex_bucket* pas_futex_bucket_for_address(unsigned* address)
{
    pthread_once(&pas_futex_buckets_once, pas_futex_initialize_buckets);
    return pas_futex_buckets + pas_hash_ptr(address) % PAS_FUTEX_NUM_BUCKETS;
}

void pas_futex_wait(unsigned* address, unsigned expected_value)
{
    pas_futex_bucket* bucket;

    bucket = pas_futex_bucket_for_address(address);
    pthread_mutex_lock(&bucket->mutex);
    if (__atomic_load_n(address, __ATOMIC_SEQ_CST) == expected_value)
        pthread_cond_wait(&bucket->condition, &bucket->mutex);
    pthread_mutex_unlock(&bucket->mutex);
}

static void pas_futex_system_wait_for(unsigned* address, unsigned expected_value, uint64_t relative_timeout_in_nanoseconds)
{
    pas_futex_bucket* bucket;
    struct timespec deadline;
    uint64_t nanoseconds;

    bucket = pas_futex_bucket_for_address(address);

    /* pthread_cond_timedwait takes a deadline on the condition's own clock. */
    clock_gettime(pas_futex_buckets_clock, &deadline);
    nanoseconds = (uint64_t)deadline.tv_nsec + relative_timeout_in_nanoseconds % 1000000000ULL;
    deadline.tv_sec += (time_t)(relative_timeout_in_nanoseconds / 1000000000ULL + nanoseconds / 1000000000ULL);
    deadline.tv_nsec = (long)(nanoseconds % 1000000000ULL);

    pthread_mutex_lock(&bucket->mutex);
    if (__atomic_load_n(address, __ATOMIC_SEQ_CST) == expected_value)
        pthread_cond_timedwait(&bucket->condition, &bucket->mutex, &deadline);
    pthread_mutex_unlock(&bucket->mutex);
}

void pas_futex_wake_one(unsigned* address)
{
    pas_futex_wake_all(address);
}

void pas_futex_wake_all(unsigned* address)
{
    pas_futex_bucket* bucket;

    bucket = pas_futex_bucket_for_address(address);
    pthread_mutex_lock(&bucket->mutex);
    pthread_cond_broadcast(&bucket->condition);
    pthread_mutex_unlock(&bucket->mutex);
}

#endif

#endif /* POSIX systems other than Darwin */

/* Whether the deadline passed is decided by the clock rather than by how the kernel wait returned,
   so an early, late, interrupted, or failed wait never reports a timeout the caller's clock disagrees
   with. */
bool pas_futex_wait_until(unsigned* address, unsigned expected_value, uint64_t absolute_deadline_in_nanoseconds)
{
    uint64_t now;
    uint64_t relative_timeout_in_nanoseconds;

    now = pas_futex_get_time_in_nanoseconds();
    if (absolute_deadline_in_nanoseconds <= now)
        return false;

    relative_timeout_in_nanoseconds = absolute_deadline_in_nanoseconds - now;
    if (relative_timeout_in_nanoseconds > PAS_FUTEX_MAX_RELATIVE_TIMEOUT_IN_NANOSECONDS)
        relative_timeout_in_nanoseconds = PAS_FUTEX_MAX_RELATIVE_TIMEOUT_IN_NANOSECONDS;

    pas_futex_system_wait_for(address, expected_value, relative_timeout_in_nanoseconds);

    return pas_futex_get_time_in_nanoseconds() < absolute_deadline_in_nanoseconds;
}
