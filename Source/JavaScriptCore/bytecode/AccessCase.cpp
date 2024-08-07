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
#include "ScopedArguments.h"
#include "ScratchRegisterAllocator.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"

namespace JSC {

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
    case InMegamorphic:
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
    case Replace:
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case InstanceOfMegamorphic:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
    case IndexedNoIndexingInMiss:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
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
        RELEASE_ASSERT(hasConstantIdentifier(stubInfo.accessType));
        return ProxyableAccessCase::create(vm, owner, Load, identifier, stubInfo.byIdSelfOffset, stubInfo.inlineAccessBaseStructure());

    case CacheType::PutByIdReplace:
        RELEASE_ASSERT(hasConstantIdentifier(stubInfo.accessType));
        return AccessCase::createReplace(vm, owner, identifier, stubInfo.byIdSelfOffset, stubInfo.inlineAccessBaseStructure(), false);

    case CacheType::InByIdSelf:
        RELEASE_ASSERT(hasConstantIdentifier(stubInfo.accessType));
        return AccessCase::create(vm, owner, InHit, identifier, stubInfo.byIdSelfOffset, stubInfo.inlineAccessBaseStructure());

    case CacheType::ArrayLength:
        RELEASE_ASSERT(hasConstantIdentifier(stubInfo.accessType));
        return AccessCase::create(vm, owner, AccessCase::ArrayLength, CacheableIdentifier::createFromImmortalIdentifier(vm.propertyNames->length.impl()));

    case CacheType::StringLength:
        RELEASE_ASSERT(hasConstantIdentifier(stubInfo.accessType));
        return AccessCase::create(vm, owner, AccessCase::StringLength, CacheableIdentifier::createFromImmortalIdentifier(vm.propertyNames->length.impl()));

    default:
        return nullptr;
    }
}

JSObject* AccessCase::tryGetAlternateBaseImpl() const
{
    switch (m_type) {
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::IntrinsicGetter:
    case AccessCase::Load:
    case AccessCase::GetGetter:
        if (!conditionSet().isEmpty())
            return conditionSet().slotBaseCondition().object();
        return nullptr;
    default:
        return nullptr;
    }
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
    case InMegamorphic:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfMegamorphic:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
        return false;
    case Load:
    case Miss:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace:
    case IndexedNoIndexingMiss:
    case IndexedNoIndexingInMiss:
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
    case InMegamorphic:
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
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case CheckPrivateBrand:
    case SetPrivateBrand:
        return true;
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfMegamorphic:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
    case IndexedNoIndexingInMiss:
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
    case InMegamorphic:
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
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfMegamorphic:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
    case IndexedNoIndexingInMiss:
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
    case InMegamorphic:
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
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case InstanceOfMegamorphic:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
    case IndexedNoIndexingInMiss:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
        break;
    }
}

bool AccessCase::doesCalls(VM&) const
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
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
    case StoreMegamorphic:
    case IndexedMegamorphicStore:
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
    case InMegamorphic:
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
    case InstanceOfMegamorphic:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
    case IndexedNoIndexingInMiss:
        doesCalls = false;
        break;
    case Replace:
        doesCalls = viaGlobalProxy();
        break;
    }
    return doesCalls;
}

