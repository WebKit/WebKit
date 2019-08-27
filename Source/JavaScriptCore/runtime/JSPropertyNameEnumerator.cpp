/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#include "LockDuringMarking.h"
#include "StrongInlines.h"

namespace JSC {

const ClassInfo JSPropertyNameEnumerator::s_info = { "JSPropertyNameEnumerator", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSPropertyNameEnumerator) };

JSPropertyNameEnumerator* JSPropertyNameEnumerator::create(VM& vm, Structure* structure, uint32_t indexedLength, uint32_t numberStructureProperties, PropertyNameArray&& propertyNames)
{
    unsigned propertyNamesSize = propertyNames.size();
    unsigned propertyNamesBufferSizeInBytes = (Checked<unsigned>(propertyNamesSize) * sizeof(WriteBarrier<JSString>)).unsafeGet();
    WriteBarrier<JSString>* propertyNamesBuffer = nullptr;
    if (propertyNamesBufferSizeInBytes) {
        propertyNamesBuffer = static_cast<WriteBarrier<JSString>*>(vm.jsValueGigacageAuxiliarySpace.allocateNonVirtual(vm, propertyNamesBufferSizeInBytes, nullptr, AllocationFailureMode::Assert));
        for (unsigned i = 0; i < propertyNamesSize; ++i)
            propertyNamesBuffer[i].clear();
    }
    JSPropertyNameEnumerator* enumerator = new (NotNull, allocateCell<JSPropertyNameEnumerator>(vm.heap)) JSPropertyNameEnumerator(vm, structure, indexedLength, numberStructureProperties, propertyNamesBuffer, propertyNamesSize);
    enumerator->finishCreation(vm, propertyNames.releaseData());
    return enumerator;
}

JSPropertyNameEnumerator::JSPropertyNameEnumerator(VM& vm, Structure* structure, uint32_t indexedLength, uint32_t numberStructureProperties, WriteBarrier<JSString>* propertyNamesBuffer, unsigned propertyNamesSize)
    : JSCell(vm, vm.propertyNameEnumeratorStructure.get())
    , m_propertyNames(vm, this, propertyNamesBuffer)
    , m_cachedStructureID(structure ? structure->id() : 0)
    , m_indexedLength(indexedLength)
    , m_endStructurePropertyIndex(numberStructureProperties)
    , m_endGenericPropertyIndex(propertyNamesSize)
    , m_cachedInlineCapacity(structure ? structure->inlineCapacity() : 0)
{
}

void JSPropertyNameEnumerator::finishCreation(VM& vm, RefPtr<PropertyNameArrayData>&& identifiers)
{
    Base::finishCreation(vm);

    PropertyNameArrayData::PropertyNameVector& vector = identifiers->propertyNameVector();
    ASSERT(m_endGenericPropertyIndex == vector.size());
    for (unsigned i = 0; i < vector.size(); ++i) {
        const Identifier& identifier = vector[i];
        m_propertyNames.get()[i].set(vm, this, jsString(vm, identifier.string()));
    }
}

void JSPropertyNameEnumerator::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSPropertyNameEnumerator* thisObject = jsCast<JSPropertyNameEnumerator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);
    if (auto* propertyNames = thisObject->m_propertyNames.get()) {
        visitor.markAuxiliary(propertyNames);
        visitor.append(propertyNames, propertyNames + thisObject->sizeOfPropertyNames());
    }
    visitor.append(thisObject->m_prototypeChain);

    if (thisObject->cachedStructureID()) {
        VM& vm = visitor.vm();
        visitor.appendUnbarriered(vm.getStructure(thisObject->cachedStructureID()));
    }
}

} // namespace JSC
