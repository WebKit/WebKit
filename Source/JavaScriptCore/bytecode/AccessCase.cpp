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
#include "InstanceOfAccessCase.h"
#include "IntrinsicGetterAccessCase.h"
#include "JSCInlines.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "ModuleNamespaceAccessCase.h"
#include "PolymorphicAccess.h"
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
    case ProxyObjectLoad:
    case Replace:
    case InstanceOfGeneric:
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
        RELEASE_ASSERT(!prototypeAccessChain);
        break;
    default:
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

Ref<AccessCase> AccessCase::createReplace(VM& vm, JSCell* owner, CacheableIdentifier identifier, PropertyOffset offset, Structure* oldStructure, bool viaProxy)
{
    auto result = adoptRef(*new AccessCase(vm, owner, Replace, identifier, offset, oldStructure, { }, { }));
    result->m_viaProxy = viaProxy;
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
    if (viaProxy())
        return false;

    if (m_polyProtoAccessChain)
        return false;

    switch (m_type) {
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case ProxyObjectLoad:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
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
        return false;
    case IndexedNoIndexingMiss:
    default:
        return true;
    }
}

bool AccessCase::requiresIdentifierNameMatch() const
{
    switch (m_type) {
    case Load:
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
    case ProxyObjectLoad:
    case CheckPrivateBrand:
    case SetPrivateBrand:
        return true;
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
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
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool AccessCase::requiresInt32PropertyCheck() const
{
    switch (m_type) {
    case Load:
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
    case ProxyObjectLoad:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
    case CheckPrivateBrand:
    case SetPrivateBrand:
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
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool AccessCase::needsScratchFPR() const
{
    switch (m_type) {
    case Load:
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
    case ProxyObjectLoad:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
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
        return false;
    case IndexedDoubleLoad:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedTypedArrayUint32Load:
    case IndexedDoubleStore:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
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
    case ProxyObjectLoad: {
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
    case ProxyObjectLoad:
        doesCalls = true;
        break;
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Load:
    case Miss:
    case GetGetter:
    case IntrinsicGetter:
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
        doesCalls = false;
        break;
    case Replace:
        doesCalls = viaProxy();
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

    if (viaProxy() != other.viaProxy())
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
    case ProxyObjectLoad:
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

void AccessCase::generateWithGuard(
    AccessGenerationState& state, CCallHelpers::JumpList& fallThrough)
{
    SuperSamplerScope superSamplerScope(false);

    checkConsistency(*state.stubInfo);

    RELEASE_ASSERT(m_state == Committed);
    m_state = Generated;

    JSGlobalObject* globalObject = state.m_globalObject;
    CCallHelpers& jit = *state.jit;
    StructureStubInfo& stubInfo = *state.stubInfo;
    VM& vm = state.m_vm;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg scratchGPR = state.scratchGPR;

    if (requiresIdentifierNameMatch() && !stubInfo.hasConstantIdentifier) {
        RELEASE_ASSERT(m_identifier);
        GPRReg propertyGPR = stubInfo.propertyGPR();
        // non-rope string check done inside polymorphic access.

        if (uid()->isSymbol())
            jit.loadPtr(MacroAssembler::Address(propertyGPR, Symbol::offsetOfSymbolImpl()), scratchGPR);
        else
            jit.loadPtr(MacroAssembler::Address(propertyGPR, JSString::offsetOfValue()), scratchGPR);
        fallThrough.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImmPtr(uid())));
    }

    auto emitDefaultGuard = [&] () {
        if (m_polyProtoAccessChain) {
            ASSERT(!viaProxy());
            GPRReg baseForAccessGPR = state.scratchGPR;
            jit.move(baseGPR, baseForAccessGPR);
            m_polyProtoAccessChain->forEach(vm, structure(), [&] (Structure* structure, bool atEnd) {
                fallThrough.append(
                    jit.branchStructure(
                        CCallHelpers::NotEqual,
                        CCallHelpers::Address(baseForAccessGPR, JSCell::structureIDOffset()),
                        structure));
                if (atEnd) {
                    if ((m_type == Miss || m_type == InMiss || m_type == Transition) && structure->hasPolyProto()) {
                        // For a Miss/InMiss/Transition, we must ensure we're at the end when the last item is poly proto.
                        // Transitions must do this because they need to verify there isn't a setter in the chain.
                        // Miss/InMiss need to do this to ensure there isn't a new item at the end of the chain that
                        // has the property.
#if USE(JSVALUE64)
                        jit.load64(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset)), baseForAccessGPR);
                        fallThrough.append(jit.branch64(CCallHelpers::NotEqual, baseForAccessGPR, CCallHelpers::TrustedImm64(JSValue::ValueNull)));
#else
                        jit.load32(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset), baseForAccessGPR);
                        fallThrough.append(jit.branchTestPtr(CCallHelpers::NonZero, baseForAccessGPR));
#endif
                    }
                } else {
                    if (structure->hasMonoProto()) {
                        JSValue prototype = structure->prototypeForLookup(globalObject);
                        RELEASE_ASSERT(prototype.isObject());
                        jit.move(CCallHelpers::TrustedImmPtr(asObject(prototype)), baseForAccessGPR);
                    } else {
                        RELEASE_ASSERT(structure->isObject()); // Primitives must have a stored prototype. We use prototypeForLookup for them.
#if USE(JSVALUE64)
                        jit.load64(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset)), baseForAccessGPR);
                        fallThrough.append(jit.branch64(CCallHelpers::Equal, baseForAccessGPR, CCallHelpers::TrustedImm64(JSValue::ValueNull)));
#else
                        jit.load32(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset), baseForAccessGPR);
                        fallThrough.append(jit.branchTestPtr(CCallHelpers::Zero, baseForAccessGPR));
#endif
                    }
                }
            });
            return;
        }
        
        if (viaProxy()) {
            fallThrough.append(
                jit.branchIfNotType(baseGPR, PureForwardingProxyType));
            
            jit.loadPtr(CCallHelpers::Address(baseGPR, JSProxy::targetOffset()), scratchGPR);
            
            fallThrough.append(
                jit.branchStructure(
                    CCallHelpers::NotEqual,
                    CCallHelpers::Address(scratchGPR, JSCell::structureIDOffset()),
                    structure()));
            return;
        }
        
        fallThrough.append(
            jit.branchStructure(
                CCallHelpers::NotEqual,
                CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()),
                structure()));
    };
    
    switch (m_type) {
    case ArrayLength: {
        ASSERT(!viaProxy());
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        fallThrough.append(
            jit.branchTest32(
                CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(IsArray)));
        fallThrough.append(
            jit.branchTest32(
                CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(IndexingShapeMask)));
        break;
    }

    case StringLength: {
        ASSERT(!viaProxy());
        fallThrough.append(
            jit.branchIfNotString(baseGPR));
        break;
    }

    case DirectArgumentsLength: {
        ASSERT(!viaProxy());
        fallThrough.append(
            jit.branchIfNotType(baseGPR, DirectArgumentsType));

        fallThrough.append(
            jit.branchTestPtr(
                CCallHelpers::NonZero,
                CCallHelpers::Address(baseGPR, DirectArguments::offsetOfMappedArguments())));
        jit.load32(
            CCallHelpers::Address(baseGPR, DirectArguments::offsetOfLength()),
            valueRegs.payloadGPR());
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        state.succeed();
        return;
    }

    case ScopedArgumentsLength: {
        ASSERT(!viaProxy());
        fallThrough.append(
            jit.branchIfNotType(baseGPR, ScopedArgumentsType));

        fallThrough.append(
            jit.branchTest8(
                CCallHelpers::NonZero,
                CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfOverrodeThings())));
        jit.load32(
            CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTotalLength()),
            valueRegs.payloadGPR());
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        state.succeed();
        return;
    }

    case ModuleNamespaceLoad: {
        ASSERT(!viaProxy());
        this->as<ModuleNamespaceAccessCase>().emit(state, fallThrough);
        return;
    }

    case ProxyObjectLoad: {
        ASSERT(!viaProxy());
        this->as<ProxyObjectAccessCase>().emit(state, fallThrough);
        return;
    }

    case IndexedScopedArgumentsLoad: {
        ASSERT(!viaProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = stubInfo.propertyGPR();

        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ScopedArgumentsType)));

        auto allocator = state.makeDefaultScratchAllocator(scratchGPR);
        
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        
        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList failAndIgnore;

        failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTotalLength())));
        
        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTable()), scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, ScopedArgumentsTable::offsetOfLength()), scratch2GPR);
        auto overflowCase = jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratch2GPR);

        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfScope()), scratch2GPR);
        jit.loadPtr(CCallHelpers::Address(scratchGPR, ScopedArgumentsTable::offsetOfArguments()), scratchGPR);
        jit.zeroExtend32ToWord(propertyGPR, scratch3GPR);
        jit.load32(CCallHelpers::BaseIndex(scratchGPR, scratch3GPR, CCallHelpers::TimesFour), scratchGPR);
        failAndIgnore.append(jit.branch32(CCallHelpers::Equal, scratchGPR, CCallHelpers::TrustedImm32(ScopeOffset::invalidOffset)));
        jit.loadValue(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight, JSLexicalEnvironment::offsetOfVariables()), valueRegs);
        auto done = jit.jump();

        overflowCase.link(&jit);
        jit.sub32(propertyGPR, scratch2GPR);
        jit.neg32(scratch2GPR);
        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfStorage()), scratch3GPR);
