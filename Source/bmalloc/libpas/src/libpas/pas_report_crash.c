/*
 * Copyright (c) 2023-2025 Apple Inc. All rights reserved.
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

#include "pas_enumerator.h"
#include "pas_ptr_hash_map.h"
#include "pas_root.h"

#include "pas_report_crash.h"
#include "pas_probabilistic_guard_malloc_allocator.h"

#ifdef __APPLE__
static void* pas_enumerator_reader_adapter(pas_enumerator* enumerator,
                                           void* remote_address, size_t size, void* arg)
{
    PAS_UNUSED_PARAM(enumerator);
    crash_reporter_memory_reader_t crm_reader = (crash_reporter_memory_reader_t)arg;
    return crm_reader(0, (vm_address_t)remote_address, size);
}

static pas_enumerator* setup_enumerator_for_crash_reporting(mach_vm_address_t pas_dead_root,
                                                           crash_reporter_memory_reader_t crm_reader)
{
    return pas_enumerator_create((pas_root*)pas_dead_root,
                                pas_enumerator_reader_adapter,
                                (void*)crm_reader,
                                NULL, /* recorder */
                                NULL, /* recorder_arg */
                                pas_enumerator_do_not_record_meta_records,
                                pas_enumerator_do_not_record_payload_records,
                                pas_enumerator_do_not_record_object_records);
}

static PAS_ALWAYS_INLINE bool PAS_WARN_UNUSED_RETURN pas_fault_address_is_in_bounds(addr64_t fault_address, addr64_t bottom, addr64_t top)
{
    return (fault_address >= bottom) && (fault_address < top);
}

static PAS_ALWAYS_INLINE kern_return_t PAS_WARN_UNUSED_RETURN pas_update_report_crash_fields(pas_report_crash_pgm_report* report, const char* error_type, const char* confidence, vm_address_t fault_address, size_t allocation_size, pas_backtrace_metadata* alloc_backtrace, pas_backtrace_metadata* dealloc_backtrace)
{
    report->error_type = error_type;
    report->confidence = confidence;
    report->fault_address = fault_address;
    report->allocation_size = allocation_size;
    report->alloc_backtrace = alloc_backtrace;
    report->dealloc_backtrace = dealloc_backtrace;
    return KERN_SUCCESS;
}

static PAS_ALWAYS_INLINE kern_return_t PAS_WARN_UNUSED_RETURN pas_update_report_crash_fields_and_cleanup(pas_enumerator* enumerator, pas_report_crash_pgm_report* report, const char* error_type, const char* confidence, vm_address_t fault_address, size_t allocation_size, pas_backtrace_metadata* alloc_backtrace, pas_backtrace_metadata* dealloc_backtrace)
{
    kern_return_t result = pas_update_report_crash_fields(report, error_type, confidence, fault_address, allocation_size, alloc_backtrace, dealloc_backtrace);
    pas_enumerator_destroy(enumerator);
    return result;
}

/*
 * This function will be called when a process crashes containing the JavaScriptCore framework.
 * The goal is to determine if the crash was caused by a PGM allocation, and if so whether the crash
 * was a UAF or OOB crash. These details will forwarded back to the Crash Reporter API, which will
 * add the information to the local crash log.
 */
