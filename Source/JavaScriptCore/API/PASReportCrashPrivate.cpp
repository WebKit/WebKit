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

#include "PASReportCrashPrivate.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <bmalloc/pas_report_crash.h>
#endif

using namespace JSC;

#ifdef __APPLE__
kern_return_t PASReportCrashExtractResults(vm_address_t fault_address, mach_vm_address_t pas_dead_root, unsigned version, task_t task, pas_report_crash_pgm_report *report, crash_reporter_memory_reader_t crm_reader)
{
#if TARGET_OS_WATCH
    UNUSED_PARAM(fault_address);
    UNUSED_PARAM(pas_dead_root);
    UNUSED_PARAM(version);
    UNUSED_PARAM(task);
    UNUSED_PARAM(report);
    UNUSED_PARAM(crm_reader);

    return KERN_FAILURE;
#else
    return pas_report_crash_extract_pgm_failure(fault_address, pas_dead_root, version, task, report, crm_reader);
#endif
}
#endif /* __APPLE__ */
