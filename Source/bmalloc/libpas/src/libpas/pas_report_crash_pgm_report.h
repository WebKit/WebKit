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


#ifndef PAS_REPORT_CRASH_PGM_REPORT_H
#define PAS_REPORT_CRASH_PGM_REPORT_H

/* This file exposes a SPI between OSAnalytics and libpas ultimately called through
 * JavaScriptCore. Upon crashing of a process, on Apple platforms, ReportCrash will call
 * into libpas (through JSC) to inspect whether it was a PGM crash in libpas or not. We will report
 * back results from libpas with any information about the PGM crash. This will be logged in
 * the local crash report logs generated on the device. */

#ifdef __APPLE__

#include "pas_backtrace_metadata.h"
#include <mach/mach_types.h>
#include <mach/vm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read memory from crashed process. */
typedef void *(*crash_reporter_memory_reader_t)(task_t task, vm_address_t address, size_t size);

/* This must be in sync between ReportCrash and libpas to generate a report. 
 * Make sure to bump version number after changing extraction structs and logic */
static const unsigned pas_crash_report_version = 4;

/* Report sent back to the ReportCrash process. */
typedef struct pas_report_crash_pgm_report pas_report_crash_pgm_report;
struct pas_report_crash_pgm_report {
    const char* error_type;
    const char* confidence;
    const char* alignment;
    vm_address_t fault_address;
    size_t allocation_size;
    pas_backtrace_metadata* alloc_backtrace;
    pas_backtrace_metadata* dealloc_backtrace;
    bool pgm_has_been_used;
};
#endif /* __APPLE__ */

#ifdef __cplusplus
}
#endif

#endif /* PAS_REPORT_CRASH_PGM_REPORT_H */