#if USE(JSVALUE64)
        jit.loadValue(CCallHelpers::BaseIndex(scratch3GPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratchGPR));
        failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
        jit.move(scratchGPR, valueRegs.payloadGPR());
#else
        jit.loadValue(CCallHelpers::BaseIndex(scratch3GPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratch2GPR, scratchGPR));
        failAndIgnore.append(jit.branchIfEmpty(scratch2GPR));
        jit.move(scratchGPR, valueRegs.payloadGPR());
        jit.move(scratch2GPR, valueRegs.tagGPR());
#endif

        done.link(&jit);

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();

        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            state.failAndIgnore.append(jit.jump());
        } else
            state.failAndIgnore.append(failAndIgnore);

        return;
    }

    case IndexedDirectArgumentsLoad: {
        ASSERT(!viaProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = stubInfo.propertyGPR();
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(DirectArgumentsType)));
        
        jit.load32(CCallHelpers::Address(baseGPR, DirectArguments::offsetOfLength()), scratchGPR);
        state.failAndRepatch.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratchGPR));
        state.failAndRepatch.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::Address(baseGPR, DirectArguments::offsetOfMappedArguments())));
        jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
        jit.loadValue(CCallHelpers::BaseIndex(baseGPR, scratchGPR, CCallHelpers::TimesEight, DirectArguments::storageOffset()), valueRegs);
        state.succeed();
        return;
    }

    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load: {
        ASSERT(!viaProxy());
        // This code is written such that the result could alias with the base or the property.

        TypedArrayType type = toTypedArrayType(m_type);

        GPRReg propertyGPR = stubInfo.propertyGPR();

        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(typeForTypedArrayType(type))));

        CCallHelpers::Address addressOfLength = CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength());
        jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
        // The length is a size_t, so either 32 or 64 bits depending on the platform.
        state.failAndRepatch.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, addressOfLength));
#else
        state.failAndRepatch.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, addressOfLength));
#endif

        auto allocator = state.makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(addressOfLength, scratchGPR);
#else
        jit.load32(addressOfLength, scratchGPR);
#endif
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfVector()), scratch2GPR);
        jit.cageConditionallyAndUntag(Gigacage::Primitive, scratch2GPR, scratchGPR, scratchGPR, false);
        jit.signExtend32ToPtr(propertyGPR, scratchGPR);
        if (isInt(type)) {
            switch (elementSize(type)) {
            case 1:
                if (JSC::isSigned(type))
                    jit.load8SignedExtendTo32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), valueRegs.payloadGPR());
                else
                    jit.load8(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), valueRegs.payloadGPR());
                break;
            case 2:
                if (JSC::isSigned(type))
                    jit.load16SignedExtendTo32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo), valueRegs.payloadGPR());
                else
                    jit.load16(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo), valueRegs.payloadGPR());
                break;
            case 4:
                jit.load32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour), valueRegs.payloadGPR());
                break;
            default:
                CRASH();
            }

            CCallHelpers::Jump done;
            if (type == TypeUint32) {
                RELEASE_ASSERT(state.scratchFPR != InvalidFPRReg);
                auto canBeInt = jit.branch32(CCallHelpers::GreaterThanOrEqual, valueRegs.payloadGPR(), CCallHelpers::TrustedImm32(0));
                
                jit.convertInt32ToDouble(valueRegs.payloadGPR(), state.scratchFPR);
                jit.addDouble(CCallHelpers::AbsoluteAddress(&CCallHelpers::twoToThe32), state.scratchFPR);
                jit.boxDouble(state.scratchFPR, valueRegs);
                done = jit.jump();
                canBeInt.link(&jit);
            }

            jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
            if (done.isSet())
                done.link(&jit);
        } else {
            ASSERT(isFloat(type));
            RELEASE_ASSERT(state.scratchFPR != InvalidFPRReg);
            switch (elementSize(type)) {
            case 4:
                jit.loadFloat(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour), state.scratchFPR);
                jit.convertFloatToDouble(state.scratchFPR, state.scratchFPR);
                break;
            case 8: {
                jit.loadDouble(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight), state.scratchFPR);
                break;
            }
            default:
                CRASH();
            }

            jit.purifyNaN(state.scratchFPR);
            jit.boxDouble(state.scratchFPR, valueRegs);
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();

        return;
    }

    case IndexedStringLoad: {
        ASSERT(!viaProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = stubInfo.propertyGPR();

        fallThrough.append(jit.branchIfNotString(baseGPR));

        auto allocator = state.makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        CCallHelpers::JumpList failAndIgnore;

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        jit.loadPtr(CCallHelpers::Address(baseGPR, JSString::offsetOfValue()), scratch2GPR);
        failAndIgnore.append(jit.branchIfRopeStringImpl(scratch2GPR));
        jit.load32(CCallHelpers::Address(scratch2GPR, StringImpl::lengthMemoryOffset()), scratchGPR);

        failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratchGPR));

        jit.load32(CCallHelpers::Address(scratch2GPR, StringImpl::flagsOffset()), scratchGPR);
        jit.loadPtr(CCallHelpers::Address(scratch2GPR, StringImpl::dataOffset()), scratch2GPR);
        auto is16Bit = jit.branchTest32(CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(StringImpl::flagIs8Bit()));
        jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
        jit.load8(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne, 0), scratch2GPR);
        auto is8BitLoadDone = jit.jump();
        is16Bit.link(&jit);
        jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
        jit.load16(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo, 0), scratch2GPR);
        is8BitLoadDone.link(&jit);

        failAndIgnore.append(jit.branch32(CCallHelpers::Above, scratch2GPR, CCallHelpers::TrustedImm32(maxSingleCharacterString)));
        jit.move(CCallHelpers::TrustedImmPtr(vm.smallStrings.singleCharacterStrings()), scratchGPR);
        jit.loadPtr(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::ScalePtr, 0), valueRegs.payloadGPR());
        jit.boxCell(valueRegs.payloadGPR(), valueRegs);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();

        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            state.failAndIgnore.append(jit.jump());
        } else
            state.failAndIgnore.append(failAndIgnore);

        return;
    }

    case IndexedNoIndexingMiss: {
        emitDefaultGuard();
        GPRReg propertyGPR = stubInfo.propertyGPR();
        state.failAndIgnore.append(jit.branch32(CCallHelpers::LessThan, propertyGPR, CCallHelpers::TrustedImm32(0)));
        break;
    }

    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad: {
        ASSERT(!viaProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = stubInfo.propertyGPR();

        // int32 check done in polymorphic access.
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        jit.and32(CCallHelpers::TrustedImm32(IndexingShapeMask), scratchGPR);

        auto allocator = state.makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratch2GPR = allocator.allocateScratchGPR();
#if USE(JSVALUE32_64)
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
#endif
        ScratchRegisterAllocator::PreservedState preservedState;

        CCallHelpers::JumpList failAndIgnore;
        auto preserveReusedRegisters = [&] {
            preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        };

        if (m_type == IndexedArrayStorageLoad) {
            jit.add32(CCallHelpers::TrustedImm32(-ArrayStorageShape), scratchGPR, scratchGPR);
            fallThrough.append(jit.branch32(CCallHelpers::Above, scratchGPR, CCallHelpers::TrustedImm32(SlowPutArrayStorageShape - ArrayStorageShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, ArrayStorage::vectorLengthOffset())));

            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
#if USE(JSVALUE64)
            jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs(scratchGPR));
            failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
            jit.move(scratchGPR, valueRegs.payloadGPR());
#else
            jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs(scratch3GPR, scratchGPR));
            failAndIgnore.append(jit.branchIfEmpty(scratch3GPR));
            jit.move(scratchGPR, valueRegs.payloadGPR());
            jit.move(scratch3GPR, valueRegs.tagGPR());
#endif
        } else {
            IndexingType expectedShape;
            switch (m_type) {
            case IndexedInt32Load:
                expectedShape = Int32Shape;
                break;
            case IndexedDoubleLoad:
                expectedShape = DoubleShape;
                break;
            case IndexedContiguousLoad:
                expectedShape = ContiguousShape;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(expectedShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength())));
            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
            if (m_type == IndexedDoubleLoad) {
                RELEASE_ASSERT(state.scratchFPR != InvalidFPRReg);
                jit.loadDouble(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), state.scratchFPR);
                failAndIgnore.append(jit.branchIfNaN(state.scratchFPR));
                jit.boxDouble(state.scratchFPR, valueRegs);
            } else {
#if USE(JSVALUE64)
                jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratchGPR));
                failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
                jit.move(scratchGPR, valueRegs.payloadGPR());
