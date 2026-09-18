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

Structure* Structure::addPropertyTransition(VM& vm, Structure* structure, PropertyName propertyName, unsigned attributes, PropertyOffset& offset)
{
    Structure* newStructure = addPropertyTransitionToExistingStructure(structure, propertyName, attributes, offset);
    if (!newStructure)
        newStructure = addNewPropertyTransition(vm, structure, propertyName, attributes, offset, PutPropertySlot::UnknownContext);

    // The only property-addition entry point that adds a property with no value in hand: every caller is a VM
    // structure builder that writes the field itself without maintaining a field type. Declaring it here
    // rather than at ~70 write sites is deliberate -- a hand audit missed RegExp.cpp's named-capture-groups
    // object -- and a missed site is a type-confusion SIGSEGV, since user code can reach the same structure by
    // adding the same property names in the same order.
    poisonFieldTypesForVMWrittenProperty(vm, newStructure, offset);
    return newStructure;
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

// The owner Structure caches this record's claim in a word of its own, so a property creation can test it with
// a load and a compare instead of a locked hash lookup. The cache must be withdrawn whenever the claim is: a
// stale-SET word is merely conservative, a stale-CLEAR word is unsound. Out of line because Structure is
// incomplete where FieldTypeRecord is declared.

void FieldTypeRecord::clearOwnerShapeClaimCache()
{
    if (Structure* owner = m_owner.decode()) {
        // entryWithoutClaim, not zero: zero means "nothing recorded yet" and would send every later creation
        // of this shape to the table forever, measured at -15.7% on json-parse-inspector.
        if (owner->fieldTypeClaimIndex() != FieldTypeClaimIndex::offsetWasReused)
            owner->setFieldTypeClaimIndex(FieldTypeClaimIndex::entryWithoutClaim);
    }
}

Structure* FieldTypeWatchpointTable::findOffsetOwnerMemoised(Structure* structure, PropertyOffset offset)
{
    // Hash (structure, offset). The multiply is a cheap spreader; the array size is a power of two so the mask is
    // a single AND. A miss costs exactly what the old code always cost, plus one store.
    unsigned index = (static_cast<unsigned>(reinterpret_cast<uintptr_t>(structure) >> 4) * 2654435761u
        + static_cast<unsigned>(offset)) & (offsetOwnerMemoLiveSize() - 1);
    OffsetOwnerMemoEntry& entry = m_offsetOwnerMemo[index];
    if (entry.structure == structure && entry.offset == offset) [[likely]] {
        if (Options::logFieldTypes() || Options::useDollarVM()) [[unlikely]]
            m_offsetOwnerMemoHits.fetch_add(1, std::memory_order_relaxed);
        return entry.owner;
    }
    if (Options::logFieldTypes() || Options::useDollarVM()) [[unlikely]]
        m_offsetOwnerMemoMisses.fetch_add(1, std::memory_order_relaxed);

    // A null owner is cached too. findOffsetOwner returns null both for "no owner" and for a SEVERED chain, and
    // callers already treat null as "be conservative"; caching it changes nothing about that, and it is the case
    // dictionaries take, which would otherwise walk on every store.
    Structure* owner = structure->findOffsetOwner(offset);
    entry.structure = structure;
    entry.offset = offset;
    entry.owner = owner;
    return owner;
}

CString fieldTypeFieldName(Structure* owner, PropertyOffset offset)
{
    CString name("<unknown>");
    if (!owner)
        return name;
    owner->forEachPropertyConcurrently([&](const PropertyTableEntry& entry) -> bool {
        if (entry.offset() == offset) {
            if (entry.key())
                name = entry.key()->utf8();
            return false;
        }
        return true;
    });
    return name;
}

void FieldTypeRecord::reportStabilityAtDependency() const
{
    StructureID claimedID = m_expected.load(std::memory_order_relaxed);
    Structure* claimed = claimedID ? claimedID.decode() : nullptr;
    if (!claimed)
        return;
    // stillLeaf=false means something has already been derived from the claimed shape, i.e. V8's is_stable() would
    // be clear and Object::OptimalType would have returned Any instead of Class -- so V8 would never have made this
    // claim, and this dependency would not exist.
    dataLogLn("[fieldtype] DEP-STABILITY owner=", m_owner.bits(), " offset=", m_offset,
        " claimed=", claimedID.bits(),
        " stillLeaf=", claimed->transitionWatchpointSetIsStillValid(),
        " likelyToFire=", claimed->transitionWatchpointIsLikelyToBeFired(),
        " dependentsSoFar=", dependents());
}

void FieldTypeRecord::reportExpensiveWithdrawal(StructureID claimedWas) const
{
    Structure* owner = m_owner.decode();
    if (!owner)
        return;
    // transitionPropertyName(), not fieldTypeFieldName(): the latter searches the property table for an entry at
    // this OFFSET and misattributes whenever an offset has been reused. The owner is by construction the structure
    // that ADDED this field -- recordAtCreation asserts owner->transitionOffset() == offset -- so its transition
    // property name is exact. Both are printed so the discrepancy is visible rather than silent.
    auto* uid = owner->transitionPropertyName();
    CString name = uid ? uid->utf8() : CString("<none>");
    CString byOffset = fieldTypeFieldName(owner, m_offset);
    // owner bits and the CLAIMED VALUE's type are both required: the field NAME here is resolved by walking the
    // property table for this offset, which misattributes under offset reuse, and the name alone cannot say whether
    // V8's IsJSReceiverMap gate (which refuses string-valued claims outright) would have prevented this claim.
    Structure* claimedStructure = claimedWas ? claimedWas.decode() : nullptr;
    dataLogLn("[fieldtype] EXPENSIVE-WITHDRAWAL field=", name.data(), " byOffset=", byOffset.data(),
        " owner=", m_owner.bits(),
        " ownerClass=", owner->classInfoForCells()->className,
        " offset=", m_offset, " dependents=", dependents(),
        " claimedType=", claimedStructure ? static_cast<unsigned>(claimedStructure->typeInfo().type()) : 999u,
        " claimedClass=", claimedStructure ? claimedStructure->classInfoForCells()->className : "<cleared>",
        " isJSReceiver=", claimedStructure ? (claimedStructure->typeInfo().type() >= ObjectType) : false,
        " expectedWas=", claimedWas.bits());
}

// Every withdrawal, named. The question this answers is whether contradiction is a property of the FIELD or of the// individual (adding structure, offset) key: the mechanism keys claims on the latter, so a shape reached by a
// second transition path re-learns from scratch. acorn-wtb contradicts 4,515 of its 5,762 distinct keys against
// delta-blue's 5 of 603, so if the doomed keys repeat a small set of NAMES then a name-keyed admission policy can
// predict them, and declining to claim is sound because it only ever removes information.
void FieldTypeRecord::reportWithdrawalWithName(bool hadClaim) const
{
    Structure* owner = m_owner.decode();
    if (!owner)
        return;
    CString name = fieldTypeFieldName(owner, m_offset);
    dataLogLn("[fieldtype] WITHDRAW-NAMED field=", name.data(),
        " ownerClass=", owner->classInfoForCells()->className,
        " owner=", m_owner.bits(), " offset=", m_offset,
        " hadClaim=", hadClaim, " dependents=", dependents(),
        " creationsSeen=", creationsSeen(), " site=", claimSite());
}

// Reads the owner shape's claim word for the lazy-entry design. Out of line for the same reason as
// clearShapeClaimCacheFor below: Structure is incomplete inside FieldTypeWatchpointTable. A dead or
// undecodable owner reports offsetWasReused, which is the "cannot carry a claim in the word" answer and
// therefore the conservative one -- it forces an eager entry.
void FieldTypeWatchpointTable::reportContradiction(StructureID owner, PropertyOffset offset, StructureID had, StructureID got)
{
    Structure* ownerStructure = owner.decode();
    Structure* hadStructure = had ? had.decode() : nullptr;
    Structure* gotStructure = got ? got.decode() : nullptr;
    auto describe = [](Structure* s) -> const char* {
        return s ? s->classInfoForCells()->className : "<none>";
    };
    auto propCount = [](Structure* s) -> int {
        if (!s)
            return -1;
        int n = 0;
        s->forEachPropertyConcurrently([&](const PropertyTableEntry&) -> bool { ++n; return true; });
        return n;
    };
    CString name = ownerStructure ? fieldTypeFieldName(ownerStructure, offset) : CString("<dead>");
    dataLogLn("[fieldtype] CONTRADICTION field=", name.data(),
        " ownerClass=", describe(ownerStructure),
        " hadClass=", describe(hadStructure), " hadProps=", propCount(hadStructure),
        " gotClass=", describe(gotStructure), " gotProps=", propCount(gotStructure),
        " sameClass=", (hadStructure && gotStructure && hadStructure->classInfoForCells() == gotStructure->classInfoForCells()),
        " sameProto=", (hadStructure && gotStructure && hadStructure->storedPrototype() == gotStructure->storedPrototype()));
}

uint16_t FieldTypeWatchpointTable::claimWordFor(StructureID owner, PropertyOffset offset)
{
    Structure* structure = owner.decode();
    if (!structure)
        return FieldTypeClaimIndex::offsetWasReused;
    // THE ONE-FIELD-PER-WORD INVARIANT, enforced here rather than assumed by the caller. The ancestor walks in
    // JSObjectInlines.h:685/833, JSObject.cpp:548, LLIntSlowPaths.cpp:141/225, JITOperations.cpp:1131 and
    // Heap.cpp:3603 all pass an offset this structure may not add.
    if (structure->transitionOffset() != offset)
        return FieldTypeClaimIndex::offsetWasReused;
    return structure->fieldTypeClaimIndex();
}

// The claim word on the owner shape is a cache of the table, so clearing it is only ever conservative. Mirrors
// FieldTypeRecord::clearOwnerShapeClaimCache for the lazy case, where there is no record to route through.
void FieldTypeWatchpointTable::clearShapeClaimCacheFor(StructureID owner)
{
    if (Structure* structure = owner.decode()) {
        if (structure->fieldTypeClaimIndex() != FieldTypeClaimIndex::offsetWasReused)
            structure->setFieldTypeClaimIndex(FieldTypeClaimIndex::entryWithoutClaim);
    }
}

void FieldTypeWatchpointTable::dumpCreationCensus(VM& vm)
{
    Locker locker { m_censusLock };
    // Delimited because pruneAfterMarking runs per collection: the LAST complete block is the one to read.
    dataLogLn("[fieldtype] CENSUS-BEGIN keys=", m_creationCensus.size());
    for (auto& entry : m_creationCensus) {
        auto& c = entry.value;
        // The name and the owner class are resolved HERE rather than captured at creation, so the hot path costs
        // one byte instead of a property-table walk and a pinned name. An owner the GC has already proved dead
        // must not be touched, and prints as <dead>; every key that acquired a dependent is reachable from live
        // code, so the population that matters is never <dead>.
        const char* ownerClass = "<dead>";
        CString name("<dead>");
        Structure* owner = c.owner.decode();
        if (owner && vm.heap.isMarked(owner)) {
            ownerClass = owner->classInfoForCells()->className;
            name = fieldTypeFieldName(owner, c.owner ? owner->transitionOffset() : invalidOffset);
        }
        dataLogLn("[fieldtype] CENSUS owner=", static_cast<uint32_t>(entry.key >> 32),
            " offset=", static_cast<int32_t>(static_cast<uint32_t>(entry.key)),
            " creations=", c.creations,
            " contradictedAt=", c.contradictedAtCreation,
            " storeHits=", c.storeHits,
            " claimedType=", c.claimedType,
            " ownerIsProto=", c.ownerIsPrototype,
            " claimedIsLeaf=", c.claimedIsLeaf,
            " ownerClass=", ownerClass,
            " field=", name.data());
    }
    dataLogLn("[fieldtype] CENSUS-END");
}

void FieldTypeWatchpointTable::pruneAfterMarking(VM& vm)
{
    Locker locker { m_lock };
    // Unconditional, and deliberately outside the useFieldTypePruneWalk gate below: the memo holds raw
    // Structure* keyed on raw Structure*, so it must not survive a collection that may have killed either. This
    // is the whole invalidation protocol for findOffsetOwnerMemoised.
    clearOffsetOwnerMemo();
    // Population counter for the GC-time walk: it runs on EVERY collection including Eden, so its cost is
    // (collections x table size) and no existing counter reports either factor. The earlier "n.s. on all six
    // bookkeeping tests" verdict for useFieldTypePruneWalk never covered splay, the suite's GC-dominated
    // benchmark, so this number decides whether that gate is worth re-running there (see F11).
    if (Options::logFieldTypes()) [[unlikely]]
        dataLogLn("[fieldtype] PRUNE-WALK entries=", m_records.size());
    if (Options::logFieldTypes()) [[unlikely]]
        dumpStoreSiteStats();
    if (Options::fieldTypeTimeStoreSite()) [[unlikely]]
        dumpStoreSiteTime();
    // Reported here, not from ~VM, because jsc's exit path does not reliably run the VM destructor -- the dump
    // added there printed nothing on a 200k-creation repro. Every collection reprints, so the LAST line covers
    // the run; the ratio is the quantity of interest and it is stable across collections.
    if (Options::fieldTypeTimeRecorder()) [[unlikely]] {
        uint64_t calls = vm.fieldTypeRecorderCalls().load(std::memory_order_relaxed);
        if (calls) {
            uint64_t nanos = vm.fieldTypeRecorderNanos().load(std::memory_order_relaxed);
            dataLogLn("[fieldtype] RECORDER-TIME mode=", Options::fieldTypeTimeRecorder(),
                " calls=", calls, " totalNanos=", nanos, " nsPerCall=", nanos / calls);
        }
    }
    if (Options::useDollarVM()) [[unlikely]] {
        uint64_t total = vm.fieldTypeCreationChaseCount().load(std::memory_order_relaxed);
        if (total) {
            uint64_t skippable = vm.fieldTypeCreationChaseSkippableCount().load(std::memory_order_relaxed);
            dataLogLn("[fieldtype] CREATION-CHASE total=", total, " skippable=", skippable,
                " pct=", (skippable * 100) / total);
        }
    }
    // Population counters for the measurement gates that act on them. Each prints only when non-zero, so a leg that
    // does not exercise a mechanism produces no line for it and a gate's must-move counter stays readable.
    if (Options::logFieldTypes() || Options::useDollarVM()) [[unlikely]] {
        if (uint64_t acq = lockAcquisitions(); acq)
            dataLogLn("[fieldtype] LOCK-WAIT acquisitions=", acq, " totalNanos=", lockWaitNanos(),
                " nsPerAcquire=", lockWaitNanos() / acq);
        if (uint64_t hits = offsetOwnerMemoHits(), misses = offsetOwnerMemoMisses(); hits || misses)
            dataLogLn("[fieldtype] OWNER-MEMO size=", offsetOwnerMemoLiveSize(), " hits=", hits, " misses=", misses,
                " hitPct=", (hits + misses) ? (100 * hits / (hits + misses)) : 0);
        if (Options::logFieldTypes())
            dumpCreationCensus(vm);
        if (uint64_t words = wordOnlyClaims())
            dataLogLn("[fieldtype] LAZY-ENTRIES wordOnlyClaims=", words, " withdrawn=", wordOnlyWithdrawals(),
                " materialised=", wordOnlyMaterialisations(),
                " pctNeverMaterialised=", 100 * (words - std::min(words, wordOnlyMaterialisations())) / words,
                " tableEntries=", sizeRelaxed());
        if (uint64_t lazy = lazyClaims())
            dataLogLn("[fieldtype] LAZY-RECORDS lazyClaims=", lazy, " materialised=", lazyMaterialisations(),
                " pctNeverMaterialised=", lazy ? (100 * (lazy - std::min(lazy, lazyMaterialisations())) / lazy) : 0);
    }
    m_records.removeIf([&](auto& entry) {
        // A dead owner means no live object can reach this entry again. Dropping it bounds the table and makes
        // the ID-recycling hazard unreachable: no entry survives the collection that killed the structure it
        // names. Runs for generalised entries too, whose record is null -- hence the owner in the entry.
        if (!vm.heap.isMarked(entry.value.owner.decode())) {
            // Mark the record terminal before the entry goes away: a compiler plan or an installed CodeBlock
            // may hold a Ref and call generalize() later, which writes through m_owner into a dead Structure
            // whose StructureID may already have been recycled.
            if (FieldTypeRecord* record = entry.value.record.get()) {
                record->markTerminalForDeadOwner();
                // The interned slot and its (bits -> slot) map entry are deliberately left in place: that is
                // what lets a recycled StructureID intern straight back onto it. See m_slotForClaimedStructure.
            }
            return true;
        }

        // A LAZY claim (StructureID in the entry, no record) needs the same dead-structure handling as a
        // materialised one, and gets it without allocating: clear the word, clear the owner shape's cache. There is
        // no watchpoint to fire because nothing can depend on a claim no compiler has consulted.
        if (!entry.value.record && entry.value.claimed) {
            if (!vm.heap.isMarked(entry.value.claimed.decode())) {
                entry.value.claimed = StructureID();
                FieldTypeWatchpointTable::clearShapeClaimCacheFor(entry.value.owner);
            }
            return false;
        }

        FieldTypeRecord* record = entry.value.record.get();
        if (!record)
            return false;

        if (StructureID expected = record->expected()) {
            if (!vm.heap.isMarked(expected.decode())) {
                // Withdraw the claim, per record. The interned slot deliberately keeps the dead structure's
                // bits: nothing ever decodes a slot, the fast path only compares.
                record->clearExpectedForDeadStructure();
                // Keep the owner's cached word in step. Leaving it set would be safe -- the fast path only
                // compares bits, never decodes them -- but it would break the invariant that the word never
                // claims more than the table does, and cost the field its chance to be re-claimed.
                record->clearOwnerShapeClaimCache();
            }
        }
        return false;
    });

    m_recordCount.store(m_records.size(), std::memory_order_relaxed);

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

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
