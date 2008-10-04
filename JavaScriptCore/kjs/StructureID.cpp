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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "StructureID.h"

#include "identifier.h"
#include "JSObject.h"
#include "PropertyNameArray.h"
#include "lookup.h"
#include <wtf/RefPtr.h>

using namespace std;

namespace JSC {

StructureID::StructureID(JSValue* prototype, const TypeInfo& typeInfo)
    : m_typeInfo(typeInfo)
    , m_isDictionary(false)
    , m_prototype(prototype)
    , m_cachedPrototypeChain(0)
    , m_previous(0)
    , m_nameInPrevious(0)
    , m_transitionCount(0)
{
    ASSERT(m_prototype);
    ASSERT(m_prototype->isObject() || m_prototype->isNull());
}

void StructureID::getEnumerablePropertyNames(ExecState* exec, PropertyNameArray& propertyNames, JSObject* baseObject)
{
    bool shouldCache = !(propertyNames.size() || m_isDictionary);

    if (shouldCache && m_cachedPropertyNameArrayData) {
        if (structureIDChainsAreEqual(m_cachedPropertyNameArrayData->cachedPrototypeChain(), cachedPrototypeChain())) {
            propertyNames.setData(m_cachedPropertyNameArrayData);
            return;
        }
    }

    m_propertyMap.getEnumerablePropertyNames(propertyNames);

    // Add properties from the static hashtables of properties
    for (const ClassInfo* info = baseObject->classInfo(); info; info = info->parentClass) {
        const HashTable* table = info->propHashTable(exec);
        if (!table)
            continue;
        table->initializeIfNeeded(exec);
        ASSERT(table->table);
        int hashSizeMask = table->hashSizeMask;
        const HashEntry* entry = table->table;
        for (int i = 0; i <= hashSizeMask; ++i, ++entry) {
            if (entry->key() && !(entry->attributes() & DontEnum))
                propertyNames.add(entry->key());
        }
    }

    if (m_prototype->isObject())
        static_cast<JSObject*>(m_prototype)->getPropertyNames(exec, propertyNames);

    if (shouldCache) {
        if (m_cachedPropertyNameArrayData)
            m_cachedPropertyNameArrayData->setCachedStructureID(0);

        m_cachedPropertyNameArrayData = propertyNames.data();

        StructureIDChain* chain = cachedPrototypeChain();
        if (!chain)
            chain = createCachedPrototypeChain();
        m_cachedPropertyNameArrayData->setCachedPrototypeChain(chain);
        m_cachedPropertyNameArrayData->setCachedStructureID(this);
    }
}

void StructureID::clearEnumerationCache()
{
    if (m_cachedPropertyNameArrayData)
        m_cachedPropertyNameArrayData->setCachedStructureID(0);
    m_cachedPropertyNameArrayData.clear();
}

void StructureID::transitionTo(StructureID* oldStructureID, StructureID* newStructureID, JSObject* slotBase)
{
    if (!slotBase->usingInlineStorage() && oldStructureID->m_propertyMap.size() != newStructureID->m_propertyMap.size())
        slotBase->allocatePropertyStorage(oldStructureID->m_propertyMap.size(), newStructureID->m_propertyMap.size());
}

PassRefPtr<StructureID> StructureID::addPropertyTransition(StructureID* structureID, const Identifier& propertyName, JSValue* value, unsigned attributes, JSObject* slotBase, PutPropertySlot& slot, PropertyStorage& propertyStorage)
{
    ASSERT(!structureID->m_isDictionary);
    ASSERT(structureID->typeInfo().type() == ObjectType);

    if (StructureID* existingTransition = structureID->m_transitionTable.get(make_pair(propertyName.ustring().rep(), attributes))) {
        if (!slotBase->usingInlineStorage() && structureID->m_propertyMap.size() != existingTransition->m_propertyMap.size())
            slotBase->allocatePropertyStorage(structureID->m_propertyMap.size(), existingTransition->m_propertyMap.size());

        size_t offset = existingTransition->propertyMap().getOffset(propertyName);
        ASSERT(offset != WTF::notFound);
        propertyStorage[offset] = value;
        slot.setNewProperty(slotBase, offset);

        return existingTransition;
    }

    if (structureID->m_transitionCount > s_maxTransitionLength) {
        RefPtr<StructureID> transition = toDictionaryTransition(structureID);
        transition->m_propertyMap.put(propertyName, value, attributes, false, slotBase, slot, propertyStorage);
        return transition.release();
    }

    RefPtr<StructureID> transition = create(structureID->m_prototype, structureID->typeInfo());
    transition->m_cachedPrototypeChain = structureID->m_cachedPrototypeChain;
    transition->m_previous = structureID;
    transition->m_nameInPrevious = propertyName.ustring().rep();
    transition->m_attributesInPrevious = attributes;
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyMap = structureID->m_propertyMap;

    transition->m_propertyMap.put(propertyName, value, attributes, false, slotBase, slot, propertyStorage);

    structureID->m_transitionTable.add(make_pair(propertyName.ustring().rep(), attributes), transition.get());
    return transition.release();
}

PassRefPtr<StructureID> StructureID::toDictionaryTransition(StructureID* structureID)
{
    ASSERT(!structureID->m_isDictionary);

    RefPtr<StructureID> transition = create(structureID->m_prototype, structureID->typeInfo());
    transition->m_isDictionary = true;
    transition->m_propertyMap = structureID->m_propertyMap;
    return transition.release();
}

PassRefPtr<StructureID> StructureID::fromDictionaryTransition(StructureID* structureID)
{
    ASSERT(structureID->m_isDictionary);

    // Since dictionary StructureIDs are not shared, and no opcodes specialize
    // for them, we don't need to allocate a new StructureID when transitioning
    // to non-dictionary status.
    structureID->m_isDictionary = false;
    return structureID;
}

PassRefPtr<StructureID> StructureID::changePrototypeTransition(StructureID* structureID, JSValue* prototype)
{
    RefPtr<StructureID> transition = create(prototype, structureID->typeInfo());
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyMap = structureID->m_propertyMap;
    return transition.release();
}

PassRefPtr<StructureID> StructureID::getterSetterTransition(StructureID* structureID)
{
    RefPtr<StructureID> transition = create(structureID->storedPrototype(), structureID->typeInfo());
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyMap = structureID->m_propertyMap;
    return transition.release();
}

StructureID::~StructureID()
{
    if (m_previous) {
        ASSERT(m_previous->m_transitionTable.contains(make_pair(m_nameInPrevious, m_attributesInPrevious)));
        m_previous->m_transitionTable.remove(make_pair(m_nameInPrevious, m_attributesInPrevious));
    }

    if (m_cachedPropertyNameArrayData)
        m_cachedPropertyNameArrayData->setCachedStructureID(0);
}

StructureIDChain* StructureID::createCachedPrototypeChain()
{
    ASSERT(typeInfo().type() == ObjectType);
    ASSERT(!m_cachedPrototypeChain);

    JSValue* prototype = storedPrototype();
    if (JSImmediate::isImmediate(prototype))
        return 0;

    RefPtr<StructureIDChain> chain = StructureIDChain::create(static_cast<JSObject*>(prototype)->structureID());
    setCachedPrototypeChain(chain.release());
    return cachedPrototypeChain();
}

StructureIDChain::StructureIDChain(StructureID* structureID)
{
    size_t size = 1;

    StructureID* tmp = structureID;
    while (!tmp->storedPrototype()->isNull()) {
        ++size;
        tmp = static_cast<JSCell*>(tmp->storedPrototype())->structureID();
    }
    
    m_vector.set(new RefPtr<StructureID>[size + 1]);

    size_t i;
    for (i = 0; i < size - 1; ++i) {
        m_vector[i] = structureID;
        structureID = static_cast<JSObject*>(structureID->storedPrototype())->structureID();
    }
    m_vector[i] = structureID;
    m_vector[i + 1] = 0;
}

bool structureIDChainsAreEqual(StructureIDChain* chainA, StructureIDChain* chainB)
{
    if (!chainA || !chainB)
        return false;

    RefPtr<StructureID>* a = chainA->head();
    RefPtr<StructureID>* b = chainB->head();
    while (1) {
        if (*a != *b)
            return false;
        if (!*a)
            return true;
        a++;
        b++;
    }
}

} // namespace JSC
