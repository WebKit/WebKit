/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef PAS_RACE_TEST_HOOKS_H
#define PAS_RACE_TEST_HOOKS_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

enum pas_race_test_hook_kind {
    pas_race_test_hook_local_allocator_stop_before_did_stop_allocating,
    pas_race_test_hook_local_allocator_stop_before_unlock
};

typedef enum pas_race_test_hook_kind pas_race_test_hook_kind;

static inline const char* pas_race_test_hook_kind_get_string(pas_race_test_hook_kind kind)
{
    switch (kind) {
    case pas_race_test_hook_local_allocator_stop_before_did_stop_allocating:
        return "local_allocator_stop_before_did_stop_allocating";
    case pas_race_test_hook_local_allocator_stop_before_unlock:
        return "local_allocator_stop_before_unlock";
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

struct pas_lock;
typedef struct pas_lock pas_lock;

#if PAS_ENABLE_TESTING
typedef void (*pas_race_test_hook_callback)(pas_race_test_hook_kind kind);
typedef void (*pas_race_test_lock_callback)(pas_lock* lock);

PAS_API extern pas_race_test_hook_callback pas_race_test_hook_callback_instance;
PAS_API extern pas_race_test_lock_callback pas_race_test_will_lock_callback;
PAS_API extern pas_race_test_lock_callback pas_race_test_did_lock_callback;
PAS_API extern pas_race_test_lock_callback pas_race_test_did_try_lock_callback;
PAS_API extern pas_race_test_lock_callback pas_race_test_will_unlock_callback;

static inline void pas_race_test_hook(pas_race_test_hook_kind kind)
{
    pas_race_test_hook_callback callback;
    callback = pas_race_test_hook_callback_instance;
    if (callback)
        callback(kind);
}

static inline void pas_race_test_will_lock(pas_lock* lock)
{
    pas_race_test_lock_callback callback;
    callback = pas_race_test_will_lock_callback;
    if (callback)
        callback(lock);
}

static inline void pas_race_test_did_lock(pas_lock* lock)
{
    pas_race_test_lock_callback callback;
    callback = pas_race_test_did_lock_callback;
    if (callback)
        callback(lock);
}

static inline void pas_race_test_did_try_lock(pas_lock* lock)
{
    pas_race_test_lock_callback callback;
    callback = pas_race_test_did_try_lock_callback;
    if (callback)
        callback(lock);
}

static inline void pas_race_test_will_unlock(pas_lock* lock)
{
    pas_race_test_lock_callback callback;
    callback = pas_race_test_will_unlock_callback;
    if (callback)
        callback(lock);
}
#else /* PAS_ENABLE_TESTING -> so !PAS_ENABLE_TESTING */
static inline void pas_race_test_hook(pas_race_test_hook_kind kind) { PAS_UNUSED_PARAM(kind); }
static inline void pas_race_test_will_lock(pas_lock* lock) { PAS_UNUSED_PARAM(lock); }
static inline void pas_race_test_did_lock(pas_lock* lock) { PAS_UNUSED_PARAM(lock); }
static inline void pas_race_test_did_try_lock(pas_lock* lock) { PAS_UNUSED_PARAM(lock); }
static inline void pas_race_test_will_unlock(pas_lock* lock) { PAS_UNUSED_PARAM(lock); }
#endif /* PAS_ENABLE_TESTING -> so end of !PAS_ENABLE_TESTING */

PAS_END_EXTERN_C;

#endif /* PAS_RACE_TEST_HOOKS_H */

