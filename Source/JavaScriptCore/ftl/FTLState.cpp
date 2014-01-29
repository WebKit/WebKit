/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "FTLState.h"

#if ENABLE(FTL_JIT)

#include "CodeBlockWithJITType.h"
#include "FTLForOSREntryJITCode.h"
#include "FTLJITCode.h"
#include "FTLJITFinalizer.h"

namespace JSC { namespace FTL {

using namespace DFG;

State::State(Graph& graph)
    : graph(graph)
    , context(llvm->ContextCreate())
    , module(0)
    , function(0)
    , generatedFunction(0)
    , compactUnwind(0)
    , compactUnwindSize(0)
{
    switch (graph.m_plan.mode) {
    case FTLMode: {
        jitCode = adoptRef(new JITCode());
        break;
    }
    case FTLForOSREntryMode: {
        RefPtr<ForOSREntryJITCode> code = adoptRef(new ForOSREntryJITCode());
        code->initializeEntryBuffer(graph.m_vm, graph.m_profiledBlock->m_numCalleeRegisters);
        code->setBytecodeIndex(graph.m_plan.osrEntryBytecodeIndex);
        jitCode = code;
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    finalizer = new JITFinalizer(graph.m_plan);
    graph.m_plan.finalizer = adoptPtr(finalizer);
}

State::~State()
{
    llvm->ContextDispose(context);
}

void State::dumpState(const char* when)
{
    dataLog("LLVM IR for ", CodeBlockWithJITType(graph.m_codeBlock, FTL::JITCode::FTLJIT), " ", when, ":\n");
    dumpModule(module);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

