/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "AccessCase.h"

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "CallLinkInfo.h"
#include "DirectArguments.h"
#include "GetterSetter.h"
#include "GetterSetterAccessCase.h"
#include "InlineCacheCompiler.h"
#include "InstanceOfAccessCase.h"
#include "IntrinsicGetterAccessCase.h"
#include "JSCInlines.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "ModuleNamespaceAccessCase.h"
#include "ProxyObjectAccessCase.h"
#include "ScopedArguments.h"
#include "ScratchRegisterAllocator.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"

namespace JSC {

namespace AccessCaseInternal {
static constexpr bool verbose = false;
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AccessCase);

AccessCase::AccessCase(VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier, PropertyOffset offset, Structure* structure, const ObjectPropertyConditionSet& conditionSet, RefPtr<PolyProtoAccessChain>&& prototypeAccessChain)
    : m_type(type)
    , m_offset(offset)
    , m_polyProtoAccessChain(WTFMove(prototypeAccessChain))
    , m_identifier(identifier)
{
    m_structureID.setMayBeNull(vm, owner, structure);
    m_conditionSet = conditionSet;
    RELEASE_ASSERT(m_conditionSet.isValid());
}

Ref<AccessCase> AccessCase::create(VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier, PropertyOffset offset, Structure* structure, const ObjectPropertyConditionSet& conditionSet, RefPtr<PolyProtoAccessChain>&& prototypeAccessChain)
{
    switch (type) {
    case LoadMegamorphic:
    case StoreMegamorphic:
    case InHit:
    case InMiss:
    case DeleteNonConfigurable:
    case DeleteMiss:
        break;
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case Replace:
    case InstanceOfGeneric:
    case IndexedProxyObjectLoad:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
        RELEASE_ASSERT(!prototypeAccessChain);
        break;
    case Load:
    case Miss:
    case Delete:
    case Transition:
    case GetGetter:
    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomValueSetter:
    case CustomAccessorGetter:
    case CustomAccessorSetter:
    case IntrinsicGetter:
    case InstanceOfHit:
    case InstanceOfMiss:
    case CheckPrivateBrand:
    case SetPrivateBrand:
        RELEASE_ASSERT_NOT_REACHED();
    };

    return adoptRef(*new AccessCase(vm, owner, type, identifier, offset, structure, conditionSet, WTFMove(prototypeAccessChain)));
}

RefPtr<AccessCase> AccessCase::createTransition(
    VM& vm, JSCell* owner, CacheableIdentifier identifier, PropertyOffset offset, Structure* oldStructure, Structure* newStructure,
    const ObjectPropertyConditionSet& conditionSet, RefPtr<PolyProtoAccessChain>&& prototypeAccessChain, const StructureStubInfo& stubInfo)
{
    RELEASE_ASSERT(oldStructure == newStructure->previousID());

    // Skip optimizing the case where we need a realloc, if we don't have
    // enough registers to make it happen.
    if (oldStructure->outOfLineCapacity() != newStructure->outOfLineCapacity()) {
        // In 64 bits jsc uses 1 register for value, and it uses 2 registers in 32 bits
        size_t requiredRegisters = 1; // stubInfo.valueRegs()
#if USE(JSVALUE32_64)
        ++requiredRegisters;
#endif

        // 1 register for the property in 64 bits
        ++requiredRegisters;
#if USE(JSVALUE32_64)
        // In 32 bits, jsc uses may use one extra register, if it is not a Cell
        if (stubInfo.propertyRegs().tagGPR() != InvalidGPRReg)
            ++requiredRegisters;
#endif

        // 1 register for the base in 64 bits
        ++requiredRegisters;
#if USE(JSVALUE32_64)
        // In 32 bits, jsc uses may use one extra register, if it is not a Cell
        if (stubInfo.baseRegs().tagGPR() != InvalidGPRReg)
            ++requiredRegisters;
#endif

        if (stubInfo.m_stubInfoGPR != InvalidGPRReg)
            ++requiredRegisters;
        if (stubInfo.m_arrayProfileGPR != InvalidGPRReg)
            ++requiredRegisters;

        // One extra register for scratchGPR
        ++requiredRegisters;

        // Check if we have enough registers when reallocating
        if (oldStructure->outOfLineCapacity() && GPRInfo::numberOfRegisters < requiredRegisters)
            return nullptr;

        // If we are (re)allocating inline, jsc needs two extra scratchGPRs
        if (!oldStructure->couldHaveIndexingHeader() && GPRInfo::numberOfRegisters < (requiredRegisters + 2))
            return nullptr;
    }

    return adoptRef(*new AccessCase(vm, owner, Transition, identifier, offset, newStructure, conditionSet, WTFMove(prototypeAccessChain)));
}

Ref<AccessCase> AccessCase::createDelete(
    VM& vm, JSCell* owner, CacheableIdentifier identifier, PropertyOffset offset, Structure* oldStructure, Structure* newStructure)
{
    RELEASE_ASSERT(oldStructure == newStructure->previousID());
    ASSERT(!newStructure->outOfLineCapacity() || oldStructure->outOfLineCapacity());
    return adoptRef(*new AccessCase(vm, owner, Delete, identifier, offset, newStructure, { }, { }));
}

Ref<AccessCase> AccessCase::createCheckPrivateBrand(VM& vm, JSCell* owner, CacheableIdentifier identifier, Structure* structure)
{
    return adoptRef(*new AccessCase(vm, owner, CheckPrivateBrand, identifier, invalidOffset, structure, { }, { }));
}

Ref<AccessCase> AccessCase::createSetPrivateBrand(
    VM& vm, JSCell* owner, CacheableIdentifier identifier, Structure* oldStructure, Structure* newStructure)
{
    RELEASE_ASSERT(oldStructure == newStructure->previousID());
    return adoptRef(*new AccessCase(vm, owner, SetPrivateBrand, identifier, invalidOffset, newStructure, { }, { }));
}

Ref<AccessCase> AccessCase::createReplace(VM& vm, JSCell* owner, CacheableIdentifier identifier, PropertyOffset offset, Structure* oldStructure, bool viaGlobalProxy)
{
    auto result = adoptRef(*new AccessCase(vm, owner, Replace, identifier, offset, oldStructure, { }, { }));
    result->m_viaGlobalProxy = viaGlobalProxy;
    return result;
}

RefPtr<AccessCase> AccessCase::fromStructureStubInfo(
    VM& vm, JSCell* owner, CacheableIdentifier identifier, StructureStubInfo& stubInfo)
{
    switch (stubInfo.cacheType()) {
    case CacheType::GetByIdSelf:
        RELEASE_ASSERT(stubInfo.hasConstantIdentifier);
        return ProxyableAccessCase::create(vm, owner, Load, identifier, stubInfo.byIdSelfOffset, stubInfo.inlineAccessBaseStructure());

    case CacheType::PutByIdReplace:
        RELEASE_ASSERT(stubInfo.hasConstantIdentifier);
        return AccessCase::createReplace(vm, owner, identifier, stubInfo.byIdSelfOffset, stubInfo.inlineAccessBaseStructure(), false);

    case CacheType::InByIdSelf:
        RELEASE_ASSERT(stubInfo.hasConstantIdentifier);
        return AccessCase::create(vm, owner, InHit, identifier, stubInfo.byIdSelfOffset, stubInfo.inlineAccessBaseStructure());

    case CacheType::ArrayLength:
        RELEASE_ASSERT(stubInfo.hasConstantIdentifier);
        return AccessCase::create(vm, owner, AccessCase::ArrayLength, identifier);

    case CacheType::StringLength:
        RELEASE_ASSERT(stubInfo.hasConstantIdentifier);
        return AccessCase::create(vm, owner, AccessCase::StringLength, identifier);

    default:
        return nullptr;
    }
}

bool AccessCase::hasAlternateBaseImpl() const
{
    return !conditionSet().isEmpty();
}

JSObject* AccessCase::alternateBaseImpl() const
{
    return conditionSet().slotBaseCondition().object();
}

Ref<AccessCase> AccessCase::cloneImpl() const
{
    auto result = adoptRef(*new AccessCase(*this));
    result->resetState();
    return result;
}

Vector<WatchpointSet*, 2> AccessCase::commit(VM& vm)
{
    // It's fine to commit something that is already committed. That arises when we switch to using
    // newly allocated watchpoints. When it happens, it's not efficient - but we think that's OK
    // because most AccessCases have no extra watchpoints anyway.
    RELEASE_ASSERT(m_state == Primordial || m_state == Committed);

    Vector<WatchpointSet*, 2> result;
    Structure* structure = this->structure();
    auto append = [&] (auto* set) {
        ASSERT(set->isStillValid());
        result.append(set);
    };

    if (m_identifier) {
        if ((structure && structure->needImpurePropertyWatchpoint())
            || m_conditionSet.needImpurePropertyWatchpoint()
            || (m_polyProtoAccessChain && m_polyProtoAccessChain->needImpurePropertyWatchpoint(vm)))
            append(vm.ensureWatchpointSetForImpureProperty(m_identifier.uid()));
    }

    if (additionalSet())
        append(additionalSet());

    if (structure
        && structure->hasRareData()
        && structure->rareData()->hasSharedPolyProtoWatchpoint()
        && structure->rareData()->sharedPolyProtoWatchpoint()->isStillValid()) {
        WatchpointSet* set = structure->rareData()->sharedPolyProtoWatchpoint()->inflate();
        append(set);
    }

    m_state = Committed;

    return result;
}

bool AccessCase::guardedByStructureCheck(const StructureStubInfo& stubInfo) const
{
    if (!stubInfo.hasConstantIdentifier)
        return false;
    return guardedByStructureCheckSkippingConstantIdentifierCheck(); 
}

bool AccessCase::guardedByStructureCheckSkippingConstantIdentifierCheck() const
{
    if (viaGlobalProxy())
        return false;

    if (m_polyProtoAccessChain)
        return false;

    switch (m_type) {
    case LoadMegamorphic:
    case StoreMegamorphic:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
    case IndexedProxyObjectLoad:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedStringLoad:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
        return false;
    case Load:
    case Miss:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace:
    case IndexedNoIndexingMiss:
    case Transition:
    case GetGetter:
    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomValueSetter:
    case CustomAccessorGetter:
    case CustomAccessorSetter:
    case InHit:
    case InMiss:
    case IntrinsicGetter:
    case CheckPrivateBrand:
    case SetPrivateBrand:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool AccessCase::requiresIdentifierNameMatch() const
{
    switch (m_type) {
    case Load:
    case LoadMegamorphic:
    case StoreMegamorphic:
    // We don't currently have a by_val for these puts, but we do care about the identifier.
    case Transition:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace: 
    case Miss:
    case GetGetter:
    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter:
    case IntrinsicGetter:
    case InHit:
    case InMiss:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case CheckPrivateBrand:
    case SetPrivateBrand:
        return true;
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
    case IndexedProxyObjectLoad:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool AccessCase::requiresInt32PropertyCheck() const
{
    switch (m_type) {
    case Load:
    case LoadMegamorphic:
    case StoreMegamorphic:
    case Transition:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace: 
    case Miss:
    case GetGetter:
    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter:
    case IntrinsicGetter:
    case InHit:
    case InMiss:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case IndexedProxyObjectLoad:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
        return false;
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool AccessCase::needsScratchFPR() const
{
    switch (m_type) {
    case Load:
    case LoadMegamorphic:
    case StoreMegamorphic:
    case Transition:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace: 
    case Miss:
    case GetGetter:
    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter:
    case InHit:
    case InMiss:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
    case IndexedProxyObjectLoad:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case IndexedInt32Store:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
        return false;
    case IndexedDoubleLoad:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedDoubleStore:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    // Used by TypedArrayLength/TypedArrayByteOffset in the process of boxing their result as a double
    case IntrinsicGetter:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Functor>
void AccessCase::forEachDependentCell(VM&, const Functor& functor) const
{
    m_conditionSet.forEachDependentCell(functor);
    if (m_structureID)
        functor(m_structureID.get());
    if (m_polyProtoAccessChain) {
        for (StructureID structureID : m_polyProtoAccessChain->chain())
            functor(structureID.decode());
    }

    switch (type()) {
    case Getter:
    case Setter: {
        auto& accessor = this->as<GetterSetterAccessCase>();
        if (accessor.callLinkInfo())
            accessor.callLinkInfo()->forEachDependentCell(functor);
        break;
    }
    case CustomValueGetter:
    case CustomValueSetter: {
        auto& accessor = this->as<GetterSetterAccessCase>();
        if (accessor.customSlotBase())
            functor(accessor.customSlotBase());
        break;
    }
    case IntrinsicGetter: {
        auto& intrinsic = this->as<IntrinsicGetterAccessCase>();
        if (intrinsic.intrinsicFunction())
            functor(intrinsic.intrinsicFunction());
        break;
    }
    case ModuleNamespaceLoad: {
        auto& accessCase = this->as<ModuleNamespaceAccessCase>();
        if (accessCase.moduleNamespaceObject())
            functor(accessCase.moduleNamespaceObject());
        if (accessCase.moduleEnvironment())
            functor(accessCase.moduleEnvironment());
        break;
    }
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case IndexedProxyObjectLoad: {
        auto& accessor = this->as<ProxyObjectAccessCase>();
        if (accessor.callLinkInfo())
            accessor.callLinkInfo()->forEachDependentCell(functor);
        break;
    }
    case InstanceOfHit:
    case InstanceOfMiss:
        if (as<InstanceOfAccessCase>().prototype())
            functor(as<InstanceOfAccessCase>().prototype());
        break;
    case CustomAccessorGetter:
    case CustomAccessorSetter:
    case Load:
    case LoadMegamorphic:
    case StoreMegamorphic:
    case Transition:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace:
    case Miss:
    case GetGetter:
    case InHit:
    case InMiss:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case InstanceOfGeneric:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
        break;
    }
}

bool AccessCase::doesCalls(VM& vm, Vector<JSCell*>* cellsToMarkIfDoesCalls) const
{
    bool doesCalls = false;
    switch (type()) {
    case Transition:
        doesCalls = newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity() && structure()->couldHaveIndexingHeader();
        break;
    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter:
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case IndexedProxyObjectLoad:
        doesCalls = true;
        break;
    case IntrinsicGetter: {
        doesCalls = this->as<IntrinsicGetterAccessCase>().doesCalls();
        break;
    }
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Load:
    case LoadMegamorphic:
    case StoreMegamorphic:
    case Miss:
    case GetGetter:
    case InHit:
    case InMiss:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
        doesCalls = false;
        break;
    case Replace:
        doesCalls = viaGlobalProxy();
        break;
    }

    if (doesCalls && cellsToMarkIfDoesCalls) {
        forEachDependentCell(vm, [&](JSCell* cell) {
            cellsToMarkIfDoesCalls->append(cell);
        });
    }
    return doesCalls;
}

bool AccessCase::couldStillSucceed() const
{
    for (const ObjectPropertyCondition& condition : m_conditionSet) {
        if (condition.condition().kind() == PropertyCondition::Equivalence) {
            if (!condition.isWatchableAssumingImpurePropertyWatchpoint(PropertyCondition::WatchabilityEffort::EnsureWatchability))
                return false;
        } else {
            if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint(Concurrency::MainThread))
                return false;
        }
    }
    return true;
}

bool AccessCase::canReplace(const AccessCase& other) const
{
    // This puts in a good effort to try to figure out if 'other' is made superfluous by '*this'.
    // It's fine for this to return false if it's in doubt.
    //
    // Note that if A->guardedByStructureCheck() && B->guardedByStructureCheck() then
    // A->canReplace(B) == B->canReplace(A).

    if (m_identifier != other.m_identifier)
        return false;

    if (viaGlobalProxy() != other.viaGlobalProxy())
        return false;

    auto checkPolyProtoAndStructure = [&] {
        if (m_polyProtoAccessChain) {
            if (!other.m_polyProtoAccessChain)
                return false;
            // This is the only check we need since PolyProtoAccessChain contains the base structure.
            // If we ever change it to contain only the prototype chain, we'll also need to change
            // this to check the base structure.
            return structure() == other.structure()
                && *m_polyProtoAccessChain == *other.m_polyProtoAccessChain;
        }

        if (!guardedByStructureCheckSkippingConstantIdentifierCheck() || !other.guardedByStructureCheckSkippingConstantIdentifierCheck())
            return false;

        return structure() == other.structure();
    };
    
    switch (type()) {
    case LoadMegamorphic:
    case StoreMegamorphic:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedStringLoad:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case IndexedProxyObjectLoad:
        return other.type() == type();

    case ModuleNamespaceLoad: {
        if (other.type() != type())
            return false;
        auto& thisCase = this->as<ModuleNamespaceAccessCase>();
        auto& otherCase = this->as<ModuleNamespaceAccessCase>();
        return thisCase.moduleNamespaceObject() == otherCase.moduleNamespaceObject();
    }

    case InstanceOfHit:
    case InstanceOfMiss: {
        if (other.type() != type())
            return false;
        
        if (this->as<InstanceOfAccessCase>().prototype() != other.as<InstanceOfAccessCase>().prototype())
            return false;
        
        return structure() == other.structure();
    }

    case InstanceOfGeneric:
        switch (other.type()) {
        case InstanceOfGeneric:
        case InstanceOfHit:
        case InstanceOfMiss:
            return true;
        default:
            return false;
        }

    case Load:
    case Transition:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace:
    case Miss:
    case GetGetter:
    case Setter:
    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter:
    case InHit:
    case InMiss:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case IndexedNoIndexingMiss:
        if (other.type() != type())
            return false;

        return checkPolyProtoAndStructure();

    case IntrinsicGetter:
    case Getter:
        if (other.type() != Getter && other.type() != IntrinsicGetter)
            return false;

        return checkPolyProtoAndStructure();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void AccessCase::dump(PrintStream& out) const
{
    out.print("\n", m_type, ": {");

    Indenter indent;
    CommaPrinter comma;

    out.print(comma, m_state);

    out.print(comma, "ident = '", m_identifier, "'");
    if (isValidOffset(m_offset))
        out.print(comma, "offset = ", m_offset);

    ++indent;

    if (m_polyProtoAccessChain) {
        out.print("\n", indent, "prototype access chain = ");
        m_polyProtoAccessChain->dump(structure(), out);
    } else {
        if (m_type == Transition || m_type == Delete || m_type == SetPrivateBrand)
            out.print("\n", indent, "from structure = ", pointerDump(structure()),
                "\n", indent, "to structure = ", pointerDump(newStructure()));
        else if (m_structureID)
            out.print("\n", indent, "structure = ", pointerDump(m_structureID.get()));
    }

    if (!m_conditionSet.isEmpty())
        out.print("\n", indent, "conditions = ", m_conditionSet);

    const_cast<AccessCase*>(this)->runWithDowncast([&](auto* accessCase) {
        accessCase->dumpImpl(out, comma, indent);
    });

    out.print("}");
}

bool AccessCase::visitWeak(VM& vm) const
{
    if (isAccessor()) {
        auto& accessor = this->as<GetterSetterAccessCase>();
        if (accessor.callLinkInfo())
            accessor.callLinkInfo()->visitWeak(vm);
    }

    bool isValid = true;
    forEachDependentCell(vm, [&](JSCell* cell) {
        isValid &= vm.heap.isMarked(cell);
    });
    return isValid;
}

template<typename Visitor>
void AccessCase::propagateTransitions(Visitor& visitor) const
{
    if (m_structureID)
        m_structureID->markIfCheap(visitor);

    if (m_polyProtoAccessChain) {
        for (StructureID structureID : m_polyProtoAccessChain->chain())
            structureID.decode()->markIfCheap(visitor);
    }

    switch (m_type) {
    case Transition:
    case Delete:
        if (visitor.isMarked(m_structureID->previousID()))
            visitor.appendUnbarriered(m_structureID.get());
        break;
    default:
        break;
    }
}

template void AccessCase::propagateTransitions(AbstractSlotVisitor&) const;
template void AccessCase::propagateTransitions(SlotVisitor&) const;


template<typename Visitor>
void AccessCase::visitAggregateImpl(Visitor& visitor) const
{
    m_identifier.visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE_WITH_MODIFIER(AccessCase, const);

template<typename Func>
inline void AccessCase::runWithDowncast(const Func& func)
{
    switch (m_type) {
    case LoadMegamorphic:
    case StoreMegamorphic:
    case Transition:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace:
    case InHit:
    case InMiss:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case InstanceOfGeneric:
        func(static_cast<AccessCase*>(this));
        break;

    case Load:
    case Miss:
    case GetGetter:
        func(static_cast<ProxyableAccessCase*>(this));
        break;

    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter:
        func(static_cast<GetterSetterAccessCase*>(this));
        break;

    case IntrinsicGetter:
        func(static_cast<IntrinsicGetterAccessCase*>(this));
        break;

    case ModuleNamespaceLoad:
        func(static_cast<ModuleNamespaceAccessCase*>(this));
        break;

    case InstanceOfHit:
    case InstanceOfMiss:
        func(static_cast<InstanceOfAccessCase*>(this));
        break;

    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case IndexedProxyObjectLoad:
        func(static_cast<ProxyObjectAccessCase*>(this));
        break;
    }
}

#if ASSERT_ENABLED
void AccessCase::checkConsistency(StructureStubInfo& stubInfo)
{
    RELEASE_ASSERT(!(requiresInt32PropertyCheck() && requiresIdentifierNameMatch()));

    if (stubInfo.hasConstantIdentifier) {
        RELEASE_ASSERT(!requiresInt32PropertyCheck());
        RELEASE_ASSERT(requiresIdentifierNameMatch());
    }
}
#endif // ASSERT_ENABLED

bool AccessCase::canBeShared(const AccessCase& lhs, const AccessCase& rhs)
{
    // We do not care m_state.
    // And we say "false" if either of them have m_polyProtoAccessChain.
    if (lhs.m_polyProtoAccessChain || rhs.m_polyProtoAccessChain)
        return false;
    if (lhs.additionalSet() || rhs.additionalSet())
        return false;

    if (lhs.m_type != rhs.m_type)
        return false;
    if (lhs.m_offset != rhs.m_offset)
        return false;
    if (lhs.m_viaGlobalProxy != rhs.m_viaGlobalProxy)
        return false;
    if (lhs.m_structureID.get() != rhs.m_structureID.get())
        return false;
    if (lhs.m_identifier != rhs.m_identifier)
        return false;
    if (lhs.m_conditionSet != rhs.m_conditionSet)
        return false;

    switch (lhs.m_type) {
    case Load:
    case LoadMegamorphic:
    case StoreMegamorphic:
    case Transition:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace:
    case Miss:
    case GetGetter:
    case InHit:
    case InMiss:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad:
    case IndexedScopedArgumentsLoad:
    case IndexedDirectArgumentsLoad:
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat32Load:
    case IndexedResizableTypedArrayFloat64Load:
    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore:
    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case InstanceOfGeneric:
        return true;

    case Getter:
    case Setter:
    case ProxyObjectHas:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case IndexedProxyObjectLoad: {
        // Getter / Setter / ProxyObjectHas / ProxyObjectLoad / ProxyObjectStore / IndexedProxyObjectLoad rely on CodeBlock, which makes sharing impossible.
        return false;
    }

    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter: {
        // They are embedding JSGlobalObject that are not tied to sharing JITStubRoutine.
        return false;
    }

    case IntrinsicGetter: {
        auto& lhsd = lhs.as<IntrinsicGetterAccessCase>();
        auto& rhsd = rhs.as<IntrinsicGetterAccessCase>();
        return lhsd.m_intrinsicFunction == rhsd.m_intrinsicFunction;
    }

    case ModuleNamespaceLoad: {
        auto& lhsd = lhs.as<ModuleNamespaceAccessCase>();
        auto& rhsd = rhs.as<ModuleNamespaceAccessCase>();
        return lhsd.m_moduleNamespaceObject == rhsd.m_moduleNamespaceObject
            && lhsd.m_moduleEnvironment == rhsd.m_moduleEnvironment
            && lhsd.m_scopeOffset == rhsd.m_scopeOffset;
    }

    case InstanceOfHit:
    case InstanceOfMiss: {
        auto& lhsd = lhs.as<InstanceOfAccessCase>();
        auto& rhsd = rhs.as<InstanceOfAccessCase>();
        return lhsd.m_prototype == rhsd.m_prototype;
    }
    }

    return true;
}

void AccessCase::operator delete(AccessCase* accessCase, std::destroying_delete_t)
{
    accessCase->runWithDowncast([](auto* accessCase) {
        std::destroy_at(accessCase);
        std::decay_t<decltype(*accessCase)>::freeAfterDestruction(accessCase);
    });
}

Ref<AccessCase> AccessCase::clone() const
{
    RefPtr<AccessCase> result;
    const_cast<AccessCase*>(this)->runWithDowncast([&](auto* accessCase) {
        result = accessCase->cloneImpl();
    });
    return result.releaseNonNull();
}

WatchpointSet* AccessCase::additionalSet() const
{
    WatchpointSet* result = nullptr;
    const_cast<AccessCase*>(this)->runWithDowncast([&](auto* accessCase) {
        result = accessCase->additionalSetImpl();
    });
    return result;
}

bool AccessCase::hasAlternateBase() const
{
    bool result = false;
    const_cast<AccessCase*>(this)->runWithDowncast([&](auto* accessCase) {
        result = accessCase->hasAlternateBaseImpl();
    });
    return result;
}

JSObject* AccessCase::alternateBase() const
{
    JSObject* result = nullptr;
    const_cast<AccessCase*>(this)->runWithDowncast([&](auto* accessCase) {
        result = accessCase->alternateBaseImpl();
    });
    return result;
}

} // namespace JSC

#endif
