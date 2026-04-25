/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#include "pas_probabilistic_guard_malloc_allocator.h"

#include "pas_heap.h"
#include "pas_large_utility_free_heap.h"
#include "pas_mte.h"
#include "pas_random.h"
#include "pas_utility_heap.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#if !PAS_OS(WINDOWS)
#include <sys/mman.h>
#include <unistd.h>
#endif

/* PlayStation does not currently support the backtrace API. Android API versions < 33 don't, either. Windows does not either. Linux only with GLibc and not uCLibc/Musl. */
#if (PAS_OS(ANDROID) && __ANDROID_API__ >= 33) || PAS_OS(DARWIN) || (PAS_OS(LINUX) && defined(__GLIBC__) && !defined(__UCLIBC__))
#include <execinfo.h>
#else
size_t backtrace(void** buffer, size_t size)
{
    PAS_UNUSED_PARAM(buffer);
    PAS_UNUSED_PARAM(size);
    return 0;
}
#endif

static size_t free_wasted_mem  = PAS_PGM_MAX_WASTED_MEMORY;
static size_t free_virtual_mem = PAS_PGM_MAX_VIRTUAL_MEMORY;
static pas_ptr_hash_map_entry pgm_metadata_vector[MAX_PGM_DEALLOCATED_METADATA_ENTRIES];
static size_t pgm_metadata_index = 0;

uint16_t pas_probabilistic_guard_malloc_random;
uint16_t pas_probabilistic_guard_malloc_counter = 0;

bool pas_probabilistic_guard_malloc_can_use = false;
bool pas_probabilistic_guard_malloc_is_initialized = false;

/*
 * Flag to indicate if PGM has enabled at all for this process,
 * even if it been subsequently disabled, or no guarded allocations have been made
*/
bool pas_probabilistic_guard_malloc_has_been_used = false;

/*
 * the hash map is used to keep track of all pgm allocations
 * key   : user's starting memory address
 * value : metadata for tracking that allocation (pas_pgm_storage)
 */
pas_ptr_hash_map pas_pgm_hash_map = PAS_HASHTABLE_INITIALIZER;
pas_ptr_hash_map_in_flux_stash pas_pgm_hash_map_in_flux_stash;

static void pas_probabilistic_guard_malloc_debug_info(const void* key, const pas_pgm_storage* value, const char* operation);

#if PAS_COMPILER(CLANG)
#pragma mark -
#pragma mark ALLOC/DEALLOC
#endif

