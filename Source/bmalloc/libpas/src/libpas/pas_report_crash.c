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

        // Lower PGM Bounds Checking
        addr64_t bottom = (addr64_t) (key - pgm_metadata->mem_to_waste - pgm_metadata->page_size);
        addr64_t top = (addr64_t) (key - pgm_metadata->mem_to_waste);

        if ((fault_address >= bottom)
            && (fault_address < top)) {
            report->error_type = "out-of-bounds";
            report->confidence = "high";
            report->fault_address = fault_address;
            report->allocation_size = pgm_metadata->allocation_size_requested;
            return KERN_SUCCESS;
        }

        // Upper PGM Bounds Checking
        bottom = (addr64_t) (key - pgm_metadata->mem_to_waste + pgm_metadata->size_of_data_pages);
        top = (addr64_t) (key - pgm_metadata->mem_to_waste + pgm_metadata->size_of_data_pages + pgm_metadata->page_size);

        if ((fault_address >= bottom)
            && (fault_address < top)) {
            report->error_type = "out-of-bounds";
            report->confidence = "high";
            report->fault_address = fault_address;
            report->allocation_size = pgm_metadata->allocation_size_requested;
            return KERN_SUCCESS;
        }

        // UAF Check
        bottom = (addr64_t) key;
        top = (addr64_t) (key - pgm_metadata->mem_to_waste + pgm_metadata->size_of_data_pages);

        if ((fault_address >= bottom)
            && (fault_address < top)) {
            report->error_type = "use-after-free";
            report->confidence = "high";
            report->fault_address = fault_address;
            report->allocation_size = pgm_metadata->allocation_size_requested;
            return KERN_SUCCESS;
        }

        // UAF + OOB Check
        bottom = (addr64_t) (key - pgm_metadata->mem_to_waste - pgm_metadata->page_size);
        top = (addr64_t) key;

        if ((fault_address >= bottom)
            && (fault_address < top)) {
            report->error_type = "OOB + UAF";
            report->confidence = "low";
            report->fault_address = fault_address;
            report->allocation_size = pgm_metadata->allocation_size_requested;
            return KERN_SUCCESS;
        }
    }

    return KERN_FAILURE;
}
#endif /* __APPLE__ */

#endif /* LIBPAS_ENABLED */
