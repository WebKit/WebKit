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

#include "JSObject.h"
#include "PropertyNameArray.h"
#include "identifier.h"
#include "lookup.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/RefPtr.h>

#if ENABLE(JSC_MULTIPLE_THREADS)
#include <wtf/Threading.h>
#endif

using namespace std;

namespace JSC {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter structureIDCounter("StructureID");

#if ENABLE(JSC_MULTIPLE_THREADS)
static Mutex ignoreSetMutex;
#endif

static bool shouldIgnoreLeaks;
static HashSet<StructureID*> ignoreSet;
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
static HashSet<StructureID*> liveStructureIDSet;

void StructureID::dumpStatistics()
{
    unsigned numberLeaf = 0;
    unsigned numberUsingSingleSlot = 0;
    unsigned numberSingletons = 0;
    unsigned totalPropertyMapsSize = 0;

    HashSet<StructureID*>::const_iterator end = liveStructureIDSet.end();
    for (HashSet<StructureID*>::const_iterator it = liveStructureIDSet.begin(); it != end; ++it) {
        StructureID* structureID = *it;
        if (structureID->m_usingSingleTransitionSlot) {
            if (!structureID->m_transitions.singleTransition)
                ++numberLeaf;
            else
                ++numberUsingSingleSlot;

           if (!structureID->m_previous && !structureID->m_transitions.singleTransition)
                ++numberSingletons;
        }

        totalPropertyMapsSize += structureID->m_propertyMap.propertyMapSize();
    }

    printf("Number of live StructureIDs: %d\n", liveStructureIDSet.size());
    printf("Number of StructureIDs using the single item optimization for transition map: %d\n", numberUsingSingleSlot);
    printf("Number of StructureIDs that are leaf nodes: %d\n", numberLeaf);
    printf("Number of StructureIDs that singletons: %d\n", numberSingletons);

    printf("Size of a single StructureIDs: %d\n", static_cast<unsigned>(sizeof(StructureID)));
    printf("Size of sum of all property maps: %d\n", totalPropertyMapsSize);
    printf("Size of average of all property maps: %f\n", static_cast<double>(totalPropertyMapsSize) / static_cast<double>(liveStructureIDSet.size()));
}
#endif

StructureID::StructureID(JSValue* prototype, const TypeInfo& typeInfo)
    : m_typeInfo(typeInfo)
    , m_prototype(prototype)
    , m_cachedPrototypeChain(0)
    , m_previous(0)
    , m_nameInPrevious(0)
    , m_transitionCount(0)
    , m_propertyStorageCapacity(JSObject::inlineStorageCapacity)
    , m_cachedTransistionOffset(WTF::notFound)
    , m_isDictionary(false)
    , m_hasGetterSetterProperties(false)
    , m_usingSingleTransitionSlot(true)
    , m_attributesInPrevious(0)
{
    ASSERT(m_prototype);
    ASSERT(m_prototype->isObject() || m_prototype->isNull());

    m_transitions.singleTransition = 0;

#ifndef NDEBUG
#if ENABLE(JSC_MULTIPLE_THREADS)
    MutexLocker protect(ignoreSetMutex);
#endif
    if (shouldIgnoreLeaks)
        ignoreSet.add(this);
    else
        structureIDCounter.increment();
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
    liveStructureIDSet.add(this);
#endif
}

StructureID::~StructureID()
{
    if (m_previous) {
        if (m_previous->m_usingSingleTransitionSlot) {
            m_previous->m_transitions.singleTransition = 0;
        } else {
            ASSERT(m_previous->m_transitions.table->contains(make_pair(m_nameInPrevious, m_attributesInPrevious)));
            m_previous->m_transitions.table->remove(make_pair(m_nameInPrevious, m_attributesInPrevious));
        }
    }

    if (m_cachedPropertyNameArrayData)
        m_cachedPropertyNameArrayData->setCachedStructureID(0);

    if (!m_usingSingleTransitionSlot)
        delete m_transitions.table;

#ifndef NDEBUG
#if ENABLE(JSC_MULTIPLE_THREADS)
    MutexLocker protect(ignoreSetMutex);
#endif
    HashSet<StructureID*>::iterator it = ignoreSet.find(this);
    if (it != ignoreSet.end())
        ignoreSet.remove(it);
    else
        structureIDCounter.decrement();
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
    liveStructureIDSet.remove(this);
#endif
}

void StructureID::startIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = true;
#endif
}

void StructureID::stopIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = false;
#endif
}

