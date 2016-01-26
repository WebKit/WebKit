/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "DFGNaturalLoops.h"
#include "DFGPhase.h"
#include "FTLCapabilities.h"
#include "JSCInlines.h"

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
        
        if (!Options::useFTLJIT())
            return false;
        
        if (m_graph.m_profiledBlock->m_didFailFTLCompilation)
            return false;

        if (!Options::bytecodeRangeToFTLCompile().isInRange(m_graph.m_profiledBlock->instructionCount()))
            return false;

#if ENABLE(FTL_JIT)
        FTL::CapabilityLevel level = FTL::canCompile(m_graph);
        if (level == FTL::CannotCompile)
            return false;
        
        if (!Options::useOSREntryToFTL())
            level = FTL::CanCompile;

        // First we find all the loops that contain a LoopHint for which we cannot OSR enter.
        // We use that information to decide if we need CheckTierUpAndOSREnter or CheckTierUpWithNestedTriggerAndOSREnter.
        m_graph.ensureNaturalLoops();
        NaturalLoops& naturalLoops = *m_graph.m_naturalLoops;

        HashSet<const NaturalLoop*> loopsContainingLoopHintWithoutOSREnter = findLoopsContainingLoopHintWithoutOSREnter(naturalLoops, level);
        
        InsertionSet insertionSet(m_graph);
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                if (node->op() != LoopHint)
                    continue;

                NodeOrigin origin = node->origin;
                if (canOSREnterAtLoopHint(level, block, nodeIndex)) {
                    const NaturalLoop* loop = naturalLoops.innerMostLoopOf(block);
                    if (loop && loopsContainingLoopHintWithoutOSREnter.contains(loop))
                        insertionSet.insertNode(nodeIndex + 1, SpecNone, CheckTierUpWithNestedTriggerAndOSREnter, origin);
                    else
                        insertionSet.insertNode(nodeIndex + 1, SpecNone, CheckTierUpAndOSREnter, origin);
                } else
                    insertionSet.insertNode(nodeIndex + 1, SpecNone, CheckTierUpInLoop, origin);
                break;
            }
            
            NodeAndIndex terminal = block->findTerminal();
            if (terminal.node->isFunctionTerminal()) {
                insertionSet.insertNode(
                    terminal.index, SpecNone, CheckTierUpAtReturn, terminal.node->origin);
            }
            
            insertionSet.execute(block);
        }
        
        m_graph.m_plan.willTryToTierUp = true;
        return true;
#else // ENABLE(FTL_JIT)
        RELEASE_ASSERT_NOT_REACHED();
        return false;
#endif // ENABLE(FTL_JIT)
    }

private:
#if ENABLE(FTL_JIT)
    bool canOSREnterAtLoopHint(FTL::CapabilityLevel level, const BasicBlock* block, unsigned nodeIndex)
    {
        Node* node = block->at(nodeIndex);
        ASSERT(node->op() == LoopHint);

        NodeOrigin origin = node->origin;
        if (level != FTL::CanCompileAndOSREnter || origin.semantic.inlineCallFrame)
            return false;

        // We only put OSR checks for the first LoopHint in the block. Note that
        // more than one LoopHint could happen in cases where we did a lot of CFG
        // simplification in the bytecode parser, but it should be very rare.
        for (unsigned subNodeIndex = nodeIndex; subNodeIndex--;) {
            if (!block->at(subNodeIndex)->isSemanticallySkippable())
                return false;
        }
        return true;
    }

    HashSet<const NaturalLoop*> findLoopsContainingLoopHintWithoutOSREnter(const NaturalLoops& naturalLoops, FTL::CapabilityLevel level)
    {
        HashSet<const NaturalLoop*> loopsContainingLoopHintWithoutOSREnter;
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                if (node->op() != LoopHint)
                    continue;

                if (!canOSREnterAtLoopHint(level, block, nodeIndex)) {
                    const NaturalLoop* loop = naturalLoops.innerMostLoopOf(block);
                    while (loop) {
                        loopsContainingLoopHintWithoutOSREnter.add(loop);
                        loop = naturalLoops.innerMostOuterLoop(*loop);
                    }
                }
            }
        }
        return loopsContainingLoopHintWithoutOSREnter;
    }
#endif
};

bool performTierUpCheckInjection(Graph& graph)
{
    SamplingRegion samplingRegion("DFG Tier-up Check Injection");
    return runPhase<TierUpCheckInjectionPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)


