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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_physical_memory_transaction.h"

void pas_physical_memory_transaction_construct(pas_physical_memory_transaction* transaction)
{
    pas_zero_memory(transaction, sizeof(pas_physical_memory_transaction));
}

void pas_physical_memory_transaction_begin(pas_physical_memory_transaction* transaction)
{
    PAS_ASSERT(!transaction->lock_held);
    
    if (!transaction->lock_to_acquire_next_time)
        return;
    
    pas_lock_lock(transaction->lock_to_acquire_next_time);
    transaction->lock_held = transaction->lock_to_acquire_next_time;
    transaction->lock_to_acquire_next_time = NULL;
}

bool pas_physical_memory_transaction_end(pas_physical_memory_transaction* transaction)
{
    if (transaction->lock_held) {
        pas_lock_unlock(transaction->lock_held);
        transaction->lock_held = NULL;
    }
    
    return !transaction->lock_to_acquire_next_time;
}

void pas_physical_memory_transaction_did_fail_to_acquire_lock(
    pas_physical_memory_transaction* transaction,
    pas_lock* lock_ptr)
{
    PAS_ASSERT(lock_ptr);
    PAS_ASSERT(lock_ptr != transaction->lock_held);
    
    if (transaction->lock_to_acquire_next_time)
        return;
    
    transaction->lock_to_acquire_next_time = lock_ptr;
}

#endif /* LIBPAS_ENABLED */