pas_allocation_result pas_probabilistic_guard_malloc_allocate(pas_large_heap* large_heap, size_t size, size_t alignment, pas_allocation_mode allocation_mode,
                                                              const pas_heap_config* heap_config, pas_physical_memory_transaction* transaction)
{
    const pas_heap_type* type;
    static const bool verbose = false;

    pas_heap_lock_assert_held();

    pas_allocation_result result = pas_allocation_result_create_failure();

    if (verbose)
        pas_log("Memory requested to allocate %zu with alignment %zu\n", size, alignment);

    if (!large_heap || !size || !heap_config || !transaction)
        return result;

    const size_t page_size = pas_page_malloc_alignment();

    type = pas_heap_for_large_heap(large_heap)->type;
    alignment = PAS_MAX(alignment, heap_config->get_type_alignment(type));
    alignment = PAS_MAX(alignment, heap_config->large_alignment);
    PAS_ASSERT(page_size >= heap_config->large_alignment);

    size_t mem_to_waste = 0;
    size_t mem_to_alloc = 0;
    if (alignment >= page_size) {
        /*
         * In this case, slide becomes always n * page_size since alignment is n * page_size.
         * Also, to achieve this alignment request, we need this expanded size, and this is not something wasted for PGM.
         * So we set 0 to mem_to_waste.
         * The worst scenario is the allocated pointer is off by one page from the alignment boundary. In that case, we need
         * to add (alignment - page_size) to obtain the next alignment boundary. This means we need to allocate at least
         * size + (alignment - page_size) memory to find alignment boundary with size.
         */
        size_t size_with_alignment_reservation = pas_round_up(size + alignment - page_size, page_size);
        mem_to_waste = 0;
        mem_to_alloc = (2 * page_size) + size_with_alignment_reservation;
    } else {
        /*
         * Since both page_size and alignment are power-of-two and alignment is smaller than page_size,
         * page_size is always n * alignment. Since size_with_alignment_reservation is m * alignment,
         * mem_to_waste is also x * alignment. Thus right-align will be guaranteed to be aligned.
         */
        size_t size_with_alignment_reservation = pas_round_up(size, alignment);
        mem_to_waste = (page_size - (size_with_alignment_reservation % page_size)) % page_size;
        mem_to_alloc = (2 * page_size) + size_with_alignment_reservation + mem_to_waste;
    }

    /*
     * calculate virtual memory
     *
     * *------------------* *------------------* *------------------*
     * | lower guard page | | user alloc pages | | upper guard page |
     * *------------------* *------------------* *------------------*
     */
    if (mem_to_waste > free_wasted_mem)
        return result;

    if (mem_to_alloc > free_virtual_mem)
        return result;

    result = pas_large_heap_try_allocate_and_forget(large_heap, mem_to_alloc, page_size, allocation_mode, heap_config, transaction);
    if (!result.did_succeed)
        return result;

    /*
     * the key is the location where the user's starting memory address is located.
     * allocations are right aligned, so the end backs up to the upper guard page.
     *
     * Take random decision to right align or left align in order to be able to catch
     * overflow and underflow conditions with equal probability.
     *
     * If alignment >= page_size, then there is only one region we can use to meet the
     * alignment requirement. Thus we do not consider about right_align.
     */
    uintptr_t key = pas_round_up(result.begin + page_size, alignment);
    uint8_t right_align = 0;
    if (alignment < page_size) {
        if (pas_get_fast_random(2)) {
            key = result.begin + page_size + mem_to_waste;
            right_align = 1;
        }
    }

    /* protect guard pages from being accessed */
    uintptr_t lower_guard = result.begin;
    size_t lower_guard_size = pas_round_down(key - lower_guard, page_size);
    uintptr_t upper_guard = pas_round_up(key + size, page_size);
    size_t upper_guard_size = (result.begin + mem_to_alloc) - upper_guard;

#if PAS_OS(WINDOWS)
    void* virtualalloc_res = VirtualAlloc((void*) lower_guard, lower_guard_size, MEM_COMMIT, PAGE_NOACCESS);
    PAS_ASSERT(virtualalloc_res);

    virtualalloc_res = VirtualAlloc((void*) upper_guard, upper_guard_size, MEM_COMMIT, PAGE_NOACCESS);
    PAS_ASSERT(virtualalloc_res);

    /*
     * ensure physical addresses are released
     * loop using meminfo here if the upper guard free is failing
     */
    bool virtualfree_res = VirtualFree((void*) lower_guard, lower_guard_size, MEM_DECOMMIT);
    PAS_ASSERT(virtualfree_res);

    virtualfree_res = VirtualFree((void*) upper_guard, upper_guard_size, MEM_DECOMMIT);
    PAS_ASSERT(virtualfree_res);
#else
    int mprotect_res = mprotect((void*)lower_guard, lower_guard_size, PROT_NONE);
    PAS_ASSERT(!mprotect_res);

    mprotect_res = mprotect((void*)upper_guard, upper_guard_size, PROT_NONE);
    PAS_ASSERT(!mprotect_res);

    /*
     * ensure physical addresses are released
     * TODO: investigate using MADV_FREE_REUSABLE instead
     */
    int madvise_res = madvise((void*)upper_guard, upper_guard_size, MADV_FREE);
    PAS_ASSERT(!madvise_res);

    madvise_res = madvise((void*)lower_guard, lower_guard_size, MADV_FREE);
    PAS_ASSERT(!madvise_res);
#endif

    PAS_PROFILE(PGM_ALLOCATE, heap_config, key);
    PAS_MTE_HANDLE(PGM_ALLOCATE, heap_config, key);

    /* create struct to hold hash map value */
    pas_pgm_storage* value = pas_utility_heap_try_allocate(sizeof(pas_pgm_storage), "pas_pgm_hash_map_VALUE");
    PAS_ASSERT(value);

    value->alloc_backtrace              = pas_utility_heap_allocate(sizeof(pas_backtrace_metadata), "pas_alloc_backtrace_metadata");
    value->alloc_backtrace->frame_size  = backtrace(value->alloc_backtrace->backtrace_buffer, PAS_PGM_BACKTRACE_MAX_FRAMES);
    value->dealloc_backtrace            = NULL;
    value->mem_to_waste                 = mem_to_waste;
    value->size_of_data_pages           = mem_to_alloc - (lower_guard_size + upper_guard_size);
    value->start_of_data_pages          = result.begin + lower_guard_size;
    value->size_of_allocated_pages      = mem_to_alloc;
    value->start_of_allocated_pages     = result.begin;
    value->allocation_size_requested    = size;
    value->page_size                    = page_size;
    value->large_heap                   = large_heap;
    value->right_align                  = right_align;

    pas_ptr_hash_map_add_result add_result = pas_ptr_hash_map_add(&pas_pgm_hash_map, (void*)key, NULL, &pas_large_utility_free_heap_allocation_config);
    PAS_ASSERT(add_result.is_new_entry);

    add_result.entry->key = (void*)key;
    add_result.entry->value = value;

    free_wasted_mem  -= mem_to_waste;
    free_virtual_mem -= mem_to_alloc;

    if (verbose)
        pas_probabilistic_guard_malloc_debug_info((void*)key, value, "Allocating memory");

    result.begin = (uintptr_t)key;

    /* 3 pages are the minimum required for PGM */
    if (free_virtual_mem < 3 * page_size)
        pas_probabilistic_guard_malloc_can_use = false;

    return result;
}