void StructureID::getEnumerablePropertyNames(ExecState* exec, PropertyNameArray& propertyNames, JSObject* baseObject)
{
    bool shouldCache = propertyNames.cacheable() && !(propertyNames.size() || m_isDictionary);

    if (shouldCache) {
        if (m_cachedPropertyNameArrayData) {
            if (structureIDChainsAreEqual(m_cachedPropertyNameArrayData->cachedPrototypeChain(), cachedPrototypeChain())) {
                propertyNames.setData(m_cachedPropertyNameArrayData);
                return;
            }
        }
        propertyNames.setCacheable(false);
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
        asObject(m_prototype)->getPropertyNames(exec, propertyNames);

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

void StructureID::growPropertyStorageCapacity()
{
    if (m_propertyStorageCapacity == JSObject::inlineStorageCapacity)
        m_propertyStorageCapacity = JSObject::nonInlineBaseStorageCapacity;
    else
        m_propertyStorageCapacity *= 2;
}

PassRefPtr<StructureID> StructureID::addPropertyTransition(StructureID* structureID, const Identifier& propertyName, unsigned attributes, size_t& offset)
{
    ASSERT(!structureID->m_isDictionary);
    ASSERT(structureID->typeInfo().type() == ObjectType);

    if (structureID->m_usingSingleTransitionSlot) {
        StructureID* existingTransition = structureID->m_transitions.singleTransition;
        if (existingTransition && existingTransition->m_nameInPrevious == propertyName.ustring().rep() && existingTransition->m_attributesInPrevious == attributes) {
            offset = structureID->m_transitions.singleTransition->cachedTransistionOffset();
            ASSERT(offset != WTF::notFound);
            return existingTransition;
        }
    } else {
        if (StructureID* existingTransition = structureID->m_transitions.table->get(make_pair(propertyName.ustring().rep(), attributes))) {
            offset = existingTransition->cachedTransistionOffset();
            ASSERT(offset != WTF::notFound);
            return existingTransition;
        }        
    }

    if (structureID->m_transitionCount > s_maxTransitionLength) {
        RefPtr<StructureID> transition = toDictionaryTransition(structureID);
        offset = transition->m_propertyMap.put(propertyName, attributes);
        if (transition->m_propertyMap.storageSize() > transition->propertyStorageCapacity())
            transition->growPropertyStorageCapacity();
        return transition.release();
    }

    RefPtr<StructureID> transition = create(structureID->m_prototype, structureID->typeInfo());
    transition->m_cachedPrototypeChain = structureID->m_cachedPrototypeChain;
    transition->m_previous = structureID;
    transition->m_nameInPrevious = propertyName.ustring().rep();
    transition->m_attributesInPrevious = attributes;
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyMap = structureID->m_propertyMap;
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structureID->m_hasGetterSetterProperties;

    offset = transition->m_propertyMap.put(propertyName, attributes);
    if (transition->m_propertyMap.storageSize() > transition->propertyStorageCapacity())
        transition->growPropertyStorageCapacity();

    transition->setCachedTransistionOffset(offset);

    if (structureID->m_usingSingleTransitionSlot) {
        if (!structureID->m_transitions.singleTransition) {
            structureID->m_transitions.singleTransition = transition.get();
            return transition.release();
        }

        StructureID* existingTransition = structureID->m_transitions.singleTransition;
        structureID->m_usingSingleTransitionSlot = false;
        StructureIDTransitionTable* transitionTable = new StructureIDTransitionTable;
        structureID->m_transitions.table = transitionTable;
        transitionTable->add(make_pair(existingTransition->m_nameInPrevious, existingTransition->m_attributesInPrevious), existingTransition);
    }
    structureID->m_transitions.table->add(make_pair(propertyName.ustring().rep(), attributes), transition.get());
    return transition.release();
}

PassRefPtr<StructureID> StructureID::toDictionaryTransition(StructureID* structureID)
{
    ASSERT(!structureID->m_isDictionary);

    RefPtr<StructureID> transition = create(structureID->m_prototype, structureID->typeInfo());
    transition->m_isDictionary = true;
    transition->m_propertyMap = structureID->m_propertyMap;
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structureID->m_hasGetterSetterProperties;
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
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structureID->m_hasGetterSetterProperties;
    return transition.release();
}

PassRefPtr<StructureID> StructureID::getterSetterTransition(StructureID* structureID)
{
    RefPtr<StructureID> transition = create(structureID->storedPrototype(), structureID->typeInfo());
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyMap = structureID->m_propertyMap;
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = transition->m_hasGetterSetterProperties;
    return transition.release();
}

size_t StructureID::addPropertyWithoutTransition(const Identifier& propertyName, unsigned attributes)
{
    size_t offset = m_propertyMap.put(propertyName, attributes);
    if (m_propertyMap.storageSize() > propertyStorageCapacity())
        growPropertyStorageCapacity();
    return offset;
}

StructureIDChain* StructureID::createCachedPrototypeChain()
{
    ASSERT(typeInfo().type() == ObjectType);
    ASSERT(!m_cachedPrototypeChain);

    JSValue* prototype = storedPrototype();
    if (JSImmediate::isImmediate(prototype))
        return 0;

    RefPtr<StructureIDChain> chain = StructureIDChain::create(asObject(prototype)->structureID());
    setCachedPrototypeChain(chain.release());
    return cachedPrototypeChain();
}

StructureIDChain::StructureIDChain(StructureID* structureID)
{
    size_t size = 1;

    StructureID* tmp = structureID;
    while (!tmp->storedPrototype()->isNull()) {
        ++size;
        tmp = asCell(tmp->storedPrototype())->structureID();
    }
    
    m_vector.set(new RefPtr<StructureID>[size + 1]);

    size_t i;
    for (i = 0; i < size - 1; ++i) {
        m_vector[i] = structureID;
        structureID = asObject(structureID->storedPrototype())->structureID();
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
