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

#ifndef PAS_PHYSICAL_MEMORY_TRANSACTION_H
#define PAS_PHYSICAL_MEMORY_TRANSACTION_H

#include "pas_lock.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_physical_memory_transaction;
typedef struct pas_physical_memory_transaction pas_physical_memory_transaction;

/* How to use this: if you are calling API that needs to hold the heap lock and perform commits
   then you need to make sure you wrap your heap lock acquisition with a commit transaction like
   so:
   
   pas_physical_memory_transaction physical_memory_transaction;
   pas_physical_memory_transaction_construct(&physical_memory_transaction);
   do {
       pas_physical_memory_transaction_begin(&physical_memory_transaction);
       pas_heap_lock_lock();
       
       do things
       
       pas_heap_lock_unlock();
   } while (!pas_physical_memory_transaction_end(&physical_memory_transaction));
   
   This ensures that if the things you want to do need to acquire a commit lock, then they can
   arrange for that commit lock to be contended for while the heap lock is not held.

   One of the properties that we get from this is that it's always safe to acquire the heap lock
   if it is not already held. */

struct pas_physical_memory_transaction {
    pas_lock* lock_to_acquire_next_time;
    pas_lock* lock_held;
};

PAS_API void pas_physical_memory_transaction_construct(pas_physical_memory_transaction* transaction);

PAS_API void pas_physical_memory_transaction_begin(pas_physical_memory_transaction* transaction);

PAS_API bool pas_physical_memory_transaction_end(pas_physical_memory_transaction* transaction);

PAS_API void pas_physical_memory_transaction_did_fail_to_acquire_lock(
    pas_physical_memory_transaction* transaction,
    pas_lock* lock_ptr);

PAS_END_EXTERN_C;

#endif /* PAS_PHYSICAL_MEMORY_TRANSACTION_H */

