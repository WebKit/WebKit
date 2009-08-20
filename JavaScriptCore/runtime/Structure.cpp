/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#include "Structure.h"

#include "Identifier.h"
#include "JSObject.h"
#include "PropertyNameArray.h"
#include "StructureChain.h"
#include "Lookup.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/RefPtr.h>

#if ENABLE(JSC_MULTIPLE_THREADS)
#include <wtf/Threading.h>
#endif

#define DUMP_STRUCTURE_ID_STATISTICS 0

#ifndef NDEBUG
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#else
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#endif

using namespace std;
using namespace WTF;

namespace JSC {

// Choose a number for the following so that most property maps are smaller,
// but it's not going to blow out the stack to allocate this number of pointers.
static const int smallMapThreshold = 1024;

// The point at which the function call overhead of the qsort implementation
// becomes small compared to the inefficiency of insertion sort.
static const unsigned tinyMapThreshold = 20;

static const unsigned newTableSize = 16;

#ifndef NDEBUG
static WTF::RefCountedLeakCounter structureCounter("Structure");

#if ENABLE(JSC_MULTIPLE_THREADS)
static Mutex& ignoreSetMutex = *(new Mutex);
#endif

static bool shouldIgnoreLeaks;
static HashSet<Structure*>& ignoreSet = *(new HashSet<Structure*>);
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
static HashSet<Structure*>& liveStructureSet = *(new HashSet<Structure*>);
#endif

void Structure::dumpStatistics()
{
#if DUMP_STRUCTURE_ID_STATISTICS
    unsigned numberLeaf = 0;
    unsigned numberUsingSingleSlot = 0;
    unsigned numberSingletons = 0;
    unsigned numberWithPropertyMaps = 0;
    unsigned totalPropertyMapsSize = 0;

    HashSet<Structure*>::const_iterator end = liveStructureSet.end();
    for (HashSet<Structure*>::const_iterator it = liveStructureSet.begin(); it != end; ++it) {
        Structure* structure = *it;
        if (structure->m_usingSingleTransitionSlot) {
            if (!structure->m_transitions.singleTransition)
                ++numberLeaf;
            else
                ++numberUsingSingleSlot;

           if (!structure->m_previous && !structure->m_transitions.singleTransition)
                ++numberSingletons;
        }

        if (structure->m_propertyTable) {
            ++numberWithPropertyMaps;
            totalPropertyMapsSize += PropertyMapHashTable::allocationSize(structure->m_propertyTable->size);
            if (structure->m_propertyTable->deletedOffsets)
                totalPropertyMapsSize += (structure->m_propertyTable->deletedOffsets->capacity() * sizeof(unsigned)); 
        }
    }

    printf("Number of live Structures: %d\n", liveStructureSet.size());
    printf("Number of Structures using the single item optimization for transition map: %d\n", numberUsingSingleSlot);
    printf("Number of Structures that are leaf nodes: %d\n", numberLeaf);
    printf("Number of Structures that singletons: %d\n", numberSingletons);
    printf("Number of Structures with PropertyMaps: %d\n", numberWithPropertyMaps);

    printf("Size of a single Structures: %d\n", static_cast<unsigned>(sizeof(Structure)));
    printf("Size of sum of all property maps: %d\n", totalPropertyMapsSize);
    printf("Size of average of all property maps: %f\n", static_cast<double>(totalPropertyMapsSize) / static_cast<double>(liveStructureSet.size()));
#else
    printf("Dumping Structure statistics is not enabled.\n");
#endif
}

Structure::Structure(JSValue prototype, const TypeInfo& typeInfo)
    : m_typeInfo(typeInfo)
    , m_prototype(prototype)
    , m_specificValueInPrevious(0)
    , m_propertyTable(0)
    , m_propertyStorageCapacity(JSObject::inlineStorageCapacity)
    , m_offset(noOffset)
    , m_isDictionary(false)
    , m_isPinnedPropertyTable(false)
    , m_hasGetterSetterProperties(false)
    , m_usingSingleTransitionSlot(true)
    , m_attributesInPrevious(0)
{
    ASSERT(m_prototype);
    ASSERT(m_prototype.isObject() || m_prototype.isNull());

    m_transitions.singleTransition = 0;

#ifndef NDEBUG
#if ENABLE(JSC_MULTIPLE_THREADS)
    MutexLocker protect(ignoreSetMutex);
#endif
    if (shouldIgnoreLeaks)
        ignoreSet.add(this);
    else
        structureCounter.increment();
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
    liveStructureSet.add(this);
#endif
}

Structure::~Structure()
{
    if (m_previous) {
        if (m_previous->m_usingSingleTransitionSlot) {
            m_previous->m_transitions.singleTransition = 0;
        } else {
            ASSERT(m_previous->m_transitions.table->contains(make_pair(m_nameInPrevious.get(), m_attributesInPrevious), m_specificValueInPrevious));
            m_previous->m_transitions.table->remove(make_pair(m_nameInPrevious.get(), m_attributesInPrevious), m_specificValueInPrevious);
        }
    }

    if (m_cachedPropertyNameArrayData)
        m_cachedPropertyNameArrayData->setCachedStructure(0);

    if (!m_usingSingleTransitionSlot)
        delete m_transitions.table;

    if (m_propertyTable) {
        unsigned entryCount = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount;
        for (unsigned i = 1; i <= entryCount; i++) {
            if (UString::Rep* key = m_propertyTable->entries()[i].key)
                key->deref();
        }

        delete m_propertyTable->deletedOffsets;
        fastFree(m_propertyTable);
    }

#ifndef NDEBUG
#if ENABLE(JSC_MULTIPLE_THREADS)
    MutexLocker protect(ignoreSetMutex);
#endif
    HashSet<Structure*>::iterator it = ignoreSet.find(this);
    if (it != ignoreSet.end())
        ignoreSet.remove(it);
    else
        structureCounter.decrement();
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
    liveStructureSet.remove(this);
#endif
}

void Structure::startIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = true;
#endif
}