#else
                jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratch3GPR, scratchGPR));
                failAndIgnore.append(jit.branchIfEmpty(scratch3GPR));
                jit.move(scratchGPR, valueRegs.payloadGPR());
                jit.move(scratch3GPR, valueRegs.tagGPR());
#endif
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();

        if (allocator.didReuseRegisters() && !failAndIgnore.empty()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            state.failAndIgnore.append(jit.jump());
        } else
            state.failAndIgnore.append(failAndIgnore);
        return;
    }

    case IndexedInt32Store:
    case IndexedDoubleStore:
    case IndexedContiguousStore:
    case IndexedArrayStorageStore: {
        ASSERT(!viaProxy());
        GPRReg propertyGPR = stubInfo.propertyGPR();

        // int32 check done in polymorphic access.
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        fallThrough.append(jit.branchTest32(CCallHelpers::NonZero, scratchGPR, CCallHelpers::TrustedImm32(CopyOnWrite)));
        jit.and32(CCallHelpers::TrustedImm32(IndexingShapeMask), scratchGPR);

        CCallHelpers::JumpList isOutOfBounds;

        auto allocator = state.makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        ScratchRegisterAllocator::PreservedState preservedState;

        CCallHelpers::JumpList failAndIgnore;
        CCallHelpers::JumpList failAndRepatch;
        auto preserveReusedRegisters = [&] {
            preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        };

        CCallHelpers::Label storeResult;
        if (m_type == IndexedArrayStorageStore) {
            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ArrayStorageShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, ArrayStorage::vectorLengthOffset())));

            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);

#if USE(JSVALUE64)
            isOutOfBounds.append(jit.branchTest64(CCallHelpers::Zero, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset())));
#else
            isOutOfBounds.append(jit.branch32(CCallHelpers::Equal, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset() + JSValue::offsetOfTag()), CCallHelpers::TrustedImm32(JSValue::EmptyValueTag)));
#endif

            storeResult = jit.label();
            jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()));
        } else {
            IndexingType expectedShape;
            switch (m_type) {
            case IndexedInt32Store:
                expectedShape = Int32Shape;
                break;
            case IndexedDoubleStore:
                expectedShape = DoubleShape;
                break;
            case IndexedContiguousStore:
                expectedShape = ContiguousShape;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(expectedShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            isOutOfBounds.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength())));
            storeResult = jit.label();
            switch (m_type) {
            case IndexedDoubleStore: {
                RELEASE_ASSERT(state.scratchFPR != InvalidFPRReg);
                auto notInt = jit.branchIfNotInt32(valueRegs);
                jit.convertInt32ToDouble(valueRegs.payloadGPR(), state.scratchFPR);
                auto ready = jit.jump();
                notInt.link(&jit);
#if USE(JSVALUE64)
                jit.unboxDoubleWithoutAssertions(valueRegs.payloadGPR(), scratch2GPR, state.scratchFPR);
#else
                jit.unboxDouble(valueRegs, state.scratchFPR);
#endif
                failAndRepatch.append(jit.branchIfNaN(state.scratchFPR));
                ready.link(&jit);

                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                jit.storeDouble(state.scratchFPR, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                break;
            }
            case IndexedInt32Store:
                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                failAndRepatch.append(jit.branchIfNotInt32(valueRegs));
                jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                break;
            case IndexedContiguousStore:
                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                // WriteBarrier must be emitted in the embedder side.
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();

        if (m_type == IndexedArrayStorageStore) {
            isOutOfBounds.link(&jit);
            if (stubInfo.m_arrayProfileGPR != InvalidGPRReg)
                jit.store8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(stubInfo.m_arrayProfileGPR, ArrayProfile::offsetOfMayStoreToHole()));
            jit.add32(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(scratchGPR, ArrayStorage::numValuesInVectorOffset()));
            jit.branch32(CCallHelpers::Below, scratch2GPR, CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset())).linkTo(storeResult, &jit);

            jit.add32(CCallHelpers::TrustedImm32(1), scratch2GPR);
            jit.store32(scratch2GPR, CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset()));
            jit.sub32(CCallHelpers::TrustedImm32(1), scratch2GPR);
            jit.jump().linkTo(storeResult, &jit);
        } else {
            isOutOfBounds.link(&jit);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfVectorLength())));
            if (stubInfo.m_arrayProfileGPR != InvalidGPRReg)
                jit.store8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(stubInfo.m_arrayProfileGPR, ArrayProfile::offsetOfMayStoreToHole()));
            jit.add32(CCallHelpers::TrustedImm32(1), propertyGPR, scratch2GPR);
            jit.store32(scratch2GPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength()));
            jit.jump().linkTo(storeResult, &jit);

        }

        if (allocator.didReuseRegisters()) {
            if (!failAndIgnore.empty()) {
                failAndIgnore.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                state.failAndIgnore.append(jit.jump());
            }
            if (!failAndRepatch.empty()) {
                failAndRepatch.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                state.failAndRepatch.append(jit.jump());
            }
        } else {
            state.failAndIgnore.append(failAndIgnore);
            state.failAndRepatch.append(failAndRepatch);
        }
        return;
    }

    case IndexedTypedArrayInt8Store:
    case IndexedTypedArrayUint8Store:
    case IndexedTypedArrayUint8ClampedStore:
    case IndexedTypedArrayInt16Store:
    case IndexedTypedArrayUint16Store:
    case IndexedTypedArrayInt32Store:
    case IndexedTypedArrayUint32Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store: {
        ASSERT(!viaProxy());
        // This code is written such that the result could alias with the base or the property.

        TypedArrayType type = toTypedArrayType(m_type);

        GPRReg propertyGPR = stubInfo.propertyGPR();

        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(typeForTypedArrayType(type))));

        if (isInt(type))
            state.failAndRepatch.append(jit.branchIfNotInt32(valueRegs));
        else {
            ASSERT(isFloat(type));
            RELEASE_ASSERT(state.scratchFPR != InvalidFPRReg);
            auto doubleCase = jit.branchIfNotInt32(valueRegs);
            jit.convertInt32ToDouble(valueRegs.payloadGPR(), state.scratchFPR);
            auto ready = jit.jump();
            doubleCase.link(&jit);
#if USE(JSVALUE64)
            state.failAndRepatch.append(jit.branchIfNotNumber(valueRegs.payloadGPR()));
            jit.unboxDoubleWithoutAssertions(valueRegs.payloadGPR(), scratchGPR, state.scratchFPR);
#else
            state.failAndRepatch.append(jit.branch32(CCallHelpers::Above, valueRegs.tagGPR(), CCallHelpers::TrustedImm32(JSValue::LowestTag)));
            jit.unboxDouble(valueRegs, state.scratchFPR);
#endif
            ready.link(&jit);
        }

        CCallHelpers::Address addressOfLength = CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength());
        jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
        // The length is a UCPURegister, so either 32 or 64 bits depending on the platform.
        state.failAndRepatch.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, addressOfLength));
