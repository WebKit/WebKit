/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Alexey Shvayka <shvaikalesh@gmail.com>.
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
#include "Structure.h"

#include "BrandedStructure.h"
#include "BuiltinNames.h"
#include "DumpContext.h"
#include "JSCInlines.h"
#include "PropertyNameArray.h"
#include "PropertyTable.h"
#include <wtf/CommaPrinter.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefPtr.h>

#define DUMP_STRUCTURE_ID_STATISTICS 0

namespace JSC {

#if DUMP_STRUCTURE_ID_STATISTICS
static HashSet<Structure*>& liveStructureSet = *(new HashSet<Structure*>);
#endif

inline void StructureTransitionTable::setSingleTransition(VM& vm, JSCell* owner, Structure* structure)
{
    ASSERT(isUsingSingleSlot());
    m_data = bitwise_cast<intptr_t>(structure) | UsingSingleSlotFlag;
    vm.writeBarrier(owner, structure);
}

bool StructureTransitionTable::contains(UniquedStringImpl* rep, unsigned attributes, TransitionKind transitionKind) const
{
    if (isUsingSingleSlot()) {
        Structure* transition = trySingleTransition();
        return transition && transition->m_transitionPropertyName == rep && transition->transitionPropertyAttributes() == attributes && transition->transitionKind() == transitionKind;
    }
    return map()->get(StructureTransitionTable::Hash::Key(rep, attributes, transitionKind));
}

void StructureTransitionTable::add(VM& vm, JSCell* owner, Structure* structure)
{
    if (isUsingSingleSlot()) {
        Structure* existingTransition = trySingleTransition();

        // This handles the first transition being added.
        if (!existingTransition) {
            setSingleTransition(vm, owner, structure);
            return;
        }

        // This handles the second transition being added
        // (or the first transition being despecified!)
        setMap(new TransitionMap(vm));
        add(vm, owner, existingTransition);
    }

    // Add the structure to the map.
    map()->set(StructureTransitionTable::Hash::Key(structure->m_transitionPropertyName.get(), structure->transitionPropertyAttributes(), structure->transitionKind()), structure);
}

void Structure::dumpStatistics()
{
#if DUMP_STRUCTURE_ID_STATISTICS
    unsigned numberLeaf = 0;
    unsigned numberUsingSingleSlot = 0;
    unsigned numberSingletons = 0;
    unsigned numberWithPropertyTables = 0;
    unsigned totalPropertyTablesSize = 0;

    HashSet<Structure*>::const_iterator end = liveStructureSet.end();
    for (HashSet<Structure*>::const_iterator it = liveStructureSet.begin(); it != end; ++it) {
        Structure* structure = *it;

        switch (structure->m_transitionTable.size()) {
            case 0:
                ++numberLeaf;
                if (!structure->previousID())
                    ++numberSingletons;
                break;

            case 1:
                ++numberUsingSingleSlot;
                break;
        }

        if (PropertyTable* table = structure->propertyTableOrNull()) {
            ++numberWithPropertyTables;
            totalPropertyTablesSize += table->sizeInMemory();
        }
    }

    dataLogF("Number of live Structures: %d\n", liveStructureSet.size());
    dataLogF("Number of Structures using the single item optimization for transition map: %d\n", numberUsingSingleSlot);
    dataLogF("Number of Structures that are leaf nodes: %d\n", numberLeaf);
    dataLogF("Number of Structures that singletons: %d\n", numberSingletons);
    dataLogF("Number of Structures with PropertyTables: %d\n", numberWithPropertyTables);

    dataLogF("Size of a single Structures: %d\n", static_cast<unsigned>(sizeof(Structure)));
    dataLogF("Size of sum of all property maps: %d\n", totalPropertyTablesSize);
    dataLogF("Size of average of all property maps: %f\n", static_cast<double>(totalPropertyTablesSize) / static_cast<double>(liveStructureSet.size()));
#else
    dataLogF("Dumping Structure statistics is not enabled.\n");
#endif
}

#if ASSERT_ENABLED
void Structure::validateFlags()
{
    const MethodTable& methodTable = m_classInfo->methodTable;

    bool overridesGetCallData = methodTable.getCallData != JSCell::getCallData;
    RELEASE_ASSERT(overridesGetCallData == typeInfo().overridesGetCallData());

    bool overridesGetOwnPropertySlot =
        methodTable.getOwnPropertySlot != JSObject::getOwnPropertySlot
        && methodTable.getOwnPropertySlot != JSCell::getOwnPropertySlot;
    // We can strengthen this into an equivalence test if there are no classes
    // that specifies this flag without overriding getOwnPropertySlot.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=212956
    if (overridesGetOwnPropertySlot)
        RELEASE_ASSERT(typeInfo().overridesGetOwnPropertySlot());

    bool overridesGetOwnPropertySlotByIndex =
        methodTable.getOwnPropertySlotByIndex != JSObject::getOwnPropertySlotByIndex
        && methodTable.getOwnPropertySlotByIndex != JSCell::getOwnPropertySlotByIndex;
    // We can strengthen this into an equivalence test if there are no classes
    // that specifies this flag without overriding getOwnPropertySlotByIndex.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=212958
    if (overridesGetOwnPropertySlotByIndex)
        RELEASE_ASSERT(typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero());

    bool overridesGetOwnPropertyNames =
        methodTable.getOwnPropertyNames != JSObject::getOwnPropertyNames
        && methodTable.getOwnPropertyNames != JSCell::getOwnPropertyNames;
    RELEASE_ASSERT(overridesGetOwnPropertyNames == typeInfo().overridesGetOwnPropertyNames());

    bool overridesGetOwnSpecialPropertyNames =
        methodTable.getOwnSpecialPropertyNames != JSObject::getOwnSpecialPropertyNames
        && methodTable.getOwnSpecialPropertyNames != JSCell::getOwnSpecialPropertyNames;
    RELEASE_ASSERT(overridesGetOwnSpecialPropertyNames == typeInfo().overridesGetOwnSpecialPropertyNames());

    bool overridesGetPrototype =
        methodTable.getPrototype != static_cast<MethodTable::GetPrototypeFunctionPtr>(JSObject::getPrototype)
        && methodTable.getPrototype != JSCell::getPrototype;
    RELEASE_ASSERT(overridesGetPrototype == typeInfo().overridesGetPrototype());

    bool overridesPut = methodTable.put != JSObject::put && ((typeInfo().type() == StringType || typeInfo().type() == SymbolType || typeInfo().type() == HeapBigIntType) || methodTable.put != JSCell::put);
    RELEASE_ASSERT(overridesPut == typeInfo().overridesPut());

    bool overridesIsExtensible =
        methodTable.isExtensible != static_cast<MethodTable::IsExtensibleFunctionPtr>(JSObject::isExtensible)
        && methodTable.isExtensible != JSCell::isExtensible;
    RELEASE_ASSERT(overridesIsExtensible == typeInfo().overridesIsExtensible());
}
#else
inline void Structure::validateFlags() { }
#endif

Structure::Structure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, unsigned inlineCapacity)
    : JSCell(vm, vm.structureStructure.get())
    , m_blob(indexingType, typeInfo)
    , m_outOfLineTypeFlags(typeInfo.outOfLineTypeFlags())
    , m_inlineCapacity(inlineCapacity)
    , m_bitField(0)
    , m_propertyHash(0)
    , m_globalObject(globalObject, WriteBarrierEarlyInit)
    , m_prototype(prototype, WriteBarrierEarlyInit)
    , m_classInfo(classInfo)
    , m_transitionWatchpointSet(IsWatched)
{
    bool hasStaticNonConfigurableProperty = m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::DontDelete));

    setDictionaryKind(NoneDictionaryKind);
    setIsPinnedPropertyTable(false);
    setHasAnyKindOfGetterSetterProperties(classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::AccessorOrCustomAccessorOrValue)));
    setHasReadOnlyOrGetterSetterPropertiesExcludingProto(hasAnyKindOfGetterSetterProperties() || classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::ReadOnly)));
    setHasNonConfigurableProperties(hasStaticNonConfigurableProperty || typeInfo.overridesGetOwnPropertySlot());
    setHasNonConfigurableReadOnlyOrGetterSetterProperties(hasStaticNonConfigurableProperty || (typeInfo.overridesGetOwnPropertySlot() && typeInfo.type() != ArrayType));
    setHasUnderscoreProtoPropertyExcludingOriginalProto(false);
    setIsQuickPropertyAccessAllowedForEnumeration(true);
    setTransitionPropertyAttributes(0);
    setTransitionKind(TransitionKind::Unknown);
    setMayBePrototype(false);
    setDidPreventExtensions(typeInfo.overridesIsExtensible());
    setDidTransition(false);
    setStaticPropertiesReified(false);
    setTransitionWatchpointIsLikelyToBeFired(false);
    setHasBeenDictionary(false);
    setProtectPropertyTableWhileTransitioning(false);
    setTransitionOffset(vm, invalidOffset);
    setMaxOffset(vm, invalidOffset);
 
    ASSERT(inlineCapacity <= JSFinalObject::maxInlineCapacity);
    ASSERT(static_cast<PropertyOffset>(inlineCapacity) < firstOutOfLineOffset);
    ASSERT(!hasRareData());
    ASSERT(hasAnyKindOfGetterSetterProperties() == m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::AccessorOrCustomAccessorOrValue)));
    ASSERT(hasReadOnlyOrGetterSetterPropertiesExcludingProto() == m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessorOrValue)));

    validateFlags();

