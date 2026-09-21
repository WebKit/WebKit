/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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
#include "WebAssemblyGCStructure.h"
#include <wtf/CommaPrinter.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefPtr.h>

#define DUMP_STRUCTURE_ID_STATISTICS 0

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {


template<typename DetailsFunc>
void Structure::checkOffsetConsistency(PropertyTable* propertyTable, const DetailsFunc& detailsFunc) const
{
    // We cannot reliably assert things about the property table in the concurrent
    // compilation thread. It is possible for the table to be stolen and then have
    // things added to it, which leads to the offsets being all messed up. We could
    // get around this by grabbing a lock here, but I think that would be overkill.
    if (isCompilationThread())
        return;

    unsigned totalSize = propertyTable->propertyStorageSize();
    unsigned inlineOverflowAccordingToTotalSize = totalSize < m_inlineCapacity ? 0 : totalSize - m_inlineCapacity;

    auto fail = [&] (const char* description) {
        dataLog("Detected offset inconsistency: ", description, "!\n");
        dataLog("this = ", RawPointer(this), "\n");
        dataLog("transitionOffset = ", transitionOffset(), "\n");
        dataLog("maxOffset = ", maxOffset(), "\n");
        dataLog("m_inlineCapacity = ", m_inlineCapacity, "\n");
        dataLog("propertyTable = ", RawPointer(propertyTable), "\n");
        dataLog("numberOfSlotsForMaxOffset = ", numberOfSlotsForMaxOffset(maxOffset(), m_inlineCapacity), "\n");
        dataLog("totalSize = ", totalSize, "\n");
        dataLog("inlineOverflowAccordingToTotalSize = ", inlineOverflowAccordingToTotalSize, "\n");
        dataLog("numberOfOutOfLineSlotsForMaxOffset = ", numberOfOutOfLineSlotsForMaxOffset(maxOffset()), "\n");
        detailsFunc();
        UNREACHABLE_FOR_PLATFORM();
    };

    if (numberOfSlotsForMaxOffset(maxOffset(), m_inlineCapacity) != totalSize)
        fail("numberOfSlotsForMaxOffset doesn't match totalSize");
    if (inlineOverflowAccordingToTotalSize != numberOfOutOfLineSlotsForMaxOffset(maxOffset()))
        fail("inlineOverflowAccordingToTotalSize doesn't match numberOfOutOfLineSlotsForMaxOffset");
}

#if DUMP_STRUCTURE_ID_STATISTICS
static UncheckedKeyHashSet<Structure*>& liveStructureSet = *(new UncheckedKeyHashSet<Structure*>);
#endif

inline void StructureTransitionTable::setSingleTransition(VM& vm, JSCell* owner, Structure* structure)
{
    ASSERT(isUsingSingleSlot());
    m_data = std::bit_cast<intptr_t>(structure) | UsingSingleSlotFlag;
    vm.writeBarrier(owner, structure);
}

bool StructureTransitionTable::contains(PointerKey rep, unsigned attributes, TransitionKind transitionKind) const
{
    if (isUsingSingleSlot()) {
        Structure* transition = trySingleTransition();
        return transition && transition->m_transitionPropertyName == rep.pointer() && transition->transitionPropertyAttributes() == attributes && transition->transitionKind() == transitionKind;
    }
    return map()->get(StructureTransitionTable::Hash::createKey(rep, attributes, transitionKind));
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
    map()->set(StructureTransitionTable::Hash::createKeyFromStructure(structure), structure);
}

void Structure::dumpStatistics()
{
#if DUMP_STRUCTURE_ID_STATISTICS
    unsigned numberLeaf = 0;
    unsigned numberUsingSingleSlot = 0;
    unsigned numberSingletons = 0;
    unsigned numberWithPropertyTables = 0;
    unsigned totalPropertyTablesSize = 0;

    for (auto* structure : liveStructureSet) {
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
    bool hasStaticPropertyTable = false;
    for (const ClassInfo* ci = classInfoForCells(); ci; ci = ci->parentClass) {
        if (ci->staticPropHashTable)
            hasStaticPropertyTable = true;
    }
    RELEASE_ASSERT(hasStaticPropertyTable == typeInfo().hasStaticPropertyTable());

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

    // MasqueradesAsUndefined requires non-null Realm.
    RELEASE_ASSERT(realm() || !typeInfo().masqueradesAsUndefined());
}
#else
inline void Structure::validateFlags() { }
#endif

Structure::Structure(VM& vm, StructureVariant variant, JSGlobalObject* globalObject, const TypeInfo& typeInfo, const ClassInfo* classInfo)
    : Structure(vm, globalObject, jsNull(), typeInfo, classInfo, NonArray, 0)
{
    m_structureVariant = variant;
    ASSERT(this->variant() == StructureVariant::WebAssemblyGC);
}

Structure::Structure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, const TypeInfo& typeInfo, const ClassInfo* classInfo, IndexingType indexingType, unsigned inlineCapacity)
    : JSCell(vm, vm.structureStructure.get())
    , m_blob(indexingType, typeInfo)
    , m_outOfLineTypeFlags(typeInfo.outOfLineTypeFlags())
    , m_inlineCapacity(inlineCapacity)
    , m_bitField(0)
    , m_propertyHash(0)
    , m_realm(globalObject, WriteBarrierEarlyInit)
    , m_prototype(prototype, WriteBarrierEarlyInit)
    , m_classInfo(classInfo)
    , m_transitionWatchpointSet(IsWatched)
{
    bool hasStaticNonEnumerableProperty = m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::DontEnum));
    bool hasStaticNonConfigurableProperty = m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::DontDelete));
    bool isArrayStorage = hasAnyArrayStorage(indexingType);

    setDictionaryKind(NoneDictionaryKind);
    setIsPinnedPropertyTable(false);
    setHasAnyKindOfGetterSetterProperties(m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::AccessorOrCustomAccessorOrValue)));
    setHasReadOnlyOrGetterSetterPropertiesExcludingProto(hasAnyKindOfGetterSetterProperties() || m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::ReadOnly)));
    setHasNonEnumerableProperties(hasStaticNonEnumerableProperty || typeInfo.overridesGetOwnPropertySlot() || isArrayStorage);
    setHasSpecialProperties(false);
    setHasRawDoubleFields(false);
    setHasNonConfigurableProperties(hasStaticNonConfigurableProperty || typeInfo.overridesGetOwnPropertySlot() || isArrayStorage);
    setHasNonConfigurableReadOnlyOrGetterSetterProperties(hasStaticNonConfigurableProperty || (typeInfo.overridesGetOwnPropertySlot() && typeInfo.type() != ArrayType) || isArrayStorage);
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

    ASSERT(WTF::roundUpToMultipleOf<Structure::atomSize>(this) == this);
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
    bool hasStaticNonEnumerableProperty = m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::DontEnum));
    bool hasStaticNonConfigurableProperty = m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::DontDelete));

    setDictionaryKind(NoneDictionaryKind);
    setIsPinnedPropertyTable(false);
    setHasAnyKindOfGetterSetterProperties(m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::AccessorOrCustomAccessorOrValue)));
    setHasReadOnlyOrGetterSetterPropertiesExcludingProto(hasAnyKindOfGetterSetterProperties() || m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::ReadOnly)));
    setHasNonEnumerableProperties(hasStaticNonEnumerableProperty || typeInfo.overridesGetOwnPropertySlot());
    setHasSpecialProperties(false);
    setHasRawDoubleFields(false);
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

    ASSERT(WTF::roundUpToMultipleOf<Structure::atomSize>(this) == this);
}

