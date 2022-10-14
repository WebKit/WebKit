/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "DFGValidateUnlinked.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGPhase.h"
#include "DFGPhiChildren.h"
#include "JSCJSValueInlines.h"

namespace JSC::DFG {

class ValidateUnlinked final : public Phase {
    static constexpr bool verbose = false;

public:
    ValidateUnlinked(Graph& graph)
        : Phase(graph, "uDFG validation")
    {
    }

    bool run();
    bool validateNode(Node*);

private:
};

bool ValidateUnlinked::run()
{
    for (BasicBlock* block : m_graph.blocksInPreOrder()) {
        for (Node* node : *block) {
            if (!validateNode(node))
                return false;
        }
    }
    return true;
}

bool ValidateUnlinked::validateNode(Node* node)
{
    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
    if (globalObject != m_graph.m_codeBlock->globalObject()) {
        if (UNLIKELY(Options::dumpUnlinkedDFGValidation())) {
            m_graph.logAssertionFailure(node, __FILE__, __LINE__, WTF_PRETTY_FUNCTION, "Bad GlobalObject");
            dataLogLn(RawPointer(globalObject), " != ", RawPointer(m_graph.m_codeBlock->globalObject()));
        }
        return false;
    }
    return true;
}


CapabilityLevel canCompileUnlinked(Graph& graph)
{
    ValidateUnlinked phase(graph);
    if (!phase.run())
        return CapabilityLevel::CannotCompile;
    return CapabilityLevel::CanCompileAndInline;
}

} // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
