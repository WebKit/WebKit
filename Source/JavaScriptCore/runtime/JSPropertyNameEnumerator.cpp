/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#include "JSObjectInlines.h"

namespace JSC {

const ClassInfo JSPropertyNameEnumerator::s_info = { "JSPropertyNameEnumerator"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSPropertyNameEnumerator) };

JSPropertyNameEnumerator* JSPropertyNameEnumerator::create(VM& vm, Structure* structure, uint32_t indexedLength, uint32_t numberStructureProperties, PropertyNameArray&& propertyNames)
{
    unsigned propertyNamesSize = propertyNames.size();
    unsigned propertyNamesBufferSizeInBytes = Checked<unsigned>(propertyNamesSize) * sizeof(WriteBarrier<JSString>);
    WriteBarrier<JSString>* propertyNamesBuffer = nullptr;
    if (propertyNamesBufferSizeInBytes) {
        propertyNamesBuffer = static_cast<WriteBarrier<JSString>*>(vm.jsValueGigacageAuxiliarySpace().allocate(vm, propertyNamesBufferSizeInBytes, nullptr, AllocationFailureMode::Assert));
        for (unsigned i = 0; i < propertyNamesSize; ++i)
            propertyNamesBuffer[i].clear();
    }
    JSPropertyNameEnumerator* enumerator = new (NotNull, allocateCell<JSPropertyNameEnumerator>(vm)) JSPropertyNameEnumerator(vm, structure, indexedLength, numberStructureProperties, propertyNamesBuffer, propertyNamesSize);
    enumerator->finishCreation(vm, propertyNames.releaseData());
    return enumerator;
}

JSPropertyNameEnumerator::JSPropertyNameEnumerator(VM& vm, Structure* structure, uint32_t indexedLength, uint32_t numberStructureProperties, WriteBarrier<JSString>* propertyNamesBuffer, unsigned propertyNamesSize)
    : JSCell(vm, vm.propertyNameEnumeratorStructure.get())
    , m_propertyNames(vm, this, propertyNamesBuffer)
    , m_cachedStructureID(vm, this, structure, WriteBarrierStructureID::MayBeNull)
    , m_indexedLength(indexedLength)
    , m_endStructurePropertyIndex(numberStructureProperties)
    , m_endGenericPropertyIndex(propertyNamesSize)
    , m_cachedInlineCapacity(structure ? structure->inlineCapacity() : 0)
{
    if (m_indexedLength)
        m_flags |= JSPropertyNameEnumerator::IndexedMode;
    if (m_endStructurePropertyIndex)
        m_flags |= JSPropertyNameEnumerator::OwnStructureMode;
    if (m_endGenericPropertyIndex - m_endStructurePropertyIndex)
        m_flags |= JSPropertyNameEnumerator::GenericMode;
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

template<typename Visitor>
void JSPropertyNameEnumerator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSPropertyNameEnumerator* thisObject = jsCast<JSPropertyNameEnumerator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);
    if (auto* propertyNames = thisObject->m_propertyNames.get()) {
        visitor.markAuxiliary(propertyNames);
        visitor.append(propertyNames, propertyNames + thisObject->sizeOfPropertyNames());
    }
    visitor.append(thisObject->m_cachedStructureID);
}

DEFINE_VISIT_CHILDREN(JSPropertyNameEnumerator);

// FIXME: Assert that properties returned by getOwnPropertyNames() are reported enumerable by getOwnPropertySlot().
// https://bugs.webkit.org/show_bug.cgi?id=219926
void getEnumerablePropertyNames(JSGlobalObject* globalObject, JSObject* base, PropertyNameArray& propertyNames, uint32_t& indexedLength, uint32_t& structurePropertyCount)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto getOwnPropertyNames = [&](JSObject* object) {
        auto mode = DontEnumPropertiesMode::Exclude;
        if (object->type() == ProxyObjectType) {
            // This ensures Proxy's [[GetOwnProperty]] trap is invoked only once per property, by OpHasEnumerableProperty.
            // Although doing this for all objects is spec-conformant, collecting DontEnum properties isn't free.
            mode = DontEnumPropertiesMode::Include;
        }
        object->methodTable()->getOwnPropertyNames(object, globalObject, propertyNames, mode);
    };

    Structure* structure = base->structure();
    if (structure->canAccessPropertiesQuicklyForEnumeration() && indexedLength == base->getArrayLength()) {
        // Inlined JSObject::getOwnNonIndexPropertyNames()
        base->methodTable()->getOwnSpecialPropertyNames(base, globalObject, propertyNames, DontEnumPropertiesMode::Exclude);
        RETURN_IF_EXCEPTION(scope, void());

        base->getNonReifiedStaticPropertyNames(vm, propertyNames, DontEnumPropertiesMode::Exclude);
        unsigned nonStructurePropertyCount = propertyNames.size();
        structure->getPropertyNamesFromStructure(vm, propertyNames, DontEnumPropertiesMode::Exclude);
        scope.assertNoException();

        // |propertyNames| contains properties exclusively from the structure.
        if (!nonStructurePropertyCount)
            structurePropertyCount = propertyNames.size();
    } else {
        getOwnPropertyNames(base);
        RETURN_IF_EXCEPTION(scope, void());
        // |propertyNames| contains all indexed properties, so disable enumeration based on getEnumerableLength().
        indexedLength = 0;
    }

    JSObject* object = base;
    unsigned prototypeCount = 0;

    while (true) {
        JSValue prototype = object->getPrototype(vm, globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        if (prototype.isNull())
            break;

        if (UNLIKELY(++prototypeCount > JSObject::maximumPrototypeChainDepth)) {
            throwStackOverflowError(globalObject, scope);
            return;
        }

        object = asObject(prototype);
        getOwnPropertyNames(object);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

JSString* JSPropertyNameEnumerator::computeNext(JSGlobalObject* globalObject, JSObject* base, uint32_t& index, Flag& mode, bool shouldAllocateIndexedNameString)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(indexedLength() || sizeOfPropertyNames());

    index++;
    switch (mode) {
    case InitMode: {
        mode = IndexedMode;
        index = 0;
        FALLTHROUGH;
    }

    case JSPropertyNameEnumerator::IndexedMode: {
        while (index < indexedLength() && !base->hasEnumerableProperty(globalObject, index)) {
            RETURN_IF_EXCEPTION(scope, nullptr);
            index++;
        }
        scope.assertNoException();

        if (index < indexedLength())
            return shouldAllocateIndexedNameString ? jsString(vm, Identifier::from(vm, index).string()) : nullptr;

        if (!sizeOfPropertyNames())
            return nullptr;

        mode = OwnStructureMode;
        index = 0;
        FALLTHROUGH;
    }

    case JSPropertyNameEnumerator::OwnStructureMode:
    case JSPropertyNameEnumerator::GenericMode: {
        JSString* name = nullptr;
        while (true) {
            if (index >= sizeOfPropertyNames())
                break;
            name = propertyNameAtIndex(index);
            if (!name)
                break;
            if (index < endStructurePropertyIndex() && base->structureID() == cachedStructureID())
                break;
            auto id = name->toIdentifier(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            if (base->hasEnumerableProperty(globalObject, id))
                break;
            RETURN_IF_EXCEPTION(scope, nullptr);
            name = nullptr;
            index++;
        }
        scope.assertNoException();

        if (index >= endStructurePropertyIndex() && index < sizeOfPropertyNames())
            mode = GenericMode;
        return name;
    }

    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace JSC