bool AccessCase::couldStillSucceed() const
{
    for (const ObjectPropertyCondition& condition : m_conditionSet) {
        if (condition.condition().kind() == PropertyCondition::Equivalence) {
            if (!condition.isWatchableAssumingImpurePropertyWatchpoint(PropertyCondition::WatchabilityEffort::EnsureWatchability, Concurrency::MainThread))
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
    case InMegamorphic:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
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

    case InstanceOfMegamorphic:
        switch (other.type()) {
        case InstanceOfMegamorphic:
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
    case IndexedNoIndexingInMiss:
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
    out.print("\n"_s, m_type, ": {"_s);

    Indenter indent;
    CommaPrinter comma;

    out.print(comma, "ident = '"_s, m_identifier, "'"_s);
    if (isValidOffset(m_offset))
        out.print(comma, "offset = "_s, m_offset);

    ++indent;

    if (m_polyProtoAccessChain) {
        out.print("\n"_s, indent, "prototype access chain = "_s);
        m_polyProtoAccessChain->dump(structure(), out);
    } else {
        if (m_type == Transition || m_type == Delete || m_type == SetPrivateBrand)
            out.print("\n"_s, indent, "from structure = "_s, pointerDump(structure()),
                "\n"_s, indent, "to structure = "_s, pointerDump(newStructure()));
        else if (m_structureID)
            out.print("\n"_s, indent, "structure = "_s, pointerDump(m_structureID.get()));
    }

    if (!m_conditionSet.isEmpty())
        out.print("\n"_s, indent, "conditions = "_s, m_conditionSet);

    const_cast<AccessCase*>(this)->runWithDowncast([&](auto* accessCase) {
        accessCase->dumpImpl(out, comma, indent);
    });

    out.print("}"_s);
}

bool AccessCase::visitWeak(VM& vm) const
{
    bool isValid = true;
    forEachDependentCell(vm, [&](JSCell* cell) {
        isValid &= vm.heap.isMarked(cell);
    });
    return isValid;
}

void AccessCase::collectDependentCells(VM& vm, Vector<JSCell*>& cells) const
{
    forEachDependentCell(vm, [&](JSCell* cell) {
        cells.append(cell);
    });
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
    case InMegamorphic:
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
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
    case IndexedNoIndexingInMiss:
    case InstanceOfMegamorphic:
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
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
    }
}

#if ASSERT_ENABLED
void AccessCase::checkConsistency(StructureStubInfo& stubInfo)
{
    RELEASE_ASSERT(!(requiresInt32PropertyCheck() && requiresIdentifierNameMatch()));

    if (hasConstantIdentifier(stubInfo.accessType)) {
        RELEASE_ASSERT(!requiresInt32PropertyCheck());
        RELEASE_ASSERT(requiresIdentifierNameMatch());
    }
}
#endif // ASSERT_ENABLED

bool AccessCase::canBeShared(const AccessCase& lhs, const AccessCase& rhs)
{
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
    if (lhs.additionalSet() != rhs.additionalSet())
        return false;
    if (lhs.m_polyProtoAccessChain || rhs.m_polyProtoAccessChain) {
        if (!lhs.m_polyProtoAccessChain || !rhs.m_polyProtoAccessChain)
            return false;
        if (*lhs.m_polyProtoAccessChain != *rhs.m_polyProtoAccessChain)
            return false;
    }

    if (lhs.tryGetAlternateBase() != rhs.tryGetAlternateBase())
        return false;

    switch (lhs.m_type) {
    case Load:
    case LoadMegamorphic:
    case StoreMegamorphic:
    case InMegamorphic:
    case Transition:
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Replace:
    case Miss:
    case GetGetter:
    case InHit:
    case InMiss:
    case ProxyObjectIn:
    case ProxyObjectLoad:
    case ProxyObjectStore:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case CheckPrivateBrand:
    case SetPrivateBrand:
    case IndexedMegamorphicLoad:
    case IndexedMegamorphicStore:
    case IndexedMegamorphicIn:
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
    case IndexedTypedArrayFloat16Load:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedResizableTypedArrayInt8Load:
    case IndexedResizableTypedArrayUint8Load:
    case IndexedResizableTypedArrayUint8ClampedLoad:
    case IndexedResizableTypedArrayInt16Load:
    case IndexedResizableTypedArrayUint16Load:
    case IndexedResizableTypedArrayInt32Load:
    case IndexedResizableTypedArrayUint32Load:
    case IndexedResizableTypedArrayFloat16Load:
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
    case IndexedTypedArrayFloat16Store:
    case IndexedTypedArrayFloat32Store:
    case IndexedTypedArrayFloat64Store:
    case IndexedResizableTypedArrayInt8Store:
    case IndexedResizableTypedArrayUint8Store:
    case IndexedResizableTypedArrayUint8ClampedStore:
    case IndexedResizableTypedArrayInt16Store:
    case IndexedResizableTypedArrayUint16Store:
    case IndexedResizableTypedArrayInt32Store:
    case IndexedResizableTypedArrayUint32Store:
    case IndexedResizableTypedArrayFloat16Store:
    case IndexedResizableTypedArrayFloat32Store:
    case IndexedResizableTypedArrayFloat64Store:
    case IndexedStringLoad:
    case IndexedNoIndexingMiss:
    case IndexedInt32InHit:
    case IndexedDoubleInHit:
    case IndexedContiguousInHit:
    case IndexedArrayStorageInHit:
    case IndexedScopedArgumentsInHit:
    case IndexedDirectArgumentsInHit:
    case IndexedTypedArrayInt8InHit:
    case IndexedTypedArrayUint8InHit:
    case IndexedTypedArrayUint8ClampedInHit:
    case IndexedTypedArrayInt16InHit:
    case IndexedTypedArrayUint16InHit:
    case IndexedTypedArrayInt32InHit:
    case IndexedTypedArrayUint32InHit:
    case IndexedTypedArrayFloat16InHit:
    case IndexedTypedArrayFloat32InHit:
    case IndexedTypedArrayFloat64InHit:
    case IndexedResizableTypedArrayInt8InHit:
    case IndexedResizableTypedArrayUint8InHit:
    case IndexedResizableTypedArrayUint8ClampedInHit:
    case IndexedResizableTypedArrayInt16InHit:
    case IndexedResizableTypedArrayUint16InHit:
    case IndexedResizableTypedArrayInt32InHit:
    case IndexedResizableTypedArrayUint32InHit:
    case IndexedResizableTypedArrayFloat16InHit:
    case IndexedResizableTypedArrayFloat32InHit:
    case IndexedResizableTypedArrayFloat64InHit:
    case IndexedStringInHit:
    case IndexedNoIndexingInMiss:
    case InstanceOfMegamorphic:
    case IndexedProxyObjectIn:
    case IndexedProxyObjectLoad:
    case IndexedProxyObjectStore:
        return true;

    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter: {
        auto& lhsd = lhs.as<GetterSetterAccessCase>();
        auto& rhsd = rhs.as<GetterSetterAccessCase>();
        return lhsd.m_customAccessor == rhsd.m_customAccessor;
    }

    case Getter:
    case Setter:
        return true;

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

WatchpointSet* AccessCase::additionalSet() const
{
    WatchpointSet* result = nullptr;
    const_cast<AccessCase*>(this)->runWithDowncast([&](auto* accessCase) {
        result = accessCase->additionalSetImpl();
    });
    return result;
}

JSObject* AccessCase::tryGetAlternateBase() const
{
    JSObject* result = nullptr;
    const_cast<AccessCase*>(this)->runWithDowncast([&](auto* accessCase) {
        result = accessCase->tryGetAlternateBaseImpl();
    });
    return result;
}

} // namespace JSC

#endif