#else
        state.failAndRepatch.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, addressOfLength));
#endif

        auto allocator = state.makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(addressOfLength, scratchGPR);
#else
        jit.load32(addressOfLength, scratchGPR);
#endif
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfVector()), scratch2GPR);
        jit.cageConditionallyAndUntag(Gigacage::Primitive, scratch2GPR, scratchGPR, scratchGPR, false);
        jit.signExtend32ToPtr(propertyGPR, scratchGPR);
        if (isInt(type)) {
            if (isClamped(type)) {
                ASSERT(elementSize(type) == 1);
                ASSERT(!JSC::isSigned(type));
                jit.getEffectiveAddress(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), scratch2GPR);
                jit.move(valueRegs.payloadGPR(), scratchGPR);
                auto inBounds = jit.branch32(CCallHelpers::BelowOrEqual, scratchGPR, CCallHelpers::TrustedImm32(0xff));
                auto tooBig = jit.branch32(CCallHelpers::GreaterThan, scratchGPR, CCallHelpers::TrustedImm32(0xff));
                jit.xor32(scratchGPR, scratchGPR);
                auto clamped = jit.jump();
                tooBig.link(&jit);
                jit.move(CCallHelpers::TrustedImm32(0xff), scratchGPR);
                clamped.link(&jit);
                inBounds.link(&jit);
                jit.store8(scratchGPR, CCallHelpers::Address(scratch2GPR));
            } else {
                switch (elementSize(type)) {
                case 1:
                    jit.store8(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne));
                    break;
                case 2:
                    jit.store16(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo));
                    break;
                case 4:
                    jit.store32(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour));
                    break;
                default:
                    CRASH();
                }
            }
        } else {
            ASSERT(isFloat(type));
            RELEASE_ASSERT(state.scratchFPR != InvalidFPRReg);
            switch (elementSize(type)) {
            case 4:
                jit.convertDoubleToFloat(state.scratchFPR, state.scratchFPR);
                jit.storeFloat(state.scratchFPR, CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour));
                break;
            case 8: {
                jit.storeDouble(state.scratchFPR, CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight));
                break;
            }
            default:
                CRASH();
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();
        return;
    }

    case InstanceOfHit:
    case InstanceOfMiss:
        emitDefaultGuard();
        
        fallThrough.append(
            jit.branchPtr(
                CCallHelpers::NotEqual, stubInfo.prototypeGPR(),
                CCallHelpers::TrustedImmPtr(as<InstanceOfAccessCase>().prototype())));
        break;
        
    case InstanceOfGeneric: {
        ASSERT(!viaProxy());
        GPRReg prototypeGPR = stubInfo.prototypeGPR();
        // Legend: value = `base instanceof prototypeGPR`.
        
        GPRReg valueGPR = valueRegs.payloadGPR();
        
        auto allocator = state.makeDefaultScratchAllocator(scratchGPR);
        
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        
        if (!state.stubInfo->prototypeIsKnownObject)
            state.failAndIgnore.append(jit.branchIfNotObject(prototypeGPR));
        
        ScratchRegisterAllocator::PreservedState preservedState =
            allocator.preserveReusedRegistersByPushing(
                jit,
                ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        CCallHelpers::JumpList failAndIgnore;

        jit.move(baseGPR, valueGPR);
        
        CCallHelpers::Label loop(&jit);

#if USE(JSVALUE64)
        JSValueRegs resultRegs(scratch2GPR);
#else
        JSValueRegs resultRegs(scratchGPR, scratch2GPR);
#endif

        jit.emitLoadPrototype(vm, valueGPR, resultRegs, failAndIgnore);
        jit.move(scratch2GPR, valueGPR);
        
        CCallHelpers::Jump isInstance = jit.branchPtr(CCallHelpers::Equal, valueGPR, prototypeGPR);

#if USE(JSVALUE64)
        jit.branchIfCell(JSValueRegs(valueGPR)).linkTo(loop, &jit);
#else
        jit.branchTestPtr(CCallHelpers::NonZero, valueGPR).linkTo(loop, &jit);
#endif
    
        jit.boxBooleanPayload(false, valueGPR);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();
        
        isInstance.link(&jit);
        jit.boxBooleanPayload(true, valueGPR);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();
        
        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            state.failAndIgnore.append(jit.jump());
        } else
            state.failAndIgnore.append(failAndIgnore);
        return;
    }

    case CheckPrivateBrand: {
        emitDefaultGuard();
        state.succeed();
        return;
    }
        
    default:
        emitDefaultGuard();
        break;
    }

    generateImpl(state);
}

void AccessCase::generate(AccessGenerationState& state)
{
    RELEASE_ASSERT(m_state == Committed);
    RELEASE_ASSERT(state.stubInfo->hasConstantIdentifier);
    m_state = Generated;

    checkConsistency(*state.stubInfo);

    generateImpl(state);
}