Structure::Structure(VM& vm, StructureVariant variant, Structure* previous)
    : JSCell(vm, vm.structureStructure.get())
    , m_inlineCapacity(previous->m_inlineCapacity)
    , m_bitField(0)
    , m_structureVariant(variant)
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
    setHasNonEnumerableProperties(previous->hasNonEnumerableProperties());
    setHasSpecialProperties(previous->hasSpecialProperties());
    setHasRawDoubleFields(previous->hasRawDoubleFields());
    // NOTE: the per-offset mask is propagated in finishCreation, NOT here. It lives in StructureRareData, and
    // allocating a cell inside this constructor trips ASSERT(!vm.isInitializingObject()). finishCreation is where
    // JSC already does the equivalent copy for the shared poly-proto watchpoint.
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

    if (previous->m_realm)
        m_realm.set(vm, this, previous->m_realm.get());
    ASSERT(hasAnyKindOfGetterSetterProperties() || !m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::AccessorOrCustomAccessorOrValue)));
    ASSERT(hasReadOnlyOrGetterSetterPropertiesExcludingProto() || !m_classInfo->hasStaticPropertyWithAnyOfAttributes(static_cast<uint8_t>(PropertyAttribute::ReadOnlyOrAccessorOrCustomAccessorOrValue)));
    ASSERT(!this->typeInfo().overridesGetCallData() || m_classInfo->methodTable.getCallData != &JSCell::getCallData);

    ASSERT(WTF::roundUpToMultipleOf<Structure::atomSize>(this) == this);
}

Structure::~Structure() = default;

