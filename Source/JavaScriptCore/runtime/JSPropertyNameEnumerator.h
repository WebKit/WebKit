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

#ifndef JSPropertyNameEnumerator_h
#define JSPropertyNameEnumerator_h

#include "JSCell.h"
#include "Operations.h"
#include "PropertyNameArray.h"
#include "Structure.h"

namespace JSC {

class Identifier;

class JSPropertyNameEnumerator : public JSCell {
public:
    typedef JSCell Base;

    static JSPropertyNameEnumerator* create(VM&);
    static JSPropertyNameEnumerator* create(VM&, Structure*, PropertyNameArray&);

    static const bool needsDestruction = true;
    static const bool hasImmortalStructure = true;
    static void destroy(JSCell*);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    DECLARE_EXPORT_INFO;

    JSString* propertyNameAtIndex(uint32_t index) const
    {
        if (index >= m_propertyNames.size())
            return nullptr;
        return m_propertyNames[index].get();
    }

    RefCountedIdentifierSet* identifierSet() const
    {
        return m_identifierSet.get();
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
    uint32_t cachedInlineCapacity() const { return m_cachedInlineCapacity; }
    static ptrdiff_t cachedStructureIDOffset() { return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_cachedStructureID); }
    static ptrdiff_t cachedInlineCapacityOffset() { return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_cachedInlineCapacity); }
    static ptrdiff_t cachedPropertyNamesLengthOffset()
    {
        return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_propertyNames) + Vector<WriteBarrier<JSString>>::sizeMemoryOffset();
    }
    static ptrdiff_t cachedPropertyNamesVectorOffset()
    {
        return OBJECT_OFFSETOF(JSPropertyNameEnumerator, m_propertyNames) + Vector<WriteBarrier<JSString>>::dataMemoryOffset();
    }

    static void visitChildren(JSCell*, SlotVisitor&);

private:
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    JSPropertyNameEnumerator(VM&, StructureID, uint32_t, RefCountedIdentifierSet*);
    void finishCreation(VM&, PassRefPtr<PropertyNameArrayData>);

    Vector<WriteBarrier<JSString>> m_propertyNames;
    RefPtr<RefCountedIdentifierSet> m_identifierSet;
    StructureID m_cachedStructureID;
    WriteBarrier<StructureChain> m_prototypeChain;
    uint32_t m_cachedInlineCapacity;
};

inline JSPropertyNameEnumerator* structurePropertyNameEnumerator(ExecState* exec, JSObject* base, uint32_t length)
{
    VM& vm = exec->vm();
    Structure* structure = base->structure(vm);
    if (JSPropertyNameEnumerator* enumerator = structure->cachedStructurePropertyNameEnumerator())
        return enumerator;

    if (!structure->canAccessPropertiesQuickly() || length != base->getArrayLength())
        return JSPropertyNameEnumerator::create(vm);

    PropertyNameArray propertyNames(exec);
    base->methodTable(vm)->getStructurePropertyNames(base, exec, propertyNames, ExcludeDontEnumProperties);

    JSPropertyNameEnumerator* enumerator = JSPropertyNameEnumerator::create(vm, structure, propertyNames);
    if (structure->canCacheStructurePropertyNameEnumerator())
        structure->setCachedStructurePropertyNameEnumerator(vm, enumerator);
    return enumerator;
}

inline JSPropertyNameEnumerator* genericPropertyNameEnumerator(ExecState* exec, JSObject* base, uint32_t length, JSPropertyNameEnumerator* structureEnumerator)
{
    VM& vm = exec->vm();
    Structure* structure = base->structure(vm);
    if (JSPropertyNameEnumerator* enumerator = structure->cachedGenericPropertyNameEnumerator()) {
        if (!length && enumerator->cachedPrototypeChain() == structure->prototypeChain(exec))
            return enumerator;
    }

    PropertyNameArray propertyNames(exec);
    propertyNames.setPreviouslyEnumeratedLength(length);
    propertyNames.setPreviouslyEnumeratedProperties(structureEnumerator);

    // If we still have the same Structure that we started with, our Structure allows us to access its properties 
    // quickly (i.e. the Structure property loop was able to do things), and we iterated the full length of the 
    // object (i.e. there are no more own indexed properties that need to be enumerated), then the generic property 
    // iteration can skip any properties it would get from the JSObject base class. This turns out to be important 
    // for hot loops because most of our time is then dominated by trying to add the own Structure properties to 
    // the new generic PropertyNameArray and failing because we've already visited them.
    Structure* cachedStructure = structureEnumerator->cachedStructure(vm);
    if (structure == cachedStructure && structure->canAccessPropertiesQuickly() && static_cast<uint32_t>(length) == base->getArrayLength())
        base->methodTable(vm)->getGenericPropertyNames(base, exec, propertyNames, ExcludeDontEnumProperties);
    else
        base->methodTable(vm)->getPropertyNames(base, exec, propertyNames, ExcludeDontEnumProperties);
    
    normalizePrototypeChain(exec, base);

    JSPropertyNameEnumerator* enumerator = JSPropertyNameEnumerator::create(vm, base->structure(vm), propertyNames);
    enumerator->setCachedPrototypeChain(vm, structure->prototypeChain(exec));
    if (!length && structure->canCacheGenericPropertyNameEnumerator())
        structure->setCachedGenericPropertyNameEnumerator(vm, enumerator);
    return enumerator;
}

} // namespace JSC

#endif // JSPropertyNameEnumerator_h
