/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#include "VMEntryScope.h"

#include "Options.h"
#include "SamplingProfiler.h"
#include "VM.h"
#include "VMEntryScopeInlines.h"
#include "WasmCapabilities.h"
#include "WasmMachineThreads.h"
#include "Watchdog.h"
#include <wtf/SystemTracing.h>
#include <wtf/WTFConfig.h>

namespace JSC {

void VMEntryScope::setUpSlow()
{
    m_vm.entryScope = this;

    auto& thread = Thread::current();
    if (UNLIKELY(!thread.isJSThread())) {
        Thread::registerJSThread(thread);

        if (Wasm::isSupported())
            Wasm::startTrackingCurrentThread();
#if HAVE(MACH_EXCEPTIONS)
        if (g_wtfConfig.signalHandlers.initState == WTF::SignalHandlers::InitState::AddedHandlers)
            registerThreadForMachExceptionHandling(thread);
#endif
    }

    m_vm.firePrimitiveGigacageEnabledIfNecessary();

    // Reset the date cache between JS invocations to force the VM to
    // observe time zone changes.
    m_vm.resetDateCacheIfNecessary();

    if (UNLIKELY(m_vm.watchdog()))
        m_vm.watchdog()->enteredVM();

#if ENABLE(SAMPLING_PROFILER)
    {
        SamplingProfiler* samplingProfiler = m_vm.samplingProfiler();
        if (UNLIKELY(samplingProfiler))
            samplingProfiler->noticeVMEntry();
    }
#endif
    if (UNLIKELY(Options::useTracePoints()))
        tracePoint(VMEntryScopeStart);
}

void VMEntryScope::tearDownSlow()
{
    ASSERT_WITH_MESSAGE(!m_vm.hasCheckpointOSRSideState(), "Exitting the VM but pending checkpoint side state still available");

    if (UNLIKELY(Options::useTracePoints()))
        tracePoint(VMEntryScopeEnd);
    
    if (UNLIKELY(m_vm.watchdog()))
        m_vm.watchdog()->exitedVM();

    m_vm.entryScope = nullptr;
    m_vm.invokeDidPopListeners();

    // If the trap bit is still set at this point, then it means that VMTraps::handleTraps()
    // has not yet been called for this termination request. As a result, we've not thrown a
    // TerminationException yet. Some client code relies on detecting the presence of the
    // TerminationException in order to signal that a termination was requested. As a result,
    // we want to stay in the TerminationInProgress state until VMTraps::handleTraps() (which
    // clears the trap bit) gets called, and the TerminationException gets thrown.
    //
    // Note: perhaps there's a better way for the client to know that a termination was
    // requested (after all, the request came from the client). However, this is how the
    // client code currently works. Changing that will take some significant effort to hunt
    // down all the places in client code that currently rely on this behavior.
    if (!m_vm.traps().needHandling(VMTraps::NeedTermination))
        m_vm.setTerminationInProgress(false);
    m_vm.clearScratchBuffers();
}

} // namespace JSC
