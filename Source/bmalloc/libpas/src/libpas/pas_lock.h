/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LOCK_H
#define PAS_LOCK_H

#include "pas_log.h"
#include "pas_race_test_hooks.h"
#include "pas_utils.h"
#include "pas_thread.h"
#include "pas_zero_memory.h"

PAS_BEGIN_EXTERN_C;

enum pas_lock_hold_mode {
    pas_lock_is_not_held,
    pas_lock_is_held
};

typedef enum pas_lock_hold_mode pas_lock_hold_mode;

enum pas_lock_lock_mode {
    pas_lock_lock_mode_try_lock,
    pas_lock_lock_mode_lock,
};

typedef enum pas_lock_lock_mode pas_lock_lock_mode;

#define PAS_LOCK_VERBOSE 0

PAS_END_EXTERN_C;

#if PAS_OS(DARWIN)

#if defined(__has_include) && __has_include(<os/lock_private.h>) && (defined(LIBPAS) || defined(PAS_BMALLOC))
#define PAS_USE_ULOCK_SPI 1
#define PAS_USE_ULOCK_FLAGS_API 0
#if !defined(OS_UNFAIR_LOCK_INLINE) || !OS_UNFAIR_LOCK_INLINE
#pragma message "OS_UNFAIR_LOCK_INLINE needs to be enabled."
#endif
#include <os/lock_private.h>

#else
#define PAS_USE_ULOCK_SPI 0
#include <os/lock.h>

#define OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION 0x00010000
#define OS_UNFAIR_LOCK_ADAPTIVE_SPIN 0x00040000

#if (PAS_PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000) \
    || (PAS_PLATFORM(MACCATALYST) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 180000) \
    || (PAS_PLATFORM(IOS) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 180000) \
    || (PAS_PLATFORM(APPLETV) && __TV_OS_VERSION_MAX_ALLOWED >= 180000) \
    || (PAS_PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MAX_ALLOWED >= 110000) \
    || (PAS_PLATFORM(VISION) && __VISION_OS_VERSION_MAX_ALLOWED >= 20000)
/* After this version, OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION etc. are public API */
#define PAS_USE_ULOCK_FLAGS_API 1
#else
#define PAS_USE_ULOCK_FLAGS_API 0
#endif

#define os_unfair_lock_lock_with_options_inline os_unfair_lock_lock_with_options
#define os_unfair_lock_trylock_inline os_unfair_lock_trylock
#define os_unfair_lock_unlock_inline os_unfair_lock_unlock
#endif

PAS_BEGIN_EXTERN_C;

struct pas_lock;
typedef struct pas_lock pas_lock;

struct pas_lock {
    os_unfair_lock lock;
};

#define PAS_LOCK_INITIALIZER ((pas_lock){ .lock = OS_UNFAIR_LOCK_INIT })

static inline void pas_lock_construct(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
}

static inline void pas_lock_construct_disabled(pas_lock* lock)
{
    pas_zero_memory(lock, sizeof(pas_lock));
}

static inline void pas_lock_lock(pas_lock* lock)
{
    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Locking lock %p\n", pthread_self(), lock);
    pas_race_test_will_lock(lock);
#if PAS_USE_ULOCK_SPI
    const os_unfair_lock_options_t options = (os_unfair_lock_options_t)(OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN);
#if defined(OS_UNFAIR_LOCK_INLINE) && OS_UNFAIR_LOCK_INLINE
    os_unfair_lock_lock_with_options_inline(&lock->lock, options);
#else
    PAS_UNUSED_PARAM(options);
    PAS_ASSERT_IF(true, !"Should not be reached");
#endif // defined(OS_UNFAIR_LOCK_INLINE) && OS_UNFAIR_LOCK_INLINE
#elif PAS_USE_ULOCK_FLAGS_API
    const os_unfair_lock_flags_t options = (os_unfair_lock_flags_t)(OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN);
    os_unfair_lock_lock_with_flags(&lock->lock, options);
#else
    os_unfair_lock_lock(&lock->lock);
#endif
    pas_race_test_did_lock(lock);
}

static inline bool pas_lock_try_lock(pas_lock* lock)
{
    bool result;
    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Trylocking lock %p\n", pthread_self(), lock);
#if PAS_USE_ULOCK_SPI && (!defined(OS_UNFAIR_LOCK_INLINE) || !OS_UNFAIR_LOCK_INLINE)
    result = false;
    PAS_ASSERT_IF(true, !"Should not be reached");
#else
    result = os_unfair_lock_trylock_inline(&lock->lock);
#endif
    if (result)
        pas_race_test_did_try_lock(lock);
    return result;
}

static inline void pas_lock_unlock(pas_lock* lock)
{
    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Unlocking lock %p\n", pthread_self(), lock);
    pas_race_test_will_unlock(lock);
#if PAS_USE_ULOCK_SPI && (!defined(OS_UNFAIR_LOCK_INLINE) || !OS_UNFAIR_LOCK_INLINE)
    PAS_ASSERT_IF(true, !"Should not be reached");
#else
    os_unfair_lock_unlock_inline(&lock->lock);
#endif
}