#if ENABLE(STRUCTURE_ID_WITH_SHIFT)
    ASSERT(WTF::roundUpToMultipleOf<Structure::atomSize>(this) == this);
#endif
}

const ClassInfo Structure::s_info = { "Structure"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(Structure) };

Structure::Structure(VM& vm, CreatingEarlyCellTag)
    : JSCell(CreatingEarlyCell)
    , m_inlineCapacity(0)
    , m_bitField(0)
    , m_propertyHash(0)
    , m_prototype(jsNull(), WriteBarrierEarlyInit)
    , m_classInfo(info())
    , m_transitionWatchpointSet(IsWatched)
{
    TypeInfo typeInfo { StructureType, StructureFlags };
    bool hasStaticNonConfigurableProperty = m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::DontDelete));

    setDictionaryKind(NoneDictionaryKind);
    setIsPinnedPropertyTable(false);
    setHasAnyKindOfGetterSetterProperties(m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::AccessorOrCustomAccessorOrValue)));
    setHasReadOnlyOrGetterSetterPropertiesExcludingProto(hasAnyKindOfGetterSetterProperties() || m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::ReadOnly)));
    setHasNonConfigurableProperties(hasStaticNonConfigurableProperty || typeInfo.overridesGetOwnPropertySlot());
    setHasNonConfigurableReadOnlyOrGetterSetterProperties(hasStaticNonConfigurableProperty || (typeInfo.overridesGetOwnPropertySlot() && typeInfo.type() != ArrayType));
    setHasUnderscoreProtoPropertyExcludingOriginalProto(false);
    setIsQuickPropertyAccessAllowedForEnumeration(true);
    setTransitionPropertyAttributes(0);
    setTransitionKind(TransitionKind::Unknown);
    setMayBePrototype(false);
    setDidPreventExtensions(typeInfo.overridesIsExtensible());
    setDidTransition(false);
    setStaticPropertiesReified(false);
    setTransitionWatchpointIsLikelyToBeFired(false);
    setHasBeenDictionary(false);
    setProtectPropertyTableWhileTransitioning(false);
    setTransitionOffset(vm, invalidOffset);
    setMaxOffset(vm, invalidOffset);
 
    m_blob = TypeInfoBlob(0, typeInfo);
    m_outOfLineTypeFlags = typeInfo.outOfLineTypeFlags();

    ASSERT(hasAnyKindOfGetterSetterProperties() == m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::AccessorOrCustomAccessorOrValue)));
    ASSERT(hasReadOnlyOrGetterSetterPropertiesExcludingProto() == m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessorOrValue)));
    ASSERT(!this->typeInfo().overridesGetCallData() || m_classInfo->methodTable.getCallData != &JSCell::getCallData);

#if ENABLE(STRUCTURE_ID_WITH_SHIFT)
    ASSERT(WTF::roundUpToMultipleOf<Structure::atomSize>(this) == this);
#endif
}

Structure::Structure(VM& vm, Structure* previous)
    : JSCell(vm, vm.structureStructure.get())
    , m_inlineCapacity(previous->m_inlineCapacity)
    , m_bitField(0)
    , m_propertyHash(previous->m_propertyHash)
    , m_seenProperties(previous->m_seenProperties)
    , m_prototype(previous->m_prototype.get(), WriteBarrierEarlyInit)
    , m_classInfo(previous->m_classInfo)
    , m_transitionWatchpointSet(IsWatched)
{
    setDictionaryKind(previous->dictionaryKind());
    setIsPinnedPropertyTable(false);
    setHasBeenFlattenedBefore(previous->hasBeenFlattenedBefore());
    setHasAnyKindOfGetterSetterProperties(previous->hasAnyKindOfGetterSetterProperties());
    setHasReadOnlyOrGetterSetterPropertiesExcludingProto(previous->hasReadOnlyOrGetterSetterPropertiesExcludingProto());
    setHasNonConfigurableProperties(previous->hasNonConfigurableProperties());
    setHasNonConfigurableReadOnlyOrGetterSetterProperties(previous->hasNonConfigurableReadOnlyOrGetterSetterProperties());
    setHasUnderscoreProtoPropertyExcludingOriginalProto(previous->hasUnderscoreProtoPropertyExcludingOriginalProto());
    setIsQuickPropertyAccessAllowedForEnumeration(previous->isQuickPropertyAccessAllowedForEnumeration());
    setTransitionPropertyAttributes(0);
    setTransitionKind(TransitionKind::Unknown);
    setMayBePrototype(previous->mayBePrototype());
    setDidPreventExtensions(previous->didPreventExtensions());
    setDidTransition(true);
    setStaticPropertiesReified(previous->staticPropertiesReified());
    setHasBeenDictionary(previous->hasBeenDictionary());
    setProtectPropertyTableWhileTransitioning(false);
    setTransitionOffset(vm, invalidOffset);
    setMaxOffset(vm, invalidOffset);
 
    TypeInfo typeInfo = previous->typeInfo();
    m_blob = TypeInfoBlob(previous->indexingModeIncludingHistory(), typeInfo);
    m_outOfLineTypeFlags = typeInfo.outOfLineTypeFlags();

    ASSERT(!previous->typeInfo().structureIsImmortal());
    setPreviousID(vm, previous);

    // Do not fire watchpoint inside Structure constructor since watchpoint can involve further heap allocations.
    // We fire watchpoint separately in Structure::finishCreation.
    previous->didTransitionFromThisStructureWithoutFiringWatchpoint();
    
    // Copy this bit now, in case previous was being watched.
    setTransitionWatchpointIsLikelyToBeFired(previous->transitionWatchpointIsLikelyToBeFired());

    if (previous->m_globalObject)
        m_globalObject.set(vm, this, previous->m_globalObject.get());
    ASSERT(hasAnyKindOfGetterSetterProperties() || !m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::AccessorOrCustomAccessorOrValue)));
    ASSERT(hasReadOnlyOrGetterSetterPropertiesExcludingProto() || !m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessorOrValue)));
    ASSERT(!this->typeInfo().overridesGetCallData() || m_classInfo->methodTable.getCallData != &JSCell::getCallData);

#if ENABLE(STRUCTURE_ID_WITH_SHIFT)
    ASSERT(WTF::roundUpToMultipleOf<Structure::atomSize>(this) == this);
#endif
}

Structure::~Structure()
{
    if (typeInfo().structureIsImmortal())
        return;

    if (isBrandedStructure())
        static_cast<BrandedStructure*>(this)->destruct();
}

void Structure::destroy(JSCell* cell)
{
    static_cast<Structure*>(cell)->Structure::~Structure();
}

