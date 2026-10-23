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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "tagged_bmalloc_heap_config.h"

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap_config.h"
#include "tagged_bmalloc_heap_innards.h"
#include "pas_designated_intrinsic_heap.h"
#include "pas_heap_config_utils_inlines.h"
#include "pas_root.h"

PAS_BEGIN_EXTERN_C;

const pas_heap_config tagged_bmalloc_heap_config = TAGGED_BMALLOC_HEAP_CONFIG;

#define TAGGED_BMALLOC_CHECK_SMALL_SEGREGATED_MAX PAS_MAX_OBJECT_SIZE_WITH_MIN_OBJECTS( \
    PAS_BASIC_SEGREGATED_PAYLOAD_SIZE_EXCLUSIVE(TAGGED_BMALLOC_MINALIGN_SHIFT, PAS_SMALL_PAGE_DEFAULT_SIZE, PAS_SMALL_PAGE_DEFAULT_SIZE), \
    TAGGED_BMALLOC_PINNED_MIN_OBJECTS_PER_PAGE)
#define TAGGED_BMALLOC_CHECK_MEDIUM_SEGREGATED_MAX PAS_MAX_OBJECT_SIZE_WITH_MIN_OBJECTS(PAS_MEDIUM_PAGE_DEFAULT_SIZE, TAGGED_BMALLOC_PINNED_MIN_OBJECTS_PER_PAGE)
#define TAGGED_BMALLOC_CHECK_SMALL_BITFIT_MAX PAS_MAX_BITFIT_OBJECT_SIZE_WITH_MIN_OBJECTS( \
    PAS_SMALL_BITFIT_PAGE_DEFAULT_SIZE - PAS_BITFIT_PAGE_HEADER_SIZE(PAS_SMALL_BITFIT_PAGE_DEFAULT_SIZE, PAS_SMALL_BITFIT_PAGE_DEFAULT_SIZE, TAGGED_BMALLOC_MINALIGN_SHIFT), \
    TAGGED_BMALLOC_MINALIGN_SHIFT, TAGGED_BMALLOC_PINNED_MIN_OBJECTS_PER_PAGE)
#define TAGGED_BMALLOC_CHECK_MEDIUM_BITFIT_MAX PAS_MAX_BITFIT_OBJECT_SIZE_WITH_MAX_BITS_AND_MIN_OBJECTS( \
    PAS_MEDIUM_BITFIT_PAGE_DEFAULT_SIZE, PAS_MIN_MEDIUM_ALIGN_SHIFT, PAS_BITFIT_MAX_FREE_MAX_VALID_MEDIUM, TAGGED_BMALLOC_PINNED_MIN_OBJECTS_PER_PAGE)

#if PAS_COMPILER(CLANG)
_Static_assert(TAGGED_BMALLOC_CHECK_SMALL_SEGREGATED_MAX <= PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE
    && TAGGED_BMALLOC_CHECK_MEDIUM_SEGREGATED_MAX <= PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE
    && TAGGED_BMALLOC_CHECK_SMALL_BITFIT_MAX <= PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE
    && TAGGED_BMALLOC_CHECK_MEDIUM_BITFIT_MAX <= PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
    "Every page config the tagged heap tags in must stay within the taggable object size");
_Static_assert(TAGGED_BMALLOC_CHECK_SMALL_SEGREGATED_MAX >= PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE
    || TAGGED_BMALLOC_CHECK_MEDIUM_SEGREGATED_MAX >= PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE
    || TAGGED_BMALLOC_CHECK_SMALL_BITFIT_MAX >= PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE
    || TAGGED_BMALLOC_CHECK_MEDIUM_BITFIT_MAX >= PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE,
    "At least one tagged heap page config must be able to allocate objects up to the taggable object size");
#endif /* PAS_COMPILER(CLANG) */

extern const pas_heap_config bmalloc_heap_config;

PAS_BASIC_HEAP_CONFIG_DEFINITIONS(
    tagged_bmalloc, TAGGED_BMALLOC,
    .allocate_page_should_zero = false,
    .intrinsic_view_cache_capacity = pas_heap_runtime_config_aggressive_view_cache_capacity);

void tagged_bmalloc_heap_config_activate(void)
{
#if PAS_OS(DARWIN)
    static const bool register_with_libmalloc = true;
#endif

    // Make sure that the bmalloc heap config initializes before we do anything else, since that
    // one will want to be designated.
    // FIXME: it's annoying that we need to do this in every heap-config other than bmalloc.
    pas_heap_config_activate(&bmalloc_heap_config);
    
    // FIXME: Find a way to install our heap as the designated intrinsic heap instead of, or alongside,
    // the regular bmalloc heap's intrinsic heap. For now leave bmalloc's hot path undisturbed.

#if PAS_OS(DARWIN)
    if (register_with_libmalloc && !pas_system_heap_should_supplant_bmalloc(pas_heap_config_kind_bmalloc))
        pas_root_ensure_for_libmalloc_enumeration();
#endif
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