static inline void pas_lock_assert_held(pas_lock* lock)
{
    os_unfair_lock_assert_owner(&lock->lock);
}

static inline void pas_lock_testing_assert_held(pas_lock* lock)
{
    if (PAS_ENABLE_TESTING)
        os_unfair_lock_assert_owner(&lock->lock);
}

PAS_END_EXTERN_C;

#elif PAS_PLATFORM(PLAYSTATION)

#include <errno.h>
#include <pthread_np.h>

PAS_BEGIN_EXTERN_C;

struct pas_lock;
typedef struct pas_lock pas_lock;

struct pas_lock {
    pthread_mutex_t mutex;
};

#define PAS_LOCK_INITIALIZER ((pas_lock){ .mutex = PTHREAD_MUTEX_INITIALIZER })

static inline void pas_lock_construct(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
}

static inline void pas_lock_construct_disabled(pas_lock* lock)
{
    pthread_mutex_destroy(&lock->mutex);
    pas_zero_memory(lock, sizeof(pas_lock));
}

static inline void pas_lock_mutex_setname(pas_lock* lock)
{
    pthread_mutex_setname_np(&lock->mutex, "SceNKLibpas");
}

static inline void pas_lock_lock(pas_lock* lock)
{
    bool unnamed;
    unnamed = (lock->mutex == PTHREAD_MUTEX_INITIALIZER);
    pas_race_test_will_lock(lock);
    pthread_mutex_lock(&lock->mutex);
    pas_race_test_did_lock(lock);
    if (PAS_UNLIKELY(unnamed))
        pas_lock_mutex_setname(lock);
}

static inline bool pas_lock_try_lock(pas_lock* lock)
{
    int error;
    bool unnamed;
    unnamed = (lock->mutex == PTHREAD_MUTEX_INITIALIZER);
    error = pthread_mutex_trylock(&lock->mutex);
    PAS_ASSERT(!error || error == EBUSY);
    if (!error) {
        pas_race_test_did_try_lock(lock);
        if (PAS_UNLIKELY(unnamed))
            pas_lock_mutex_setname(lock);
    }
    return !error;
}

static inline void pas_lock_unlock(pas_lock* lock)
{
    pas_race_test_will_unlock(lock);
    pthread_mutex_unlock(&lock->mutex);
}

static inline bool pas_lock_test_held(pas_lock* lock)
{
    if (pthread_mutex_trylock(&lock->mutex))
        return true;
    pthread_mutex_unlock(&lock->mutex);
    return false;
}

static inline void pas_lock_assert_held(pas_lock* lock)
{
    PAS_ASSERT(pas_lock_test_held(lock));
}

static inline void pas_lock_testing_assert_held(pas_lock* lock)
{
    PAS_TESTING_ASSERT(pas_lock_test_held(lock));
}

PAS_END_EXTERN_C;

#elif PAS_OS(LINUX) || PAS_OS(WINDOWS) || PAS_OS(FREEBSD)

PAS_BEGIN_EXTERN_C;

struct pas_lock;
typedef struct pas_lock pas_lock;

struct pas_lock {
    unsigned futex;
};

#define PAS_LOCK_INITIALIZER ((pas_lock){ .futex = 0 })

static inline void pas_lock_construct(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
}

static inline void pas_lock_construct_disabled(pas_lock* lock)
{
    pas_zero_memory(lock, sizeof(pas_lock));
}

PAS_API PAS_NEVER_INLINE void pas_lock_lock_slow(pas_lock* lock, unsigned expected);
PAS_API PAS_NEVER_INLINE void pas_lock_unlock_slow(pas_lock* lock);

static inline void pas_lock_lock(pas_lock* lock)
{
    unsigned expected = 0;

    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Locking lock %p\n", (void*)pthread_self(), lock);

    pas_race_test_will_lock(lock);

    /* Fast path: Try to acquire 0 -> 1 */
    if (PAS_UNLIKELY(!__atomic_compare_exchange_n(&lock->futex, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)))
        pas_lock_lock_slow(lock, expected);

    pas_race_test_did_lock(lock);
}

static inline bool pas_lock_try_lock(pas_lock* lock)
{
    unsigned expected = 0;
    bool result;

    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Trylocking lock %p\n", (void*)pthread_self(), lock);

    result = __atomic_compare_exchange_n(&lock->futex, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);

    if (result)
        pas_race_test_did_try_lock(lock);

    return result;
}

static inline void pas_lock_unlock(pas_lock* lock)
{
    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Unlocking lock %p\n", (void*)pthread_self(), lock);

    pas_race_test_will_unlock(lock);

    /* If we had no waiters (state was 1), just unlock to 0 */
    if (PAS_UNLIKELY(__atomic_fetch_sub(&lock->futex, 1, __ATOMIC_RELEASE) != 1))
        pas_lock_unlock_slow(lock);
}

