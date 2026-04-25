/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#include "iso_heap.h"

#if PAS_ENABLE_ISO

#include "Verifier.h"
#include "iso_heap_config.h"
#include "iso_heap_innards.h"
#include "pas_bootstrap_free_heap.h"
#include "pas_fd_stream.h"
#include "pas_heap.h"
#include "pas_large_sharing_pool.h"
#include "pas_page_sharing_pool.h"
#include "pas_scavenger.h"
#include "pas_status_reporter.h"
#include "pas_string_stream.h"
#include "pas_utility_heap.h"
#include "pas_utility_heap_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if PAS_OS(DARWIN)
#include <malloc/malloc.h>
#endif

static const bool verbose = false;
#ifdef PAS_VERIFIED
static const bool verbose_scavenge = true;
#else
static const bool verbose_scavenge = false;
#endif
static const bool validate = true;
static const bool really_scavenge_at_end = true;

static void install_verifier_if_necessary(void)
{
#ifdef PAS_VERIFIED
    pas_install_verifier();
#endif
}

void* mbmalloc(size_t size)
{
    if (verbose)
        printf("malloc(%zu)\n", size);
    install_verifier_if_necessary();
    return iso_try_allocate_common_primitive(size, pas_non_compact_allocation_mode);
}

void* mbmemalign(size_t alignment, size_t size)
{
    if (verbose)
        printf("memalign(%zu, %zu)\n", alignment, size);
    install_verifier_if_necessary();
    return iso_try_allocate_common_primitive_with_alignment(size, alignment, pas_non_compact_allocation_mode);
}

void* mbrealloc(void* p, size_t ignored_old_size, size_t new_size)
{
    if (verbose)
        printf("realloc(%p, %zu)\n", p, new_size);
    install_verifier_if_necessary();
    return iso_try_reallocate_common_primitive(p, new_size, pas_reallocate_free_if_successful, pas_non_compact_allocation_mode);
}

void mbfree(void* p, size_t ignored_size)
{
    if (verbose)
        printf("free(%p)\n", p);
    install_verifier_if_necessary();
    iso_deallocate(p);
}

PAS_UNUSED static void dump_summary(pas_heap_summary summary, const char* name)
{
    printf("%s free: %zu\n", name, summary.free);
    printf("%s allocated: %zu\n", name, summary.allocated);
    printf("%s committed: %zu\n", name, summary.committed);
    printf("%s decommitted: %zu\n", name, summary.decommitted);
}

void mbscavenge(void)
{
    install_verifier_if_necessary();
    pas_scavenger_suspend();
    
    if (verbose_scavenge) {
        printf("Before scavenging!\n");
        printf("Num mapped bytes in bootstrap: %zu\n", pas_bootstrap_free_heap.num_mapped_bytes);
        printf("Num free bytes in bootstrap: %zu\n",
               pas_simple_large_free_heap_get_num_free_bytes(&pas_bootstrap_free_heap));
        printf("Num mapped bytes in primitive large: %zu\n",
               pas_fast_large_free_heap_get_num_mapped_bytes(
                   &iso_common_primitive_heap.large_heap.free_heap));
        printf("Num free bytes in primitive large: %zu\n",
               iso_common_primitive_heap.large_heap.free_heap.num_mapped_bytes);
        printf("Num committed pages: %zu\n",
               pas_segregated_heap_num_committed_views(
                   &iso_common_primitive_heap.segregated_heap));
        printf("Num empty pages: %zu\n",
               pas_segregated_heap_num_empty_views(
                   &iso_common_primitive_heap.segregated_heap));
        printf("Num empty granules: %zu\n",
               pas_segregated_heap_num_empty_granules(
                   &iso_common_primitive_heap.segregated_heap));
        printf("Have current participant in physical sharing pool: %d\n",
               pas_page_sharing_pool_has_current_participant(
                   &pas_physical_page_sharing_pool));
    }
    
    if (validate) {
        pas_page_sharing_pool_verify(
            &pas_physical_page_sharing_pool,
            pas_lock_is_not_held);
    }
    
    if (really_scavenge_at_end) {
        pas_scavenger_run_synchronously_now();
#if PAS_OS(DARWIN)
        malloc_zone_pressure_relief(NULL, 0);
#endif
    }
    
    if (verbose_scavenge) {
        printf("After scavenging!\n");
        printf("Bootstrap heap:\n");
        pas_simple_large_free_heap_dump_to_printf(&pas_bootstrap_free_heap);
        printf("Num committed pages: %zu\n",
               pas_segregated_heap_num_committed_views(
                   &iso_common_primitive_heap.segregated_heap));
        printf("Num fully empty pages: %zu\n",
               pas_segregated_heap_num_empty_views(
                   &iso_common_primitive_heap.segregated_heap));
        printf("Num empty granules: %zu\n",
               pas_segregated_heap_num_empty_granules(
                   &iso_common_primitive_heap.segregated_heap));
        printf("Num committed pages in utility: %zu\n",
               pas_segregated_heap_num_committed_views(
                   &pas_utility_segregated_heap));
        printf("Num empty pages in utility: %zu\n",
               pas_segregated_heap_num_empty_views(
                   &pas_utility_segregated_heap));
        dump_summary(
            pas_large_sharing_pool_compute_summary(
                pas_range_create(0, (uintptr_t)1 << PAS_ADDRESS_BITS),
                pas_large_sharing_pool_compute_summary_unknown_allocation_state,
                pas_lock_is_not_held),
            "Large pool");
        pas_heap_lock_lock();
        dump_summary(
            pas_large_heap_compute_summary(&iso_common_primitive_heap.large_heap),
            "Iso large heap");
        pas_heap_lock_unlock();
        printf("Have current participant in physical sharing pool: %d\n",
               pas_page_sharing_pool_has_current_participant(
                   &pas_physical_page_sharing_pool));

        pas_fd_stream fd_stream;
        pas_fd_stream_construct(&fd_stream, PAS_LOG_DEFAULT_FD);
        pas_heap_lock_lock();
        pas_status_reporter_enabled = 3;
        pas_status_reporter_dump_everything((pas_stream*)&fd_stream);
        pas_heap_lock_unlock();
    }

    if (validate) {
        pas_page_sharing_pool_verify(
            &pas_physical_page_sharing_pool,
            pas_lock_is_not_held);
    }

#ifdef PAS_VERIFIED
    pas_dump_state();
#endif

    if (verbose_scavenge) {
        char buf[64];
        
        fflush(stdout);
        fflush(stderr);

        snprintf(buf, sizeof(buf), "/usr/bin/vmmap %d", getpid());
        system(buf);
    }
    
    pas_scavenger_resume();
}

#endif /* PAS_ENABLE_ISO */
