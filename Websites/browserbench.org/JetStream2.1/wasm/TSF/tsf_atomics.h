/*
 * Copyright (C) 2014, 2015 Filip Pizlo. All rights reserved.
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

#ifndef FP_TSF_ATOMICS_H
#define FP_TSF_ATOMICS_H

#include "tsf_config.h"
#include "tsf_types.h"
#include <stdlib.h>

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#ifdef TSF_HAVE_PTHREAD
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if 0
/* hack to deconfuse emacs */
}
#endif

struct tsf_mutex;
struct tsf_atomic_count;
struct tsf_once;

typedef struct tsf_mutex tsf_mutex_t;
typedef struct tsf_atomic_count tsf_atomic_count_t;
typedef struct tsf_once tsf_once_t;

#ifdef TSF_HAVE_BUILTIN_CAS
static TSF_inline tsf_bool_t tsf_cas32(
    volatile void *pointer, int32_t expected, int32_t new_value) {
    return __sync_bool_compare_and_swap(
        (volatile int32_t*)pointer, expected, new_value);
}

static TSF_inline tsf_bool_t tsf_cas_ptr(
    volatile void *pointer, intptr_t expected, intptr_t new_value) {
    return __sync_bool_compare_and_swap(
        (volatile intptr_t*)pointer, expected, new_value);
}
#define TSF_HAVE_CAS 1
#elif !defined(TSF_HAVE_PTHREAD)
static TSF_inline tsf_bool_t tsf_cas32(
    volatile void *rawPointer, int32_t expected, int32_t new_value) {
    volatile int32_t *pointer = (volatile int32_t*)rawPointer;
    if (*pointer == expected) {
        *pointer = new_value;
        return tsf_true;
    } else {
        return tsf_false;
    }
}

static TSF_inline tsf_bool_t tsf_cas_ptr(
    volatile void *rawPointer, intptr_t expected, intptr_t new_value) {
    volatile intptr_t *pointer = (volatile intptr_t*)rawPointer;
    if (*pointer == expected) {
        *pointer = new_value;
        return tsf_true;
    } else {
        return tsf_false;
    }
}
#define TSF_HAVE_CAS 1
#endif

#ifdef TSF_HAVE_BUILTIN_XADD
static TSF_inline int32_t tsf_xadd32(volatile void *pointer, int32_t value) {
    return __sync_fetch_and_add((volatile int32_t*)pointer, value);
}
#define TSF_HAVE_XADD 1
#elif defined(TSF_HAVE_CAS)
static TSF_inline int32_t tsf_xadd32(volatile void *rawPointer, int32_t value) {
    int32_t result;
    volatile int32_t *pointer = (volatile int32_t*)rawPointer;
    do {
        result = *pointer;
    } while (!tsf_cas32(pointer, result, result + value));
    return result;
}
#define TSF_HAVE_XADD 1
#endif

#ifdef TSF_HAVE_X86_INLINE_ASM_FENCES
/* This is a fence that would have no hardware effect on TSO architectures. Placing
 * it between two operations gives you guarantees like those that TSO would have
 * given you. */
static TSF_inline void tsf_basic_fence(void) {
    asm volatile ("" : : : "memory");
}

/* This is a fence that you would need even on TSO and guarantees memory SC. */
static TSF_inline void tsf_strong_fence(void) {
    asm volatile ("mfence" : : : "memory");
}

#define TSF_HAVE_FENCES 1
#elif !defined(TSF_HAVE_PTHREAD)
static TSF_inline void tsf_basic_fence(void) { }
static TSF_inline void tsf_strong_fence(void) { }
#define TSF_HAVE_FENCES 1
#endif

struct tsf_mutex {
#ifdef TSF_HAVE_PTHREAD
    pthread_mutex_t mutex;
#else
    char dummy;
#endif
};
    
struct tsf_atomic_count {
#if !defined(TSF_HAVE_XADD)
    tsf_mutex_t mutex;
#endif
    volatile uint32_t count;
};

static inline void tsf_yield(void)
{
#ifdef HAVE_SCHED_YIELD
    sched_yield();
#endif
}

static inline void* tsf_thread_self(void)
{
#if TSF_HAVE_PTHREAD
    return (void*)pthread_self();
#else
    return NULL;
#endif
}

/* Note that we assume that locking operations always succeed. There is little point in worrying
   about what happens if they fail. */
