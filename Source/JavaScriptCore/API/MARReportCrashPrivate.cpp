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

#ifdef __APPLE__

#if !USE(SYSTEM_MALLOC)
#include <bmalloc/BPlatform.h>
#if BENABLE(LIBPAS)
#include <bmalloc/pas_mar_report_crash.h>
#endif
#endif

kern_return_t MARReportCrashExtractResults(vm_address_t faultAddress, mach_vm_address_t marGlobalRegistry, unsigned version, task_t task, pas_mar_crash_report* report, crash_reporter_memory_reader_t crmReader)
{
#if !USE(SYSTEM_MALLOC)
#if BENABLE(LIBPAS)
    return pas_mar_extract_crash_report(faultAddress, marGlobalRegistry, version, task, report, crmReader);
#endif
#endif
    UNUSED_PARAM(faultAddress);
    UNUSED_PARAM(marGlobalRegistry);
    UNUSED_PARAM(version);
    UNUSED_PARAM(task);
    UNUSED_PARAM(report);
    UNUSED_PARAM(crmReader);
    return KERN_FAILURE;
}
#endif /* __APPLE__ */

