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
#include "DFGOSRExitBase.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGBasicBlock.h"
#include "DFGNode.h"
#include "Operations.h"

namespace JSC { namespace DFG {

bool OSRExitBase::considerAddingAsFrequentExitSiteSlow(CodeBlock* profiledCodeBlock)
{
    return baselineCodeBlockForOriginAndBaselineCodeBlock(
        m_codeOriginForExitProfile, profiledCodeBlock)->addFrequentExitSite(
            FrequentExitSite(m_codeOriginForExitProfile.bytecodeIndex, m_kind));
}

bool OSRExitBase::doSearchForForwardConversion(
    BasicBlock* block, Node* currentNode, unsigned nodeIndex, bool hasValueRecovery,
    Node*& node, Node*& lastMovHint)
{
    // Check that either the current node is a SetLocal, or the preceding node was a
    // SetLocal with the same code origin, or that we've provided a valueRecovery.
    if (!ASSERT_DISABLED
        && !hasValueRecovery
        && !currentNode->containsMovHint()) {
        Node* setLocal = block->at(nodeIndex - 1);
        ASSERT_UNUSED(setLocal, setLocal->containsMovHint());
        ASSERT_UNUSED(setLocal, setLocal->codeOriginForExitTarget == currentNode->codeOriginForExitTarget);
    }
    
    // Find the first node for the next bytecode instruction. Also track the last mov hint
    // on this node.
    unsigned indexInBlock = nodeIndex + 1;
    node = 0;
    lastMovHint = 0;
    for (;;) {
        if (indexInBlock == block->size()) {
            // This is an inline return. Give up and do a backwards speculation. This is safe
            // because an inline return has its own bytecode index and it's always safe to
            // reexecute that bytecode.
            ASSERT(node->op() == Jump);
            return false;
        }
        node = block->at(indexInBlock);
        if (node->containsMovHint() && node->child1() == currentNode)
            lastMovHint = node;
        if (node->codeOriginForExitTarget != currentNode->codeOriginForExitTarget)
            break;
        indexInBlock++;
    }
    
    ASSERT(node->codeOriginForExitTarget != currentNode->codeOriginForExitTarget);
    return true;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

