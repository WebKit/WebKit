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

#ifndef PAS_LARGE_UTILITY_FREE_HEAP_H
#define PAS_LARGE_UTILITY_FREE_HEAP_H

#include "pas_allocation_config.h"
#include "pas_allocation_kind.h"
#include "pas_fast_large_free_heap.h"
#include "pas_heap_summary.h"

PAS_BEGIN_EXTERN_C;

#define PAS_LARGE_FREE_HEAP_NAME pas_large_utility_free_heap
#define PAS_LARGE_FREE_HEAP_ID(suffix) pas_large_utility_free_heap ## suffix
#include "pas_large_free_heap_declarations.def"
#undef PAS_LARGE_FREE_HEAP_NAME
#undef PAS_LARGE_FREE_HEAP_ID

PAS_END_EXTERN_C;

#endif /* PAS_LARGE_UTILITY_FREE_HEAP_H */

