/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "CompilationResult.h"
#include "ExecutionCounter.h"
#include "Options.h"
#include <wtf/Atomics.h>
#include <wtf/SegmentedVector.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace Wasm {

class OSREntryData;

// This class manages the tier up counts for Wasm binaries. The main interesting thing about
// wasm tiering up counts is that the least significant bit indicates if the tier up has already
// started. Also, wasm code does not atomically update this count. This is because we
// don't care too much if the countdown is slightly off. The tier up trigger is atomic, however,
// so tier up will be triggered exactly once.
class TierUpCount : public UpperTierExecutionCounter {
    WTF_MAKE_NONCOPYABLE(TierUpCount);
public:
    enum class TriggerReason : uint8_t {
        DontTrigger,
        CompilationDone,
        StartCompilation,
    };

    enum class CompilationStatus : uint8_t {
        NotCompiled,
        StartCompilation,
        Compiled,
    };

    TierUpCount();
    ~TierUpCount();

    static int32_t loopIncrement() { return Options::omgTierUpCounterIncrementForLoop(); }
    static int32_t functionEntryIncrement() { return Options::omgTierUpCounterIncrementForEntry(); }

    SegmentedVector<TriggerReason, 16>& osrEntryTriggers() { return m_osrEntryTriggers; }
    Vector<uint32_t>& outerLoops() { return m_outerLoops; }
    Lock& getLock() { return m_lock; }

    OSREntryData& addOSREntryData(uint32_t functionIndex, uint32_t loopIndex);

    void optimizeAfterWarmUp(uint32_t functionIndex)
    {
        dataLogLnIf(Options::verboseOSR(), functionIndex, ": OMG-optimizing after warm-up.");
        setNewThreshold(Options::thresholdForOMGOptimizeAfterWarmUp(), nullptr);
    }

    bool checkIfOptimizationThresholdReached()
    {
        return checkIfThresholdCrossedAndSet(nullptr);
    }

    void dontOptimizeAnytimeSoon(uint32_t functionIndex)
    {
        dataLogLnIf(Options::verboseOSR(), functionIndex, ": Not OMG-optimizing anytime soon.");
        deferIndefinitely();
    }

    void optimizeNextInvocation(uint32_t functionIndex)
    {
        dataLogLnIf(Options::verboseOSR(), functionIndex, ": OMG-optimizing next invocation.");
        setNewThreshold(0, nullptr);
    }

    void optimizeSoon(uint32_t functionIndex)
    {
        dataLogLnIf(Options::verboseOSR(), functionIndex, ": OMG-optimizing soon.");
        // FIXME: Need adjustment once we get more information about wasm functions.
        setNewThreshold(Options::thresholdForOMGOptimizeSoon(), nullptr);
    }

    void setOptimizationThresholdBasedOnCompilationResult(uint32_t functionIndex, CompilationResult result)
    {
        switch (result) {
        case CompilationSuccessful:
            optimizeNextInvocation(functionIndex);
            return;
        case CompilationFailed:
            dontOptimizeAnytimeSoon(functionIndex);
            return;
        case CompilationDeferred:
            optimizeAfterWarmUp(functionIndex);
            return;
        case CompilationInvalidated:
            // This is weird - it will only happen in cases when the DFG code block (i.e.
            // the code block that this JITCode belongs to) is also invalidated. So it
            // doesn't really matter what we do. But, we do the right thing anyway. Note
            // that us counting the reoptimization actually means that we might count it
            // twice. But that's generally OK. It's better to overcount reoptimizations
            // than it is to undercount them.
            optimizeAfterWarmUp(functionIndex);
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    Lock m_lock;
    CompilationStatus m_compilationStatusForOMG { CompilationStatus::NotCompiled };
    CompilationStatus m_compilationStatusForOMGForOSREntry { CompilationStatus::NotCompiled };
    SegmentedVector<TriggerReason, 16> m_osrEntryTriggers;
    Vector<uint32_t> m_outerLoops;
    Vector<std::unique_ptr<OSREntryData>> m_osrEntryData;
};
    
} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