kern_return_t pas_report_crash_extract_pgm_failure(vm_address_t fault_address, mach_vm_address_t pas_dead_root, unsigned version, task_t task, pas_report_crash_pgm_report* report, crash_reporter_memory_reader_t crm_reader)
{
    PAS_UNUSED_PARAM(task);

    if (!report)
        return KERN_INVALID_ARGUMENT;
    if (!crm_reader)
        return KERN_INVALID_ARGUMENT;

    pas_enumerator* enumerator = setup_enumerator_for_crash_reporting(pas_dead_root, crm_reader);
    if (!enumerator)
        return KERN_FAILURE;

    pas_ptr_hash_map hash_map;
    pas_ptr_hash_map_entry hash_map_entry;
    pas_pgm_storage pgm_metadata;
    bool pgm_has_been_used;

    size_t table_size = 0;

    unsigned dead_root_crash_report_version = enumerator->root->pas_crash_report_version;
    if (version != dead_root_crash_report_version) {
        pas_enumerator_destroy(enumerator);
        return KERN_FAILURE;
    }

    if (!enumerator->root->probabilistic_guard_malloc_has_been_used) {
        pas_enumerator_destroy(enumerator);
        return KERN_FAILURE;
    }

    if (!pas_enumerator_copy_remote(enumerator, &pgm_has_been_used, enumerator->root->probabilistic_guard_malloc_has_been_used, sizeof(bool))) {
        pas_enumerator_destroy(enumerator);
        return KERN_FAILURE;
    }
    report->pgm_has_been_used = pgm_has_been_used;

    if (!pas_enumerator_copy_remote(enumerator, &hash_map, enumerator->root->pas_pgm_hash_map_instance, sizeof(pas_ptr_hash_map))) {
        pas_enumerator_destroy(enumerator);
        return KERN_FAILURE;
    }

    table_size = hash_map.table_size;

    /* Check if hash_map has a valid table before iterating */
    if (!hash_map.table) {
        pas_enumerator_destroy(enumerator);
        return KERN_FAILURE;
    }

    for (size_t i = 0; i < table_size; i++) {
        if (!pas_enumerator_copy_remote(enumerator, &hash_map_entry, hash_map.table + i, sizeof(pas_ptr_hash_map_entry))) {
            pas_enumerator_destroy(enumerator);
            return KERN_FAILURE;
        }

        /* Skip entry if not there */
        if (hash_map_entry.key == (void*)UINTPTR_MAX)
            continue;

        if (!pas_enumerator_copy_remote(enumerator, &pgm_metadata, hash_map_entry.value, sizeof(pas_pgm_storage))) {
            pas_enumerator_destroy(enumerator);
            return KERN_FAILURE;
        }

        pas_backtrace_metadata alloc_backtrace_data;
        if (pgm_metadata.alloc_backtrace) {
            if (!pas_enumerator_copy_remote(enumerator, &alloc_backtrace_data, pgm_metadata.alloc_backtrace, sizeof(pas_backtrace_metadata))) {
                pas_enumerator_destroy(enumerator);
                return KERN_FAILURE;
            }
            if (alloc_backtrace_data.frame_size < 0 || alloc_backtrace_data.frame_size > PGM_BACKTRACE_MAX_FRAMES) {
                pas_enumerator_destroy(enumerator);
                return KERN_FAILURE;
            }
        }

        pas_backtrace_metadata dealloc_backtrace_data;
        if (pgm_metadata.dealloc_backtrace) {
            if (!pas_enumerator_copy_remote(enumerator, &dealloc_backtrace_data, pgm_metadata.dealloc_backtrace, sizeof(pas_backtrace_metadata))) {
                pas_enumerator_destroy(enumerator);
                return KERN_FAILURE;
            }
            if (dealloc_backtrace_data.frame_size < 0 || dealloc_backtrace_data.frame_size > PGM_BACKTRACE_MAX_FRAMES) {
                pas_enumerator_destroy(enumerator);
                return KERN_FAILURE;
            }
        }

        addr64_t key = (addr64_t)hash_map_entry.key;
        addr64_t lower_guard = (addr64_t)pgm_metadata.start_of_allocated_pages;
        size_t lower_guard_size = pgm_metadata.start_of_data_pages - pgm_metadata.start_of_allocated_pages;
        addr64_t upper_guard = (addr64_t)(pgm_metadata.start_of_data_pages + pgm_metadata.size_of_data_pages);
        size_t upper_guard_size = pgm_metadata.size_of_allocated_pages - lower_guard_size - pgm_metadata.size_of_data_pages;

        if (pgm_metadata.right_align) {
            /* [ lower_guard ][ remaining ][ allocated ][ upper_guard ] */
            report->alignment = "address right-aligned";

            /* Right-aligned "Lower PGM OOB" checking */
            addr64_t bottom = (addr64_t)lower_guard;
            addr64_t top = (addr64_t)(lower_guard + lower_guard_size);

            if (pas_fault_address_is_in_bounds(fault_address, lower_guard, top))
                return pas_update_report_crash_fields_and_cleanup(enumerator, report, pgm_metadata.free_status ? "long-range UAF" : "long-range OOB", "low", fault_address, pgm_metadata.allocation_size_requested, &alloc_backtrace_data, &dealloc_backtrace_data);

            /* Right-aligned "UAF + OOB" checking towards lower guard page */
            bottom = (addr64_t)pgm_metadata.start_of_data_pages;
            top = (addr64_t)key;

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields_and_cleanup(enumerator, report, pgm_metadata.free_status ? "UAF" : "OOB", "low", fault_address, pgm_metadata.allocation_size_requested, &alloc_backtrace_data, &dealloc_backtrace_data);

            /* Right-aligned "Upper PGM OOB" checking */
            bottom = (addr64_t)upper_guard;
            top = (addr64_t)(upper_guard + upper_guard_size);

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields_and_cleanup(enumerator, report, pgm_metadata.free_status ? "UAF" : "OOB", "high", fault_address, pgm_metadata.allocation_size_requested, &alloc_backtrace_data, &dealloc_backtrace_data);

        } else {
            /* [ lower_guard ][ allocated ][ remaining ][ upper_guard ] */
            report->alignment = "address left-aligned";

            /* Left-aligned "Lower PGM OOB" checking */
            addr64_t bottom = lower_guard;
            addr64_t top = key;

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields_and_cleanup(enumerator, report, pgm_metadata.free_status ? "UAF" : "OOB", "high", fault_address, pgm_metadata.allocation_size_requested, &alloc_backtrace_data, &dealloc_backtrace_data);

            /* Left-aligned "UAF + OOB" checking towards upper guard page */
            bottom = (addr64_t)(key + pgm_metadata.allocation_size_requested);
            top = (addr64_t)upper_guard;

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields_and_cleanup(enumerator, report, pgm_metadata.free_status ? "UAF" : "OOB", "low", fault_address, pgm_metadata.allocation_size_requested, &alloc_backtrace_data, &dealloc_backtrace_data);

            /* Left-aligned "Upper PGM OOB" checking */
            bottom = upper_guard;
            top = upper_guard + upper_guard_size;

            if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
                return pas_update_report_crash_fields_and_cleanup(enumerator, report, pgm_metadata.free_status ? "long-range UAF" : "long-range OOB", "low", fault_address, pgm_metadata.allocation_size_requested, &alloc_backtrace_data, &dealloc_backtrace_data);
        }

        /* Left-aligned "UAF" checking calculations are same as right-aligned checking "UAF" check */
        addr64_t bottom = (addr64_t)key;
        addr64_t top = (addr64_t)(key + pgm_metadata.allocation_size_requested);

        if (pas_fault_address_is_in_bounds(fault_address, bottom, top))
            return pas_update_report_crash_fields_and_cleanup(enumerator, report, pgm_metadata.free_status ? "UAF" : "undefined", "low", fault_address, pgm_metadata.allocation_size_requested, &alloc_backtrace_data, &dealloc_backtrace_data);
    }

    pas_enumerator_destroy(enumerator);
    return KERN_NOT_FOUND;
}
#endif /* __APPLE__ */

#endif /* LIBPAS_ENABLED */
