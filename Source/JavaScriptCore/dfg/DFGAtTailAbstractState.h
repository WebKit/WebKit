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

#ifndef DFGAtTailAbstractState_h
#define DFGAtTailAbstractState_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGAbstractValue.h"
#include "DFGBasicBlock.h"
#include "DFGGraph.h"

namespace JSC { namespace DFG { 

class AtTailAbstractState {
public:
    AtTailAbstractState();
    
    ~AtTailAbstractState();
    
    void initializeTo(BasicBlock* block)
    {
        m_block = block;
    }
    
    void createValueForNode(Node*);
    AbstractValue& forNode(Node*);
    AbstractValue& forNode(Edge edge) { return forNode(edge.node()); }
    Operands<AbstractValue>& variables() { return m_block->valuesAtTail; }
    
    BasicBlock* block() const { return m_block; }
    
    bool isValid() { return m_block->cfaDidFinish; }
    
    void setDidClobber(bool) { }
    void setIsValid(bool isValid) { m_block->cfaDidFinish = isValid; }
    void setBranchDirection(BranchDirection) { }
    void setFoundConstants(bool) { }
    bool haveStructures() const { return true; } // It's always safe to return true.
    void setHaveStructures(bool) { }

private:
    BasicBlock* m_block;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAtTailAbstractState_h

