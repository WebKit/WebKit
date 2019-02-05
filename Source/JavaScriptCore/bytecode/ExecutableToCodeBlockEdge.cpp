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

#include "config.h"
#include "ExecutableToCodeBlockEdge.h"

#include "IsoCellSetInlines.h"

namespace JSC {

const ClassInfo ExecutableToCodeBlockEdge::s_info = { "ExecutableToCodeBlockEdge", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(ExecutableToCodeBlockEdge) };

Structure* ExecutableToCodeBlockEdge::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

ExecutableToCodeBlockEdge* ExecutableToCodeBlockEdge::create(VM& vm, CodeBlock* codeBlock)
{
    ExecutableToCodeBlockEdge* result = new (NotNull, allocateCell<ExecutableToCodeBlockEdge>(vm.heap)) ExecutableToCodeBlockEdge(vm, codeBlock);
    result->finishCreation(vm);
    return result;
}

void ExecutableToCodeBlockEdge::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(!isActive());
}

void ExecutableToCodeBlockEdge::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    VM& vm = visitor.vm();
    ExecutableToCodeBlockEdge* edge = jsCast<ExecutableToCodeBlockEdge*>(cell);
    Base::visitChildren(cell, visitor);

    CodeBlock* codeBlock = edge->m_codeBlock.get();
    
    // It's possible for someone to hold a pointer to the edge after the edge has cleared its weak
    // reference to the codeBlock. In a conservative GC like ours, that could happen at random for
    // no good reason and it's Totally OK (TM). See finalizeUnconditionally() for where we clear
    // m_codeBlock.
    if (!codeBlock)
        return;
    
    if (!edge->isActive()) {
        visitor.appendUnbarriered(codeBlock);
        return;
    }
    
    ConcurrentJSLocker locker(codeBlock->m_lock);
    
    if (codeBlock->shouldVisitStrongly(locker))
        visitor.appendUnbarriered(codeBlock);
    
    if (!Heap::isMarked(codeBlock))
        vm.executableToCodeBlockEdgesWithFinalizers.add(edge);
    
    if (JITCode::isOptimizingJIT(codeBlock->jitType())) {
        // If we jettison ourselves we'll install our alternative, so make sure that it
        // survives GC even if we don't.
        visitor.append(codeBlock->m_alternative);
    }
    
    // NOTE: There are two sides to this constraint, with different requirements for correctness.
    // Because everything is ultimately protected with weak references and jettisoning, it's
    // always "OK" to claim that something is dead prematurely and it's "OK" to keep things alive.
    // But both choices could lead to bad perf - either recomp cycles or leaks.
    //
    // Determining CodeBlock liveness: This part is the most consequential. We want to keep the
    // output constraint active so long as we think that we may yet prove that the CodeBlock is
    // live but we haven't done it yet.
    //
    // Marking Structures if profitable: It's important that we do a pass of this. Logically, this
    // seems like it is a constraint of CodeBlock. But we have always first run this as a result
    // of the edge being marked even before we determine the liveness of the CodeBlock. This
    // allows a CodeBlock to mark itself by first proving that all of the Structures it weakly
    // depends on could be strongly marked. (This part is also called propagateTransitions.)
    //
    // As a weird caveat, we only fixpoint the constraints so long as the CodeBlock is not live.
    // This means that we may overlook structure marking opportunities created by other marking
    // that happens after the CodeBlock is marked. This was an accidental policy decision from a
    // long time ago, but it is probably OK, since it's only worthwhile to keep fixpointing the
    // structure marking if we still have unmarked structures after the first round. We almost
    // never will because we will mark-if-profitable based on the owning global object being
    // already marked. We mark it just in case that hadn't happened yet. And if the CodeBlock is
    // not yet marked because it weakly depends on a structure that we did not yet mark, then we
    // will keep fixpointing until the end.
    visitor.appendUnbarriered(codeBlock->globalObject());
    vm.executableToCodeBlockEdgesWithConstraints.add(edge);
    edge->runConstraint(locker, vm, visitor);
}

void ExecutableToCodeBlockEdge::visitOutputConstraints(JSCell* cell, SlotVisitor& visitor)
{
    VM& vm = visitor.vm();
    ExecutableToCodeBlockEdge* edge = jsCast<ExecutableToCodeBlockEdge*>(cell);
    
    edge->runConstraint(NoLockingNecessary, vm, visitor);
}

void ExecutableToCodeBlockEdge::finalizeUnconditionally(VM& vm)
{
    CodeBlock* codeBlock = m_codeBlock.get();
    
    if (!Heap::isMarked(codeBlock)) {
        if (codeBlock->shouldJettisonDueToWeakReference())
            codeBlock->jettison(Profiler::JettisonDueToWeakReference);
        else
            codeBlock->jettison(Profiler::JettisonDueToOldAge);
        m_codeBlock.clear();
    }
    
    vm.executableToCodeBlockEdgesWithFinalizers.remove(this);
    vm.executableToCodeBlockEdgesWithConstraints.remove(this);
}

inline void ExecutableToCodeBlockEdge::activate()
{
    setPerCellBit(true);
}

inline void ExecutableToCodeBlockEdge::deactivate()
{
    setPerCellBit(false);
}

inline bool ExecutableToCodeBlockEdge::isActive() const
{
    return perCellBit();
}

CodeBlock* ExecutableToCodeBlockEdge::deactivateAndUnwrap(ExecutableToCodeBlockEdge* edge)
{
    if (!edge)
        return nullptr;
    edge->deactivate();
    return edge->codeBlock();
}

ExecutableToCodeBlockEdge* ExecutableToCodeBlockEdge::wrap(CodeBlock* codeBlock)
{
    if (!codeBlock)
        return nullptr;
    return codeBlock->ownerEdge();
}
    
ExecutableToCodeBlockEdge* ExecutableToCodeBlockEdge::wrapAndActivate(CodeBlock* codeBlock)
{
    if (!codeBlock)
        return nullptr;
    ExecutableToCodeBlockEdge* result = codeBlock->ownerEdge();
    result->activate();
    return result;
}

ExecutableToCodeBlockEdge::ExecutableToCodeBlockEdge(VM& vm, CodeBlock* codeBlock)
    : Base(vm, vm.executableToCodeBlockEdgeStructure.get())
    , m_codeBlock(vm, this, codeBlock)
{
}

void ExecutableToCodeBlockEdge::runConstraint(const ConcurrentJSLocker& locker, VM& vm, SlotVisitor& visitor)
{
    CodeBlock* codeBlock = m_codeBlock.get();
    
    codeBlock->propagateTransitions(locker, visitor);
    codeBlock->determineLiveness(locker, visitor);
    
    if (Heap::isMarked(codeBlock))
        vm.executableToCodeBlockEdgesWithConstraints.remove(this);
}

} // namespace JSC

