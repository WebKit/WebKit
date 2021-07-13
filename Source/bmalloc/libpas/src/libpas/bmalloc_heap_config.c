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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "bmalloc_heap_config.h"

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap_innards.h"
#include "pas_designated_intrinsic_heap.h"
#include "pas_heap_config_utils_inlines.h"
#include "pas_root.h"

pas_heap_config bmalloc_heap_config = BMALLOC_HEAP_CONFIG;

PAS_BASIC_HEAP_CONFIG_DEFINITIONS(
    bmalloc, BMALLOC,
    .allocate_page_should_zero = false);

void bmalloc_heap_config_activate(void)
{
    // FIXME: Temporarily disable it for now until bmalloc is replaced with libpas.
    static const bool register_with_libmalloc = false;
    
    pas_designated_intrinsic_heap_initialize(&bmalloc_common_primitive_heap.segregated_heap,
                                             &bmalloc_heap_config);

    if (register_with_libmalloc)
        pas_root_ensure_for_libmalloc_enumeration();
}

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
