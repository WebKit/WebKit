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

#pragma once

#include "JSCast.h"
#include "Operations.h"
#include "PropertyNameArray.h"
#include "Structure.h"

namespace JSC {

class JSPropertyNameEnumerator final : public JSCell {
public:
    using Base = JSCell;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.propertyNameEnumeratorSpace;
    }

    static JSPropertyNameEnumerator* create(VM&, Structure*, uint32_t, uint32_t, PropertyNameArray&&);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    DECLARE_EXPORT_INFO;

    JSString* propertyNameAtIndex(uint32_t index)
    {
        if (index >= sizeOfPropertyNames())
            return nullptr;
        return m_propertyNames.get()[index].get();
    }

    StructureChain* cachedPrototypeChain() const { return m_prototypeChain.get(); }
    void setCachedPrototypeChain(VM& vm, StructureChain* prototypeChain) { return m_prototypeChain.set(vm, this, prototypeChain); }

    Structure* cachedStructure(VM& vm) const
    {
        if (!m_cachedStructureID)
            return nullptr;
        return vm.heap.structureIDTable().get(m_cachedStructureID);
    }
    StructureID cachedStructureID() const { return m_cachedStructureID; }
    uint32_t indexedLength() const { return m_indexedLength; }
    uint32_t endStructurePropertyIndex() const { return m_endStructurePropertyIndex; }
    uint32_t endGenericPropertyIndex() const { return m_endGenericPropertyIndex; }
    uint32_t cachedInlineCapacity() const { return m_cachedInlineCapacity; }
    uint32_t sizeOfPropertyNames() const { return endGenericPropertyIndex(); }
    static ptrdiff_t cachedStructureIDOffset() { return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_cachedStructureID); }
    static ptrdiff_t indexedLengthOffset() { return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_indexedLength); }
    static ptrdiff_t endStructurePropertyIndexOffset() { return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_endStructurePropertyIndex); }
    static ptrdiff_t endGenericPropertyIndexOffset() { return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_endGenericPropertyIndex); }
    static ptrdiff_t cachedInlineCapacityOffset() { return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_cachedInlineCapacity); }
    static ptrdiff_t cachedPropertyNamesVectorOffset()
    {
        return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_propertyNames);
    }

    static void visitChildren(JSCell*, SlotVisitor&);

private:
    friend class LLIntOffsetsExtractor;

    JSPropertyNameEnumerator(VM&, Structure*, uint32_t, uint32_t, WriteBarrier<JSString>*, unsigned);
    void finishCreation(VM&, RefPtr<PropertyNameArrayData>&&);

    AuxiliaryBarrier<WriteBarrier<JSString>*> m_propertyNames;
    WriteBarrier<StructureChain> m_prototypeChain;
    StructureID m_cachedStructureID;
    uint32_t m_indexedLength;
    uint32_t m_endStructurePropertyIndex;
    uint32_t m_endGenericPropertyIndex;
    uint32_t m_cachedInlineCapacity;
};

inline JSPropertyNameEnumerator* propertyNameEnumerator(JSGlobalObject* globalObject, JSObject* base)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint32_t indexedLength = base->methodTable(vm)->getEnumerableLength(globalObject, base);

    JSPropertyNameEnumerator* enumerator = nullptr;

    Structure* structure = base->structure(vm);
    if (!indexedLength
        && (enumerator = structure->cachedPropertyNameEnumerator())
        && enumerator->cachedPrototypeChain() == structure->prototypeChain(globalObject, base))
        return enumerator;

    uint32_t numberStructureProperties = 0;

    PropertyNameArray propertyNames(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);

    if (structure->canAccessPropertiesQuicklyForEnumeration() && indexedLength == base->getArrayLength()) {
        base->methodTable(vm)->getStructurePropertyNames(base, globalObject, propertyNames, EnumerationMode());
        scope.assertNoException();

        numberStructureProperties = propertyNames.size();

        base->methodTable(vm)->getGenericPropertyNames(base, globalObject, propertyNames, EnumerationMode());
    } else {
        // Generic property names vector contains all indexed property names.
        // So disable indexed property enumeration phase by setting |indexedLength| to 0.
        indexedLength = 0;
        base->methodTable(vm)->getPropertyNames(base, globalObject, propertyNames, EnumerationMode());
    }
    RETURN_IF_EXCEPTION(scope, nullptr);

    ASSERT(propertyNames.size() < UINT32_MAX);

    bool sawPolyProto;
    bool successfullyNormalizedChain = normalizePrototypeChain(globalObject, base, sawPolyProto) != InvalidPrototypeChain;

    Structure* structureAfterGettingPropertyNames = base->structure(vm);
    enumerator = JSPropertyNameEnumerator::create(vm, structureAfterGettingPropertyNames, indexedLength, numberStructureProperties, WTFMove(propertyNames));
    if (!indexedLength && successfullyNormalizedChain && structureAfterGettingPropertyNames == structure) {
        enumerator->setCachedPrototypeChain(vm, structure->prototypeChain(globalObject, base));
        if (structure->canCachePropertyNameEnumerator(vm))
            structure->setCachedPropertyNameEnumerator(vm, enumerator);
    }
    return enumerator;
}

} // namespace JSC
