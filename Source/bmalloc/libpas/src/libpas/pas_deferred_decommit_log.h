/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef PAS_DEFERRED_DECOMMIT_LOG_H
#define PAS_DEFERRED_DECOMMIT_LOG_H

#include "pas_bootstrap_free_heap.h"
#include "pas_range_locked_mode.h"
#include "pas_utils.h"
#include "pas_virtual_range_min_heap.h"

PAS_BEGIN_EXTERN_C;

struct pas_deferred_decommit_log;
struct pas_physical_memory_transaction;
typedef struct pas_deferred_decommit_log pas_deferred_decommit_log;
typedef struct pas_physical_memory_transaction pas_physical_memory_transaction;

struct pas_deferred_decommit_log {
    pas_virtual_range_min_heap impl;
    size_t total; /* This is accurate so long as the ranges are non-overlapping. */
    size_t common_lock_hold_count;
    pas_lock** locks_already_held;
    size_t num_locks_already_held;
    pas_physical_memory_transaction* transaction;
};

/* Constructing one of these with a transaction implies:
   
   - You are passing NULL for locks_already_held because you'll hold no other locks other
     than what that transaction would do.
     
     This is just a simplification. If we needed to also hold other locks, we could make that
     work. It's just that nobody would use this right now and it's more code due to how things
     are structured.
   
   - You want the transaction to know if this fails lock acquisition, and then you want the
     transaction to loop you around. You may not always want that.
   
   The incremental decommit logic currently passes NULL transaction because it has no
   intention to retry the allocation just because decommit failed. That code may also hold
   some commit locks. The scavenger passes non-NULL transaction because it has to succeed at
   decommitting. It holds no other locks. */
PAS_API void pas_deferred_decommit_log_construct(pas_deferred_decommit_log* log,
                                                 pas_lock** locks_already_held,
                                                 size_t num_locks_already_held,
                                                 pas_physical_memory_transaction* transaction);

PAS_API void pas_deferred_decommit_log_destruct(pas_deferred_decommit_log* log,
                                                pas_lock_hold_mode heap_lock_hold_mode);

PAS_API bool pas_deferred_decommit_log_lock_for_adding(pas_deferred_decommit_log* log,
                                                       pas_lock* lock_ptr,
                                                       pas_lock_hold_mode heap_lock_hold_mode);

/* This attempts to lock the range. Returns true if the range was locked and added. Returns
   false if it wasn't. If the heap lock is not held, the heap is empty, and there are no
   previously held locks, it will try to acquire the range lock in a blocking way. Otherwise
   it will be a try_lock, so it might fail, causing this to return false. */
PAS_API bool pas_deferred_decommit_log_add(pas_deferred_decommit_log* log,
                                           pas_virtual_range range,
                                           pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_deferred_decommit_log_add_already_locked(pas_deferred_decommit_log* log,
                                                          pas_virtual_range range,
                                                          pas_lock_hold_mode heap_lock_hold_mode);

PAS_API bool pas_deferred_decommit_log_add_maybe_locked(pas_deferred_decommit_log* log,
                                                        pas_virtual_range range,
                                                        pas_range_locked_mode range_locked_mode,
                                                        pas_lock_hold_mode heap_lock_hold_mode);

PAS_API void pas_deferred_decommit_log_unlock_after_aborted_add(pas_deferred_decommit_log* log,
                                                                pas_lock* lock_ptr);

PAS_API void pas_deferred_decommit_log_decommit_all(pas_deferred_decommit_log* log);

/* Useful for writing tests. */
PAS_API void pas_deferred_decommit_log_pretend_to_decommit_all(pas_deferred_decommit_log* log);

PAS_END_EXTERN_C;

#endif /* PAS_DEFERRED_DECOMMIT_LOG_H */