Structure* Structure::create(PolyProtoTag, VM& vm, JSGlobalObject* globalObject, JSObject* prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, unsigned inlineCapacity)
{
    Structure* result = Structure::create(vm, globalObject, prototype, typeInfo, classInfo, indexingType, inlineCapacity);

    unsigned oldOutOfLineCapacity = result->outOfLineCapacity();
    result->addPropertyWithoutTransition(
        vm, vm.propertyNames->builtinNames().polyProtoName(), static_cast<unsigned>(PropertyAttribute::DontEnum),
        [&] (const GCSafeConcurrentJSLocker&, PropertyOffset offset, PropertyOffset newMaxOffset) {
            RELEASE_ASSERT(Structure::outOfLineCapacity(newMaxOffset) == oldOutOfLineCapacity);
            RELEASE_ASSERT(offset == knownPolyProtoOffset);
            RELEASE_ASSERT(isInlineOffset(knownPolyProtoOffset));
            result->m_prototype.setWithoutWriteBarrier(JSValue());
            result->setMaxOffset(vm, newMaxOffset);
        });

    ASSERT(result->type() == StructureType);
    return result;
}

bool Structure::isValidPrototype(JSValue prototype)
{
    return prototype.isNull() || (prototype.isObject() && prototype.getObject()->mayBePrototype());
}

bool Structure::findStructuresAndMapForMaterialization(Vector<Structure*, 8>& structures, Structure*& structure, PropertyTable*& table)
{
    ASSERT(structures.isEmpty());
    table = nullptr;

    for (structure = this; structure; structure = structure->previousID()) {
        structure->m_lock.lock();
        
        table = structure->propertyTableOrNull();
        if (table) {
            // Leave the structure locked, so that the caller can do things to it atomically
            // before it loses its property table.
            return true;
        }
        
        structures.append(structure);
        structure->m_lock.unlock();
    }
    
    ASSERT(!structure);
    ASSERT(!table);
    return false;
}

