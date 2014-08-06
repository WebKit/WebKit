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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSPropertyNameEnumerator.h"

#include "JSCInlines.h"
#include "StrongInlines.h"

namespace JSC {

const ClassInfo JSPropertyNameEnumerator::s_info = { "JSPropertyNameEnumerator", 0, 0, CREATE_METHOD_TABLE(JSPropertyNameEnumerator) };

JSPropertyNameEnumerator* JSPropertyNameEnumerator::create(VM& vm)
{
    if (!vm.emptyPropertyNameEnumerator.get()) {
        PropertyNameArray propertyNames(&vm);
        vm.emptyPropertyNameEnumerator = Strong<JSCell>(vm, create(vm, 0, propertyNames));
    }
    return jsCast<JSPropertyNameEnumerator*>(vm.emptyPropertyNameEnumerator.get());
}

JSPropertyNameEnumerator* JSPropertyNameEnumerator::create(VM& vm, Structure* structure, PropertyNameArray& propertyNames)
{
    StructureID structureID = structure ? structure->id() : 0;
    uint32_t inlineCapacity = structure ? structure->inlineCapacity() : 0;
    JSPropertyNameEnumerator* enumerator = new (NotNull, 
        allocateCell<JSPropertyNameEnumerator>(vm.heap)) JSPropertyNameEnumerator(vm, structureID, inlineCapacity, propertyNames.identifierSet());
    enumerator->finishCreation(vm, propertyNames.data());
    return enumerator;
}

JSPropertyNameEnumerator::JSPropertyNameEnumerator(VM& vm, StructureID structureID, uint32_t inlineCapacity, RefCountedIdentifierSet* set)
    : JSCell(vm, vm.propertyNameEnumeratorStructure.get())
    , m_identifierSet(set)
    , m_cachedStructureID(structureID)
    , m_cachedInlineCapacity(inlineCapacity)
{
}

void JSPropertyNameEnumerator::finishCreation(VM& vm, PassRefPtr<PropertyNameArrayData> idents)
{
    Base::finishCreation(vm);

    RefPtr<PropertyNameArrayData> identifiers = idents;
    PropertyNameArrayData::PropertyNameVector& vector = identifiers->propertyNameVector();
    m_propertyNames.resize(vector.size());
    for (unsigned i = 0; i < vector.size(); ++i) {
        const Identifier& identifier = vector[i];
        m_propertyNames[i].set(vm, this, jsString(&vm, identifier.string()));
    }
}

void JSPropertyNameEnumerator::destroy(JSCell* cell)
{
    jsCast<JSPropertyNameEnumerator*>(cell)->JSPropertyNameEnumerator::~JSPropertyNameEnumerator();
}

void JSPropertyNameEnumerator::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Base::visitChildren(cell, visitor);
    JSPropertyNameEnumerator* thisObject = jsCast<JSPropertyNameEnumerator*>(cell);
    for (unsigned i = 0; i < thisObject->m_propertyNames.size(); ++i)
        visitor.append(&thisObject->m_propertyNames[i]);
    visitor.append(&thisObject->m_prototypeChain);
}

} // namespace JSC
