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
#include "DFGTierUpCheckInjectionPhase.h"

#if ENABLE(DFG_JIT)

#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGPhase.h"
#include "FTLCapabilities.h"
#include "Operations.h"

namespace JSC { namespace DFG {

class TierUpCheckInjectionPhase : public Phase {
public:
    TierUpCheckInjectionPhase(Graph& graph)
        : Phase(graph, "tier-up check injection")
    {
    }
    
    bool run()
    {
        RELEASE_ASSERT(m_graph.m_plan.mode == DFGMode);
        
        if (!Options::useExperimentalFTL())
            return false;

#if ENABLE(FTL_JIT)
        FTL::CapabilityLevel level = FTL::canCompile(m_graph);
        if (level == FTL::CannotCompile)
            return false;
        
        InsertionSet insertionSet(m_graph);
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            if (block->at(0)->op() == LoopHint) {
                CodeOrigin codeOrigin = block->at(0)->codeOrigin;
                NodeType nodeType;
                if (level == FTL::CanCompileAndOSREnter && !codeOrigin.inlineCallFrame) {
                    nodeType = CheckTierUpAndOSREnter;
                    RELEASE_ASSERT(block->bytecodeBegin == codeOrigin.bytecodeIndex);
                } else
                    nodeType = CheckTierUpInLoop;
                insertionSet.insertNode(1, SpecNone, nodeType, codeOrigin);
            }
            
            if (block->last()->op() == Return) {
                insertionSet.insertNode(
                    block->size() - 1, SpecNone, CheckTierUpAtReturn, block->last()->codeOrigin);
            }
            
            insertionSet.execute(block);
        }
        
        return true;
#else // ENABLE(FTL_JIT)
        RELEASE_ASSERT_NOT_REACHED();
        return false;
#endif // ENABLE(FTL_JIT)
    }
};

bool performTierUpCheckInjection(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Tier-up Check Injection");
    return runPhase<TierUpCheckInjectionPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


