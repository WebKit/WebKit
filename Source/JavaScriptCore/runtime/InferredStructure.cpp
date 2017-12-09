/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "InferredStructure.h"

#include "JSCInlines.h"

namespace JSC {

const ClassInfo InferredStructure::s_info = { "InferredStructure", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(InferredStructure) };

InferredStructure* InferredStructure::create(VM& vm, GCDeferralContext& deferralContext, InferredType* parent, Structure* structure)
{
    InferredStructure* result = new (NotNull, allocateCell<InferredStructure>(vm.heap, &deferralContext)) InferredStructure(vm, parent, structure);
    result->finishCreation(vm, structure);
    return result;
}

void InferredStructure::destroy(JSCell* cell)
{
    InferredStructure* inferredStructure = static_cast<InferredStructure*>(cell);
    inferredStructure->InferredStructure::~InferredStructure();
}

Structure* InferredStructure::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

void InferredStructure::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    InferredStructure* inferredStructure = jsCast<InferredStructure*>(cell);
    
    visitor.append(inferredStructure->m_parent);
}

void InferredStructure::finalizeUnconditionally(VM&)
{
    ASSERT(Heap::isMarked(m_parent.get()));
    
    // Monotonicity ensures that we shouldn't see a new structure that is different from us, but we
    // could have been nulled. We only rely on it being the null case only in debug.
    if (this == m_parent->m_structure.get()) {
        if (!Heap::isMarked(m_structure.get()))
            m_parent->removeStructure();
    } else
        ASSERT(!m_parent->m_structure);
    
    // We'll get deleted on the next GC. That's a little weird, but not harmful, since it only happens
    // when the InferredType that we were paired to would have survived. It just means a tiny missed
    // opportunity for heap size reduction.
}

InferredStructure::InferredStructure(VM& vm, InferredType* parent, Structure* structure)
    : Base(vm, vm.inferredStructureStructure.get())
    , m_parent(vm, this, parent)
    , m_structure(vm, this, structure)
{
}

void InferredStructure::finishCreation(VM& vm, Structure* structure)
{
    Base::finishCreation(vm);
    structure->addTransitionWatchpoint(&m_watchpoint);
}

} // namespace JSC


