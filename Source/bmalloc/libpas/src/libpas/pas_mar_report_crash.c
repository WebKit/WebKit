/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "pas_mar_report_crash.h"

#include "pas_mar_crash_reporter_report.h"

#if PAS_OS(DARWIN)

#include <malloc/malloc.h>

PAS_BEGIN_EXTERN_C;

kern_return_t pas_mar_populate_crash_report(pas_mar_crash_report* report, const char* error_type, const char* confidence,
        vm_address_t fault_address, size_t allocation_size_bytes,
        pas_mar_backtrace* allocation_backtrace, pas_mar_backtrace* deallocation_backtrace)
{
    report->error_type = error_type;
    report->confidence = confidence;
    report->fault_address = fault_address;
    report->allocation_size_bytes = allocation_size_bytes;

    report->allocation_backtrace.num_frames = allocation_backtrace->num_frames;
    memcpy(&report->allocation_backtrace.backtrace_buffer, &allocation_backtrace->backtrace_buffer, sizeof(report->allocation_backtrace.backtrace_buffer));

    report->deallocation_backtrace.num_frames = deallocation_backtrace->num_frames;
    memcpy(&report->deallocation_backtrace.backtrace_buffer, &deallocation_backtrace->backtrace_buffer, sizeof(report->deallocation_backtrace.backtrace_buffer));
    return KERN_SUCCESS;
}

// Note that local_memory will be invalidated by future calls to the reader.
// FIXME: improve this interface (rdar://161831626)

static crash_reporter_memory_reader_t memory_reader;

static kern_return_t memory_reader_adapter(task_t task, vm_address_t address, vm_size_t size, void** local_memory)
{
    if (!local_memory)
        return KERN_FAILURE;

    void* ptr = memory_reader(task, address, size);
    *local_memory = ptr;
    return ptr ? KERN_SUCCESS : KERN_FAILURE;
}

static memory_reader_t* setup_memory_reader(crash_reporter_memory_reader_t crm_reader)
{
    memory_reader = crm_reader;
    return memory_reader_adapter;
}

kern_return_t pas_mar_extract_crash_report(vm_address_t fault_address, mach_vm_address_t mar_global_registry, unsigned version, task_t task, pas_mar_crash_report* report, crash_reporter_memory_reader_t crm_reader)
{
    if (version != pas_mar_crash_report_version)
        return KERN_FAILURE;

    pas_mar_registry* dead_registry = NULL;
    memory_reader_t* reader = setup_memory_reader(crm_reader);
    kern_return_t kr = reader(task, mar_global_registry, sizeof(pas_mar_registry), (void**)&dead_registry);
    if (kr != KERN_SUCCESS)
        return KERN_FAILURE;

    struct pas_mar_exported_allocation_record result = pas_mar_get_allocation_record((pas_mar_registry*)dead_registry, (void*)fault_address);

    if (!result.is_valid)
        return KERN_NOT_FOUND;

    if (!result.deallocation_trace.num_frames)
        return pas_mar_populate_crash_report(report, "UAF", "high", fault_address, result.allocation_size_bytes, &result.allocation_trace, &result.deallocation_trace);
    return pas_mar_populate_crash_report(report, "bad access", "high", fault_address, result.allocation_size_bytes, &result.allocation_trace, &result.deallocation_trace);
}

PAS_END_EXTERN_C;

#endif /* PAS_OS(DARWIN) */

#endif /* LIBPAS_ENABLED */
