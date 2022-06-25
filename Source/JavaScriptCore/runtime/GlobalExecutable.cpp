/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "GlobalExecutable.h"

#include "IsoCellSetInlines.h"
#include "JSCellInlines.h"
#include "ScriptExecutableInlines.h"

namespace JSC {

const ClassInfo GlobalExecutable::s_info = { "GlobalExecutable"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(GlobalExecutable) };

template<typename Visitor>
void GlobalExecutable::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* executable = jsCast<GlobalExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(executable, info());
    Base::visitChildren(executable, visitor);
    visitor.append(executable->m_unlinkedCodeBlock);

    if (auto* codeBlock = executable->codeBlock()) {
        // If CodeBlocks is not marked yet, we will run output-constraints.
        // We maintain the invariant that, whenever we see unmarked CodeBlock, then we must run finalizer.
        // And whenever we set a bit on outputConstraintsSet, we must already set a bit in finalizerSet.
        visitCodeBlockEdge(visitor, codeBlock);
        if (!visitor.isMarked(codeBlock)) {
            Heap::ScriptExecutableSpaceAndSets::finalizerSetFor(*executable->subspace()).add(executable);
            Heap::ScriptExecutableSpaceAndSets::outputConstraintsSetFor(*executable->subspace()).add(executable);
        }
    }
}

DEFINE_VISIT_CHILDREN(GlobalExecutable);

template<typename Visitor>
void GlobalExecutable::visitOutputConstraintsImpl(JSCell* cell, Visitor& visitor)
{
    auto* executable = jsCast<GlobalExecutable*>(cell);
    if (CodeBlock* codeBlock = executable->codeBlock()) {
        if (!visitor.isMarked(codeBlock))
            runConstraint(NoLockingNecessary, visitor, codeBlock);
        if (visitor.isMarked(codeBlock))
            Heap::ScriptExecutableSpaceAndSets::outputConstraintsSetFor(*executable->subspace()).remove(executable);
    }
}

DEFINE_VISIT_OUTPUT_CONSTRAINTS(GlobalExecutable);

CodeBlock* GlobalExecutable::replaceCodeBlockWith(VM& vm, CodeBlock* newCodeBlock)
{
    CodeBlock* oldCodeBlock = codeBlock();
    m_codeBlock.setMayBeNull(vm, this, newCodeBlock);
    return oldCodeBlock;
}

void GlobalExecutable::finalizeUnconditionally(VM& vm)
{
    finalizeCodeBlockEdge(vm, m_codeBlock);
    Heap::ScriptExecutableSpaceAndSets::outputConstraintsSetFor(*subspace()).remove(this);
    Heap::ScriptExecutableSpaceAndSets::finalizerSetFor(*subspace()).remove(this);
}

} // namespace JSC
