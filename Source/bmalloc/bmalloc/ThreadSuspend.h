/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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

#include "BExport.h"
#include "BInline.h"
#include "BPlatform.h"

#ifdef __cplusplus

#include "pas_thread_suspend.h"
#if !BOS(DARWIN) && !BOS(WINDOWS)
#include "pas_thread_suspend_signal_handler.h"
#endif

#if !BOS(WINDOWS)
#include <pthread.h>
#endif

namespace bmalloc { namespace api {

BINLINE void threadSuspendSignalHandlerInstall(void)
{
    pas_thread_suspend_initialize();
}

#if !BOS(DARWIN) && !BOS(WINDOWS)
BEXPORT int threadSuspendSignalNumber();

BINLINE void setThreadSuspendSignal(int signal)
{
    pas_thread_suspend_set_signal(signal);
}
#endif // !BOS(DARWIN) && !BOS(WINDOWS)

#if BOS(DARWIN)
BINLINE bool suspendThread(pthread_t thread)
{
    return pas_thread_suspend_suspend(thread);
}

BINLINE void resumeThread(pthread_t thread)
{
    pas_thread_suspend_resume(thread);
}
#else // BOS(DARWIN)
class ThreadSuspendData {
    public:

    BINLINE ThreadSuspendData(pas_native_thread_handle thread, void* stackOrigin, void* stackBounds)
        : data(pas_thread_suspend_data_create(thread, pas_machine_stack_bounds { stackOrigin, stackBounds }))
    {
    }

    // Returns a pointer to register state valid until resumeThread.
    // scratch must outlive the suspended period
    BINLINE pas_machine_registers* getRegisters(pas_machine_registers* scratch)
    {
        return pas_thread_suspend_get_registers(&data, scratch);
    }

private:
    pas_thread_suspend_data data;
    friend bool suspendThread(ThreadSuspendData&);
    friend void resumeThread(ThreadSuspendData&);
};

BINLINE bool suspendThread(ThreadSuspendData& data)
{
    bool result = pas_thread_suspend_suspend(&data.data);
    return result;
}

BINLINE void resumeThread(ThreadSuspendData& data)
{
    pas_thread_suspend_resume(&data.data);
}

#endif // BOS(DARWIN)

} } // namespace bmalloc::api

#endif // __cplusplus