static inline void tsf_mutex_init(tsf_mutex_t *mutex) {
#ifdef TSF_HAVE_PTHREAD
    pthread_mutex_init(&mutex->mutex, NULL);
#endif
}

static inline void tsf_mutex_lock(tsf_mutex_t *mutex) {
#ifdef TSF_HAVE_PTHREAD
    pthread_mutex_lock(&mutex->mutex);
#endif
}

static inline void tsf_mutex_unlock(tsf_mutex_t *mutex) {
#ifdef TSF_HAVE_PTHREAD
    pthread_mutex_unlock(&mutex->mutex);
#endif
}

static inline void tsf_mutex_destroy(tsf_mutex_t *mutex) {
#ifdef TSF_HAVE_PTHREAD
    pthread_mutex_destroy(&mutex->mutex);
#endif
}

static inline void tsf_atomic_count_init(tsf_atomic_count_t *count,
                                         uint32_t initial_value) {
    count->count = initial_value;
#if !defined(TSF_HAVE_XADD)
    tsf_mutex_init(&count->mutex);
#endif
}

static inline void tsf_atomic_count_destroy(tsf_atomic_count_t *count) {
#if !defined(TSF_HAVE_XADD)
    tsf_mutex_destroy(&count->mutex);
#endif
}

static inline uint32_t tsf_atomic_count_value(tsf_atomic_count_t *count) {
    return count->count;
}

static inline void tsf_atomic_count_set_value(tsf_atomic_count_t *count,
                                              uint32_t value) {
    count->count = value;
}

static inline uint32_t tsf_atomic_count_xadd(tsf_atomic_count_t *count,
                                             uint32_t delta) {
#ifdef TSF_HAVE_XADD
    return tsf_xadd32(&count->count, delta);
#else
    uint32_t result;
    tsf_mutex_lock(&count->mutex);
    result = count->count;
    count->count += delta;
    tsf_mutex_unlock(&count->mutex);
    return result;
#endif
}

static inline uint32_t tsf_atomic_count_xsub(tsf_atomic_count_t *count,
                                             uint32_t delta) {
    return tsf_atomic_count_xadd(count, (uint32_t)-(int32_t)delta);
}

struct tsf_once {
#ifdef TSF_HAVE_PTHREAD
#if defined(TSF_HAVE_BUILTIN_CAS) && defined(TSF_HAVE_FENCES)
    volatile int32_t state;
#else
    pthread_once_t once_control;
#endif
#else
    tsf_bool_t is_initialized;
#endif
};

#ifdef TSF_HAVE_PTHREAD
#if defined(TSF_HAVE_BUILTIN_CAS) && defined(TSF_HAVE_FENCES)
#define TSF_ONCE_INIT { 0 }
#else
#define TSF_ONCE_INIT { PTHREAD_ONCE_INIT }
#endif
#else
#define TSF_ONCE_INIT { tsf_false }
#endif

static inline void tsf_once(tsf_once_t *control, void (*action)(void)) {
#ifdef TSF_HAVE_PTHREAD
#if defined(TSF_HAVE_BUILTIN_CAS) && defined(TSF_HAVE_FENCES)
    /* Define the possible values of control->state. */
    static const int32_t notInitialized = 0;
    static const int32_t initializing = 1;
    static const int32_t initialized = 2;
    
    for (;;) {
        int32_t current_state = control->state;
        if (current_state == initialized) {
            tsf_basic_fence();
            break;
        } else if (current_state == initializing) {
            tsf_yield();
        } else if (current_state == notInitialized) {
            if (tsf_cas32(&control->state, notInitialized, initializing)) {
                action();
                tsf_basic_fence();
                control->state = initialized;
                break;
            } else {
                tsf_yield();
            }
        } else {
            /* should never get here. */
            abort();
        }
    }
#else
    pthread_once(&control->once_control, action);
#endif
#else
    if (!control->is_initialized) {
        action();
        control->is_initialized = tsf_true;
    }
#endif
}

#if defined(TSF_HAVE_FENCES)
#define TSF_HAVE_INIT_POINTER 1

static inline void *tsf_init_pointer(void **pointer, void *(*initializer)(void)) {
    void *result = *pointer;
    if (result) {
        tsf_basic_fence();
    } else {
        result = initializer();
        tsf_basic_fence();
        *pointer = result;
    }
    return result;
}
#else
#undef TSF_HAVE_INIT_POINTER
#endif

#ifdef __cplusplus
}
#endif

#endif // FP_TSF_ATOMICS_H
