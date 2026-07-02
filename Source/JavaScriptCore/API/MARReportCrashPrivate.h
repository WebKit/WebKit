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

#pragma once

#include <JavaScriptCore/JSBase.h>

#ifdef __APPLE__
#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/vm_types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This needs to stay in sync with pas_mar_crash_reporter_report.h */
#define PASMARCrashReportBacktraceSize 31

/*
 * Crash version number: used to keep MAR and ReportCrash in sync
 * This number should monotonically increase every time the layout
 * of mar_crash_report or its subfields change
 * This needs to stay in sync with pas_mar_crash_reporter_report.h
 */
static const unsigned PASMARCrashReportVersion = 1;

typedef void *(*crash_reporter_memory_reader_t)(task_t task, vm_address_t address, size_t size);

typedef struct OpaquePASMARCrashReportBacktrace* PASMARCrashReportBacktraceRef;
typedef struct OpaquePASMARCrashReport* PASMARCrashReportRef;

JS_EXPORT PASMARCrashReportRef MARCrashReportCreate();
JS_EXPORT void MARCrashReportRelease(PASMARCrashReportRef);

JS_EXPORT kern_return_t MARReportCrashExtractResults(vm_address_t fault_address, mach_vm_address_t mar_global_registry, unsigned version, task_t, PASMARCrashReportRef, crash_reporter_memory_reader_t crm_reader);

JS_EXPORT unsigned MARCrashReportGetVersion(PASMARCrashReportRef);
JS_EXPORT const char* MARCrashReportGetErrorType(PASMARCrashReportRef);
JS_EXPORT const char* MARCrashReportGetConfidence(PASMARCrashReportRef);
JS_EXPORT vm_address_t MARCrashReportGetFaultAddress(PASMARCrashReportRef);
JS_EXPORT size_t MARCrashReportGetAllocationSizeBytes(PASMARCrashReportRef);
JS_EXPORT PASMARCrashReportBacktraceRef MARCrashReportGetAllocationBacktrace(PASMARCrashReportRef);
JS_EXPORT PASMARCrashReportBacktraceRef MARCrashReportGetDeallocationBacktrace(PASMARCrashReportRef);

JS_EXPORT unsigned MARCrashReportBacktraceGetNumFrames(PASMARCrashReportBacktraceRef);

/* 
 * This buffer is only valid for MARCrashReportBacktraceGetNumFrames(backtrace) entries, and at most
 * PASMARCrashReportBacktraceSize entries. Accesses beyond these limits are UB.
 */
JS_EXPORT void** MARCrashReportBacktraceGetBacktraceBuffer(PASMARCrashReportBacktraceRef);

#ifdef __cplusplus
}
#endif

#endif /* __APPLE__ */