PropertyTable* Structure::materializePropertyTable(VM& vm, bool setPropertyTable)
{
    ASSERT(!isCompilationThread());
    ASSERT(structure()->classInfoForCells() == info());
    ASSERT(!protectPropertyTableWhileTransitioning());
    
    DeferGC deferGC(vm);
    
    Vector<Structure*, 8> structures;
    Structure* structure;
    PropertyTable* table;
    
    bool didFindStructure = findStructuresAndMapForMaterialization(structures, structure, table);
    
    unsigned capacity = numberOfSlotsForMaxOffset(maxOffset(), m_inlineCapacity);
    if (didFindStructure) {
        table = table->copy(vm, capacity);
        structure->m_lock.unlock();
    } else
        table = PropertyTable::create(vm, capacity);
    
    // Must hold the lock on this structure, since we will be modifying this structure's
    // property map. We don't want getConcurrently() to see the property map in a half-baked
    // state.
    GCSafeConcurrentJSLocker locker(m_lock, vm);
    if (setPropertyTable)
        this->setPropertyTable(vm, table);

    for (size_t i = structures.size(); i--;) {
        structure = structures[i];
        if (!structure->m_transitionPropertyName || structure->transitionKind() == TransitionKind::SetBrand)
            continue;
        switch (structure->transitionKind()) {
        case TransitionKind::PropertyAddition: {
            PropertyTableEntry entry(structure->m_transitionPropertyName.get(), structure->transitionOffset(), structure->transitionPropertyAttributes());
            auto nextOffset = table->nextOffset(structure->inlineCapacity());
            ASSERT_UNUSED(nextOffset, nextOffset == structure->transitionOffset());
            auto [offset, attribute, result] = table->add(vm, entry);
            ASSERT_UNUSED(result, result);
            ASSERT_UNUSED(offset, offset == nextOffset);
            UNUSED_VARIABLE(attribute);
            break;
        }
        case TransitionKind::PropertyDeletion: {
            auto [offset, attributes] = table->take(vm, structure->m_transitionPropertyName.get());
            ASSERT_UNUSED(offset, offset != invalidOffset);
            UNUSED_VARIABLE(attributes);
            table->addDeletedOffset(structure->transitionOffset());
            break;
        }
        case TransitionKind::PropertyAttributeChange: {
            PropertyOffset offset = table->updateAttributeIfExists(structure->m_transitionPropertyName.get(), structure->transitionPropertyAttributes());
            ASSERT_UNUSED(offset, offset == structure->transitionOffset());
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    
    checkOffsetConsistency(
        table,
        [&] () {
            dataLog("Detected in materializePropertyTable.\n");
            dataLog("Found structure = ", RawPointer(structure), "\n");
            dataLog("structures = ");
            CommaPrinter comma;
            for (Structure* structure : structures)
                dataLog(comma, RawPointer(structure));
            dataLog("\n");
        });
    
    return table;
}

bool Structure::holesMustForwardToPrototypeSlow(JSObject* base) const
{
    ASSERT(base->structure() == this);

    if (this->mayInterceptIndexedAccesses())
        return true;

    JSValue prototype = this->storedPrototype(base);
    if (!prototype.isObject())
        return false;
    JSObject* object = asObject(prototype);

    while (true) {
        Structure& structure = *object->structure();
        if (hasIndexedProperties(object->indexingType()) || structure.mayInterceptIndexedAccesses())
            return true;
        prototype = structure.storedPrototype(object);
        if (!prototype.isObject())
            return false;
        object = asObject(prototype);
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

Structure* Structure::addPropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    Structure* newStructure = addPropertyTransitionToExistingStructure(structure, propertyName, attributes, offset);
    if (newStructure)
        return newStructure;

    return addNewPropertyTransition(vm, structure, propertyName, attributes, offset, PutPropertySlot::UnknownContext);
}

Structure* Structure::addNewPropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset, PutPropertySlot::Context context, DeferredStructureTransitionWatchpointFire* deferred)
{
    ASSERT(!structure->isDictionary());
    ASSERT(structure->isObject());
    ASSERT(!Structure::addPropertyTransitionToExistingStructure(structure, propertyName, attributes, offset));
    
    if (structure->shouldDoCacheableDictionaryTransitionForAdd(context)) {
        ASSERT(!isCopyOnWrite(structure->indexingMode()));
        Structure* transition = toCacheableDictionaryTransition(vm, structure, deferred);
        ASSERT(structure != transition);
        offset = transition->add(vm, propertyName, attributes);
        return transition;
    }
    
    Structure* transition = Structure::create(vm, structure, deferred);

    transition->m_cachedPrototypeChain.setMayBeNull(vm, transition, structure->m_cachedPrototypeChain.get());
    
    // While we are adding the property, rematerializing the property table is super weird: we already
    // have a m_transitionPropertyName and transitionPropertyAttributes but the m_transitionOffset is still wrong. If the
    // materialization algorithm runs, it'll build a property table that already has the property but
    // at a bogus offset. Rather than try to teach the materialization code how to create a table under
    // those conditions, we just tell the GC not to blow the table away during this period of time.
    // Holding the lock ensures that we either do this before the GC starts scanning the structure, in
    // which case the GC will not blow the table away, or we do it after the GC already ran in which
    // case all is well.  If it wasn't for the lock, the GC would have TOCTOU: if could read
    // protectPropertyTableWhileTransitioning before we set it to true, and then blow the table away after.
    {
        ConcurrentJSLocker locker(transition->m_lock);
        transition->setProtectPropertyTableWhileTransitioning(true);
    }

    transition->m_blob.setIndexingModeIncludingHistory(structure->indexingModeIncludingHistory() & ~CopyOnWrite);
    transition->m_transitionPropertyName = propertyName.uid();
    transition->setTransitionPropertyAttributes(attributes);
    transition->setTransitionKind(TransitionKind::PropertyAddition);
    transition->setPropertyTable(vm, structure->takePropertyTableOrCloneIfPinned(vm));
    transition->setMaxOffset(vm, structure->maxOffset());

    offset = transition->add(vm, propertyName, attributes);
    transition->setTransitionOffset(vm, offset);

    // Now that everything is fine with the new structure's bookkeeping, the GC is free to blow the
    // table away if it wants. We can now rebuild it fine.
    WTF::storeStoreFence();
    transition->setProtectPropertyTableWhileTransitioning(false);

    checkOffset(transition->transitionOffset(), transition->inlineCapacity());
    if (!structure->hasBeenDictionary()) {
        GCSafeConcurrentJSLocker locker(structure->m_lock, vm);
        structure->m_transitionTable.add(vm, structure, transition);
    }
    transition->checkOffsetConsistency();
    structure->checkOffsetConsistency();
    return transition;
}

Structure* Structure::removePropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, PropertyOffset& offset, DeferredStructureTransitionWatchpointFire* deferred)
{
    Structure* newStructure = removePropertyTransitionFromExistingStructure(structure, propertyName, offset);
    if (newStructure)
        return newStructure;

    return removeNewPropertyTransition(
        vm, structure, propertyName, offset, deferred);
}

Structure* Structure::removePropertyTransitionFromExistingStructureImpl(Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    ASSERT(!structure->isUncacheableDictionary());
    ASSERT(structure->isObject());

    offset = invalidOffset;

    if (structure->hasBeenDictionary())
        return nullptr;

    if (Structure* existingTransition = structure->m_transitionTable.get(propertyName.uid(), attributes, TransitionKind::PropertyDeletion)) {
        validateOffset(existingTransition->transitionOffset(), existingTransition->inlineCapacity());
        offset = existingTransition->transitionOffset();
        return existingTransition;
    }

    return nullptr;
}

Structure* Structure::removePropertyTransitionFromExistingStructure(Structure* structure, PropertyName propertyName, PropertyOffset& offset)
{
    ASSERT(!isCompilationThread());
    unsigned attributes = 0;
    if (structure->getConcurrently(propertyName.uid(), attributes) == invalidOffset)
        return nullptr;
    return removePropertyTransitionFromExistingStructureImpl(structure, propertyName, attributes, offset);
}

Structure* Structure::removePropertyTransitionFromExistingStructureConcurrently(Structure* structure, PropertyName propertyName, PropertyOffset& offset)
{
    unsigned attributes = 0;
    if (structure->getConcurrently(propertyName.uid(), attributes) == invalidOffset)
        return nullptr;
    ConcurrentJSLocker locker(structure->m_lock);
    return removePropertyTransitionFromExistingStructureImpl(structure, propertyName, attributes, offset);
}

Structure* Structure::removeNewPropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, PropertyOffset& offset, DeferredStructureTransitionWatchpointFire* deferred)
{
    ASSERT(!isCompilationThread());
    ASSERT(!structure->isUncacheableDictionary());
    ASSERT(structure->isObject());
    ASSERT(!Structure::removePropertyTransitionFromExistingStructure(structure, propertyName, offset));
    ASSERT(structure->getConcurrently(propertyName.uid()) != invalidOffset);

    if (structure->transitionCountHasOverflowed()) {
        ASSERT(!isCopyOnWrite(structure->indexingMode()));
        Structure* transition = toUncacheableDictionaryTransition(vm, structure, deferred);
        ASSERT(structure != transition);
        offset = transition->remove(vm, propertyName);
        return transition;
    }

    Structure* transition = Structure::create(vm, structure, deferred);
    transition->m_cachedPrototypeChain.setMayBeNull(vm, transition, structure->m_cachedPrototypeChain.get());

    // While we are deleting the property, we need to make sure the table is not cleared.
    {
        ConcurrentJSLocker locker(transition->m_lock);
        transition->setProtectPropertyTableWhileTransitioning(true);
    }

    transition->m_blob.setIndexingModeIncludingHistory(structure->indexingModeIncludingHistory() & ~CopyOnWrite);
    transition->m_transitionPropertyName = propertyName.uid();
    transition->setTransitionKind(TransitionKind::PropertyDeletion);
    transition->setPropertyTable(vm, structure->takePropertyTableOrCloneIfPinned(vm));
    transition->setMaxOffset(vm, structure->maxOffset());

    offset = transition->remove(vm, propertyName);
    ASSERT(offset != invalidOffset);
    transition->setTransitionOffset(vm, offset);

    // Now that everything is fine with the new structure's bookkeeping, the GC is free to blow the
    // table away if it wants. We can now rebuild it fine.
    WTF::storeStoreFence();
    transition->setProtectPropertyTableWhileTransitioning(false);

    checkOffset(transition->transitionOffset(), transition->inlineCapacity());
    if (!structure->hasBeenDictionary()) {
        GCSafeConcurrentJSLocker locker(structure->m_lock, vm);
        structure->m_transitionTable.add(vm, structure, transition);
    }
    transition->checkOffsetConsistency();
    structure->checkOffsetConsistency();
    return transition;
}

Structure* Structure::changePrototypeTransition(VM& vm, Structure* structure, JSValue prototype, DeferredStructureTransitionWatchpointFire& deferred)
{
    ASSERT(isValidPrototype(prototype));

    DeferGC deferGC(vm);
    Structure* transition = Structure::create(vm, structure, &deferred);

    transition->m_prototype.set(vm, transition, prototype);

    PropertyTable* table = structure->copyPropertyTableForPinning(vm);
    transition->pin(Locker { transition->m_lock }, vm, table);
    transition->setMaxOffset(vm, structure->maxOffset());
    
    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::attributeChangeTransitionToExistingStructure(Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    ASSERT(structure->isObject());

    offset = invalidOffset;

    if (structure->hasBeenDictionary())
        return nullptr;

    if (Structure* existingTransition = structure->m_transitionTable.get(propertyName.uid(), attributes, TransitionKind::PropertyAttributeChange)) {
        validateOffset(existingTransition->transitionOffset(), existingTransition->inlineCapacity());
        offset = existingTransition->transitionOffset();
        return existingTransition;
    }

    return nullptr;
}

Structure* Structure::attributeChangeTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes, DeferredStructureTransitionWatchpointFire* deferred)
{
    if (structure->isUncacheableDictionary()) {
        structure->attributeChangeWithoutTransition(vm, propertyName, attributes, [](const GCSafeConcurrentJSLocker&, PropertyOffset, PropertyOffset) { });
        structure->checkOffsetConsistency();
        return structure;
    }

    ASSERT(!structure->isUncacheableDictionary());
    PropertyOffset offset = invalidOffset;
    if (Structure* existingTransition = attributeChangeTransitionToExistingStructure(structure, propertyName, attributes, offset)) {
        validateOffset(existingTransition->transitionOffset(), existingTransition->inlineCapacity());
        existingTransition->checkOffsetConsistency();
        return existingTransition;
    }

    if (structure->transitionCountHasOverflowed()) {
        ASSERT(!isCopyOnWrite(structure->indexingMode()));
        Structure* transition = toUncacheableDictionaryTransition(vm, structure, deferred);
        ASSERT(structure != transition);
        transition->attributeChange(vm, propertyName, attributes);
        return transition;
    }

    // Even if the current structure is dictionary, we should perform transition since this changes attributes of existing properties to keep
    // structure still cacheable.
    Structure* transition = Structure::create(vm, structure, deferred);
    transition->m_cachedPrototypeChain.setMayBeNull(vm, transition, structure->m_cachedPrototypeChain.get());

    {
        ConcurrentJSLocker locker(transition->m_lock);
        transition->setProtectPropertyTableWhileTransitioning(true);
    }

    transition->m_blob.setIndexingModeIncludingHistory(structure->indexingModeIncludingHistory() & ~CopyOnWrite);
    transition->m_transitionPropertyName = propertyName.uid();
    transition->setTransitionPropertyAttributes(attributes);
    transition->setTransitionKind(TransitionKind::PropertyAttributeChange);
    transition->setPropertyTable(vm, structure->takePropertyTableOrCloneIfPinned(vm));
    transition->setMaxOffset(vm, structure->maxOffset());

    offset = transition->attributeChange(vm, propertyName, attributes);
    transition->setTransitionOffset(vm, offset);

    // Now that everything is fine with the new structure's bookkeeping, the GC is free to blow the
    // table away if it wants. We can now rebuild it fine.
    WTF::storeStoreFence();
    transition->setProtectPropertyTableWhileTransitioning(false);

    checkOffset(transition->transitionOffset(), transition->inlineCapacity());
    if (!structure->hasBeenDictionary()) {
        GCSafeConcurrentJSLocker locker(structure->m_lock, vm);
        structure->m_transitionTable.add(vm, structure, transition);
    }
    transition->checkOffsetConsistency();
    structure->checkOffsetConsistency();
    return transition;
}

Structure* Structure::toDictionaryTransition(VM& vm, Structure* structure, DictionaryKind kind, DeferredStructureTransitionWatchpointFire* deferred)
{
    ASSERT(!structure->isUncacheableDictionary());
    DeferGC deferGC(vm);
    
    Structure* transition = Structure::create(vm, structure, deferred);

    PropertyTable* table = structure->copyPropertyTableForPinning(vm);
    transition->pin(Locker { transition->m_lock }, vm, table);
    transition->setMaxOffset(vm, structure->maxOffset());
    transition->setDictionaryKind(kind);
    transition->setHasBeenDictionary(true);
    
    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::toCacheableDictionaryTransition(VM& vm, Structure* structure, DeferredStructureTransitionWatchpointFire* deferred)
{
    return toDictionaryTransition(vm, structure, CachedDictionaryKind, deferred);
}

Structure* Structure::toUncacheableDictionaryTransition(VM& vm, Structure* structure, DeferredStructureTransitionWatchpointFire* deferred)
{
    return toDictionaryTransition(vm, structure, UncachedDictionaryKind, deferred);
}

Structure* Structure::sealTransition(VM& vm, Structure* structure, DeferredStructureTransitionWatchpointFire* deferred)
{
    return nonPropertyTransition(vm, structure, TransitionKind::Seal, deferred);
}

Structure* Structure::freezeTransition(VM& vm, Structure* structure, DeferredStructureTransitionWatchpointFire* deferred)
{
    return nonPropertyTransition(vm, structure, TransitionKind::Freeze, deferred);
}

Structure* Structure::preventExtensionsTransition(VM& vm, Structure* structure, DeferredStructureTransitionWatchpointFire* deferred)
{
    return nonPropertyTransition(vm, structure, TransitionKind::PreventExtensions, deferred);
}

Structure* Structure::becomePrototypeTransition(VM& vm, Structure* structure, DeferredStructureTransitionWatchpointFire* deferred)
{
    return nonPropertyTransition(vm, structure, TransitionKind::BecomePrototype, deferred);
}

PropertyTable* Structure::takePropertyTableOrCloneIfPinned(VM& vm)
{
    // This must always return a property table. It can't return null.
    PropertyTable* result = propertyTableOrNull();
    if (result) {
        if (isPinnedPropertyTable())
            return result->copy(vm, result->size() + 1);
        ConcurrentJSLocker locker(m_lock);
        setPropertyTable(vm, nullptr);
        return result;
    }
    bool setPropertyTable = false;
    return materializePropertyTable(vm, setPropertyTable);
}

Structure* Structure::nonPropertyTransitionSlow(VM& vm, Structure* structure, TransitionKind transitionKind, DeferredStructureTransitionWatchpointFire* deferred)
{
    IndexingType indexingModeIncludingHistory = newIndexingType(structure->indexingModeIncludingHistory(), transitionKind);
    
    if (!structure->isDictionary()) {
        if (Structure* existingTransition = structure->m_transitionTable.get(nullptr, 0, transitionKind)) {
            ASSERT(existingTransition->transitionKind() == transitionKind);
            ASSERT(existingTransition->indexingModeIncludingHistory() == indexingModeIncludingHistory);
            return existingTransition;
        }
    }
    
    DeferGC deferGC(vm);
    
    Structure* transition = Structure::create(vm, structure, deferred);
    transition->setTransitionKind(transitionKind);
    transition->m_blob.setIndexingModeIncludingHistory(indexingModeIncludingHistory);

    if (changesIndexingType(transitionKind) && hasAnyArrayStorage(indexingModeIncludingHistory)) {
        transition->setHasNonConfigurableProperties(true);
        transition->setHasNonConfigurableReadOnlyOrGetterSetterProperties(true);
    }
    
    if (preventsExtensions(transitionKind))
        transition->setDidPreventExtensions(true);

    if (transitionKind == TransitionKind::BecomePrototype)
        transition->setMayBePrototype(true);
    
    if (setsDontDeleteOnAllProperties(transitionKind)
        || setsReadOnlyOnNonAccessorProperties(transitionKind)) {
        // We pin the property table on transitions that do wholesale editing of the property
        // table, since our logic for walking the property transition chain to rematerialize the
        // table doesn't know how to take into account such wholesale edits.

        ASSERT(transitionKind == TransitionKind::Seal || transitionKind == TransitionKind::Freeze);

        PropertyTable* table = structure->copyPropertyTableForPinning(vm);
        transition->pinForCaching(Locker { transition->m_lock }, vm, table);
        transition->setMaxOffset(vm, structure->maxOffset());
        
        table = transition->propertyTableOrNull();
        RELEASE_ASSERT(table);
        if (transitionKind == TransitionKind::Seal)
            table->seal();
        else
            table->freeze();

        transition->setHasNonConfigurableProperties(true);
        transition->setHasNonConfigurableReadOnlyOrGetterSetterProperties(true);
    } else {
        transition->setPropertyTable(vm, structure->takePropertyTableOrCloneIfPinned(vm));
        transition->setMaxOffset(vm, structure->maxOffset());
        checkOffset(transition->maxOffset(), transition->inlineCapacity());
    }
    
    if (setsReadOnlyOnNonAccessorProperties(transitionKind)
        && !transition->propertyTableOrNull()->isEmpty())
        transition->setHasReadOnlyOrGetterSetterPropertiesExcludingProto(true);
    
    if (structure->isDictionary()) {
        PropertyTable* table = transition->ensurePropertyTable(vm);
        transition->pin(Locker { transition->m_lock }, vm, table);
    } else {
        Locker locker { structure->m_lock };
        structure->m_transitionTable.add(vm, structure, transition);
    }

    transition->checkOffsetConsistency();
    return transition;
}

// In future we may want to cache this property.
bool Structure::isSealed(VM& vm)
{
    if (isStructureExtensible())
        return false;

    PropertyTable* table = ensurePropertyTableIfNotEmpty(vm);
    if (!table)
        return true;
    return table->isSealed();
}

// In future we may want to cache this property.
bool Structure::isFrozen(VM& vm)
{
    if (isStructureExtensible())
        return false;

    PropertyTable* table = ensurePropertyTableIfNotEmpty(vm);
    if (!table)
        return true;
    return table->isFrozen();
}

Structure* Structure::flattenDictionaryStructure(VM& vm, JSObject* object)
{
    ASSERT(!isCompilationThread());
    checkOffsetConsistency();
    ASSERT(isDictionary());
    ASSERT(object->structure() == this);

    Locker<JSCellLock> cellLocker(NoLockingNecessary);

    PropertyTable* table = nullptr;
    size_t beforeOutOfLineCapacity = this->outOfLineCapacity();
    size_t afterOutOfLineCapacity = beforeOutOfLineCapacity;
    if (isUncacheableDictionary()) {
        table = propertyTableOrNull();
        ASSERT(table);
        PropertyOffset maxOffset = invalidOffset;
        if (unsigned propertyCount = table->size())
            maxOffset = offsetForPropertyNumber(propertyCount - 1, m_inlineCapacity);
        afterOutOfLineCapacity = outOfLineCapacity(maxOffset);
    }

    // This is the only case we shrink butterfly in this function. We should take a cell lock to protect against concurrent access to the butterfly.
    if (beforeOutOfLineCapacity != afterOutOfLineCapacity)
        cellLocker = Locker { object->cellLock() };

    GCSafeConcurrentJSLocker locker(m_lock, vm);

    object->setStructureIDDirectly(id().nuke());
    WTF::storeStoreFence();

    if (isUncacheableDictionary()) {
        size_t propertyCount = table->size();

        // Holds our values compacted by insertion order. This is OK since GC is deferred.
        Vector<JSValue> values(propertyCount);

        // Copies out our values from their hashed locations, compacting property table offsets as we go.
        PropertyOffset offset = table->renumberPropertyOffsets(object, m_inlineCapacity, values);
        setMaxOffset(vm, offset);
        ASSERT(transitionOffset() == invalidOffset);
        
        // Copies in our values to their compacted locations.
        for (unsigned i = 0; i < propertyCount; i++)
            object->putDirectOffset(vm, offsetForPropertyNumber(i, m_inlineCapacity), values[i]);

        // We need to zero our unused property space; otherwise the GC might see a
        // stale pointer when we add properties in the future.
        gcSafeZeroMemory(
            object->inlineStorageUnsafe() + inlineSize(),
            (inlineCapacity() - inlineSize()) * sizeof(EncodedJSValue));

        Butterfly* butterfly = object->butterfly();
        size_t preCapacity = butterfly->indexingHeader()->preCapacity(this);
        void* base = butterfly->base(preCapacity, beforeOutOfLineCapacity);
        void* startOfPropertyStorageSlots = reinterpret_cast<EncodedJSValue*>(base) + preCapacity;
        gcSafeZeroMemory(static_cast<JSValue*>(startOfPropertyStorageSlots), (beforeOutOfLineCapacity - outOfLineSize()) * sizeof(EncodedJSValue));
        checkOffsetConsistency();
    }

    setDictionaryKind(NoneDictionaryKind);
    setHasBeenFlattenedBefore(true);

    ASSERT(this->outOfLineCapacity() == afterOutOfLineCapacity);

    if (object->butterfly() && beforeOutOfLineCapacity != afterOutOfLineCapacity) {
        ASSERT(beforeOutOfLineCapacity > afterOutOfLineCapacity);
        // If the object had a Butterfly but after flattening/compacting we no longer have need of it,
        // we need to zero it out because the collector depends on the Structure to know the size for copying.
        if (!afterOutOfLineCapacity && !this->hasIndexingHeader(object))
            object->setButterfly(vm, nullptr);
        // If the object was down-sized to the point where the base of the Butterfly is no longer within the 
        // first CopiedBlock::blockSize bytes, we'll get the wrong answer if we try to mask the base back to 
        // the CopiedBlock header. To prevent this case we need to memmove the Butterfly down.
        else
            object->shiftButterflyAfterFlattening(locker, vm, this, afterOutOfLineCapacity);
    }
    
    WTF::storeStoreFence();
    object->setStructureIDDirectly(id());

    // We need to do a writebarrier here because the GC thread might be scanning the butterfly while
    // we are shuffling properties around. See: https://bugs.webkit.org/show_bug.cgi?id=166989
    vm.writeBarrier(object);

    return this;
}

void Structure::pinForCaching(const AbstractLocker&, VM& vm, PropertyTable* table)
{
    setIsPinnedPropertyTable(true);
    setPropertyTable(vm, table);
    m_transitionPropertyName = nullptr;
}

void Structure::allocateRareData(VM& vm)
{
    ASSERT(!hasRareData());
    StructureRareData* rareData = StructureRareData::create(vm, previousID());
    WTF::storeStoreFence();
    m_previousOrRareData.set(vm, this, rareData);
    ASSERT(hasRareData());
}

WatchpointSet* Structure::ensurePropertyReplacementWatchpointSet(VM& vm, PropertyOffset offset)
{
    ASSERT(!isUncacheableDictionary());

    // In some places it's convenient to call this with an invalid offset. So, we do the check here.
    if (!isValidOffset(offset))
        return nullptr;
    
    if (!hasRareData())
        allocateRareData(vm);
    ConcurrentJSLocker locker(m_lock);
    Structure* structure = this;
    StructureRareData* rareData = structure->rareData();
    auto result = rareData->m_replacementWatchpointSets.add(offset, nullptr);
    if (result.isNewEntry) {
        result.iterator->value = WatchpointSet::create(IsWatched);
        rareData->incrementActiveReplacementWatchpointSet();
        structure->setIsWatchingReplacement(true);
    }
    return result.iterator->value.get();
}

WatchpointSet* Structure::firePropertyReplacementWatchpointSet(VM& vm, PropertyOffset offset, const char* reason)
{
    ASSERT(!isCompilationThread());
    auto* structure = this;
    auto* watchpointSet = structure->ensurePropertyReplacementWatchpointSet(vm, offset);
    if (watchpointSet && watchpointSet->state() == IsWatched) {
        StructureRareData* rareData = structure->rareData();
        watchpointSet->fireAll(vm, reason);
        if (!rareData->decrementActiveReplacementWatchpointSet())
            structure->setIsWatchingReplacement(false);
    }
    return watchpointSet;
}

void Structure::startWatchingPropertyForReplacements(VM& vm, PropertyName propertyName)
{
    ASSERT(!isUncacheableDictionary());
    
    startWatchingPropertyForReplacements(vm, get(vm, propertyName));
}

void Structure::didReplacePropertySlow(PropertyOffset offset)
{
    firePropertyReplacementWatchpointSet(vm(), offset, "Property did get replaced");
}

void Structure::startWatchingInternalProperties(VM& vm)
{
    if (!isUncacheableDictionary()) {
        startWatchingPropertyForReplacements(vm, vm.propertyNames->toString);
        startWatchingPropertyForReplacements(vm, vm.propertyNames->valueOf);
    }
    setDidWatchInternalProperties(true);
}

#if DUMP_PROPERTYMAP_STATS

PropertyTableStats* propertyTableStats = 0;

struct PropertyTableStatisticsExitLogger {
    PropertyTableStatisticsExitLogger();
    ~PropertyTableStatisticsExitLogger();
};

DEFINE_GLOBAL_FOR_LOGGING(PropertyTableStatisticsExitLogger, logger, { });

PropertyTableStatisticsExitLogger::PropertyTableStatisticsExitLogger()
{
    propertyTableStats = adoptPtr(new PropertyTableStats()).leakPtr();
}

PropertyTableStatisticsExitLogger::~PropertyTableStatisticsExitLogger()
{
    unsigned finds = propertyTableStats->numFinds;
    unsigned collisions = propertyTableStats->numCollisions;
    dataLogF("\nJSC::PropertyTable statistics for process %d\n\n", getCurrentProcessID());
    dataLogF("%d finds\n", finds);
    dataLogF("%d collisions (%.1f%%)\n", collisions, 100.0 * collisions / finds);
    dataLogF("%d lookups\n", propertyTableStats->numLookups.load());
    dataLogF("%d lookup probings\n", propertyTableStats->numLookupProbing.load());
    dataLogF("%d adds\n", propertyTableStats->numAdds.load());
    dataLogF("%d removes\n", propertyTableStats->numRemoves.load());
    dataLogF("%d rehashes\n", propertyTableStats->numRehashes.load());
    dataLogF("%d reinserts\n", propertyTableStats->numReinserts.load());
}

#endif

PropertyTable* Structure::copyPropertyTableForPinning(VM& vm)
{
    if (PropertyTable* table = propertyTableOrNull())
        return PropertyTable::clone(vm, *table);
    bool setPropertyTable = false;
    return materializePropertyTable(vm, setPropertyTable);
}

PropertyOffset Structure::getConcurrently(UniquedStringImpl* uid, unsigned& attributes)
{
    Vector<Structure*, 8> structures;
    Structure* tableStructure;
    PropertyTable* table;

    bool didFindStructure = findStructuresAndMapForMaterialization(structures, tableStructure, table);

    for (auto* structure : structures) {
        if (!structure->m_transitionPropertyName)
            continue;

        switch (structure->transitionKind()) {
        case TransitionKind::PropertyAddition:
        case TransitionKind::PropertyAttributeChange:
            break;
        case TransitionKind::PropertyDeletion:
            if (structure->m_transitionPropertyName.get() == uid) {
                if (didFindStructure) {
                    assertIsHeld(tableStructure->m_lock); // Sadly Clang needs some help here.
                    tableStructure->m_lock.unlock();
                }
                return invalidOffset;
            }
            continue;
        case TransitionKind::SetBrand:
            continue;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        if (structure->m_transitionPropertyName.get() == uid) {
            PropertyOffset result = structure->transitionOffset();
            attributes = structure->transitionPropertyAttributes();
            if (didFindStructure) {
                assertIsHeld(tableStructure->m_lock); // Sadly Clang needs some help here.
                tableStructure->m_lock.unlock();
            }
            return result;
        }
    }

    PropertyOffset result = invalidOffset;

    if (didFindStructure) {
        assertIsHeld(tableStructure->m_lock); // Sadly Clang needs some help here.
        // Because uid is UniquedStringImpl, it is guaranteed that the hash is already computed.
        // So we can use PropertyTable::get even from the concurrent compilers.
        // Even though taking a lock, all you can do is getting value from this table. We must not modify the table
        // from non mutator thread.
        auto [offset, entryAttributes] = table->get(uid);
        if (offset != invalidOffset) {
            result = offset;
            attributes = entryAttributes;
        }
        tableStructure->m_lock.unlock();
    }

    return result;
}

Vector<PropertyTableEntry> Structure::getPropertiesConcurrently()
{
    Vector<PropertyTableEntry> result;

    forEachPropertyConcurrently(
        [&] (const PropertyTableEntry& entry) -> bool {
            result.append(entry);
            return true;
        });
    
    return result;
}

PropertyOffset Structure::add(VM& vm, PropertyName propertyName, unsigned attributes)
{
    return add<ShouldPin::No>(
        vm, propertyName, attributes,
        [this, &vm] (const GCSafeConcurrentJSLocker&, PropertyOffset, PropertyOffset newMaxOffset) {
            setMaxOffset(vm, newMaxOffset);
        });
}

PropertyOffset Structure::remove(VM& vm, PropertyName propertyName)
{
    return remove<ShouldPin::No>(vm, propertyName, [this, &vm] (const GCSafeConcurrentJSLocker&, PropertyOffset, PropertyOffset newMaxOffset) {
        setMaxOffset(vm, newMaxOffset);
    });
}

PropertyOffset Structure::attributeChange(VM& vm, PropertyName propertyName, unsigned attributes)
{
    return attributeChange<ShouldPin::No>(
        vm, propertyName, attributes,
        [this, &vm] (const GCSafeConcurrentJSLocker&, PropertyOffset, PropertyOffset newMaxOffset) {
            setMaxOffset(vm, newMaxOffset);
        });
}

void Structure::getPropertyNamesFromStructure(VM& vm, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    PropertyTable* table = ensurePropertyTableIfNotEmpty(vm);
    if (!table)
        return;
    
    bool knownUnique = propertyNames.canAddKnownUniqueForStructure();
    bool foundSymbol = false;

    auto checkDontEnumAndAdd = [&](const auto& entry) {
        if (mode == DontEnumPropertiesMode::Include || !(entry.attributes() & PropertyAttribute::DontEnum)) {
            if (knownUnique)
                propertyNames.addUnchecked(entry.key());
            else
                propertyNames.add(entry.key());
        }
    };
    
    table->forEachProperty([&](const auto& entry) {
        ASSERT(!isQuickPropertyAccessAllowedForEnumeration() || !(entry.attributes() & PropertyAttribute::DontEnum));
        ASSERT(!isQuickPropertyAccessAllowedForEnumeration() || !entry.key()->isSymbol());
        if (entry.key()->isSymbol()) {
            foundSymbol = true;
            if (propertyNames.propertyNameMode() != PropertyNameMode::Symbols)
                return IterationStatus::Continue;
        }
        checkDontEnumAndAdd(entry);
        return IterationStatus::Continue;
    });

    if (foundSymbol && propertyNames.propertyNameMode() == PropertyNameMode::StringsAndSymbols) {
        // To ensure the order defined in the spec, we append symbols at the last elements of keys.
        // https://tc39.es/ecma262/#sec-ordinaryownpropertykeys
        table->forEachProperty([&](const auto& entry) {
            if (entry.key()->isSymbol())
                checkDontEnumAndAdd(entry);
            return IterationStatus::Continue;
        });
    }
}

void StructureFireDetail::dump(PrintStream& out) const
{
    out.print("Structure transition from ", *m_structure);
}

void Structure::didTransitionFromThisStructureWithoutFiringWatchpoint() const
{
    // If the structure is being watched, and this is the kind of structure that the DFG would
    // like to watch, then make sure to note for all future versions of this structure that it's
    // unwise to watch it.
    if (m_transitionWatchpointSet.isBeingWatched())
        const_cast<Structure*>(this)->setTransitionWatchpointIsLikelyToBeFired(true);
}

void Structure::fireStructureTransitionWatchpoint(DeferredStructureTransitionWatchpointFire* deferred) const
{
    if (deferred) {
        ASSERT(deferred->structure() == this);
        m_transitionWatchpointSet.fireAll(vm(), deferred);
    } else
        m_transitionWatchpointSet.fireAll(vm(), StructureFireDetail(this));
}

void Structure::didTransitionFromThisStructure(DeferredStructureTransitionWatchpointFire* deferred) const
{
    didTransitionFromThisStructureWithoutFiringWatchpoint();
    fireStructureTransitionWatchpoint(deferred);
}

template<typename Visitor>
void Structure::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Structure* thisObject = jsCast<Structure*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    
    ConcurrentJSLocker locker(thisObject->m_lock);
    
    visitor.append(thisObject->m_globalObject);
    if (!thisObject->isObject()) {
        // We do not need to clear JSPropertyNameEnumerator since it is never cached for non-object Structure.
        // We do not have code clearing JSPropertyNameEnumerator since this function can be called concurrently.
        thisObject->m_cachedPrototypeChain.clear();
#if ASSERT_ENABLED
        if (auto* rareData = thisObject->tryRareData())
            ASSERT(!rareData->cachedPropertyNameEnumerator());
#endif
    } else {
        visitor.append(thisObject->m_prototype);
        visitor.append(thisObject->m_cachedPrototypeChain);
    }
    visitor.append(thisObject->m_previousOrRareData);

    if (thisObject->isPinnedPropertyTable() || thisObject->protectPropertyTableWhileTransitioning()) {
        // NOTE: This can interleave in pin(), in which case it may see a null property table.
        // That's fine, because then the barrier will fire and we will scan this again.
        visitor.append(thisObject->m_propertyTableUnsafe);
    } else if (visitor.vm().isAnalyzingHeap())
        visitor.append(thisObject->m_propertyTableUnsafe);
    else if (thisObject->m_propertyTableUnsafe)
        thisObject->m_propertyTableUnsafe.clear();

    if (thisObject->isBrandedStructure())
        BrandedStructure::visitAdditionalChildren(cell, visitor);

    // Mark only in non Full collection. In full collection, we handle it as a weak-link.
    if (!(visitor.heap()->collectionScope() == CollectionScope::Full)) {
        if (auto* transition = thisObject->m_transitionTable.trySingleTransition())
            visitor.appendUnbarriered(transition);
    }
}

DEFINE_VISIT_CHILDREN(Structure);

template<typename Visitor>
ALWAYS_INLINE bool Structure::isCheapDuringGC(Visitor& visitor)
{
    // FIXME: We could make this even safer by returning false if this structure's property table
    // has any large property names.
    // https://bugs.webkit.org/show_bug.cgi?id=157334
    
    return (!m_globalObject || visitor.isMarked(m_globalObject.get()))
        && (hasPolyProto() || !storedPrototypeObject() || visitor.isMarked(storedPrototypeObject()));
}

template<typename Visitor>
bool Structure::markIfCheap(Visitor& visitor)
{
    if (!isCheapDuringGC(visitor))
        return visitor.isMarked(this);

    visitor.appendUnbarriered(this);
    return true;
}

template bool Structure::markIfCheap(AbstractSlotVisitor&);
template bool Structure::markIfCheap(SlotVisitor&);

Ref<StructureShape> Structure::toStructureShape(JSValue value, bool& sawPolyProtoStructure)
{
    Ref<StructureShape> baseShape = StructureShape::create();
    RefPtr<StructureShape> curShape = baseShape.ptr();
    Structure* curStructure = this;
    JSValue curValue = value;
    sawPolyProtoStructure = false;
    while (curStructure) {
        sawPolyProtoStructure |= curStructure->hasPolyProto();
        curStructure->forEachPropertyConcurrently(
            [&] (const PropertyTableEntry& entry) -> bool {
                if (!PropertyName(entry.key()).isPrivateName())
                    curShape->addProperty(*entry.key());
                return true;
            });

        if (JSObject* curObject = curValue.getObject())
            curShape->setConstructorName(JSObject::calculatedClassName(curObject));
        else
            curShape->setConstructorName(curStructure->classInfoForCells()->className);

        if (curStructure->isDictionary())
            curShape->enterDictionaryMode();

        curShape->markAsFinal();

        if (!curValue.isObject())
            break;

        JSObject* object = asObject(curValue);
        JSObject* prototypeObject = object->structure()->storedPrototypeObject(object);
        if (!prototypeObject)
            break;

        auto newShape = StructureShape::create();
        curShape->setProto(newShape.copyRef());
        curShape = WTFMove(newShape);
        curValue = prototypeObject;
        curStructure = prototypeObject->structure();
    }
    
    return baseShape;
}

void Structure::dump(PrintStream& out) const
{
    auto* structureID = reinterpret_cast<void*>(id().bits());
    out.print(RawPointer(this), ":[", RawPointer(structureID),
        "/", (uint32_t)(reinterpret_cast<uintptr_t>(structureID)), ", ",
        classInfoForCells()->className, ", (", inlineSize(), "/", inlineCapacity(), ", ",
        outOfLineSize(), "/", outOfLineCapacity(), "){");

    CommaPrinter comma;
    
    const_cast<Structure*>(this)->forEachPropertyConcurrently(
        [&] (const PropertyTableEntry& entry) -> bool {
            out.print(comma, entry.key(), ":", static_cast<int>(entry.offset()));
            return true;
        });
    
    out.print("}, ", IndexingTypeDump(indexingMode()));
    
    if (hasPolyProto())
        out.print(", PolyProto offset:", knownPolyProtoOffset);
    else if (m_prototype.get().isCell())
        out.print(", Proto:", RawPointer(m_prototype.get().asCell()));

    switch (dictionaryKind()) {
    case NoneDictionaryKind:
        if (hasBeenDictionary())
            out.print(", Has been dictionary");
        break;
    case CachedDictionaryKind:
        out.print(", Dictionary");
        break;
    case UncachedDictionaryKind:
        out.print(", UncacheableDictionary");
        break;
    }

    if (transitionWatchpointSetIsStillValid())
        out.print(", Leaf");
    else if (transitionWatchpointIsLikelyToBeFired())
        out.print(", Shady leaf");
    
    if (transitionWatchpointSet().isBeingWatched())
        out.print(" (Watched)");

    out.print("]");
}

void Structure::dumpInContext(PrintStream& out, DumpContext* context) const
{
    if (context)
        context->structures.dumpBrief(this, out);
    else
        dump(out);
}

void Structure::dumpBrief(PrintStream& out, const CString& string) const
{
    out.print("%", string, ":", classInfoForCells()->className);
    if (indexingType() & IndexingShapeMask)
        out.print(",", IndexingTypeDump(indexingType()));
}

void Structure::dumpContextHeader(PrintStream& out)
{
    out.print("Structures:");
}

bool ClassInfo::hasStaticPropertyWithAnyOfAttributes(uint8_t attributes) const
{
    for (const ClassInfo* ci = this; ci; ci = ci->parentClass) {
        if (const HashTable* table = ci->staticPropHashTable) {
            if (table->seenPropertyAttributes & attributes)
                return true;
        }
    }
    return false;
}

void Structure::setCachedPropertyNameEnumerator(VM& vm, JSPropertyNameEnumerator* enumerator, StructureChain* chain)
{
    ASSERT(typeInfo().isObject());
    ASSERT(!isDictionary());
    if (!hasRareData())
        allocateRareData(vm);
    ASSERT(chain == m_cachedPrototypeChain.get());
    rareData()->setCachedPropertyNameEnumerator(vm, this, enumerator, chain);
}

JSPropertyNameEnumerator* Structure::cachedPropertyNameEnumerator() const
{
    if (!hasRareData())
        return nullptr;
    return rareData()->cachedPropertyNameEnumerator();
}

uintptr_t Structure::cachedPropertyNameEnumeratorAndFlag() const
{
    if (!hasRareData())
        return 0;
    return rareData()->cachedPropertyNameEnumeratorAndFlag();
}

bool Structure::canCachePropertyNameEnumerator(VM&) const
{
    if (!this->canCacheOwnPropertyNames())
        return false;

    StructureChain* structureChain = m_cachedPrototypeChain.get();
    ASSERT(structureChain);
    StructureID* currentStructureID = structureChain->head();
    while (true) {
        StructureID structureID = *currentStructureID;
        if (!structureID)
            return true;
        Structure* structure = structureID.decode();
        if (!structure->canCacheOwnPropertyNames())
            return false;
        currentStructureID++;
    }

    ASSERT_NOT_REACHED();
    return true;
}
    
bool Structure::canAccessPropertiesQuicklyForEnumeration() const
{
    if (!isQuickPropertyAccessAllowedForEnumeration())
        return false;
    if (hasAnyKindOfGetterSetterProperties())
        return false;
    if (isUncacheableDictionary())
        return false;
    if (typeInfo().overridesGetOwnPropertyNames())
        return false;
    return true;
}

auto Structure::findPropertyHashEntry(PropertyName propertyName) const -> std::optional<PropertyHashEntry>
{
    for (const ClassInfo* info = classInfoForCells(); info; info = info->parentClass) {
        if (const HashTable* propHashTable = info->staticPropHashTable) {
            if (const HashTableValue* entry = propHashTable->entry(propertyName))
                return PropertyHashEntry { propHashTable, entry };
        }
    }
    return std::nullopt;
}

Structure* Structure::setBrandTransitionFromExistingStructureImpl(Structure* structure, UniquedStringImpl* brandID)
{
    ASSERT(structure->isObject());

    if (structure->hasBeenDictionary())
        return nullptr;

    if (Structure* existingTransition = structure->m_transitionTable.get(brandID, 0, TransitionKind::SetBrand))
        return existingTransition;

    return nullptr;
}

Structure* Structure::setBrandTransitionFromExistingStructureConcurrently(Structure* structure, UniquedStringImpl* brandID)
{
    ConcurrentJSLocker locker(structure->m_lock);
    return setBrandTransitionFromExistingStructureImpl(structure, brandID);
}

Structure* Structure::setBrandTransition(VM& vm, Structure* structure, Symbol* brand, DeferredStructureTransitionWatchpointFire* deferred)
{
    Structure* existingTransition = setBrandTransitionFromExistingStructureImpl(structure, &brand->uid());
    if (existingTransition) 
        return existingTransition;

    Structure* transition = BrandedStructure::create(vm, structure, &brand->uid(), deferred);
    transition->setTransitionKind(TransitionKind::SetBrand);

    transition->m_cachedPrototypeChain.setMayBeNull(vm, transition, structure->m_cachedPrototypeChain.get());
    transition->m_blob.setIndexingModeIncludingHistory(structure->indexingModeIncludingHistory());
    transition->m_transitionPropertyName = &brand->uid();
    transition->setTransitionPropertyAttributes(0);
    transition->setPropertyTable(vm, structure->takePropertyTableOrCloneIfPinned(vm));
    transition->setMaxOffset(vm, structure->maxOffset());
    checkOffset(transition->maxOffset(), transition->inlineCapacity());

    if (structure->isDictionary()) {
        PropertyTable* table = transition->ensurePropertyTable(vm);
        transition->pin(Locker { transition->m_lock }, vm, table);
    } else {
        Locker locker { structure->m_lock };
        structure->m_transitionTable.add(vm, structure, transition);
    }

    transition->checkOffsetConsistency();
    return transition;
}

void DeferredStructureTransitionWatchpointFire::fireAllSlow()
{
    StructureFireDetail detail(m_structure);
    watchpointsToFire().fireAll(m_vm, detail);
}

void Structure::finalizeUnconditionally(VM& vm, CollectionScope collectionScope)
{
    m_transitionTable.finalizeUnconditionally(vm, collectionScope);
}

} // namespace JSC
