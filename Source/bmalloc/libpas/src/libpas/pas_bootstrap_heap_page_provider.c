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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_bootstrap_heap_page_provider.h"

#include "pas_bootstrap_free_heap.h"

static PAS_ALWAYS_INLINE pas_allocation_result bootstrap_heap_page_provider_impl(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_memory_flavor flavor)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_BOOTSTRAP_HEAPS);
    pas_allocation_result retval;

    if (verbose)
        pas_log("%s bootstrap heap page-provider allocating %zu for %s\n", pas_memory_flavor_get_string(flavor), size, name);

    switch (flavor) {
    case pas_memory_flavor_plain:
        retval = pas_bootstrap_free_heap_try_allocate_with_alignment(size, alignment, name, pas_delegate_allocation);
        break;
    case pas_memory_flavor_mte:
        retval = pas_mte_bootstrap_free_heap_try_allocate_with_alignment(size, alignment, name, pas_delegate_allocation);
        break;
    case pas_memory_flavor_rwx:
        PAS_ASSERT_NOT_REACHED();
        retval = pas_allocation_result_create_failure();
        break;
    }

    if (verbose)
        pas_log("%s bootstrap heap page-provider done allocating\n", pas_memory_flavor_get_string(flavor));

    return retval;
}

pas_allocation_result pas_bootstrap_heap_page_provider(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_heap* heap,
    pas_physical_memory_transaction* transaction,
    void *arg)
{
    PAS_UNUSED_PARAM(arg);
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(transaction);
    return bootstrap_heap_page_provider_impl(size, alignment, name, pas_memory_flavor_plain);
}

pas_allocation_result pas_mte_bootstrap_heap_page_provider(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_heap* heap,
    pas_physical_memory_transaction* transaction,
    void *arg)
{
    PAS_UNUSED_PARAM(arg);
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(transaction);
    return bootstrap_heap_page_provider_impl(size, alignment, name, pas_memory_flavor_mte);
}

#endif /* LIBPAS_ENABLED */