void Structure::stopIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = false;
#endif
}

static bool isPowerOf2(unsigned v)
{
    // Taken from http://www.cs.utk.edu/~vose/c-stuff/bithacks.html
    
    return !(v & (v - 1)) && v;
}

static unsigned nextPowerOf2(unsigned v)
{
    // Taken from http://www.cs.utk.edu/~vose/c-stuff/bithacks.html
    // Devised by Sean Anderson, Sepember 14, 2001

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

static unsigned sizeForKeyCount(size_t keyCount)
{
    if (keyCount == notFound)
        return newTableSize;

    if (keyCount < 8)
        return newTableSize;

    if (isPowerOf2(keyCount))
        return keyCount * 4;

    return nextPowerOf2(keyCount) * 2;
}

void Structure::materializePropertyMap()
{
    ASSERT(!m_propertyTable);

    Vector<Structure*, 8> structures;
    structures.append(this);

    Structure* structure = this;

    // Search for the last Structure with a property table. 
    while ((structure = structure->previousID())) {
        if (structure->m_isPinnedPropertyTable) {
            ASSERT(structure->m_propertyTable);
            ASSERT(!structure->m_previous);

            m_propertyTable = structure->copyPropertyTable();
            break;
        }

        structures.append(structure);
    }

    if (!m_propertyTable)
        createPropertyMapHashTable(sizeForKeyCount(m_offset + 1));
    else {
        if (sizeForKeyCount(m_offset + 1) > m_propertyTable->size)
            rehashPropertyMapHashTable(sizeForKeyCount(m_offset + 1)); // This could be made more efficient by combining with the copy above. 
    }

    for (ptrdiff_t i = structures.size() - 2; i >= 0; --i) {
        structure = structures[i];
        structure->m_nameInPrevious->ref();
        PropertyMapEntry entry(structure->m_nameInPrevious.get(), structure->m_offset, structure->m_attributesInPrevious, structure->m_specificValueInPrevious, ++m_propertyTable->lastIndexUsed);
        insertIntoPropertyMapHashTable(entry);
    }
}

void Structure::getEnumerablePropertyNames(ExecState* exec, PropertyNameArray& propertyNames, JSObject* baseObject)
{
    bool shouldCache = propertyNames.shouldCache() && !(propertyNames.size() || m_isDictionary);

    if (shouldCache && m_cachedPropertyNameArrayData) {
        if (m_cachedPropertyNameArrayData->cachedPrototypeChain() == prototypeChain(exec)) {
            propertyNames.setData(m_cachedPropertyNameArrayData);
            return;
        }
        clearEnumerationCache();
    }

    getEnumerableNamesFromPropertyTable(propertyNames);
    getEnumerableNamesFromClassInfoTable(exec, baseObject->classInfo(), propertyNames);

    if (m_prototype.isObject()) {
        propertyNames.setShouldCache(false); // No need for our prototypes to waste memory on caching, since they're not being enumerated directly.
        asObject(m_prototype)->getPropertyNames(exec, propertyNames);
    }

    if (shouldCache) {
        StructureChain* protoChain = prototypeChain(exec);
        m_cachedPropertyNameArrayData = propertyNames.data();
        if (!protoChain->isCacheable())
            return;
        m_cachedPropertyNameArrayData->setCachedPrototypeChain(protoChain);
        m_cachedPropertyNameArrayData->setCachedStructure(this);
    }
}

void Structure::clearEnumerationCache()
{
    if (m_cachedPropertyNameArrayData)
        m_cachedPropertyNameArrayData->setCachedStructure(0);
    m_cachedPropertyNameArrayData.clear();
}

void Structure::growPropertyStorageCapacity()
{
    if (m_propertyStorageCapacity == JSObject::inlineStorageCapacity)
        m_propertyStorageCapacity = JSObject::nonInlineBaseStorageCapacity;
    else
        m_propertyStorageCapacity *= 2;
}

void Structure::despecifyDictionaryFunction(const Identifier& propertyName)
{
    const UString::Rep* rep = propertyName._ustring.rep();

    materializePropertyMapIfNecessary();

    ASSERT(m_isDictionary);
    ASSERT(m_propertyTable);

    unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
    ASSERT(entryIndex != emptyEntryIndex);

    if (rep == m_propertyTable->entries()[entryIndex - 1].key) {
        m_propertyTable->entries()[entryIndex - 1].specificValue = 0;
        return;
    }

#if DUMP_PROPERTYMAP_STATS
    ++numCollisions;
#endif

    unsigned k = 1 | doubleHash(rep->computedHash());

    while (1) {
        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif

        entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        ASSERT(entryIndex != emptyEntryIndex);

        if (rep == m_propertyTable->entries()[entryIndex - 1].key) {
            m_propertyTable->entries()[entryIndex - 1].specificValue = 0;
            return;
        }
    }
}

PassRefPtr<Structure> Structure::addPropertyTransitionToExistingStructure(Structure* structure, const Identifier& propertyName, unsigned attributes, JSCell* specificValue, size_t& offset)
{
    ASSERT(!structure->m_isDictionary);
    ASSERT(structure->typeInfo().type() == ObjectType);

    if (structure->m_usingSingleTransitionSlot) {
        Structure* existingTransition = structure->m_transitions.singleTransition;
        if (existingTransition && existingTransition->m_nameInPrevious.get() == propertyName.ustring().rep()
            && existingTransition->m_attributesInPrevious == attributes
            && (existingTransition->m_specificValueInPrevious == specificValue || existingTransition->m_specificValueInPrevious == 0)) {

            ASSERT(structure->m_transitions.singleTransition->m_offset != noOffset);
            offset = structure->m_transitions.singleTransition->m_offset;
            return existingTransition;
        }
    } else {
        if (Structure* existingTransition = structure->m_transitions.table->get(make_pair(propertyName.ustring().rep(), attributes), specificValue)) {
            ASSERT(existingTransition->m_offset != noOffset);
            offset = existingTransition->m_offset;
            return existingTransition;
        }
    }

    return 0;
}

PassRefPtr<Structure> Structure::addPropertyTransition(Structure* structure, const Identifier& propertyName, unsigned attributes, JSCell* specificValue, size_t& offset)
{
    ASSERT(!structure->m_isDictionary);
    ASSERT(structure->typeInfo().type() == ObjectType);
    ASSERT(!Structure::addPropertyTransitionToExistingStructure(structure, propertyName, attributes, specificValue, offset));

    if (structure->transitionCount() > s_maxTransitionLength) {
        RefPtr<Structure> transition = toDictionaryTransition(structure);
        ASSERT(structure != transition);
        offset = transition->put(propertyName, attributes, specificValue);
        if (transition->propertyStorageSize() > transition->propertyStorageCapacity())
            transition->growPropertyStorageCapacity();
        return transition.release();
    }

    RefPtr<Structure> transition = create(structure->m_prototype, structure->typeInfo());

    transition->m_cachedPrototypeChain = structure->m_cachedPrototypeChain;
    transition->m_previous = structure;
    transition->m_nameInPrevious = propertyName.ustring().rep();
    transition->m_attributesInPrevious = attributes;
    transition->m_specificValueInPrevious = specificValue;
    transition->m_propertyStorageCapacity = structure->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structure->m_hasGetterSetterProperties;

    if (structure->m_propertyTable) {
        if (structure->m_isPinnedPropertyTable)
            transition->m_propertyTable = structure->copyPropertyTable();
        else {
            transition->m_propertyTable = structure->m_propertyTable;
            structure->m_propertyTable = 0;
        }
    } else {
        if (structure->m_previous)
            transition->materializePropertyMap();
        else
            transition->createPropertyMapHashTable();
    }

    offset = transition->put(propertyName, attributes, specificValue);
    if (transition->propertyStorageSize() > transition->propertyStorageCapacity())
        transition->growPropertyStorageCapacity();

    transition->m_offset = offset;

    if (structure->m_usingSingleTransitionSlot) {
        if (!structure->m_transitions.singleTransition) {
            structure->m_transitions.singleTransition = transition.get();
            return transition.release();
        }

        Structure* existingTransition = structure->m_transitions.singleTransition;
        structure->m_usingSingleTransitionSlot = false;
        StructureTransitionTable* transitionTable = new StructureTransitionTable;
        structure->m_transitions.table = transitionTable;
        transitionTable->add(make_pair(existingTransition->m_nameInPrevious.get(), existingTransition->m_attributesInPrevious), existingTransition, existingTransition->m_specificValueInPrevious);
    }
    structure->m_transitions.table->add(make_pair(propertyName.ustring().rep(), attributes), transition.get(), specificValue);
    return transition.release();
}

PassRefPtr<Structure> Structure::removePropertyTransition(Structure* structure, const Identifier& propertyName, size_t& offset)
{
    ASSERT(!structure->m_isDictionary);

    RefPtr<Structure> transition = toDictionaryTransition(structure);

    offset = transition->remove(propertyName);

    return transition.release();
}

PassRefPtr<Structure> Structure::changePrototypeTransition(Structure* structure, JSValue prototype)
{
    RefPtr<Structure> transition = create(prototype, structure->typeInfo());

    transition->m_propertyStorageCapacity = structure->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structure->m_hasGetterSetterProperties;

    // Don't set m_offset, as one can not transition to this.

    structure->materializePropertyMapIfNecessary();
    transition->m_propertyTable = structure->copyPropertyTable();
    transition->m_isPinnedPropertyTable = true;

    return transition.release();
}

PassRefPtr<Structure> Structure::despecifyFunctionTransition(Structure* structure, const Identifier& replaceFunction)
{
    RefPtr<Structure> transition = create(structure->storedPrototype(), structure->typeInfo());

    transition->m_propertyStorageCapacity = structure->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structure->m_hasGetterSetterProperties;

    // Don't set m_offset, as one can not transition to this.

    structure->materializePropertyMapIfNecessary();
    transition->m_propertyTable = structure->copyPropertyTable();
    transition->m_isPinnedPropertyTable = true;

    bool removed = transition->despecifyFunction(replaceFunction);
    ASSERT_UNUSED(removed, removed);

    return transition.release();
}

PassRefPtr<Structure> Structure::getterSetterTransition(Structure* structure)
{
    RefPtr<Structure> transition = create(structure->storedPrototype(), structure->typeInfo());
    transition->m_propertyStorageCapacity = structure->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = transition->m_hasGetterSetterProperties;

    // Don't set m_offset, as one can not transition to this.

    structure->materializePropertyMapIfNecessary();
    transition->m_propertyTable = structure->copyPropertyTable();
    transition->m_isPinnedPropertyTable = true;

    return transition.release();
}

PassRefPtr<Structure> Structure::toDictionaryTransition(Structure* structure)
{
    ASSERT(!structure->m_isDictionary);

    RefPtr<Structure> transition = create(structure->m_prototype, structure->typeInfo());
    transition->m_isDictionary = true;
    transition->m_propertyStorageCapacity = structure->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structure->m_hasGetterSetterProperties;

    structure->materializePropertyMapIfNecessary();
    transition->m_propertyTable = structure->copyPropertyTable();
    transition->m_isPinnedPropertyTable = true;

    return transition.release();
}

PassRefPtr<Structure> Structure::fromDictionaryTransition(Structure* structure)
{
    ASSERT(structure->m_isDictionary);

    // Since dictionary Structures are not shared, and no opcodes specialize
    // for them, we don't need to allocate a new Structure when transitioning
    // to non-dictionary status.

    // FIMXE: We can make this more efficient by canonicalizing the Structure (draining the
    // deleted offsets vector) before transitioning from dictionary. 
    if (!structure->m_propertyTable || !structure->m_propertyTable->deletedOffsets || structure->m_propertyTable->deletedOffsets->isEmpty())
        structure->m_isDictionary = false;

    return structure;
}

size_t Structure::addPropertyWithoutTransition(const Identifier& propertyName, unsigned attributes, JSCell* specificValue)
{
    ASSERT(!m_transitions.singleTransition);

    materializePropertyMapIfNecessary();

    m_isPinnedPropertyTable = true;
    size_t offset = put(propertyName, attributes, specificValue);
    if (propertyStorageSize() > propertyStorageCapacity())
        growPropertyStorageCapacity();
    clearEnumerationCache();
    return offset;
}

size_t Structure::removePropertyWithoutTransition(const Identifier& propertyName)
{
    ASSERT(!m_transitions.singleTransition);
    ASSERT(m_isDictionary);

    materializePropertyMapIfNecessary();

    m_isPinnedPropertyTable = true;
    size_t offset = remove(propertyName);
    clearEnumerationCache();
    return offset;
}

#if DUMP_PROPERTYMAP_STATS

static int numProbes;
static int numCollisions;
static int numRehashes;
static int numRemoves;

struct PropertyMapStatisticsExitLogger {
    ~PropertyMapStatisticsExitLogger();
};

static PropertyMapStatisticsExitLogger logger;

PropertyMapStatisticsExitLogger::~PropertyMapStatisticsExitLogger()
{
    printf("\nJSC::PropertyMap statistics\n\n");
    printf("%d probes\n", numProbes);
    printf("%d collisions (%.1f%%)\n", numCollisions, 100.0 * numCollisions / numProbes);
    printf("%d rehashes\n", numRehashes);
    printf("%d removes\n", numRemoves);
}

#endif

static const unsigned deletedSentinelIndex = 1;

#if !DO_PROPERTYMAP_CONSTENCY_CHECK

inline void Structure::checkConsistency()
{
}

#endif

PropertyMapHashTable* Structure::copyPropertyTable()
{
    if (!m_propertyTable)
        return 0;

    size_t tableSize = PropertyMapHashTable::allocationSize(m_propertyTable->size);
    PropertyMapHashTable* newTable = static_cast<PropertyMapHashTable*>(fastMalloc(tableSize));
    memcpy(newTable, m_propertyTable, tableSize);

    unsigned entryCount = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; ++i) {
        if (UString::Rep* key = newTable->entries()[i].key)
            key->ref();
    }

    // Copy the deletedOffsets vector.
    if (m_propertyTable->deletedOffsets)
        newTable->deletedOffsets = new Vector<unsigned>(*m_propertyTable->deletedOffsets);

    return newTable;
}