void pas_probabilistic_guard_malloc_deallocate(void* mem)
{
    pas_heap_lock_assert_held();
    static const bool verbose = false;

    if (verbose)
        pas_log("Memory Address Requested to Deallocate %p\n", mem);

    uintptr_t key = (uintptr_t)mem;
    PAS_PROFILE(PGM_DEALLOCATE, key);
    PAS_MTE_HANDLE(PGM_DEALLOCATE, key);

    pas_ptr_hash_map_entry* entry = pas_ptr_hash_map_find(&pas_pgm_hash_map, (void*)key);
    if (!entry || !entry->value)
        return;

    pas_pgm_storage* value = (pas_pgm_storage*)entry->value;
#if PAS_OS(WINDOWS)
    MEMORY_BASIC_INFORMATION memInfo;
    VirtualQuery((void *) value->start_of_data_pages, &memInfo, sizeof(memInfo));

    void* virtualalloc_res = NULL;
    bool virtualfree_res = false;
    size_t totalSeen = 0;
    void *currentPtr = (void*) value->start_of_data_pages;
    while (totalSeen < value->size_of_data_pages) {
        MEMORY_BASIC_INFORMATION memInfo;
        VirtualQuery(currentPtr, &memInfo, sizeof(memInfo));
        virtualalloc_res = VirtualAlloc(currentPtr, memInfo.RegionSize, MEM_COMMIT, PAGE_NOACCESS);
        PAS_ASSERT(virtualalloc_res);

        /* ensure physical addresses are released */
        virtualfree_res = VirtualFree(currentPtr, memInfo.RegionSize, MEM_DECOMMIT);
        PAS_ASSERT(virtualfree_res);

        PAS_ASSERT(memInfo.RegionSize > 0);
        currentPtr = (void*) ((uintptr_t) currentPtr + memInfo.RegionSize);
        totalSeen += memInfo.RegionSize;
    }
#else
    int mprotect_res = mprotect((void*)value->start_of_data_pages, value->size_of_data_pages, PROT_NONE);
    PAS_ASSERT(!mprotect_res);

    /* grab some memory for dealloc backtrace and capture deallocation backtrace */
    value->dealloc_backtrace = pas_utility_heap_allocate(sizeof(pas_backtrace_metadata), "pas_dealloc_backtrace_metadata");
    value->dealloc_backtrace->frame_size = backtrace(value->dealloc_backtrace->backtrace_buffer, PAS_PGM_BACKTRACE_MAX_FRAMES);

    /*
     * ensure physical addresses are released
     * TODO: investigate using MADV_FREE_REUSABLE instead
     */
    int madvise_res = madvise((void*)value->start_of_data_pages, value->size_of_data_pages, MADV_FREE);
    PAS_ASSERT(!madvise_res);
#endif

    free_wasted_mem  += value->mem_to_waste;
    free_virtual_mem += value->size_of_allocated_pages;

    /*
     * Mark the physical memory status free and check if the max entries reached.
     * If so deallocate the space for metadata as well
     */
    value->free_status = true;
    pas_ptr_hash_map_entry* old_entry = &pgm_metadata_vector[pgm_metadata_index];
    if (old_entry->key) {
        pas_pgm_storage* old_pas_pgm_storage = (pas_pgm_storage*)(old_entry->value);
        pas_utility_heap_deallocate((void*)old_pas_pgm_storage->alloc_backtrace);
        pas_utility_heap_deallocate((void*)old_pas_pgm_storage->dealloc_backtrace);
        pas_utility_heap_deallocate((void*)old_pas_pgm_storage);
        bool removed = pas_ptr_hash_map_remove(&pas_pgm_hash_map, (void*)old_entry->key, NULL, &pas_large_utility_free_heap_allocation_config);
        PAS_ASSERT(removed);
    }
    pgm_metadata_vector[pgm_metadata_index] = *entry;
    pgm_metadata_index = (pgm_metadata_index + 1) % MAX_PGM_DEALLOCATED_METADATA_ENTRIES;

    if (verbose)
        pas_probabilistic_guard_malloc_debug_info((void*)key, value, "Deallocating Memory");

    pas_probabilistic_guard_malloc_can_use = true;
}