void AccessCase::generateImpl(AccessGenerationState& state)
{
    SuperSamplerScope superSamplerScope(false);
    if (AccessCaseInternal::verbose)
        dataLog("\n\nGenerating code for: ", *this, "\n");

    ASSERT(m_state == Generated); // We rely on the callers setting this for us.

    CCallHelpers& jit = *state.jit;
    VM& vm = state.m_vm;
    CodeBlock* codeBlock = jit.codeBlock();
    ECMAMode ecmaMode = state.m_ecmaMode;
    StructureStubInfo& stubInfo = *state.stubInfo;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg thisGPR = stubInfo.thisValueIsInExtraGPR() ? stubInfo.thisGPR() : baseGPR;
    GPRReg scratchGPR = state.scratchGPR;

    for (const ObjectPropertyCondition& condition : m_conditionSet) {
        RELEASE_ASSERT(!m_polyProtoAccessChain);

        if (condition.isWatchableAssumingImpurePropertyWatchpoint(PropertyCondition::WatchabilityEffort::EnsureWatchability)) {
            state.installWatchpoint(codeBlock, condition);
            continue;
        }

        // For now, we only allow equivalence when it's watchable.
        RELEASE_ASSERT(condition.condition().kind() != PropertyCondition::Equivalence);

        if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint(Concurrency::MainThread)) {
            // The reason why this cannot happen is that we require that PolymorphicAccess calls
            // AccessCase::generate() only after it has verified that
            // AccessCase::couldStillSucceed() returned true.

            dataLog("This condition is no longer met: ", condition, "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        // We will emit code that has a weak reference that isn't otherwise listed anywhere.
        Structure* structure = condition.object()->structure();
        state.weakStructures.append(structure->id());

        jit.move(CCallHelpers::TrustedImmPtr(condition.object()), scratchGPR);
        state.failAndRepatch.append(
            jit.branchStructure(
                CCallHelpers::NotEqual,
                CCallHelpers::Address(scratchGPR, JSCell::structureIDOffset()),
                structure));
    }

    switch (m_type) {
    case InHit:
    case InMiss:
        jit.boxBoolean(m_type == InHit, valueRegs);
        state.succeed();
        return;

    case Miss:
        jit.moveTrustedValue(jsUndefined(), valueRegs);
        state.succeed();
        return;

    case InstanceOfHit:
    case InstanceOfMiss:
        jit.boxBooleanPayload(m_type == InstanceOfHit, valueRegs.payloadGPR());
        state.succeed();
        return;
        
    case Load:
    case GetGetter:
    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter: {
        GPRReg valueRegsPayloadGPR = valueRegs.payloadGPR();

        Structure* currStructure = hasAlternateBase() ? alternateBase()->structure() : structure();
        if (isValidOffset(m_offset))
            currStructure->startWatchingPropertyForReplacements(vm, offset());

        bool doesPropertyStorageLoads = m_type == Load 
            || m_type == GetGetter
            || m_type == Getter
            || m_type == Setter;

        bool takesPropertyOwnerAsCFunctionArgument = m_type == CustomValueGetter || m_type == CustomValueSetter;

        GPRReg receiverGPR = baseGPR;
        GPRReg propertyOwnerGPR;

        if (m_polyProtoAccessChain) {
            // This isn't pretty, but we know we got here via generateWithGuard,
            // and it left the baseForAccess inside scratchGPR. We could re-derive the base,
            // but it'd require emitting the same code to load the base twice.
            propertyOwnerGPR = scratchGPR;
        } else if (hasAlternateBase()) {
            jit.move(
                CCallHelpers::TrustedImmPtr(alternateBase()), scratchGPR);
            propertyOwnerGPR = scratchGPR;
        } else if (viaProxy() && doesPropertyStorageLoads) {
            // We only need this when loading an inline or out of line property. For customs accessors,
            // we can invoke with a receiver value that is a JSProxy. For custom values, we unbox to the
            // JSProxy's target. For getters/setters, we'll also invoke them with the JSProxy as |this|,
            // but we need to load the actual GetterSetter cell from the JSProxy's target.

            if (m_type == Getter || m_type == Setter)
                propertyOwnerGPR = scratchGPR;
            else
                propertyOwnerGPR = valueRegsPayloadGPR;

            jit.loadPtr(
                CCallHelpers::Address(baseGPR, JSProxy::targetOffset()), propertyOwnerGPR);
        } else if (viaProxy() && takesPropertyOwnerAsCFunctionArgument) {
            propertyOwnerGPR = scratchGPR;
            jit.loadPtr(
                CCallHelpers::Address(baseGPR, JSProxy::targetOffset()), propertyOwnerGPR);
        } else
            propertyOwnerGPR = receiverGPR;

        GPRReg loadedValueGPR = InvalidGPRReg;
        if (doesPropertyStorageLoads) {
            if (m_type == Load || m_type == GetGetter)
                loadedValueGPR = valueRegsPayloadGPR;
            else
                loadedValueGPR = scratchGPR;

            ASSERT((m_type != Getter && m_type != Setter) || loadedValueGPR != baseGPR);
            ASSERT(m_type != Setter || loadedValueGPR != valueRegsPayloadGPR);

            GPRReg storageGPR;
            if (isInlineOffset(m_offset))
                storageGPR = propertyOwnerGPR;
            else {
                jit.loadPtr(
                    CCallHelpers::Address(propertyOwnerGPR, JSObject::butterflyOffset()),
                    loadedValueGPR);
                storageGPR = loadedValueGPR;
            }

#if USE(JSVALUE64)
            jit.load64(
                CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset)), loadedValueGPR);
#else
            if (m_type == Load || m_type == GetGetter) {
                jit.loadValue(
                    CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset)),
                    JSValueRegs { valueRegs.tagGPR(), loadedValueGPR });

            } else {
                jit.load32(
                    CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset) + PayloadOffset),
                    loadedValueGPR);
            }
