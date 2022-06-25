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

#ifndef HOTBIT_HEAP_INNARDS_H
#define HOTBIT_HEAP_INNARDS_H

#include "pas_config.h"
#include "pas_allocator_counts.h"
#include "pas_dynamic_primitive_heap_map.h"
#include "pas_intrinsic_heap_support.h"
#include "pas_heap_ref.h"

#if PAS_ENABLE_HOTBIT

PAS_BEGIN_EXTERN_C;

PAS_API extern pas_heap hotbit_common_primitive_heap;
PAS_API extern pas_intrinsic_heap_support hotbit_common_primitive_heap_support;
PAS_API extern pas_allocator_counts hotbit_allocator_counts;

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_HOTBIT */

#endif /* HOTBIT_HEAP_INNARDS_h */