size_t Structure::get(const UString::Rep* rep, unsigned& attributes, JSCell*& specificValue)
{
    materializePropertyMapIfNecessary();
    if (!m_propertyTable)
        return notFound;

    unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
    if (entryIndex == emptyEntryIndex)
        return notFound;

    if (rep == m_propertyTable->entries()[entryIndex - 1].key) {
        attributes = m_propertyTable->entries()[entryIndex - 1].attributes;
        specificValue = m_propertyTable->entries()[entryIndex - 1].specificValue;
        return m_propertyTable->entries()[entryIndex - 1].offset;
    }

#if DUMP_PROPERTYMAP_STATS
    ++numCollisions;
#endif

    unsigned k = 1 | doubleHash(rep->computedHash());

    while (1) {
        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif

        entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return notFound;

        if (rep == m_propertyTable->entries()[entryIndex - 1].key) {
            attributes = m_propertyTable->entries()[entryIndex - 1].attributes;
            specificValue = m_propertyTable->entries()[entryIndex - 1].specificValue;
            return m_propertyTable->entries()[entryIndex - 1].offset;
        }
    }
}

bool Structure::despecifyFunction(const Identifier& propertyName)
{
    ASSERT(!propertyName.isNull());

    materializePropertyMapIfNecessary();
    if (!m_propertyTable)
        return false;

    UString::Rep* rep = propertyName._ustring.rep();

    unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
    if (entryIndex == emptyEntryIndex)
        return false;

    if (rep == m_propertyTable->entries()[entryIndex - 1].key) {
        ASSERT(m_propertyTable->entries()[entryIndex - 1].specificValue);
        m_propertyTable->entries()[entryIndex - 1].specificValue = 0;
        return true;
    }

#if DUMP_PROPERTYMAP_STATS
    ++numCollisions;
#endif

    unsigned k = 1 | doubleHash(rep->computedHash());

    while (1) {
        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif

        entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return false;

        if (rep == m_propertyTable->entries()[entryIndex - 1].key) {
            ASSERT(m_propertyTable->entries()[entryIndex - 1].specificValue);
            m_propertyTable->entries()[entryIndex - 1].specificValue = 0;
            return true;
        }
    }
}

