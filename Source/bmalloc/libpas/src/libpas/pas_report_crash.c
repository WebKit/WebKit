/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include "pas_ptr_hash_set.h"
#include "pas_ptr_hash_map.h"
#include "pas_root.h"

#include "pas_report_crash.h"
#include "pas_probabilistic_guard_malloc_allocator.h"

#ifdef __APPLE__
static crash_reporter_memory_reader_t memory_reader;

static kern_return_t memory_reader_adapter(task_t task, vm_address_t address, vm_size_t size, void **local_memory)
{
    if (!local_memory)
        return KERN_FAILURE;

    void *ptr = memory_reader(task, address, size);
    *local_memory = ptr;
    return ptr ? KERN_SUCCESS : KERN_FAILURE;
}

static memory_reader_t * setup_memory_reader(crash_reporter_memory_reader_t crm_reader)
{
    memory_reader = crm_reader;
    return memory_reader_adapter;
}

static PAS_ALWAYS_INLINE bool PAS_WARN_UNUSED_RETURN pas_fault_address_is_in_bounds(addr64_t fault_address, addr64_t bottom, addr64_t top)
{
    return (fault_address >= bottom) && (fault_address < top);
}

static PAS_ALWAYS_INLINE kern_return_t PAS_WARN_UNUSED_RETURN pas_update_report_crash_fields(pas_report_crash_pgm_report *report, const char *error_type, const char *confidence, vm_address_t fault_address, size_t allocation_size)
{
    report->error_type = error_type;
    report->confidence = confidence;
    report->fault_address = fault_address;
    report->allocation_size = allocation_size;
    return KERN_SUCCESS;
}

// This function will be called when a process crashes containing the JavaScriptCore framework.
// The goal is to determine if the crash was caused by a PGM allocation, and if so whether the crash
// was a UAF or OOB crash. These details will forwarded back to the Crash Reporter API, which will
// add the information to the local crash log.
kern_return_t pas_report_crash_extract_pgm_failure(vm_address_t fault_address, mach_vm_address_t pas_dead_root, unsigned version, task_t task, pas_report_crash_pgm_report *report, crash_reporter_memory_reader_t crm_reader)
{
    if (version != pas_crash_report_version)
        return KERN_FAILURE;

    memory_reader_t *reader = setup_memory_reader(crm_reader);

    pas_root* read_pas_dead_root = NULL;
    pas_ptr_hash_map* hash_map = NULL;
    pas_ptr_hash_map_entry* hash_map_entry = NULL;
    pas_pgm_storage* pgm_metadata = NULL;

    size_t table_size = 0;

    kern_return_t kr = reader(task, pas_dead_root, sizeof(pas_root), (void **) &read_pas_dead_root);
    if (kr != KERN_SUCCESS)
        return KERN_FAILURE;

    kr = reader(task, (vm_address_t) read_pas_dead_root->pas_pgm_hash_map_instance, sizeof(pas_ptr_hash_map), (void **) &hash_map);
    if (kr != KERN_SUCCESS)
        return KERN_FAILURE;

    table_size = hash_map->table_size;

    for (size_t i = 0; i < table_size; i++) {
        kr = reader(task, (vm_address_t) (hash_map->table + i), sizeof(pas_ptr_hash_map_entry), (void **) &hash_map_entry);
        if (kr != KERN_SUCCESS)
            return KERN_FAILURE;

        // Skip entry if not there
        if (hash_map_entry->key == (void*)UINTPTR_MAX)
            continue;

        kr = reader(task, (vm_address_t) hash_map_entry->value, sizeof(pas_pgm_storage), (void **) &pgm_metadata);
        if (kr != KERN_SUCCESS)
            return KERN_FAILURE;

        addr64_t key = (addr64_t) hash_map_entry->key;

        if (pgm_metadata->right_align) {
            report->alignment = "address right-aligned";

            // Right-aligned "Lower PGM OOB" checking
            addr64_t bottom = (addr64_t) (key - pgm_metadata->mem_to_waste - pgm_metadata->page_size);
            addr64_t top = (addr64_t) (key - pgm_metadata->mem_to_waste);

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields(report, pgm_metadata->free_status ? "long-range UAF" : "long-range OOB", "low", fault_address, pgm_metadata->allocation_size_requested);


            // Right-aligned "UAF + OOB" checking towards lower guard page
            bottom = (addr64_t) (key - pgm_metadata->mem_to_waste);
            top = (addr64_t) key;

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields(report, pgm_metadata->free_status ? "UAF" : "OOB", "low", fault_address, pgm_metadata->allocation_size_requested);

            // Right-aligned "Upper PGM OOB" checking
            bottom = (addr64_t) (key - pgm_metadata->mem_to_waste + pgm_metadata->size_of_data_pages);
            top = (addr64_t) (key - pgm_metadata->mem_to_waste + pgm_metadata->size_of_data_pages + pgm_metadata->page_size);

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields(report, pgm_metadata->free_status ? "UAF" : "OOB", "high", fault_address, pgm_metadata->allocation_size_requested);

        } else {
            report->alignment = "address left-aligned";

            // Left-aligned "Lower PGM OOB" checking
            addr64_t bottom = key - pgm_metadata->page_size;
            addr64_t top = key;

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields(report, pgm_metadata->free_status ? "UAF" : "OOB", "high", fault_address, pgm_metadata->allocation_size_requested);

            // Left-aligned "UAF + OOB" checking towards upper guard page
            bottom = (addr64_t) (key + pgm_metadata->size_of_data_pages - pgm_metadata->mem_to_waste);
            top = (addr64_t) (key + pgm_metadata->size_of_data_pages);

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields(report, pgm_metadata->free_status ? "UAF" : "OOB", "low", fault_address, pgm_metadata->allocation_size_requested);

            // Left-aligned "Upper PGM OOB" checking
            bottom = (addr64_t) (key + pgm_metadata->size_of_data_pages);
            top = (addr64_t) (key + pgm_metadata->size_of_data_pages + pgm_metadata->page_size);

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields(report, pgm_metadata->free_status ? "long-range UAF" : "long-range OOB", "low", fault_address, pgm_metadata->allocation_size_requested);

        }

        // Left-aligned "UAF" checking calculations are same as right-aligned checking
        // "UAF" check
        addr64_t bottom = (addr64_t) key;
        addr64_t top = (addr64_t) (key - pgm_metadata->mem_to_waste + pgm_metadata->size_of_data_pages);

        if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
            return pas_update_report_crash_fields(report, pgm_metadata->free_status ? "UAF" : "undefined", "low", fault_address, pgm_metadata->allocation_size_requested);

    }

    return KERN_FAILURE;
}
#endif /* __APPLE__ */

#endif /* LIBPAS_ENABLED */
