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

#ifndef PAS_PTR_WORKLIST_H
#define PAS_PTR_WORKLIST_H

#include "pas_ptr_hash_set.h"

PAS_BEGIN_EXTERN_C;

struct pas_ptr_worklist;
typedef struct pas_ptr_worklist pas_ptr_worklist;

struct pas_ptr_worklist {
    pas_ptr_hash_set seen;
    void** worklist;
    size_t worklist_size;
    size_t worklist_capacity;
};

PAS_API void pas_ptr_worklist_construct(pas_ptr_worklist* worklist);

PAS_API void pas_ptr_worklist_destruct(pas_ptr_worklist* worklist,
                                       const pas_allocation_config* allocation_config);

/* Returns true if this is a new worklist entry. Always returns false if you try to push NULL. */
PAS_API bool pas_ptr_worklist_push(pas_ptr_worklist* worklist,
                                   void* ptr,
                                   const pas_allocation_config* allocation_config);

PAS_API void* pas_ptr_worklist_pop(pas_ptr_worklist* worklist);

PAS_END_EXTERN_C;

#endif /* PAS_PTR_WORKLIST_H */

