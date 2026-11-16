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


#ifndef MAR_CRASH_REPORTER_REPORT_H
#define MAR_CRASH_REPORTER_REPORT_H

#include <stddef.h>

#ifdef __APPLE__

#include <mach/mach_types.h>
#include <mach/vm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAS_MAR_BACKTRACE_MAX_SIZE 31

/* Read memory from crashed process. */
typedef void *(*crash_reporter_memory_reader_t)(task_t task, vm_address_t address, size_t size);

// Crash version number: used to keep MAR and ReportCrash in sync
// This number should monotonically increase every time the layout
// of mar_crash_report or its subfields change
static const unsigned pas_mar_crash_report_version = 1;

typedef struct pas_mar_backtrace pas_mar_backtrace;
struct pas_mar_backtrace {
    unsigned num_frames;
    void* backtrace_buffer[PAS_MAR_BACKTRACE_MAX_SIZE];
};

typedef struct pas_mar_crash_report pas_mar_crash_report;
struct pas_mar_crash_report {
    unsigned report_version;
    const char* error_type;
    const char* confidence;
    vm_address_t fault_address;
    size_t allocation_size_bytes;
    pas_mar_backtrace allocation_backtrace;
    pas_mar_backtrace deallocation_backtrace;
};

#ifdef __cplusplus
}
#endif

#endif /* __APPLE__ */

#endif /* MAR_CRASH_REPORTER_REPORT_H */

