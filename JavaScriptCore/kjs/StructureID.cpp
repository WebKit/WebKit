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
#include <wtf/RefPtr.h>

using namespace std;

namespace JSC {

    StructureID::StructureID(JSValue* prototype, JSType type)
    : m_isDictionary(false)
    , m_type(type)
    , m_prototype(prototype)
    , m_cachedPrototypeChain(0)
    , m_previous(0)
    , m_nameInPrevious(0)
    , m_transitionCount(0)
{
    ASSERT(m_prototype);
    ASSERT(m_prototype->isObject() || m_prototype->isNull());
}

PassRefPtr<StructureID> StructureID::addPropertyTransition(StructureID* structureID, const Identifier& propertyName, JSValue* value, unsigned attributes, bool checkReadOnly, JSObject* slotBase, PutPropertySlot& slot, PropertyStorage& propertyStorage)
{
    ASSERT(!structureID->m_isDictionary);
    ASSERT(structureID->m_type == ObjectType);

    if (StructureID* existingTransition = structureID->m_transitionTable.get(make_pair(propertyName.ustring().rep(), attributes))) {
        if (structureID->m_propertyMap.size() != existingTransition->m_propertyMap.size())
            existingTransition->m_propertyMap.resizePropertyStorage(propertyStorage, structureID->m_propertyMap.size());

        size_t offset = existingTransition->propertyMap().getOffset(propertyName);
        ASSERT(offset != WTF::notFound);
        propertyStorage[offset] = value;
        slot.setExistingProperty(slotBase, offset);

        return existingTransition;
    }

    if (structureID->m_transitionCount > s_maxTransitionLength) {
        RefPtr<StructureID> transition = toDictionaryTransition(structureID);
        transition->m_propertyMap.put(propertyName, value, attributes, checkReadOnly, slotBase, slot, propertyStorage);
        return transition.release();
    }

    RefPtr<StructureID> transition = create(structureID->m_prototype);
    transition->m_cachedPrototypeChain = structureID->m_cachedPrototypeChain;
    transition->m_previous = structureID;
    transition->m_nameInPrevious = propertyName.ustring().rep();
    transition->m_attributesInPrevious = attributes;
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyMap = structureID->m_propertyMap;

    transition->m_propertyMap.put(propertyName, value, attributes, checkReadOnly, slotBase, slot, propertyStorage);

    structureID->m_transitionTable.add(make_pair(propertyName.ustring().rep(), attributes), transition.get());
    return transition.release();
}

PassRefPtr<StructureID> StructureID::toDictionaryTransition(StructureID* structureID)
{
    ASSERT(!structureID->m_isDictionary);

    RefPtr<StructureID> transition = create(structureID->m_prototype);
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
    RefPtr<StructureID> transition = create(prototype);
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyMap = structureID->m_propertyMap;
    return transition.release();
}

PassRefPtr<StructureID> StructureID::getterSetterTransition(StructureID* structureID)
{
    RefPtr<StructureID> transition = create(structureID->storedPrototype());
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
}

StructureIDChain::StructureIDChain(StructureID* structureID)
{
    size_t size = 1;

    StructureID* tmp = structureID;
    while (!tmp->storedPrototype()->isNull()) {
        ++size;
        tmp = static_cast<JSCell*>(tmp->storedPrototype())->structureID();
    }
    
    m_vector.set(new RefPtr<StructureID>[size]);

    size_t i;
    for (i = 0; i < size - 1; ++i) {
        m_vector[i] = structureID;
        structureID = static_cast<JSObject*>(structureID->storedPrototype())->structureID();
    }
    m_vector[i] = structureID;
}

} // namespace JSC
