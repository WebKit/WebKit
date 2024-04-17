/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
#include "FTLLink.h"

#if ENABLE(FTL_JIT)

#include "CCallHelpers.h"
#include "CodeBlockWithJITType.h"
#include "FTLJITCode.h"
#include "JITOperations.h"
#include "JITThunks.h"
#include "LinkBuffer.h"
#include "ProfilerCompilation.h"
#include "ThunkGenerators.h"

namespace JSC { namespace FTL {

void link(State& state)
{
    using namespace DFG;
    Graph& graph = state.graph;
    CodeBlock* codeBlock = graph.m_codeBlock;

    state.jitCode->common.requiredRegisterCountForExit = graph.requiredRegisterCountForExit();

    if (!graph.m_plan.inlineCallFrames()->isEmpty())
        state.jitCode->common.inlineCallFrames = graph.m_plan.inlineCallFrames();
    if (!graph.m_stringSearchTable8.isEmpty()) {
        FixedVector<std::unique_ptr<BoyerMooreHorspoolTable<uint8_t>>> tables(graph.m_stringSearchTable8.size());
        unsigned index = 0;
        for (auto& entry : graph.m_stringSearchTable8)
            tables[index++] = WTFMove(entry.value);
        state.jitCode->common.m_stringSearchTable8 = WTFMove(tables);
    }

    graph.registerFrozenValues();

    {
        bool dumpDisassembly = shouldDumpDisassembly() || Options::asyncDisassembly();

        MacroAssemblerCodeRef<JSEntryPtrTag> b3CodeRef =
            FINALIZE_CODE_IF(dumpDisassembly, *state.b3CodeLinkBuffer, JSEntryPtrTag, nullptr,
                "FTL B3 code for %s", toCString(CodeBlockWithJITType(codeBlock, JITType::FTLJIT)).data());

        state.jitCode->initializeB3Code(b3CodeRef);
        state.jitCode->common.m_jumpReplacements = WTFMove(state.jumpReplacements);
    }

    state.finalizer->m_codeSize = state.b3CodeLinkBuffer->size();
    state.finalizer->m_jitCode = state.jitCode;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

