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

#ifndef PAS_LOW_MEMORY_MODE_H
#define PAS_LOW_MEMORY_MODE_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* When true, libpas trades some performance for a smaller initial footprint:
   - skips the eager zero-init of pas_utility_heap_allocators (~50 KB of pages
     stay clean until each size class is actually used).

   The embedder sets this before performing the first libpas allocation. JSC
   sets it from StructureAlignedMemoryAllocator init when bmalloc has been
   routed through system malloc (e.g. Malloc=X) or when VM mini mode is on,
   both of which are signals that "memory matters right now". */
PAS_API extern bool pas_low_memory_mode;

PAS_END_EXTERN_C;

#endif /* PAS_LOW_MEMORY_MODE_H */