void Structure::destroy(JSCell* cell)
{
    auto* structure = static_cast<Structure*>(cell);
    switch (structure->variant()) {
    case StructureVariant::Normal:
        structure->Structure::~Structure();
        break;
    case StructureVariant::Branded:
        static_cast<BrandedStructure*>(structure)->BrandedStructure::~BrandedStructure();
        break;
    case StructureVariant::WebAssemblyGC:
#if ENABLE(WEBASSEMBLY)
        static_cast<WebAssemblyGCStructure*>(structure)->WebAssemblyGCStructure::~WebAssemblyGCStructure();
#endif
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
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
        if (!structure->m_transitionPropertyName)
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
        case TransitionKind::SetBrand: {
            continue;
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

// AN ACCESSOR SLOT CANNOT BE RAW-DOUBLE, and letting it claim to be is a TYPE CONFUSION, not just a wrong number.
// The slot holds a GetterSetter or CustomGetterSetter CELL. If the mask says raw, getDirectRawDoubleAware boxes that
// POINTER as a double -- observed: 2.29698091e-314 for a GetterSetter at 0x115397b60 -- and
// getOwnNonIndexPropertySlot then fails to recognise the accessor at all, falling through to
// PropertySlot::setValue and tripping its `!(attributes & Accessor)` assertion (PropertySlot.h:248).
//
// How it is reached: redefining a DOUBLE-valued property as an accessor ORs Accessor onto the property's existing
// attributes, which already carry RepresentationDouble from the creating store. Neither transition path re-derived
// the bit, so it survived. Found by lldb on spread-set-own-symbol-iterator-side-effects: attributes == 17 ==
// RepresentationDouble|Accessor. See analysis/prompt/box2d/07-PLAN-double-field.md section 5ao.
//
// Normalising here, at the top of both transition paths, covers the stored attributes, the transition KEY, the
// summary bit and the per-offset mask in one place, because everything downstream reads this local.
static ALWAYS_INLINE unsigned normalizeRepresentationAttributes(unsigned attributes)
{
    if (attributes & PropertyAttribute::AccessorOrCustomAccessorOrValue) [[unlikely]]
        return attributes & ~static_cast<unsigned>(PropertyAttribute::RepresentationDouble);
    return attributes;
}

Structure* Structure::addPropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    Structure* newStructure = addPropertyTransitionToExistingStructure(structure, propertyName, attributes, offset);
    if (newStructure)
        return newStructure;

    return addNewPropertyTransition(vm, structure, propertyName, attributes, offset, PutPropertySlot::UnknownContext);
}

Structure* Structure::addNewPropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset, PutPropertySlot::Context context, DeferredStructureTransitionWatchpointFire* deferred, MayReplaceExistingTransition mayReplace)
{
    attributes = normalizeRepresentationAttributes(attributes);
    ASSERT(!structure->isDictionary());
    ASSERT(structure->isObject());
    // Normally a caller must have checked that no transition exists -- addPropertyTransition does. The one exception is
    // replaceRawPropertyAdditionWithBoxed, which deliberately builds a SECOND PropertyAddition child for a name that
    // already has one, in order to displace a raw claim a script has disproved. See its comment.
    ASSERT(mayReplace == MayReplaceExistingTransition::Yes
        || !Structure::addPropertyTransitionToExistingStructure(structure, propertyName, attributes, offset));
    UNUSED_PARAM(mayReplace);
    
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
    // The property table is materialized LAZILY from this transition chain, so the summary bit must be established
    // HERE as well as in StructureInlines.h's table-insertion paths -- otherwise a Structure carries a
    // Double-represented property while hasRawDoubleFields() still reads false, and readers skip their check.
    if (attributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        transition->setHasRawDoubleFields(true);
    transition->setTransitionKind(TransitionKind::PropertyAddition);
    transition->setPropertyTable(vm, structure->takePropertyTableOrCloneIfPinned(vm));
    transition->setMaxOffset(vm, structure->maxOffset());

    offset = transition->add(vm, propertyName, attributes);
    transition->setTransitionOffset(vm, offset);

    // Record the OFFSET in the per-offset mask, not just the summary bit above. This must come after
    // transition->add(), which is what assigns `offset`. Consumers that cannot walk the property table -- above all
    // GC tracing, which runs where forEachPropertyConcurrently would allocate -- need the per-offset answer.
    // setRawDoubleOffset declines offsets it cannot represent, and declining means "stays NaN-boxed", the safe
    // direction of the one-directional invariant (Structure.h, m_rawDoubleMask).
    if (attributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        transition->setRawDoubleOffset(vm, offset);

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
    transition->validateRawDoubleMaskAgreement(vm, "addNewPropertyTransition");
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

    if (structure->shouldDoCacheableDictionaryTransitionForRemoveAndAttributeChange()) {
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
    JSObject* key = prototype.isNull() ? nullptr : asObject(prototype);

    bool shouldChain = !structure->hasPolyProto() && structure->typeInfo().type() != GlobalObjectType && !structure->hasBeenDictionary();
    if (shouldChain) {
        ASSERT(structure->isObject());
        if (Structure* existingTransition = structure->m_transitionTable.get(key, 0, TransitionKind::ChangePrototype)) {
            ASSERT(!existingTransition->hasPolyProto());
            existingTransition->checkOffsetConsistency();
            return existingTransition;
        }
    }

    // Changing [[Prototype]] means that we refresh this object completely.
    // This is very likely that this object will behaves differently from the previous one.
    // Let's pin the table and break the edge to the previous Structure.
    Structure* transition = Structure::create(vm, structure, &deferred);
    PropertyTable* table = structure->copyPropertyTableForPinning(vm);
    transition->pin(Locker { transition->m_lock }, vm, table);
    transition->m_prototype.set(vm, transition, prototype);
    transition->setTransitionKind(TransitionKind::ChangePrototype);
    transition->setMaxOffset(vm, structure->maxOffset());
    checkOffset(transition->transitionOffset(), transition->inlineCapacity());
    if (shouldChain) {
        GCSafeConcurrentJSLocker locker(structure->m_lock, vm);
        structure->m_transitionTable.add(vm, structure, transition);
    }

    transition->checkOffsetConsistency();
    structure->checkOffsetConsistency();
    return transition;
}

Structure* Structure::changeGlobalProxyTargetTransition(VM& vm, Structure* structure, JSGlobalObject* globalObject, DeferredStructureTransitionWatchpointFire& deferred)
{
    DeferGC deferGC(vm);
    Structure* transition = Structure::create(vm, structure, &deferred);

    transition->setRealm(vm, globalObject);

    PropertyTable* table = structure->copyPropertyTableForPinning(vm);
    transition->pin(Locker { transition->m_lock }, vm, table);
    transition->setMaxOffset(vm, structure->maxOffset());

    transition->checkOffsetConsistency();
    return transition;
}

Structure* Structure::attributeChangeTransitionToExistingStructureImpl(Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
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

Structure* Structure::attributeChangeTransitionToExistingStructure(Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    ASSERT(!isCompilationThread());
    return attributeChangeTransitionToExistingStructureImpl(structure, propertyName, attributes, offset);
}

Structure* Structure::attributeChangeTransitionToExistingStructureConcurrently(Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    ConcurrentJSLocker locker(structure->m_lock);
    return attributeChangeTransitionToExistingStructureImpl(structure, propertyName, attributes, offset);
}

Structure* Structure::attributeChangeTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes, DeferredStructureTransitionWatchpointFire* deferred)
{
    attributes = normalizeRepresentationAttributes(attributes);
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

    if (structure->shouldDoCacheableDictionaryTransitionForRemoveAndAttributeChange()) {
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
    // The property table is materialized LAZILY from this transition chain, so the summary bit must be established
    // HERE as well as in StructureInlines.h's table-insertion paths -- otherwise a Structure carries a
    // Double-represented property while hasRawDoubleFields() still reads false, and readers skip their check.
    if (attributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        transition->setHasRawDoubleFields(true);
    transition->setTransitionKind(TransitionKind::PropertyAttributeChange);
    transition->setPropertyTable(vm, structure->takePropertyTableOrCloneIfPinned(vm));
    transition->setMaxOffset(vm, structure->maxOffset());

    offset = transition->attributeChange(vm, propertyName, attributes);
    transition->setTransitionOffset(vm, offset);

    // Per-offset mask, after attributeChange() has assigned `offset`. An attribute change can also REMOVE the Double
    // representation, so clear as well as set -- a stale set bit would over-claim, which is the unsafe direction.
    if (attributes & static_cast<unsigned>(PropertyAttribute::RepresentationDouble))
        transition->setRawDoubleOffset(vm, offset);
    else
        transition->clearRawDoubleOffset(offset);

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
    transition->validateRawDoubleMaskAgreement(vm, "attributeChangeTransition");
    return transition;
}

Structure* Structure::ensureBoxedRepresentation(VM& vm, Structure* structure, PropertyName propertyName,
    PropertyOffset offset, DeferredStructureTransitionWatchpointFire* deferred)
{
    if (!structure->isRawDoubleOffset(offset)) [[likely]]
        return structure;

    unsigned attributes = 0;
    PropertyOffset found = structure->get(vm, propertyName, attributes);
    if (found != offset) [[unlikely]] {
        // The caller's (structure, offset) pair does not describe this property. Nothing safe to do but leave the
        // structure alone; the caller's own store then hits the RELEASE_ASSERT in putDirectOffsetRawDoubleAware
        // rather than silently corrupting a slot.
        return structure;
    }

    // attributeChangeTransition preserves offsets and inline capacity and re-derives BOTH the summary bit and the
    // per-offset mask from the attributes it is handed, so clearing the bit yields a sibling that declines the slot.
    // Objects already using `structure` are untouched: their slots really do hold raw doubles. Reachable only
    // because the attribute-change key now carries the representation bit (StructureTransitionTable.h); while it was
    // masked, this lookup was answered by the raw-claiming sibling and the widen silently no-opped.
    Structure* boxed = attributeChangeTransition(vm, structure, propertyName,
        attributes & ~static_cast<unsigned>(PropertyAttribute::RepresentationDouble), deferred);
    RELEASE_ASSERT(!boxed->isRawDoubleOffset(offset));
    return boxed;
}

// DISPLACE A DISPROVED RAW CLAIM, by building a second PropertyAddition child of `base` whose attributes do NOT
// carry RepresentationDouble, and letting StructureTransitionTable::add() overwrite the entry with it.
//
// WHY A FRESH DIRECT CHILD AND NOT THE ATTRIBUTE-CHANGE SIBLING. ensureBoxedRepresentation produces
// `base -> S_raw -> S_boxed`, a GRANDCHILD. Returning that from the transition lookup crashes: every consumer
// requires a direct child, and asserts it -- Repatch.cpp:1176 and LLIntSlowPaths.cpp:1146 both check
// `newStructure->previousID() == oldStructure`, and the outOfLineCapacity comparisons and
// NukeStructureAndSetButterfly logic depend on the same thing. Measured: rc=137 on the first run.
//
// WHY THIS IS SAFE. It never touches a live slot. Objects already on S_raw keep S_raw, which keeps claiming their
// slots raw -- so nothing is reinterpreted, and the collector's view of every existing object is unchanged. The only
// effect is on FUTURE adds, and it moves the claim from raw to boxed, i.e. toward UNDER-claiming, which is the safe
// direction of the one-directional invariant (Structure.h, m_rawDoubleMask). It is also monotonic: a field can lose
// raw representation and never regain it, so it cannot oscillate.
//
// WHY IT IS NEEDED. The transition key masks RepresentationDouble out, so `(base, name)` has ONE entry pointing at
// whichever structure the first store created. If that was a double, every later `o.f = <non-number>` gets the raw
// sibling, widens away from it, and -- fatally -- the table's answer then DISAGREES with the structure the object
// actually ends up with. Repatch.cpp:1164 derives the IC's new structure from that lookup and bails on the mismatch
// (`if (baseValue.asCell()->structure() != newStructure) return GiveUpOnCache;`), so the put-by-id IC gives up
// permanently; the LLInt loses caching the same way via slot.disableCaching(). Measured on JetStream3 splay:
// operationPutByIdSloppyGaveUp plus the full C++ generic put path, 257 of 394 attributable samples, C++ time doubling
// 869 -> 1,764, score -9.73% with the OSR-exit storm already fixed.
//
// After the replacement the lookup returns a structure that does not claim the slot raw, so the widening branch in
// putDirectInternal stops firing and no revocation flag is needed -- the corrected table IS the state.
Structure* Structure::replaceRawPropertyAdditionWithBoxed(VM& vm, Structure* structure, PropertyName propertyName,
    unsigned attributes, PropertyOffset& offset, DeferredStructureTransitionWatchpointFire* deferred)
{
    Structure* boxed = addNewPropertyTransition(vm, structure, propertyName,
        attributes & ~static_cast<unsigned>(PropertyAttribute::RepresentationDouble), offset,
        PutPropertySlot::UnknownContext, deferred, MayReplaceExistingTransition::Yes);
    RELEASE_ASSERT(!boxed->isRawDoubleOffset(offset));
    RELEASE_ASSERT(boxed->previousID() == structure);
    // DIAGNOSTIC: how often does a shape actually get its raw claim displaced? One line per (base, property), because
    // the replacement is self-limiting -- afterwards the lookup no longer returns a raw-claiming structure. Measured
    // on JetStream3 splay: exactly ONE, which is what converged the site.
    dataLogLnIf(Options::dumpDoubleFieldSplitCensus(), "[rawdouble] REPLACED raw addition offset=", offset,
        " base=", RawPointer(structure), " boxed=", RawPointer(boxed),
        " name=", String(propertyName.uid()));
    return boxed;
}

Structure* Structure::addPropertyTransitionForBoxedSlot(VM& vm, Structure* structure, PropertyName propertyName,
    unsigned attributes, PropertyOffset& offset)
{
    // REVERTED to forcing a boxed sibling. The RELEASE_ASSERT this guarded against is gone (the writer gives the
    // claim up and stores), so the guarantee is no longer REQUIRED -- but making it a plain transition measured
    // neutral-to-negative (sum -5.78 -> -7.94 across 14 tests, about -0.48 excluding two non-significant swings), and
    // js-tokens did NOT improve (-1.16 -> -1.25) even though its profile named this exact path with the tightest
    // signal in the whole investigation: hasRawDoubleFields +112 samples (spread 9, base 0/0/0), gcSafeZeroMemory
    // +115.7 (spread 6, base 0), Heap::barrierThreshold +42 (spread 7, base 0).
    //
    // THE LESSON, and it cost seven refuted hypotheses to learn: a reproducible sample delta on an INLINED function
    // does not localise the cost. Those +112 samples are inline-frame attributions spread over many call sites, so
    // removing one caller moves nothing. Only deltas on genuinely out-of-line functions -- decodeState was the one --
    // have converted into wall-clock wins.
    return ensureBoxedRepresentation(vm,
        addPropertyTransition(vm, structure, propertyName, attributes, offset), propertyName, offset);
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
        transition->setHasNonEnumerableProperties(true);
        transition->setHasNonConfigurableProperties(true);
        transition->setHasNonConfigurableReadOnlyOrGetterSetterProperties(true);
    }
    
    if (preventsExtensions(transitionKind))
        transition->setDidPreventExtensions(true);

    if (transitionKind == TransitionKind::BecomePrototype)
        transition->setMayBePrototype(true);
    
    if (setsDontDeleteOnAllProperties(transitionKind) || setsReadOnlyOnNonAccessorProperties(transitionKind)) {
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

        transition->setHasNonEnumerableProperties(true);
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
    transition->validateRawDoubleMaskAgreement(vm, "nonPropertyTransitionSlow");
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

    // Must outlive cellLocker. The collection this defers until scope exit would otherwise run
    // while the cell lock is held, and the collector takes that same cell lock to scan an array
    // storage butterfly, so it would deadlock against us.
    DeferGC deferGC(vm);

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

    ConcurrentJSLocker locker(m_lock);

    object->setStructureIDDirectly(id().nuke());
    WTF::storeStoreFence();

    if (isUncacheableDictionary()) {
        size_t propertyCount = table->size();

        // Holds our values compacted by insertion order. This is OK since GC is deferred.
        Vector<JSValue> values(propertyCount);

        // RENUMBER THE RAW-DOUBLE MASK ALONGSIDE THE OFFSETS. m_rawDoubleMask is keyed on PropertyOffset and the
        // compaction below MOVES properties between offsets, so a mask left alone describes the WRONG slots. Both
        // directions are unsafe and one is memory-unsafe: an over-claimed offset is SKIPPED by GC tracing
        // (JSObject.cpp appendNonRawDoubleValues), so a live cell sitting there is swept while still referenced.
        // renumberPropertyOffsets fills this in as it goes, because that is the only place where a property's old
        // and new offsets are both in hand; a separate pre-walk could silently diverge from the real one.
        std::array<uint64_t, s_rawDoubleMaskWords> renumberedRawDoubleMask { };

        // Copies out our values from their hashed locations, compacting property table offsets as we go.
        PropertyOffset offset = table->renumberPropertyOffsets(*this, object, m_inlineCapacity, values, renumberedRawDoubleMask);
        setMaxOffset(vm, offset);
        ASSERT(transitionOffset() == invalidOffset);

        // BEFORE the write-back below, which resolves the destination through object->structure() -- i.e. this very
        // Structure -- and must therefore see the NEW mask to write raw slots as raw bits.
        renumberRawDoubleMask(renumberedRawDoubleMask);

        // Copies in our values to their compacted locations.
        for (unsigned i = 0; i < propertyCount; i++)
            object->putDirectOffset(vm, offsetForPropertyNumber(i, m_inlineCapacity), values[i]);

        // We need to zero our unused property space; otherwise the GC might see a
        // stale pointer when we add properties in the future.
        gcSafeZeroMemory(
            object->inlineStorageUnsafe() + inlineSize(),
            (inlineCapacity() - inlineSize()) * sizeof(EncodedJSValue));

        if (Butterfly* butterfly = object->butterfly()) {
            size_t preCapacity = butterfly->indexingHeader()->preCapacity(this);
            void* base = butterfly->base(preCapacity, beforeOutOfLineCapacity);
            void* startOfPropertyStorageSlots = reinterpret_cast<EncodedJSValue*>(base) + preCapacity;
            gcSafeZeroMemory(static_cast<JSValue*>(startOfPropertyStorageSlots), (beforeOutOfLineCapacity - outOfLineSize()) * sizeof(EncodedJSValue));
        }
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

    // We need to do a writebarrier here because a GC thread might be scanning the butterfly while
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

// PHASE B2. THE REPLACEMENT FOR THE STRUCTURE FORK.
//
// Old behaviour on a claim violation (JSObject::widenDoubleRepresentation): attribute-change transition, move the
// violating object to a new Structure, re-box the slot. The objects already on the old Structure stayed there, so
// every read site saw two Structures for the rest of the program -- which is what [18] traced splay's residual to,
// and it is unfixable by construction because JSC objects do not migrate.
//
// New behaviour: clear the claim and fire. NOTHING IS REWRITTEN and no Structure is created, because under
// --useBoxedDoubleFieldSlots the slot always held an ordinary NaN-boxed JSValue whether claimed or not. So every
// object of every Structure in the lineage stops being claimed simultaneously -- V8's map deprecation plus
// MigrateFastToFast, for free, which is the one place this design beats V8 rather than merely matching it.
//
// The fire is the DependentCode::kFieldRepresentationGroup equivalent: compiled code that skipped the three-way
// dispatch registered on this set via Graph::registerClaimWatchpointIfNeeded and is jettisoned here. Eager, once,
// at the violating write -- not 1.8M lazy OSR exits.
bool Structure::giveUpClaim(VM& vm, PropertyOffset offset, const char* reason)
{
    ASSERT(!isCompilationThread());
    if (!hasRareData())
        return false;
    auto* mask = rareData()->m_rawDoubleMask.get();
    if (!mask || !mask->claimRecord)
        return false;

    unsigned bit = static_cast<unsigned>(offset);
    bool hadBit = bit < s_rawDoubleMaskBits && (mask->bits[bit / 64] & (1ULL << (bit % 64)));

    // Clear BEFORE firing. Firing can reenter (jettison runs arbitrary teardown), and a reader that ran in between
    // must not still see the claim -- the one direction of this invariant that is unsafe.
    // ZERO THE WHOLE MASK, not just this offset's bit. Giving up is per-LINEAGE (one shared DoubleFieldClaimRecord),
    // so claimGivenUp() already makes EVERY offset on this Structure answer false -- clearing the rest changes no
    // answer. What it does change is mightHaveClaimAt(), which reads only the summary bit and the [first, last] range:
    // leaving those set sends every later store to the out-of-line writer to be told "not claimed". On
    // json-parse-inspector that is 13 violated keys carrying 1,720,146 stores.
    mask->bits[0] = 0;
    mask->bits[1] = 0;
    renarrowRawDoubleRange();

    // Set the cached bit BEFORE firing, for the same reason the mask bit is cleared first: fireAll can reenter, and
    // a reader that runs in between must not still see the claim as live.
    mask->claimRecord->givenUp = true;
    if (mask->claimRecord->set.isStillValid()) {
        dataLogLnIf(Options::dumpDoubleFieldSplitCensus(), "[rawdouble] CLAIM GIVEN UP offset=", offset,
            " structure=", RawPointer(this), " reason=", reason);
        mask->claimRecord->set.fireAll(vm, reason);
    }
    return hadBit;
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

void Structure::getPropertyNamesFromStructure(VM& vm, PropertyNameArrayBuilder& propertyNames, DontEnumPropertiesMode mode)
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

StructureFireDetail::StructureFireDetail(ClangVTableWorkaroundTag)
    : m_structure(nullptr)
{
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
    Structure* thisObject = uncheckedDowncast<Structure>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    
    ConcurrentJSLocker locker(thisObject->m_lock);
    
    visitor.append(thisObject->m_realm);
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

    switch (thisObject->variant()) {
    case StructureVariant::Normal:
        break;
    case StructureVariant::Branded:
        BrandedStructure::visitAdditionalChildren(cell, visitor);
        break;
    case StructureVariant::WebAssemblyGC:
#if ENABLE(WEBASSEMBLY)
        WebAssemblyGCStructure::visitAdditionalChildren(cell, visitor);
        break;
#endif
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

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
    
    return (!m_realm || visitor.isMarked(m_realm.get()))
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
        curShape = WTF::move(newShape);
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
            out.print(comma, entry.key(), ":"_s, static_cast<int>(entry.offset()));
            return true;
        });

    out.print("}, "_s, IndexingTypeDump(indexingMode()));

    out.print(", "_s, TransitionKindDump(transitionKind()));

    if (hasPolyProto())
        out.print(", PolyProto offset:"_s, knownPolyProtoOffset);
    else if (m_prototype.get().isCell())
        out.print(", Proto:"_s, RawPointer(m_prototype.get().asCell()));

    switch (dictionaryKind()) {
    case NoneDictionaryKind:
        if (hasBeenDictionary())
            out.print(", Has been dictionary"_s);
        break;
    case CachedDictionaryKind:
        out.print(", Dictionary"_s);
        break;
    case UncachedDictionaryKind:
        out.print(", UncacheableDictionary"_s);
        break;
    }

    if (transitionWatchpointSetIsStillValid())
        out.print(", Leaf"_s);
    else if (transitionWatchpointIsLikelyToBeFired())
        out.print(", Shady leaf"_s);
    
    if (transitionWatchpointSet().isBeingWatched())
        out.print(" (Watched)"_s);

    out.print("]"_s);
}

void Structure::dumpInContext(PrintStream& out, DumpContext* context) const
{
    if (context)
        context->structures.dumpBrief(this, out);
    else
        dump(out);
}

void Structure::dumpBrief(PrintStream& out, const ASCIICString& string) const
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

bool ClassInfo::hasStaticProperty(PropertyName propertyName) const
{
    for (const ClassInfo* ci = this; ci; ci = ci->parentClass) {
        if (const HashTable* table = ci->staticPropHashTable) {
            auto* entry = table->entry(propertyName);
            if (entry)
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

void Structure::reconcileWeakReferencesAtGCEnd(VM& vm, CollectionScope collectionScope)
{
    m_transitionTable.reconcileWeakReferencesAtGCEnd(vm, collectionScope);
}

void dumpTransitionKind(PrintStream& out, TransitionKind kind)
{
    const char* kindName;
    switch (kind) {
    case TransitionKind::Unknown:
        kindName = "Unknown";
        break;
    case TransitionKind::PropertyAddition:
        kindName = "PropertyAddition";
        break;
    case TransitionKind::PropertyDeletion:
        kindName = "PropertyDeletion";
        break;
    case TransitionKind::PropertyAttributeChange:
        kindName = "PropertyAttributeChange";
        break;
    case TransitionKind::AllocateUndecided:
        kindName = "AllocateUndecided";
        break;
    case TransitionKind::AllocateInt32:
        kindName = "AllocateInt32";
        break;
    case TransitionKind::AllocateDouble:
        kindName = "AllocateDouble";
        break;
    case TransitionKind::AllocateContiguous:
        kindName = "AllocateContiguous";
        break;
    case TransitionKind::AllocateArrayStorage:
        kindName = "AllocateArrayStorage";
        break;
    case TransitionKind::AllocateSlowPutArrayStorage:
        kindName = "AllocateSlowPutArrayStorage";
        break;
    case TransitionKind::SwitchToSlowPutArrayStorage:
        kindName = "SwitchToSlowPutArrayStorage";
        break;
    case TransitionKind::AddIndexedAccessors:
        kindName = "AddIndexedAccessors";
        break;
    case TransitionKind::PreventExtensions:
        kindName = "PreventExtensions";
        break;
    case TransitionKind::Seal:
        kindName = "Seal";
        break;
    case TransitionKind::Freeze:
        kindName = "Freeze";
        break;
    case TransitionKind::BecomePrototype:
        kindName = "BecomePrototype";
        break;
    case TransitionKind::ChangePrototype:
        kindName = "ChangePrototype";
        break;
    case TransitionKind::SetBrand:
        kindName = "SetBrand";
        break;
    }

    out.print(kindName);
}

void Structure::checkOffsetConsistency() const
{
    if (auto* propertyTable = propertyTableOrNull())
        checkOffsetConsistency(propertyTable, [] { });
    else
        ASSERT(!isPinnedPropertyTable());
}

#if ASSERT_ENABLED
void Structure::checkConsistency()
{
    checkOffsetConsistency();
}
#endif

unsigned Structure::rawDoubleMaskDebugState(PropertyOffset offset) const
{
    if (!hasRawDoubleFields())
        return 0;
    if (!hasRareData())
        return 1;
    const auto* mask = rareData()->m_rawDoubleMask.get();
    if (!mask)
        return 2;
    unsigned bit = static_cast<unsigned>(offset);
    if (!isValidOffset(offset) || bit >= s_rawDoubleMaskBits)
        return 3;
    return (mask->bits[bit / 64] & (1ULL << (bit % 64))) ? 5 : 4;
}

void Structure::validateRawDoubleMaskAgreement(VM& vm, const char* site)
{
    if (!Options::useRawDoubleFieldStorage() || !Options::dumpRawDoubleCorruption()) [[likely]]
        return;
    // PROOF OF LIFE. A validator that reports "nothing wrong" is only evidence once it has been shown capable of
    // reporting something. Count what it actually inspected, so a silent pass can be distinguished from a dead check.
    unsigned inspected = 0;
    unsigned rawSeen = 0;
    bool bad = false;
    forEachProperty(vm, [&](const PropertyTableEntry& entry) -> bool {
        ++inspected;
        if (attributesSayDoubleRepresentation(entry.attributes()) || isRawDoubleOffset(entry.offset()))
            ++rawSeen;
        bool attrsDouble = attributesSayDoubleRepresentation(entry.attributes());
        bool maskRaw = isRawDoubleOffset(entry.offset());
        if (attrsDouble != maskRaw) {
            bad = true;
            dataLogLn("[rawdouble] MASK/ATTR DISAGREEMENT at ", site,
                " structure=", RawPointer(this),
                " prop=", String(entry.key()),
                " offset=", entry.offset(),
                " attrsDouble=", attrsDouble,
                " maskRaw=", maskRaw,
                " summaryBit=", hasRawDoubleFields(),
                " hasRareData=", hasRareData(),
                " isDictionary=", isDictionary(),
                " maxOffset=", maxOffset(),
                " inlineCapacity=", inlineCapacity());
        }
        return true;
    });
    if (hasRawDoubleFields())
        dataLogLn("[rawdouble] validator@", site, " structure=", RawPointer(this),
            " inspected=", inspected, " rawOrDoubleProps=", rawSeen, " agreed=", !bad);
    RELEASE_ASSERT(!bad);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
