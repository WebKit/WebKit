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

#include "config.h"
#include "MARReportCrashPrivate.h"
#include <bmalloc/BPlatform.h>

#ifdef __APPLE__

#if BENABLE(LIBPAS)
#include <bmalloc/pas_mar_report_crash.h>
#endif

#if BENABLE(LIBPAS)

ALWAYS_INLINE pas_mar_backtrace* toInternalRepresentation(PASMARCrashReportBacktraceRef backtrace)
{
    return reinterpret_cast<pas_mar_backtrace*>(backtrace);
}

ALWAYS_INLINE pas_mar_crash_report* toInternalRepresentation(PASMARCrashReportRef report)
{
    return reinterpret_cast<pas_mar_crash_report*>(report);
}

#endif /* BENABLE(LIBPAS) */

kern_return_t MARReportCrashExtractResults(vm_address_t faultAddress, mach_vm_address_t marGlobalRegistry, unsigned version, task_t task, PASMARCrashReportRef report, crash_reporter_memory_reader_t crmReader)
{
#if BENABLE(LIBPAS)
    return pas_mar_extract_crash_report(faultAddress, marGlobalRegistry, version, task, toInternalRepresentation(report), crmReader);
#endif
    UNUSED_PARAM(faultAddress);
    UNUSED_PARAM(marGlobalRegistry);
    UNUSED_PARAM(version);
    UNUSED_PARAM(task);
    UNUSED_PARAM(report);
    UNUSED_PARAM(crmReader);
    return KERN_FAILURE;
}

#if BENABLE(LIBPAS)

PASMARCrashReportRef MARCrashReportCreate()
{
    void* result = fastMalloc(sizeof(pas_mar_crash_report));
    return reinterpret_cast<PASMARCrashReportRef>(new (result) pas_mar_crash_report);
}

void MARCrashReportRelease(PASMARCrashReportRef report)
{
    fastFree(report);
}

unsigned MARCrashReportGetVersion(PASMARCrashReportRef report)
{
    return toInternalRepresentation(report)->report_version;
}

const char* MARCrashReportGetErrorType(PASMARCrashReportRef report)
{
    return toInternalRepresentation(report)->error_type;
}

const char* MARCrashReportGetConfidence(PASMARCrashReportRef report)
{
    return toInternalRepresentation(report)->confidence;
}

vm_address_t MARCrashReportGetFaultAddress(PASMARCrashReportRef report)
{
    return toInternalRepresentation(report)->fault_address;
}

size_t MARCrashReportGetAllocationSizeBytes(PASMARCrashReportRef report)
{
    return toInternalRepresentation(report)->allocation_size_bytes;
}

PASMARCrashReportBacktraceRef MARCrashReportGetAllocationBacktrace(PASMARCrashReportRef report)
{
    return reinterpret_cast<PASMARCrashReportBacktraceRef>(&toInternalRepresentation(report)->allocation_backtrace);
}

PASMARCrashReportBacktraceRef MARCrashReportGetDeallocationBacktrace(PASMARCrashReportRef report)
{
    return reinterpret_cast<PASMARCrashReportBacktraceRef>(&toInternalRepresentation(report)->deallocation_backtrace);
}

unsigned MARCrashReportBacktraceGetNumFrames(PASMARCrashReportBacktraceRef backtrace)
{
    return toInternalRepresentation(backtrace)->num_frames;
}

void** MARCrashReportBacktraceGetBacktraceBuffer(PASMARCrashReportBacktraceRef backtrace)
{
    return toInternalRepresentation(backtrace)->backtrace_buffer;
}

#else

IGNORE_CLANG_WARNINGS_BEGIN("missing-noreturn")

PASMARCrashReportRef MARCrashReportCreate()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void MARCrashReportRelease(PASMARCrashReportRef)
{
    ASSERT_NOT_REACHED();
}

unsigned MARCrashReportGetVersion(PASMARCrashReportRef)
{
    ASSERT_NOT_REACHED();
    return 0;
}

const char* MARCrashReportGetErrorType(PASMARCrashReportRef)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

const char* MARCrashReportGetConfidence(PASMARCrashReportRef)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

vm_address_t MARCrashReportGetFaultAddress(PASMARCrashReportRef)
{
    ASSERT_NOT_REACHED();
    return 0;
}

size_t MARCrashReportGetAllocationSizeBytes(PASMARCrashReportRef)
{
    ASSERT_NOT_REACHED();
    return 0;
}

PASMARCrashReportBacktraceRef MARCrashReportGetAllocationBacktrace(PASMARCrashReportRef)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

PASMARCrashReportBacktraceRef MARCrashReportGetDeallocationBacktrace(PASMARCrashReportRef)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

unsigned MARCrashReportBacktraceGetNumFrames(PASMARCrashReportBacktraceRef)
{
    ASSERT_NOT_REACHED();
    return 0;
}

void** MARCrashReportBacktraceGetBacktraceBuffer(PASMARCrashReportBacktraceRef)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

IGNORE_CLANG_WARNINGS_END // "missing-noreturn"

#endif /* BENABLE(LIBPAS) */

#endif /* __APPLE__ */

