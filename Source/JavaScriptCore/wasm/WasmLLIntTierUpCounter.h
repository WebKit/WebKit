/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ExecutionCounter.h"
#include "InstructionStream.h"
#include "Options.h"
#include <wtf/HashMap.h>

namespace JSC { namespace Wasm {

class LLIntTierUpCounter : public BaselineExecutionCounter {
    WTF_MAKE_NONCOPYABLE(LLIntTierUpCounter);

public:
    enum class CompilationStatus : uint8_t {
        NotCompiled,
        Compiling,
        Compiled,
    };

    LLIntTierUpCounter()
    {
        optimizeAfterWarmUp();
    }

    void optimizeAfterWarmUp()
    {
        if (Options::wasmLLIntTiersUpToBBQ())
            setNewThreshold(Options::thresholdForBBQOptimizeAfterWarmUp());
        else
            setNewThreshold(Options::thresholdForOMGOptimizeAfterWarmUp());
    }

    bool checkIfOptimizationThresholdReached()
    {
        return checkIfThresholdCrossedAndSet(nullptr);
    }

    void optimizeSoon()
    {
        if (Options::wasmLLIntTiersUpToBBQ())
            setNewThreshold(Options::thresholdForBBQOptimizeSoon());
        else
            setNewThreshold(Options::thresholdForOMGOptimizeSoon());
    }

    struct OSREntryData {
        uint32_t loopIndex;
        Vector<VirtualRegister> values;
    };

    void addOSREntryDataForLoop(InstructionStream::Offset, OSREntryData&&);

    const OSREntryData& osrEntryDataForLoop(InstructionStream::Offset) const;

    Lock m_lock;
    CompilationStatus m_compilationStatus { CompilationStatus::NotCompiled };
    CompilationStatus m_loopCompilationStatus { CompilationStatus::NotCompiled };
    HashMap<InstructionStream::Offset, OSREntryData> m_osrEntryData;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