#endif
        }

        if (m_type == Load || m_type == GetGetter) {
            state.succeed();
            return;
        }

        if (m_type == CustomAccessorGetter && this->as<GetterSetterAccessCase>().domAttribute()) {
            auto& access = this->as<GetterSetterAccessCase>();
            // We do not need to emit CheckDOM operation since structure check ensures
            // that the structure of the given base value is structure()! So all we should
            // do is performing the CheckDOM thingy in IC compiling time here.
            if (!structure()->classInfoForCells()->isSubClassOf(access.domAttribute()->classInfo)) {
                state.failAndIgnore.append(jit.jump());
                return;
            }

            if (Options::useDOMJIT() && access.domAttribute()->domJIT) {
                access.emitDOMJITGetter(state, access.domAttribute()->domJIT, receiverGPR);
                return;
            }
        }

        // Stuff for custom getters/setters.
        CCallHelpers::Call operationCall;


        // This also does the necessary calculations of whether or not we're an
        // exception handling call site.
        AccessGenerationState::SpillState spillState = state.preserveLiveRegistersToStackForCall();

        auto restoreLiveRegistersFromStackForCall = [&](AccessGenerationState::SpillState& spillState, bool callHasReturnValue) {
            RegisterSet dontRestore;
            if (callHasReturnValue) {
                // This is the result value. We don't want to overwrite the result with what we stored to the stack.
                // We sometimes have to store it to the stack just in case we throw an exception and need the original value.
                dontRestore.add(valueRegs, IgnoreVectors);
            }
            state.restoreLiveRegistersFromStackForCall(spillState, dontRestore);
        };

        jit.store32(
            CCallHelpers::TrustedImm32(state.callSiteIndexForExceptionHandlingOrOriginal().bits()),
            CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

        if (m_type == Getter || m_type == Setter) {
            auto& access = this->as<GetterSetterAccessCase>();
            ASSERT(baseGPR != loadedValueGPR);
            ASSERT(m_type != Setter || valueRegsPayloadGPR != loadedValueGPR);

            JSGlobalObject* globalObject = state.m_globalObject;

            // Create a JS call using a JS call inline cache. Assume that:
            //
            // - SP is aligned and represents the extent of the calling compiler's stack usage.
            //
            // - FP is set correctly (i.e. it points to the caller's call frame header).
            //
            // - SP - FP is an aligned difference.
            //
            // - Any byte between FP (exclusive) and SP (inclusive) could be live in the calling
            //   code.
            //
            // Therefore, we temporarily grow the stack for the purpose of the call and then
            // shrink it after.

            state.setSpillStateForJSCall(spillState);

            RELEASE_ASSERT(!access.callLinkInfo());
            auto* callLinkInfo = state.m_callLinkInfos.add(stubInfo.codeOrigin, codeBlock->useDataIC() ? CallLinkInfo::UseDataIC::Yes : CallLinkInfo::UseDataIC::No);
            access.m_callLinkInfo = callLinkInfo;

            // FIXME: If we generated a polymorphic call stub that jumped back to the getter
            // stub, which then jumped back to the main code, then we'd have a reachability
            // situation that the GC doesn't know about. The GC would ensure that the polymorphic
            // call stub stayed alive, and it would ensure that the main code stayed alive, but
            // it wouldn't know that the getter stub was alive. Ideally JIT stub routines would
            // be GC objects, and then we'd be able to say that the polymorphic call stub has a
            // reference to the getter stub.
            // https://bugs.webkit.org/show_bug.cgi?id=148914
            callLinkInfo->disallowStubs();

            callLinkInfo->setUpCall(CallLinkInfo::Call, loadedValueGPR);

            CCallHelpers::JumpList done;

            // There is a "this" argument.
            unsigned numberOfParameters = 1;
            // ... and a value argument if we're calling a setter.
            if (m_type == Setter)
                numberOfParameters++;

            // Get the accessor; if there ain't one then the result is jsUndefined().
            // Note that GetterSetter always has cells for both. If it is not set (like, getter exits, but setter is not set), Null{Getter,Setter}Function is stored.
            std::optional<CCallHelpers::Jump> returnUndefined;
            if (m_type == Setter) {
                jit.loadPtr(
                    CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfSetter()),
                    loadedValueGPR);
                if (ecmaMode.isStrict()) {
                    CCallHelpers::Jump shouldNotThrowError = jit.branchIfNotType(loadedValueGPR, NullSetterFunctionType);
                    // We replace setter with this AccessCase's JSGlobalObject::nullSetterStrictFunction, which will throw an error with the right JSGlobalObject.
                    jit.move(CCallHelpers::TrustedImmPtr(globalObject->nullSetterStrictFunction()), loadedValueGPR);
                    shouldNotThrowError.link(&jit);
                }
            } else {
                jit.loadPtr(
                    CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfGetter()),
                    loadedValueGPR);
                returnUndefined = jit.branchIfType(loadedValueGPR, NullSetterFunctionType);
            }

            unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
            ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
            unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);

            unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf(stackAlignmentBytes(), numberOfBytesForCall);

            jit.subPtr(
                CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall),
                CCallHelpers::stackPointerRegister);

            CCallHelpers::Address calleeFrame = CCallHelpers::Address(
                CCallHelpers::stackPointerRegister,
                -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

            jit.store32(
                CCallHelpers::TrustedImm32(numberOfParameters),
                calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));

            jit.storeCell(
                loadedValueGPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));

            jit.storeCell(
                thisGPR,
                calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));

            if (m_type == Setter) {
                jit.storeValue(
                    valueRegs,
                    calleeFrame.withOffset(
                        virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));
            }

            auto slowCase = CallLinkInfo::emitFastPath(jit, access.callLinkInfo(), loadedValueGPR, loadedValueGPR == GPRInfo::regT2 ? GPRInfo::regT0 : GPRInfo::regT2);
            auto doneLocation = jit.label();

            if (m_type == Getter)
                jit.setupResults(valueRegs);
            done.append(jit.jump());

            slowCase.link(&jit);
            auto slowPathStart = jit.label();
            jit.move(loadedValueGPR, GPRInfo::regT0);
#if USE(JSVALUE32_64)
            // We *always* know that the getter/setter, if non-null, is a cell.
            jit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), GPRInfo::regT1);
