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

#ifndef TAGGED_BMALLOC_HEAP_UTILS_H
#define TAGGED_BMALLOC_HEAP_UTILS_H

#include "pas_heap_ref.h"

#if PAS_ENABLE_BMALLOC

PAS_BEGIN_EXTERN_C;

PAS_BAPI size_t tagged_bmalloc_heap_ref_get_type_size(pas_heap_ref* heap_ref);
PAS_API pas_heap* tagged_bmalloc_get_heap(void* ptr);
PAS_BAPI size_t tagged_bmalloc_get_allocation_size(void* ptr);

/* Returns true if ptr was allocated from the tagged bmalloc heap, and false if it came
   from the untagged one (or is not a libpas object at all).

   Callers that hold an object but not the mode/size it was allocated with need this,
   because libpas can only find an object in the heap config it is asked about. Only
   meaningful when MTE is enabled; when it isn't, nothing is ever allocated from the
   tagged heap and this is always false. */
PAS_API bool tagged_bmalloc_owns_object(void* ptr);

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* TAGGED_BMALLOC_HEAP_UTILS_H */
