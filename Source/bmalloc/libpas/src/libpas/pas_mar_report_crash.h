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

#pragma once

#include "pas_mar_crash_reporter_report.h"
#include "pas_platform.h"
#include "pas_mar_registry.h"

#include <stddef.h>

#if PAS_OS(DARWIN)

PAS_BEGIN_EXTERN_C;

extern PAS_API kern_return_t pas_mar_populate_crash_report(pas_mar_crash_report*, const char* error_type, const char* confidence,
        vm_address_t fault_address, size_t allocation_size_bytes,
        pas_mar_backtrace* allocation_backtrace, pas_mar_backtrace* deallocation_backtrace);

extern PAS_API kern_return_t pas_mar_extract_crash_report(vm_address_t fault_address, mach_vm_address_t mar_global_registry, unsigned version, task_t, pas_mar_crash_report*, crash_reporter_memory_reader_t crm_reader);

PAS_END_EXTERN_C;

#endif /* PAS_OS(DARWIN) */
