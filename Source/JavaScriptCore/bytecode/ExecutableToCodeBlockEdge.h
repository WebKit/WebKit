/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "ConcurrentJSLock.h"
#include "IsoSubspace.h"
#include "JSCast.h"
#include "VM.h"

namespace JSC {

class CodeBlock;
class LLIntOffsetsExtractor;

class ExecutableToCodeBlockEdge final : public JSCell {
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.executableToCodeBlockEdgeSpace;
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    static ExecutableToCodeBlockEdge* create(VM&, CodeBlock*);
    
    DECLARE_INFO;

    CodeBlock* codeBlock() const { return m_codeBlock.get(); }
    
    static void visitChildren(JSCell*, SlotVisitor&);
    static void visitOutputConstraints(JSCell*, SlotVisitor&);
    void finalizeUnconditionally(VM&);
    
    static CodeBlock* unwrap(ExecutableToCodeBlockEdge* edge)
    {
        if (!edge)
            return nullptr;
        return edge->codeBlock();
    }
    
    static CodeBlock* deactivateAndUnwrap(ExecutableToCodeBlockEdge* edge);
    
    static ExecutableToCodeBlockEdge* wrap(CodeBlock* codeBlock);
    
    static ExecutableToCodeBlockEdge* wrapAndActivate(CodeBlock* codeBlock);
    
private:
    friend class LLIntOffsetsExtractor;

    ExecutableToCodeBlockEdge(VM&, CodeBlock*);

    void finishCreation(VM&);

    void activate();
    void deactivate();
    bool isActive() const;
    
    void runConstraint(const ConcurrentJSLocker&, VM&, SlotVisitor&);
    
    WriteBarrier<CodeBlock> m_codeBlock;
};

} // namespace JSC

