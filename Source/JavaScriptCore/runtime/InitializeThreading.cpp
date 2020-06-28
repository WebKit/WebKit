/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InitializeThreading.h"

#include "DisallowVMReentry.h"
#include "ExecutableAllocator.h"
#include "JSCConfig.h"
#include "JSCPtrTag.h"
#include "LLIntData.h"
#include "Options.h"
#include "SigillCrashAnalyzer.h"
#include "SuperSampler.h"
#include "VMTraps.h"
#include "WasmCalleeRegistry.h"
#include "WasmCapabilities.h"
#include "WasmFaultSignalHandler.h"
#include "WasmThunks.h"
#include <mutex>
#include <wtf/Threading.h>
#include <wtf/threads/Signals.h>

namespace JSC {

static_assert(sizeof(bool) == 1, "LLInt and JIT assume sizeof(bool) is always 1 when touching it directly from assembly code.");

void initialize()
{
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        WTF::initialize();
        Options::initialize();

        initializePtrTagLookup();

#if ENABLE(WRITE_BARRIER_PROFILING)
        WriteBarrierCounters::initialize();
#endif
        {
            Options::AllowUnfinalizedAccessScope scope;
            ExecutableAllocator::initialize();
            VM::computeCanUseJIT();
            if (!g_jscConfig.vm.canUseJIT) {
                Options::useJIT() = false;
                Options::recomputeDependentOptions();
            }
        }
        Options::finalize();

        if (Options::useSigillCrashAnalyzer())
            enableSigillCrashAnalyzer();

        LLInt::initialize();
#ifndef NDEBUG
        DisallowGC::initialize();
        DisallowVMReentry::initialize();
#endif
        initializeSuperSampler();
        Thread& thread = Thread::current();
        thread.setSavedLastStackTop(thread.stack().origin());

#if ENABLE(WEBASSEMBLY)
        if (Wasm::isSupported()) {
            Wasm::Thunks::initialize();
            Wasm::CalleeRegistry::initialize();
        }
#endif

        if (VM::isInMiniMode())
            WTF::fastEnableMiniMode();

#if HAVE(MACH_EXCEPTIONS)
        // JSLock::lock() can call registerThreadForMachExceptionHandling() which crashes if this has not been called first.
        WTF::startMachExceptionHandlerThread();
#endif
        VMTraps::initializeSignals();
#if ENABLE(WEBASSEMBLY)
        Wasm::prepareFastMemory();
#endif

        WTF::compilerFence();
        RELEASE_ASSERT(!g_jscConfig.initializeHasBeenCalled);
        g_jscConfig.initializeHasBeenCalled = true;
    });
}

} // namespace JSC