static inline void pas_lock_assert_held(pas_lock* lock)
{
    PAS_ASSERT(__atomic_load_n(&lock->futex, __ATOMIC_RELAXED) != 0);
}

static inline void pas_lock_testing_assert_held(pas_lock* lock)
{
    if (PAS_ENABLE_TESTING)
        PAS_TESTING_ASSERT(__atomic_load_n(&lock->futex, __ATOMIC_RELAXED) != 0);
}

PAS_END_EXTERN_C;

#else
#error "No pas_lock implementation found"
#endif

PAS_BEGIN_EXTERN_C;

#define PAS_DECLARE_LOCK(name) \
    PAS_API extern pas_lock name##_lock; \
    \
    PAS_UNUSED static inline void name##_lock_lock(void) \
    { \
        pas_lock_lock(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline bool name##_lock_try_lock(void) \
    { \
        return pas_lock_try_lock(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline bool name##_lock_lock_with_mode(pas_lock_lock_mode mode) \
    { \
        return pas_lock_lock_with_mode(&name##_lock, mode); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_unlock(void) \
    { \
        pas_lock_unlock(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_assert_held(void) \
    { \
        pas_lock_assert_held(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_testing_assert_held(void) \
    { \
        pas_lock_testing_assert_held(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_lock_conditionally(pas_lock_hold_mode hold_mode) \
    { \
        if (hold_mode == pas_lock_is_not_held) \
            name##_lock_lock(); \
        pas_compiler_fence(); \
    } \
    \
    PAS_UNUSED static inline bool name##_lock_try_lock_conditionally(pas_lock_hold_mode hold_mode) \
    { \
        if (hold_mode == pas_lock_is_not_held) { \
            if (!name##_lock_try_lock()) \
                return false; \
        } \
        pas_compiler_fence(); \
        return true; \
    } \
    \
    PAS_UNUSED static inline void name##_lock_lock_dependently(unsigned token) \
    { \
        pas_lock_lock(&name##_lock + token); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_unlock_conditionally(pas_lock_hold_mode hold_mode) \
    { \
        pas_compiler_fence(); \
        if (hold_mode == pas_lock_is_not_held) \
            name##_lock_unlock(); \
    } \
    struct pas_dummy

#define PAS_DEFINE_LOCK(name) \
    pas_lock name##_lock = PAS_LOCK_INITIALIZER; \
    struct pas_dummy

static inline bool pas_lock_lock_with_mode(pas_lock* lock,
                                           pas_lock_lock_mode mode)
{
    switch (mode) {
    case pas_lock_lock_mode_try_lock:
        return pas_lock_try_lock(lock);
    case pas_lock_lock_mode_lock:
        pas_lock_lock(lock);
        return true;
    }
    PAS_ASSERT_NOT_REACHED();
    return false;
}

static PAS_ALWAYS_INLINE bool pas_lock_switch_with_mode(pas_lock** held_lock,
                                                        pas_lock* lock,
                                                        pas_lock_lock_mode mode)
{
    bool result;
    pas_compiler_fence();
    if (PAS_LIKELY(lock == *held_lock))
        return true;
    if (*held_lock)
        pas_lock_unlock(*held_lock);
    if (lock)
        result = pas_lock_lock_with_mode(lock, mode);
    else
        result = true;
    if (result)
        *held_lock = lock;
    else
        *held_lock = NULL;
    pas_compiler_fence();
    return result;
}

static PAS_ALWAYS_INLINE void pas_lock_switch(pas_lock** held_lock, pas_lock* lock)
{
    bool result;
    result = pas_lock_switch_with_mode(held_lock, lock, pas_lock_lock_mode_lock);
    PAS_ASSERT(result);
}

static inline void pas_lock_lock_conditionally(pas_lock* lock,
                                               pas_lock_hold_mode hold_mode)
{
    if (hold_mode == pas_lock_is_not_held)
        pas_lock_lock(lock);
    pas_compiler_fence();
}

static inline void pas_lock_unlock_conditionally(pas_lock* lock,
                                                 pas_lock_hold_mode hold_mode)
{
    pas_compiler_fence();
    if (hold_mode == pas_lock_is_not_held)
        pas_lock_unlock(lock);
}

static PAS_ALWAYS_INLINE pas_lock* pas_lock_for_switch_conditionally(pas_lock* lock,
                                                                     pas_lock_hold_mode hold_mode)
{
    switch (hold_mode) {
    case pas_lock_is_held:
        return NULL;
    case pas_lock_is_not_held:
        return lock;
    }
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

static PAS_ALWAYS_INLINE void pas_lock_switch_conditionally(pas_lock** held_lock,
                                                            pas_lock* lock,
                                                            pas_lock_hold_mode hold_mode)
{
    pas_lock_switch(held_lock, pas_lock_for_switch_conditionally(lock, hold_mode));
}

PAS_END_EXTERN_C;

#endif /* PAS_LOCK_H */