bool pas_probabilistic_guard_malloc_check_exists(uintptr_t mem)
{
    pas_heap_lock_assert_held();
    static const bool verbose = false;

    if (verbose)
        pas_log("Checking if is PGM entry\n");

    pas_ptr_hash_map_entry* entry = pas_ptr_hash_map_find(&pas_pgm_hash_map, (void*)mem);
    return (entry && entry->value);
}

pas_large_map_entry pas_probabilistic_guard_malloc_return_as_large_map_entry(uintptr_t mem)
{
    pas_heap_lock_assert_held();
    static const bool verbose = false;

    pas_large_map_entry ret = { };

    if (verbose)
        pas_log("Grabbing PGM allocated size\n");

    pas_ptr_hash_map_entry* entry = pas_ptr_hash_map_find(&pas_pgm_hash_map, (void*)mem);
    if (entry && entry->value) {
        pas_pgm_storage* entry_val = (pas_pgm_storage*)entry->value;
        ret.begin = mem;
        ret.end = mem + entry_val->allocation_size_requested;
        ret.heap = entry_val->large_heap;
    }

    return ret;
}

#if PAS_COMPILER(CLANG)
#pragma mark -
#pragma mark Helper Functions
#endif

size_t pas_probabilistic_guard_malloc_get_free_virtual_memory(void)
{
    pas_heap_lock_assert_held();
    return free_virtual_mem;
}

size_t pas_probabilistic_guard_malloc_get_free_wasted_memory(void)
{
    pas_heap_lock_assert_held();
    return free_wasted_mem;
}

pas_ptr_hash_map_entry* pas_probabilistic_guard_malloc_get_metadata_array(void)
{
    return pgm_metadata_vector;
}