#endif
            jit.move(CCallHelpers::TrustedImmPtr(globalObject), GPRInfo::regT3);
            access.callLinkInfo()->emitSlowPath(vm, jit);

            if (m_type == Getter)
                jit.setupResults(valueRegs);
            done.append(jit.jump());

            if (returnUndefined) {
                ASSERT(m_type == Getter);
                returnUndefined.value().link(&jit);
                jit.moveTrustedValue(jsUndefined(), valueRegs);
            }
            done.link(&jit);

            int stackPointerOffset = (codeBlock->stackPointerOffset() * sizeof(Register)) - state.preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation;
            jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

            bool callHasReturnValue = isGetter();
            restoreLiveRegistersFromStackForCall(spillState, callHasReturnValue);

            jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
                this->as<GetterSetterAccessCase>().callLinkInfo()->setCodeLocations(
                    linkBuffer.locationOf<JSInternalPtrTag>(slowPathStart),
                    linkBuffer.locationOf<JSInternalPtrTag>(doneLocation));
            });
        } else {
            ASSERT(m_type == CustomValueGetter || m_type == CustomAccessorGetter || m_type == CustomValueSetter || m_type == CustomAccessorSetter);
            ASSERT(!doesPropertyStorageLoads); // Or we need an extra register. We rely on propertyOwnerGPR being correct here.

            // We do not need to keep globalObject alive since
            // 1. if it is CustomValue, the owner CodeBlock (even if JSGlobalObject* is one of CodeBlock that is inlined and held by DFG CodeBlock) must keep it alive.
            // 2. if it is CustomAccessor, structure should hold it.
            JSGlobalObject* globalObject = currStructure->globalObject();

            // Need to make room for the C call so any of our stack spillage isn't overwritten. It's
            // hard to track if someone did spillage or not, so we just assume that we always need
            // to make some space here.
            jit.makeSpaceOnStackForCCall();

            // Check if it is a super access
            GPRReg receiverForCustomGetGPR = baseGPR != thisGPR ? thisGPR : receiverGPR;

            // getter: EncodedJSValue (*GetValueFunc)(JSGlobalObject*, EncodedJSValue thisValue, PropertyName);
            // setter: bool (*PutValueFunc)(JSGlobalObject*, EncodedJSValue thisObject, EncodedJSValue value, PropertyName);
            // Custom values are passed the slotBase (the property holder), custom accessors are passed the thisValue (receiver).
            GPRReg baseForCustom = takesPropertyOwnerAsCFunctionArgument ? propertyOwnerGPR : receiverForCustomGetGPR; 
            // We do not need to keep globalObject alive since the owner CodeBlock (even if JSGlobalObject* is one of CodeBlock that is inlined and held by DFG CodeBlock)
            // must keep it alive.
            if (m_type == CustomValueGetter || m_type == CustomAccessorGetter) {
                RELEASE_ASSERT(m_identifier);
                if (Options::useJITCage()) {
                    jit.setupArguments<GetValueFuncWithPtr>(
                        CCallHelpers::TrustedImmPtr(globalObject),
                        CCallHelpers::CellValue(baseForCustom),
                        CCallHelpers::TrustedImmPtr(uid()),
                        CCallHelpers::TrustedImmPtr(this->as<GetterSetterAccessCase>().m_customAccessor.taggedPtr()));
                } else {
                    jit.setupArguments<GetValueFunc>(
                        CCallHelpers::TrustedImmPtr(globalObject),
                        CCallHelpers::CellValue(baseForCustom),
                        CCallHelpers::TrustedImmPtr(uid()));
                }
            } else {
                if (Options::useJITCage()) {
                    jit.setupArguments<PutValueFuncWithPtr>(
                        CCallHelpers::TrustedImmPtr(globalObject),
                        CCallHelpers::CellValue(baseForCustom),
                        valueRegs,
                        CCallHelpers::TrustedImmPtr(uid()),
                        CCallHelpers::TrustedImmPtr(this->as<GetterSetterAccessCase>().m_customAccessor.taggedPtr()));
                } else {
                    jit.setupArguments<PutValueFunc>(
                        CCallHelpers::TrustedImmPtr(globalObject),
                        CCallHelpers::CellValue(baseForCustom),
                        valueRegs,
                        CCallHelpers::TrustedImmPtr(uid()));
                }
            }
            jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);

            if (Options::useJITCage())
                operationCall = jit.call(OperationPtrTag);
            else
                operationCall = jit.call(CustomAccessorPtrTag);
            jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
                if (Options::useJITCage()) {
                    if (m_type == CustomValueGetter || m_type == CustomAccessorGetter)
                        linkBuffer.link<OperationPtrTag>(operationCall, vmEntryCustomGetter);
                    else
                        linkBuffer.link<OperationPtrTag>(operationCall, vmEntryCustomSetter);
                } else
                    linkBuffer.link(operationCall, this->as<GetterSetterAccessCase>().m_customAccessor);
            });

            if (m_type == CustomValueGetter || m_type == CustomAccessorGetter)
                jit.setupResults(valueRegs);
            jit.reclaimSpaceOnStackForCCall();

            CCallHelpers::Jump noException =
            jit.emitExceptionCheck(vm, CCallHelpers::InvertedExceptionCheck);

            state.restoreLiveRegistersFromStackForCallWithThrownException(spillState);
            state.emitExplicitExceptionHandler();

            noException.link(&jit);
            bool callHasReturnValue = isGetter();
            restoreLiveRegistersFromStackForCall(spillState, callHasReturnValue);
        }
        state.succeed();
        return;
    }

    case Replace: {
        GPRReg base = baseGPR;
        if (viaProxy()) {
            // This aint pretty, but the path that structure checks loads the real base into scratchGPR.
            base = scratchGPR;
        }

        if (isInlineOffset(m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    base,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(m_offset) * sizeof(JSValue)));
        } else {
            jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    scratchGPR, offsetInButterfly(m_offset) * sizeof(JSValue)));
        }

        if (viaProxy()) {
            CCallHelpers::JumpList skipBarrier;
            skipBarrier.append(jit.branchIfNotCell(valueRegs));
            if (!isInlineOffset(m_offset))
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSProxy::targetOffset()), scratchGPR);
            skipBarrier.append(jit.barrierBranch(vm, scratchGPR, scratchGPR));

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSProxy::targetOffset()), scratchGPR);

            auto spillState = state.preserveLiveRegistersToStackForCallWithoutExceptions();

            jit.setupArguments<decltype(operationWriteBarrierSlowPath)>(CCallHelpers::TrustedImmPtr(&vm), scratchGPR);
            jit.prepareCallOperation(vm);
            auto operationCall = jit.call(OperationPtrTag);
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link<OperationPtrTag>(operationCall, operationWriteBarrierSlowPath);
            });
            state.restoreLiveRegistersFromStackForCall(spillState);

            skipBarrier.link(&jit);
        }

        state.succeed();
        return;
    }

    case Transition: {
        ASSERT(!viaProxy());
        // AccessCase::createTransition() should have returned null if this wasn't true.
        RELEASE_ASSERT(GPRInfo::numberOfRegisters >= 6 || !structure()->outOfLineCapacity() || structure()->outOfLineCapacity() == newStructure()->outOfLineCapacity());

        // NOTE: This logic is duplicated in AccessCase::doesCalls(). It's important that doesCalls() knows
        // exactly when this would make calls.
        bool allocating = newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity();
        bool reallocating = allocating && structure()->outOfLineCapacity();
        bool allocatingInline = allocating && !structure()->couldHaveIndexingHeader();

        auto allocator = state.makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratchGPR2 = InvalidGPRReg;
        GPRReg scratchGPR3 = InvalidGPRReg;
        if (allocatingInline) {
            scratchGPR2 = allocator.allocateScratchGPR();
            scratchGPR3 = allocator.allocateScratchGPR();
        }

        ScratchRegisterAllocator::PreservedState preservedState =
            allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

        CCallHelpers::JumpList slowPath;

        ASSERT(structure()->transitionWatchpointSetHasBeenInvalidated());

        if (allocating) {
            size_t newSize = newStructure()->outOfLineCapacity() * sizeof(JSValue);

            if (allocatingInline) {
                Allocator allocator = vm.jsValueGigacageAuxiliarySpace().allocatorForNonInline(newSize, AllocatorForMode::AllocatorIfExists);

                jit.emitAllocate(scratchGPR, JITAllocator::constant(allocator), scratchGPR2, scratchGPR3, slowPath);
                jit.addPtr(CCallHelpers::TrustedImm32(newSize + sizeof(IndexingHeader)), scratchGPR);

                size_t oldSize = structure()->outOfLineCapacity() * sizeof(JSValue);
                ASSERT(newSize > oldSize);

                if (reallocating) {
                    // Handle the case where we are reallocating (i.e. the old structure/butterfly
                    // already had out-of-line property storage).

                    jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR3);

                    // We have scratchGPR = new storage, scratchGPR3 = old storage,
                    // scratchGPR2 = available
                    for (size_t offset = 0; offset < oldSize; offset += sizeof(void*)) {
                        jit.loadPtr(
                            CCallHelpers::Address(
                                scratchGPR3,
                                -static_cast<ptrdiff_t>(
                                    offset + sizeof(JSValue) + sizeof(void*))),
                            scratchGPR2);
                        jit.storePtr(
                            scratchGPR2,
                            CCallHelpers::Address(
                                scratchGPR,
                                -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))));
                    }
                }

                for (size_t offset = oldSize; offset < newSize; offset += sizeof(void*))
                    jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::Address(scratchGPR, -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))));
            } else {
                // Handle the case where we are allocating out-of-line using an operation.
                RegisterSet extraRegistersToPreserve;
                extraRegistersToPreserve.add(baseGPR, IgnoreVectors);
                extraRegistersToPreserve.add(valueRegs, IgnoreVectors);
                AccessGenerationState::SpillState spillState = state.preserveLiveRegistersToStackForCall(extraRegistersToPreserve);
                
                jit.store32(
                    CCallHelpers::TrustedImm32(
                        state.callSiteIndexForExceptionHandlingOrOriginal().bits()),
                    CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
                
                jit.makeSpaceOnStackForCCall();
                
                if (!reallocating) {
                    jit.setupArguments<decltype(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR);
                    jit.prepareCallOperation(vm);
                    
                    CCallHelpers::Call operationCall = jit.call(OperationPtrTag);
                    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                        linkBuffer.link<OperationPtrTag>(
                            operationCall,
                            operationReallocateButterflyToHavePropertyStorageWithInitialCapacity);
                    });
                } else {
                    // Handle the case where we are reallocating (i.e. the old structure/butterfly
                    // already had out-of-line property storage).
                    jit.setupArguments<decltype(operationReallocateButterflyToGrowPropertyStorage)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR, CCallHelpers::TrustedImm32(newSize / sizeof(JSValue)));
                    jit.prepareCallOperation(vm);
                    
                    CCallHelpers::Call operationCall = jit.call(OperationPtrTag);
                    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                        linkBuffer.link<OperationPtrTag>(
                            operationCall,
                            operationReallocateButterflyToGrowPropertyStorage);
                    });
                }
                
                jit.reclaimSpaceOnStackForCCall();
                jit.move(GPRInfo::returnValueGPR, scratchGPR);
                
                CCallHelpers::Jump noException = jit.emitExceptionCheck(vm, CCallHelpers::InvertedExceptionCheck);
                
                state.restoreLiveRegistersFromStackForCallWithThrownException(spillState);
                state.emitExplicitExceptionHandler();
                
                noException.link(&jit);
                RegisterSet resultRegisterToExclude;
                resultRegisterToExclude.add(scratchGPR, IgnoreVectors);
                state.restoreLiveRegistersFromStackForCall(spillState, resultRegisterToExclude);
            }
        }

        if (isInlineOffset(m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    baseGPR,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(m_offset) * sizeof(JSValue)));
        } else {
            if (!allocating)
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(scratchGPR, offsetInButterfly(m_offset) * sizeof(JSValue)));
        }
        
        if (allocatingInline) {
            // If we were to have any indexed properties, then we would need to update the indexing mask on the base object.
            RELEASE_ASSERT(!newStructure()->couldHaveIndexingHeader());
            // We set the new butterfly and the structure last. Doing it this way ensures that
            // whatever we had done up to this point is forgotten if we choose to branch to slow
            // path.
            jit.nukeStructureAndStoreButterfly(vm, scratchGPR, baseGPR);
        }
        
        uint32_t structureBits = bitwise_cast<uint32_t>(newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));
        
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();
        
        // We will have a slow path if we were allocating without the help of an operation.
        if (allocatingInline) {
            if (allocator.didReuseRegisters()) {
                slowPath.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                state.failAndIgnore.append(jit.jump());
            } else
                state.failAndIgnore.append(slowPath);
        } else
            RELEASE_ASSERT(slowPath.empty());
        return;
    }

    case Delete: {
        ASSERT(structure()->transitionWatchpointSetHasBeenInvalidated());
        ASSERT(newStructure()->transitionKind() == TransitionKind::PropertyDeletion);
        ASSERT(baseGPR != scratchGPR);
        ASSERT(!valueRegs.uses(baseGPR));
        ASSERT(!valueRegs.uses(scratchGPR));

        jit.moveValue(JSValue(), valueRegs);

        if (isInlineOffset(m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    baseGPR,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(m_offset) * sizeof(JSValue)));
        } else {
            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(scratchGPR, offsetInButterfly(m_offset) * sizeof(JSValue)));
        }

        uint32_t structureBits = bitwise_cast<uint32_t>(newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        jit.move(MacroAssembler::TrustedImm32(true), valueRegs.payloadGPR());

        state.succeed();
        return;
    }

    case SetPrivateBrand: {
        ASSERT(structure()->transitionWatchpointSetHasBeenInvalidated());
        ASSERT(newStructure()->transitionKind() == TransitionKind::SetBrand);

        uint32_t structureBits = bitwise_cast<uint32_t>(newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        state.succeed();
        return;
    }

    case DeleteNonConfigurable: {
        jit.move(MacroAssembler::TrustedImm32(false), valueRegs.payloadGPR());
        state.succeed();
        return;
    }

    case DeleteMiss: {
        jit.move(MacroAssembler::TrustedImm32(true), valueRegs.payloadGPR());
        state.succeed();
        return;
    }
        
    case ArrayLength: {
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset()), scratchGPR);
        state.failAndIgnore.append(
            jit.branch32(CCallHelpers::LessThan, scratchGPR, CCallHelpers::TrustedImm32(0)));
        jit.boxInt32(scratchGPR, valueRegs);
        state.succeed();
        return;
    }
        
    case StringLength: {
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSString::offsetOfValue()), scratchGPR);
        auto isRope = jit.branchIfRopeStringImpl(scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, StringImpl::lengthMemoryOffset()), valueRegs.payloadGPR());
        auto done = jit.jump();

        isRope.link(&jit);
        jit.load32(CCallHelpers::Address(baseGPR, JSRopeString::offsetOfLength()), valueRegs.payloadGPR());

        done.link(&jit);
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        state.succeed();
        return;
    }

    case IndexedNoIndexingMiss:
        jit.moveTrustedValue(jsUndefined(), valueRegs);
        state.succeed();
        return;

    case IntrinsicGetter: {
        RELEASE_ASSERT(isValidOffset(offset()));

        // We need to ensure the getter value does not move from under us. Note that GetterSetters
        // are immutable so we just need to watch the property not any value inside it.
        Structure* currStructure;
        if (!hasAlternateBase())
            currStructure = structure();
        else
            currStructure = alternateBase()->structure();
        currStructure->startWatchingPropertyForReplacements(vm, offset());
        
        this->as<IntrinsicGetterAccessCase>().emitIntrinsicGetter(state);
        return;
    }
        
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case ProxyObjectLoad:
    case InstanceOfGeneric:
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
    case IndexedStringLoad:
    case CheckPrivateBrand:
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
        // These need to be handled by generateWithGuard(), since the guard is part of the
        // algorithm. We can be sure that nobody will call generate() directly for these since they
        // are not guarded by structure checks.
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

