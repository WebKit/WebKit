/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "CompilerTimingScope.h"
#include "DFGCommon.h"
#include "DFGGraph.h"

namespace JSC { namespace DFG {

class Phase {
public:
    Phase(Graph& graph, const char* name)
        : m_graph(graph)
        , m_name(name)
    {
        beginPhase();
    }
    
    ~Phase()
    {
        endPhase();
    }
    
    const char* name() const { return m_name; }
    
    Graph& graph() { return m_graph; }
    
    // Each phase must have a run() method.
    
    Prefix prefix;

protected:
    // Things you need to have a DFG compiler phase.
    Graph& m_graph;
    
    VM& vm() { return m_graph.m_vm; }
    CodeBlock* codeBlock() { return m_graph.m_codeBlock; }
    CodeBlock* profiledBlock() { return m_graph.m_profiledBlock; }

    // This runs validation, and uses the graph dump before the phase if possible.
    void validate();
    
    const char* m_name;
    
private:
    // Call these hooks when starting and finishing.
    void beginPhase();
    void endPhase();
    
    CString m_graphDumpBeforePhase;
};

template<typename PhaseType>
bool runAndLog(PhaseType& phase)
{
    CompilerTimingScope timingScope("DFG", phase.name());
    
    bool result = phase.run();

    if (result && logCompilationChanges(phase.graph().m_plan.mode()))
        dataLogLn(phase.graph().prefix(), "Phase ", phase.name(), " changed the IR.\n");
    return result;
}

template<typename PhaseType, typename... Args>
bool runPhase(Graph& graph, Args... args)
{
    PhaseType phase(graph, args...);
    return runAndLog(phase);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