size_t Structure::put(const Identifier& propertyName, unsigned attributes, JSCell* specificValue)
{
    ASSERT(!propertyName.isNull());
    ASSERT(get(propertyName) == notFound);

    checkConsistency();

    UString::Rep* rep = propertyName._ustring.rep();

    if (!m_propertyTable)
        createPropertyMapHashTable();

    // FIXME: Consider a fast case for tables with no deleted sentinels.

    unsigned i = rep->computedHash();
    unsigned k = 0;
    bool foundDeletedElement = false;
    unsigned deletedElementIndex = 0; // initialize to make the compiler happy

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    while (1) {
        unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            break;

        if (entryIndex == deletedSentinelIndex) {
            // If we find a deleted-element sentinel, remember it for use later.
            if (!foundDeletedElement) {
                foundDeletedElement = true;
                deletedElementIndex = i;
            }
        }

        if (k == 0) {
            k = 1 | doubleHash(rep->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    // Figure out which entry to use.
    unsigned entryIndex = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount + 2;
    if (foundDeletedElement) {
        i = deletedElementIndex;
        --m_propertyTable->deletedSentinelCount;

        // Since we're not making the table bigger, we can't use the entry one past
        // the end that we were planning on using, so search backwards for the empty
        // slot that we can use. We know it will be there because we did at least one
        // deletion in the past that left an entry empty.
        while (m_propertyTable->entries()[--entryIndex - 1].key) { }
    }

    // Create a new hash table entry.
    m_propertyTable->entryIndices[i & m_propertyTable->sizeMask] = entryIndex;

    // Create a new hash table entry.
    rep->ref();
    m_propertyTable->entries()[entryIndex - 1].key = rep;
    m_propertyTable->entries()[entryIndex - 1].attributes = attributes;
    m_propertyTable->entries()[entryIndex - 1].specificValue = specificValue;
    m_propertyTable->entries()[entryIndex - 1].index = ++m_propertyTable->lastIndexUsed;

    unsigned newOffset;
    if (m_propertyTable->deletedOffsets && !m_propertyTable->deletedOffsets->isEmpty()) {
        newOffset = m_propertyTable->deletedOffsets->last();
        m_propertyTable->deletedOffsets->removeLast();
    } else
        newOffset = m_propertyTable->keyCount;
    m_propertyTable->entries()[entryIndex - 1].offset = newOffset;

    ++m_propertyTable->keyCount;

    if ((m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount) * 2 >= m_propertyTable->size)
        expandPropertyMapHashTable();

    checkConsistency();
    return newOffset;
}

bool Structure::hasTransition(UString::Rep* rep, unsigned attributes)
{
    if (m_usingSingleTransitionSlot) {
        return m_transitions.singleTransition &&
            m_transitions.singleTransition->m_nameInPrevious.get() == rep &&
            m_transitions.singleTransition->m_attributesInPrevious == attributes;
    }
    return m_transitions.table->hasTransition(make_pair(rep, attributes));
}

size_t Structure::remove(const Identifier& propertyName)
{
    ASSERT(!propertyName.isNull());

    checkConsistency();

    UString::Rep* rep = propertyName._ustring.rep();

    if (!m_propertyTable)
        return notFound;

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
    ++numRemoves;
#endif

    // Find the thing to remove.
    unsigned i = rep->computedHash();
    unsigned k = 0;
    unsigned entryIndex;
    UString::Rep* key = 0;
    while (1) {
        entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return notFound;

        key = m_propertyTable->entries()[entryIndex - 1].key;
        if (rep == key)
            break;

        if (k == 0) {
            k = 1 | doubleHash(rep->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    // Replace this one element with the deleted sentinel. Also clear out
    // the entry so we can iterate all the entries as needed.
    m_propertyTable->entryIndices[i & m_propertyTable->sizeMask] = deletedSentinelIndex;

    size_t offset = m_propertyTable->entries()[entryIndex - 1].offset;

    key->deref();
    m_propertyTable->entries()[entryIndex - 1].key = 0;
    m_propertyTable->entries()[entryIndex - 1].attributes = 0;
    m_propertyTable->entries()[entryIndex - 1].specificValue = 0;
    m_propertyTable->entries()[entryIndex - 1].offset = 0;

    if (!m_propertyTable->deletedOffsets)
        m_propertyTable->deletedOffsets = new Vector<unsigned>;
    m_propertyTable->deletedOffsets->append(offset);

    ASSERT(m_propertyTable->keyCount >= 1);
    --m_propertyTable->keyCount;
    ++m_propertyTable->deletedSentinelCount;

    if (m_propertyTable->deletedSentinelCount * 4 >= m_propertyTable->size)
        rehashPropertyMapHashTable();

    checkConsistency();
    return offset;
}

void Structure::insertIntoPropertyMapHashTable(const PropertyMapEntry& entry)
{
    ASSERT(m_propertyTable);

    unsigned i = entry.key->computedHash();
    unsigned k = 0;

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    while (1) {
        unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            break;

        if (k == 0) {
            k = 1 | doubleHash(entry.key->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    unsigned entryIndex = m_propertyTable->keyCount + 2;
    m_propertyTable->entryIndices[i & m_propertyTable->sizeMask] = entryIndex;
    m_propertyTable->entries()[entryIndex - 1] = entry;

    ++m_propertyTable->keyCount;
}

void Structure::createPropertyMapHashTable()
{
    ASSERT(sizeForKeyCount(7) == newTableSize);
    createPropertyMapHashTable(newTableSize);
}

void Structure::createPropertyMapHashTable(unsigned newTableSize)
{
    ASSERT(!m_propertyTable);
    ASSERT(isPowerOf2(newTableSize));

    checkConsistency();

    m_propertyTable = static_cast<PropertyMapHashTable*>(fastZeroedMalloc(PropertyMapHashTable::allocationSize(newTableSize)));
    m_propertyTable->size = newTableSize;
    m_propertyTable->sizeMask = newTableSize - 1;

    checkConsistency();
}

void Structure::expandPropertyMapHashTable()
{
    ASSERT(m_propertyTable);
    rehashPropertyMapHashTable(m_propertyTable->size * 2);
}

void Structure::rehashPropertyMapHashTable()
{
    ASSERT(m_propertyTable);
    ASSERT(m_propertyTable->size);
    rehashPropertyMapHashTable(m_propertyTable->size);
}

void Structure::rehashPropertyMapHashTable(unsigned newTableSize)
{
    ASSERT(m_propertyTable);
    ASSERT(isPowerOf2(newTableSize));

    checkConsistency();

    PropertyMapHashTable* oldTable = m_propertyTable;

    m_propertyTable = static_cast<PropertyMapHashTable*>(fastZeroedMalloc(PropertyMapHashTable::allocationSize(newTableSize)));
    m_propertyTable->size = newTableSize;
    m_propertyTable->sizeMask = newTableSize - 1;

    unsigned lastIndexUsed = 0;
    unsigned entryCount = oldTable->keyCount + oldTable->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; ++i) {
        if (oldTable->entries()[i].key) {
            lastIndexUsed = max(oldTable->entries()[i].index, lastIndexUsed);
            insertIntoPropertyMapHashTable(oldTable->entries()[i]);
        }
    }
    m_propertyTable->lastIndexUsed = lastIndexUsed;
    m_propertyTable->deletedOffsets = oldTable->deletedOffsets;

    fastFree(oldTable);

    checkConsistency();
}

static int comparePropertyMapEntryIndices(const void* a, const void* b)
{
    unsigned ia = static_cast<PropertyMapEntry* const*>(a)[0]->index;
    unsigned ib = static_cast<PropertyMapEntry* const*>(b)[0]->index;
    if (ia < ib)
        return -1;
    if (ia > ib)
        return +1;
    return 0;
}

void Structure::getEnumerableNamesFromPropertyTable(PropertyNameArray& propertyNames)
{
    materializePropertyMapIfNecessary();
    if (!m_propertyTable)
        return;

    if (m_propertyTable->keyCount < tinyMapThreshold) {
        PropertyMapEntry* a[tinyMapThreshold];
        int i = 0;
        unsigned entryCount = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount;
        for (unsigned k = 1; k <= entryCount; k++) {
            if (m_propertyTable->entries()[k].key && !(m_propertyTable->entries()[k].attributes & DontEnum)) {
                PropertyMapEntry* value = &m_propertyTable->entries()[k];
                int j;
                for (j = i - 1; j >= 0 && a[j]->index > value->index; --j)
                    a[j + 1] = a[j];
                a[j + 1] = value;
                ++i;
            }
        }
        if (!propertyNames.size()) {
            for (int k = 0; k < i; ++k)
                propertyNames.addKnownUnique(a[k]->key);
        } else {
            for (int k = 0; k < i; ++k)
                propertyNames.add(a[k]->key);
        }

        return;
    }

    // Allocate a buffer to use to sort the keys.
    Vector<PropertyMapEntry*, smallMapThreshold> sortedEnumerables(m_propertyTable->keyCount);

    // Get pointers to the enumerable entries in the buffer.
    PropertyMapEntry** p = sortedEnumerables.data();
    unsigned entryCount = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        if (m_propertyTable->entries()[i].key && !(m_propertyTable->entries()[i].attributes & DontEnum))
            *p++ = &m_propertyTable->entries()[i];
    }

    size_t enumerableCount = p - sortedEnumerables.data();
    // Sort the entries by index.
    qsort(sortedEnumerables.data(), enumerableCount, sizeof(PropertyMapEntry*), comparePropertyMapEntryIndices);
    sortedEnumerables.resize(enumerableCount);

    // Put the keys of the sorted entries into the list.
    if (!propertyNames.size()) {
        for (size_t i = 0; i < sortedEnumerables.size(); ++i)
            propertyNames.addKnownUnique(sortedEnumerables[i]->key);
    } else {
        for (size_t i = 0; i < sortedEnumerables.size(); ++i)
            propertyNames.add(sortedEnumerables[i]->key);
    }
}

void Structure::getEnumerableNamesFromClassInfoTable(ExecState* exec, const ClassInfo* classInfo, PropertyNameArray& propertyNames)
{
    // Add properties from the static hashtables of properties
    for (; classInfo; classInfo = classInfo->parentClass) {
        const HashTable* table = classInfo->propHashTable(exec);
        if (!table)
            continue;
        table->initializeIfNeeded(exec);
        ASSERT(table->table);

        int hashSizeMask = table->compactSize - 1;
        const HashEntry* entry = table->table;
        for (int i = 0; i <= hashSizeMask; ++i, ++entry) {
            if (entry->key() && !(entry->attributes() & DontEnum))
                propertyNames.add(entry->key());
        }
    }
}

#if DO_PROPERTYMAP_CONSTENCY_CHECK

void Structure::checkConsistency()
{
    if (!m_propertyTable)
        return;

    ASSERT(m_propertyTable->size >= newTableSize);
    ASSERT(m_propertyTable->sizeMask);
    ASSERT(m_propertyTable->size == m_propertyTable->sizeMask + 1);
    ASSERT(!(m_propertyTable->size & m_propertyTable->sizeMask));

    ASSERT(m_propertyTable->keyCount <= m_propertyTable->size / 2);
    ASSERT(m_propertyTable->deletedSentinelCount <= m_propertyTable->size / 4);

    ASSERT(m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount <= m_propertyTable->size / 2);

    unsigned indexCount = 0;
    unsigned deletedIndexCount = 0;
    for (unsigned a = 0; a != m_propertyTable->size; ++a) {
        unsigned entryIndex = m_propertyTable->entryIndices[a];
        if (entryIndex == emptyEntryIndex)
            continue;
        if (entryIndex == deletedSentinelIndex) {
            ++deletedIndexCount;
            continue;
        }
        ASSERT(entryIndex > deletedSentinelIndex);
        ASSERT(entryIndex - 1 <= m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount);
        ++indexCount;

        for (unsigned b = a + 1; b != m_propertyTable->size; ++b)
            ASSERT(m_propertyTable->entryIndices[b] != entryIndex);
    }
    ASSERT(indexCount == m_propertyTable->keyCount);
    ASSERT(deletedIndexCount == m_propertyTable->deletedSentinelCount);

    ASSERT(m_propertyTable->entries()[0].key == 0);

    unsigned nonEmptyEntryCount = 0;
    for (unsigned c = 1; c <= m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount; ++c) {
        UString::Rep* rep = m_propertyTable->entries()[c].key;
        if (!rep)
            continue;
        ++nonEmptyEntryCount;
        unsigned i = rep->computedHash();
        unsigned k = 0;
        unsigned entryIndex;
        while (1) {
            entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
            ASSERT(entryIndex != emptyEntryIndex);
            if (rep == m_propertyTable->entries()[entryIndex - 1].key)
                break;
            if (k == 0)
                k = 1 | doubleHash(rep->computedHash());
            i += k;
        }
        ASSERT(entryIndex == c + 1);
    }

    ASSERT(nonEmptyEntryCount == m_propertyTable->keyCount);
}

#endif // DO_PROPERTYMAP_CONSTENCY_CHECK

} // namespace JSC