TypedArrayType AccessCase::toTypedArrayType(AccessType accessType)
{
    switch (accessType) {
    case IndexedTypedArrayInt8Load:
    case IndexedTypedArrayInt8Store:
        return TypeInt8;
    case IndexedTypedArrayUint8Load:
    case IndexedTypedArrayUint8Store:
        return TypeUint8;
    case IndexedTypedArrayUint8ClampedLoad:
    case IndexedTypedArrayUint8ClampedStore:
        return TypeUint8Clamped;
    case IndexedTypedArrayInt16Load:
    case IndexedTypedArrayInt16Store:
        return TypeInt16;
    case IndexedTypedArrayUint16Load:
    case IndexedTypedArrayUint16Store:
        return TypeUint16;
    case IndexedTypedArrayInt32Load:
    case IndexedTypedArrayInt32Store:
        return TypeInt32;
    case IndexedTypedArrayUint32Load:
    case IndexedTypedArrayUint32Store:
        return TypeUint32;
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat32Store:
        return TypeFloat32;
    case IndexedTypedArrayFloat64Load:
    case IndexedTypedArrayFloat64Store:
        return TypeFloat64;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template<typename Func>
inline void AccessCase::runWithDowncast(const Func& func)
{
    switch (m_type) {
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

    case ProxyObjectLoad:
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
    if (lhs.m_viaProxy != rhs.m_viaProxy)
        return false;
    if (lhs.m_structureID.get() != rhs.m_structureID.get())
        return false;
    if (lhs.m_identifier != rhs.m_identifier)
        return false;
    if (lhs.m_conditionSet != rhs.m_conditionSet)
        return false;

    switch (lhs.m_type) {
    case Load:
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
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case InstanceOfGeneric:
        return true;

    case Getter:
    case Setter:
    case ProxyObjectLoad: {
        // Getter, Setter, and ProxyObjectLoad relies on CodeBlock, which makes sharing impossible.
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