/*
 * During heap creation we want to check whether we should enable PGM.
 * PGM being enabled in the heap config does not mean it will be enabled at runtime.
 * This function will be run once for all heaps (ISO, bmalloc, JIT, etc...), but only those with
 * pgm_enabled config will ultimately be called.
 *
 * PGM has two layers before an allocation is called. The first layer is the activation check, which is called once on process initialization.
 * This will determine if PGM will be enabled while the process is alive. This is currently set to 1 in 1000, but
 * from benchmarking turning this on 100% of the time will not cause any noticeable memory or performance degradation.
 * If the activation check fails then PGM will never be enabled during the process' lifetime.
 *
 * The second layer is the probability check, which is set to perform a PGM allocation between every 1 in 4000 to 1 in 5000 times.
 * The probability check will be performed every time an allocation is made. The implementation is a simple counter, which is incremented
 * every allocation. Once it hits the calculated probability randomly generated number we will perform a PGM allocation. Having a slight variation
 * in how often we call PGM will hopefully help to catch issues more quickly.
 *
 * These numbers are set to correlate with macOS's system malloc, but these can be tuned as needed. Just to note that
 * increasing these numbers may increase the number of duplicate crashes seen. On the other hand, decreasing the probability
 * may result in issues being missed.
 */
void pas_probabilistic_guard_malloc_initialize_pgm(void)
{
    if (!pas_probabilistic_guard_malloc_is_initialized) {
        pas_probabilistic_guard_malloc_is_initialized = true;
        pas_probabilistic_guard_malloc_can_use = false;
        pas_probabilistic_guard_malloc_has_been_used = true;
    }
}

/*
 * This function shall be called for testing PGM behavior, since PGM enablement will be non-deterministic otherwise.
 */
void pas_probabilistic_guard_malloc_initialize_pgm_as_enabled(uint16_t pgm_random_rate)
{
    pas_probabilistic_guard_malloc_is_initialized = true;
    pas_probabilistic_guard_malloc_can_use = true;
    pas_probabilistic_guard_malloc_has_been_used = true;
    pas_probabilistic_guard_malloc_random = pgm_random_rate;
    pas_probabilistic_guard_malloc_counter = 0;
    memset(pgm_metadata_vector, 0, sizeof(pgm_metadata_vector));
}

void pas_probabilistic_guard_malloc_debug_info(const void* key, const pas_pgm_storage* value, const char* operation)
{
    uintptr_t lower_guard = value->start_of_allocated_pages;
    size_t lower_guard_size = value->start_of_data_pages - value->start_of_allocated_pages;
    uintptr_t upper_guard = value->start_of_data_pages + value->size_of_data_pages;
    size_t upper_guard_size = value->size_of_allocated_pages - lower_guard_size - value->size_of_data_pages;

    pas_log("******************************************************\n"
        " %s\n\n"
        " Overall System Stats"
        " free_wasted_mem  : %zu\n"
        " free_virtual_mem : %zu\n"
        "\n"
        " Allocation\n"
        " Allocation Size Requested : %zu \n"
        " Page Size                 : %hu \n"
        " Memory Wasted             : %hu \n"
        " Data Pages                : %p  \n"
        " Data Pages Size           : %zu \n"
        " Lower Guard               : %p  \n"
        " Lower Guard Size          : %zu \n"
        " Upper Guard               : %p  \n"
        " Upper Guard Size          : %zu \n"
        " Memory Address for User   : %p  \n"
        "******************************************************\n\n\n",
        operation,
        free_wasted_mem,
        free_virtual_mem,
        value->allocation_size_requested,
        value->page_size,
        value->mem_to_waste,
        ((void*)value->start_of_data_pages),
        value->size_of_data_pages,
        ((void*)lower_guard),
        lower_guard_size,
        ((void*)upper_guard),
        upper_guard_size,
        key);
}

#endif /* LIBPAS_ENABLED */
