/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "StructureChain.h"

#include "JSObject.h"
#include "JSCInlines.h"
#include "Structure.h"

namespace JSC {
    
const ClassInfo StructureChain::s_info = { "StructureChain", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(StructureChain) };

StructureChain::StructureChain(VM& vm, Structure* structure, WriteBarrier<Structure>* vector)
    : Base(vm, structure)
    , m_vector(vm, this, vector)
{
}

StructureChain* StructureChain::create(VM& vm, JSObject* head)
{
    // FIXME: Make StructureChain::create fail for large chain. Caching large chain is not so profitable.
    // By making the size <= UINT16_MAX, we can store length in a high bits of auxiliary pointer.
    // https://bugs.webkit.org/show_bug.cgi?id=200290
    size_t size = 0;
    for (JSObject* current = head; current; current = current->structure(vm)->storedPrototypeObject(current))
        ++size;
    ++size; // Sentinel nullptr.
    WriteBarrier<Structure>* vector = static_cast<WriteBarrier<Structure>*>(vm.jsValueGigacageAuxiliarySpace.allocateNonVirtual(vm, (Checked<size_t>(size) * sizeof(WriteBarrier<Structure>)).unsafeGet(), nullptr, AllocationFailureMode::Assert));
    for (size_t i = 0; i < size; ++i)
        vector[i].clear();
    StructureChain* chain = new (NotNull, allocateCell<StructureChain>(vm.heap)) StructureChain(vm, vm.structureChainStructure.get(), vector);
    chain->finishCreation(vm, head);
    return chain;
}

void StructureChain::finishCreation(VM& vm, JSObject* head)
{
    Base::finishCreation(vm);
    size_t i = 0;
    for (JSObject* current = head; current; current = current->structure(vm)->storedPrototypeObject(current))
        m_vector.get()[i++].set(vm, this, current->structure(vm));
}

void StructureChain::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    StructureChain* thisObject = jsCast<StructureChain*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.markAuxiliary(thisObject->m_vector.get());
    for (auto* current = thisObject->m_vector.get(); *current; ++current)
        visitor.append(*current);
}

} // namespace JSC
