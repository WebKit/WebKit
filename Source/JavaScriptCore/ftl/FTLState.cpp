/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "FTLForOSREntryJITCode.h"
#include "FTLJITCode.h"
#include "FTLJITFinalizer.h"
#include "FTLPatchpointExceptionHandle.h"

namespace JSC { namespace FTL {

using namespace B3;
using namespace DFG;

State::State(Graph& graph)
    : graph(graph)
{
    switch (graph.m_plan.mode()) {
    case JITCompilationMode::FTL: {
        jitCode = adoptRef(new JITCode());
        break;
    }
    case JITCompilationMode::FTLForOSREntry: {
        RefPtr<ForOSREntryJITCode> code = adoptRef(new ForOSREntryJITCode());
        code->initializeEntryBuffer(graph.m_vm, graph.m_profiledBlock->numCalleeLocals());
        code->setBytecodeIndex(graph.m_plan.osrEntryBytecodeIndex());
        jitCode = code;
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    graph.m_plan.setFinalizer(makeUnique<JITFinalizer>(graph.m_plan));
    finalizer = static_cast<JITFinalizer*>(graph.m_plan.finalizer());

    proc = makeUnique<Procedure>();

    if (graph.m_vm.shouldBuilderPCToCodeOriginMapping())
        proc->setNeedsPCToOriginMap();

    proc->setOriginPrinter(
        [] (PrintStream& out, B3::Origin origin) {
            out.print(bitwise_cast<Node*>(origin.data()));
        });

    proc->setFrontendData(&graph);
}

State::~State()
{
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

