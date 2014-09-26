/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "DFGInsertOSRHintsForUpdate.h"

#if ENABLE(DFG_JIT)

#include "JSCInlines.h"

namespace JSC { namespace DFG {

void insertOSRHintsForUpdate(
    InsertionSet& insertionSet, unsigned nodeIndex, NodeOrigin origin,
    AvailabilityMap& availability, Node* originalNode, Node* newNode)
{
    for (unsigned i = availability.m_locals.size(); i--;) {
        int operand = availability.m_locals.operandForIndex(i);
        
        if (availability.m_locals[i].hasNode() && availability.m_locals[i].node() == originalNode) {
            insertionSet.insertNode(
                nodeIndex, SpecNone, MovHint, origin, OpInfo(operand),
                newNode->defaultEdge());
        }
    }
    
    for (auto pair : availability.m_heap) {
        if (pair.value.hasNode() && pair.value.node() == originalNode) {
            insertionSet.insert(
                nodeIndex, pair.key.createHint(insertionSet.graph(), origin, newNode));
        }
    }
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

