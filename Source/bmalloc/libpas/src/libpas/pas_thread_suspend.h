/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#ifndef PAS_THREAD_SUSPEND_H
#define PAS_THREAD_SUSPEND_H

#include "pas_config.h"

#include "pas_machine_registers.h"
#include "pas_utils.h"

#if !PAS_OS(WINDOWS)
#include <pthread.h>
#endif

PAS_BEGIN_EXTERN_C;

#if PAS_OS(WINDOWS)
    typedef void* pas_native_thread_handle;
#else
    typedef pthread_t pas_native_thread_handle;
#endif

typedef struct pas_thread_suspend_data {
    bool did_suspend;
    pas_native_thread_handle native_thread;
    pas_machine_stack_bounds stack_bounds;
    pas_machine_registers* registers;
} pas_thread_suspend_data;

static PAS_ALWAYS_INLINE pas_thread_suspend_data pas_thread_suspend_data_create(pas_native_thread_handle thread, pas_machine_stack_bounds bounds)
{
    pas_thread_suspend_data data;
    data.did_suspend = false;
    data.native_thread = thread;
    data.stack_bounds = bounds;
    data.registers = 0;
    return data;
}

PAS_API void pas_thread_suspend_initialize(void);
#if PAS_OS(DARWIN)
PAS_API bool pas_thread_suspend_suspend(pthread_t thread);
PAS_API void pas_thread_suspend_resume(pthread_t thread);
#else
PAS_API bool pas_thread_suspend_suspend(pas_thread_suspend_data*);
PAS_API void pas_thread_suspend_resume(pas_thread_suspend_data*);

// Returns a pointer to register state valid until pas_thread_suspend_resume.
// scratch must outlive the suspended period and the return value
PAS_API pas_machine_registers* pas_thread_suspend_get_registers(
    pas_thread_suspend_data* data, pas_machine_registers* scratch);
#endif

PAS_END_EXTERN_C;

#endif /* PAS_THREAD_SUSPEND_H */
