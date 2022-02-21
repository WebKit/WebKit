/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_HEAP_PAGE_PROVIDER_H
#define PAS_HEAP_PAGE_PROVIDER_H

#include "pas_alignment.h"
#include "pas_allocation_result.h"
#include "pas_heap_ref.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_physical_memory_transaction;
typedef struct pas_physical_memory_transaction pas_physical_memory_transaction;

/* Heap and transaction can be NULL, in which case, assume the worst. Most implementations of this
   function never need the transaction or the heap. */
typedef pas_allocation_result (*pas_heap_page_provider)(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_heap* heap, /* Can be NULL. */
    pas_physical_memory_transaction* transaction, /* Can be NULL. */
    void *arg);

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_PAGE_PROVIDER_H */

