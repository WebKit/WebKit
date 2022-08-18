/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_FAST_TLS_H
#define PAS_FAST_TLS_H

#include "pas_heap_lock.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_fast_tls;
typedef struct pas_fast_tls pas_fast_tls;

#if PAS_HAVE_PTHREAD_MACHDEP_H

#define PAS_HAVE_THREAD_KEYWORD 0
#define PAS_HAVE_PTHREAD_TLS 0

struct pas_fast_tls {
    bool is_initialized;
};

#define PAS_FAST_TLS_INITIALIZER { .is_initialized = false }

#define PAS_FAST_TLS_CONSTRUCT_IF_NECESSARY(static_key, passed_fast_tls, passed_destructor) do { \
        pas_fast_tls* fast_tls = (passed_fast_tls); \
        void (*the_destructor)(void* arg) = (passed_destructor); \
        pas_heap_lock_assert_held(); \
        if (!fast_tls->is_initialized) { \
            pthread_key_init_np(static_key, the_destructor); \
            fast_tls->is_initialized = true; \
        } \
    } while (false)

#define PAS_FAST_TLS_GET(static_key, fast_tls) \
    _pthread_getspecific_direct(static_key)

#define PAS_FAST_TLS_SET(static_key, fast_tls, value) \
    _pthread_setspecific_direct(static_key, (value))

#else /* PAS_HAVE_PTHREAD_MACHDEP_H -> so !PAS_HAVE_PTHREAD_MACHDEP_H */

struct pas_fast_tls {
    bool is_initialized;
    pthread_key_t key;
};

#define PAS_FAST_TLS_INITIALIZER { .is_initialized = false }

/* This assumes that we will initialize TLS from some thread without racing. That's true, but it's
   not a great assumption. On the other hand, the PAS_HAVE_PTHREAD_MACHDEP_H path makes no such
   assumption.

   The way that the code makes this assumption is that there is no fencing between checking
   is_initialized and using the key. */

#define PAS_FAST_TLS_CONSTRUCT_IF_NECESSARY(static_key, passed_fast_tls, passed_destructor) do { \
        pas_fast_tls* fast_tls = (passed_fast_tls); \
        void (*the_destructor)(void* arg) = (passed_destructor); \
        pas_heap_lock_assert_held(); \
        if (!fast_tls->is_initialized) { \
            pthread_key_create(&fast_tls->key, the_destructor); \
            fast_tls->is_initialized = true; \
        } \
    } while (false)

#if PAS_OS(DARWIN)

#define PAS_HAVE_THREAD_KEYWORD 0
#define PAS_HAVE_PTHREAD_TLS 1

/* __thread keyword implementation does not work since __thread value will be reset to the initial value after it is cleared.
   This broke our pthread exiting detection. We use repeated pthread_setspecific to successfully shutting down. */
#define PAS_FAST_TLS_GET(static_key, passed_fast_tls) ({ \
        pas_fast_tls* fast_tls = (passed_fast_tls); \
        void* result; \
        if (fast_tls->is_initialized) \
            result = pthread_getspecific((fast_tls)->key); \
        else \
            result = NULL; \
        result; \
    })

#define PAS_FAST_TLS_SET(static_key, passed_fast_tls, value) do { \
        pas_fast_tls* fast_tls = (passed_fast_tls); \
        PAS_ASSERT(fast_tls->is_initialized); \
        pthread_setspecific(fast_tls->key, (value)); \
    } while (false)

#else

#define PAS_HAVE_THREAD_KEYWORD 1
#define PAS_HAVE_PTHREAD_TLS 0

/* This is the PAS_HAVE_THREAD_KEYWORD case. Hence, static_key here is not actually a key, but a thread
   local cache of the value (declared with the __thread attribute). Regardless of whether fast_tls has been
   initialized yet or not, it is safe to access this thread local cache of the value. */
#define PAS_FAST_TLS_GET(static_key, fast_tls) static_key

#define PAS_FAST_TLS_SET(static_key, passed_fast_tls, passed_value) do { \
        pas_fast_tls* fast_tls = (passed_fast_tls); \
        PAS_TYPEOF(passed_value) value = (passed_value); \
        PAS_ASSERT(fast_tls->is_initialized); \
        static_key = value; \
        if (((uintptr_t)value) != PAS_THREAD_LOCAL_CACHE_DESTROYED) { \
            /* Using pthread_setspecific to configure callback for thread exit. */ \
            pthread_setspecific(fast_tls->key, value); \
        } \
    } while (false)

#endif

#endif /* PAS_HAVE_PTHREAD_MACHDEP_H -> so end of !PAS_HAVE_PTHREAD_MACHDEP_H */

PAS_END_EXTERN_C;

#endif /* PAS_FAST_TLS_H */

