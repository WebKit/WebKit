/*
 * Copyright (C) 2014-2026 Apple Inc. All rights reserved.
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

#include "DFGCFG.h"
#include "DFGDominators.h"
#include "DFGGraph.h"
#include <wtf/SSACalculator.h>

namespace JSC { namespace DFG {

class SSACalculatorAdapter {
public:
    using CFG = JSC::DFG::SSACFG;
    using Value = JSC::DFG::Node*;

    SSACalculatorAdapter(Graph& graph)
        : m_graph(graph)
    {
    }

    CFG& cfg() const { return selectCFG<SSACFG>(m_graph); }
    SSADominators& dominators() { return m_graph.ensureSSADominators(); }

    static void dumpValue(PrintStream& out, Value value) { out.print(value); }

private:
    Graph& m_graph;
};

using SSACalculator = WTF::SSACalculator<SSACalculatorAdapter>;

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
