/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "LinkBuffer.h"
#include "ModuleNamespaceAccessCase.h"
#include "PolymorphicAccess.h"
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

AccessCase::AccessCase(VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier, PropertyOffset offset, Structure* structure, const ObjectPropertyConditionSet& conditionSet, std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain)
    : m_type(type)
    , m_offset(offset)
    , m_polyProtoAccessChain(WTFMove(prototypeAccessChain))
    , m_identifier(identifier)
{
    m_structure.setMayBeNull(vm, owner, structure);
    m_conditionSet = conditionSet;
    RELEASE_ASSERT(m_conditionSet.isValid());
}

std::unique_ptr<AccessCase> AccessCase::create(VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier, PropertyOffset offset, Structure* structure, const ObjectPropertyConditionSet& conditionSet, std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain)
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
        RELEASE_ASSERT(!prototypeAccessChain);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    };

    return std::unique_ptr<AccessCase>(new AccessCase(vm, owner, type, identifier, offset, structure, conditionSet, WTFMove(prototypeAccessChain)));
}

std::unique_ptr<AccessCase> AccessCase::createTransition(
    VM& vm, JSCell* owner, CacheableIdentifier identifier, PropertyOffset offset, Structure* oldStructure, Structure* newStructure,
    const ObjectPropertyConditionSet& conditionSet, std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain)
{
    RELEASE_ASSERT(oldStructure == newStructure->previousID());

    // Skip optimizing the case where we need a realloc, if we don't have
    // enough registers to make it happen.
    if (GPRInfo::numberOfRegisters < 6
        && oldStructure->outOfLineCapacity() != newStructure->outOfLineCapacity()
        && oldStructure->outOfLineCapacity()) {
        return nullptr;
    }

    return std::unique_ptr<AccessCase>(new AccessCase(vm, owner, Transition, identifier, offset, newStructure, conditionSet, WTFMove(prototypeAccessChain)));
}

std::unique_ptr<AccessCase> AccessCase::createDelete(
    VM& vm, JSCell* owner, CacheableIdentifier identifier, PropertyOffset offset, Structure* oldStructure, Structure* newStructure)
{
    RELEASE_ASSERT(oldStructure == newStructure->previousID());
    ASSERT(!newStructure->outOfLineCapacity() || oldStructure->outOfLineCapacity());
    return std::unique_ptr<AccessCase>(new AccessCase(vm, owner, Delete, identifier, offset, newStructure, { }, { }));
}

AccessCase::~AccessCase()
{
}

std::unique_ptr<AccessCase> AccessCase::fromStructureStubInfo(
    VM& vm, JSCell* owner, CacheableIdentifier identifier, StructureStubInfo& stubInfo)
{
    switch (stubInfo.cacheType()) {
    case CacheType::GetByIdSelf:
        RELEASE_ASSERT(stubInfo.hasConstantIdentifier);
        return ProxyableAccessCase::create(vm, owner, Load, identifier, stubInfo.u.byIdSelf.offset, stubInfo.u.byIdSelf.baseObjectStructure.get());

    case CacheType::PutByIdReplace:
        RELEASE_ASSERT(stubInfo.hasConstantIdentifier);
        return AccessCase::create(vm, owner, Replace, identifier, stubInfo.u.byIdSelf.offset, stubInfo.u.byIdSelf.baseObjectStructure.get());

    case CacheType::InByIdSelf:
        RELEASE_ASSERT(stubInfo.hasConstantIdentifier);
        return AccessCase::create(vm, owner, InHit, identifier, stubInfo.u.byIdSelf.offset, stubInfo.u.byIdSelf.baseObjectStructure.get());

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

bool AccessCase::hasAlternateBase() const
{
    return !conditionSet().isEmpty();
}

JSObject* AccessCase::alternateBase() const
{
    return conditionSet().slotBaseCondition().object();
}

std::unique_ptr<AccessCase> AccessCase::clone() const
{
    std::unique_ptr<AccessCase> result(new AccessCase(*this));
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

    if (m_identifier) {
        if ((structure && structure->needImpurePropertyWatchpoint())
            || m_conditionSet.needImpurePropertyWatchpoint()
            || (m_polyProtoAccessChain && m_polyProtoAccessChain->needImpurePropertyWatchpoint(vm)))
            result.append(vm.ensureWatchpointSetForImpureProperty(m_identifier.uid()));
    }

    if (additionalSet())
        result.append(additionalSet());

    if (structure
        && structure->hasRareData()
        && structure->rareData()->hasSharedPolyProtoWatchpoint()
        && structure->rareData()->sharedPolyProtoWatchpoint()->isStillValid()) {
        WatchpointSet* set = structure->rareData()->sharedPolyProtoWatchpoint()->inflate();
        result.append(set);
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
        return false;
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
    case InstanceOfHit:
    case InstanceOfMiss:
    case InstanceOfGeneric:
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
    case IntrinsicGetter:
    case InHit:
    case InMiss:
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
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
        return false;
    case IndexedDoubleLoad:
    case IndexedTypedArrayFloat32Load:
    case IndexedTypedArrayFloat64Load:
    case IndexedTypedArrayUint32Load:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Functor>
void AccessCase::forEachDependentCell(VM& vm, const Functor& functor) const
{
    m_conditionSet.forEachDependentCell(functor);
    if (m_structure)
        functor(m_structure.get());
    if (m_polyProtoAccessChain) {
        for (StructureID structureID : m_polyProtoAccessChain->chain())
            functor(vm.getStructure(structureID));
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
        doesCalls = true;
        break;
    case Delete:
    case DeleteNonConfigurable:
    case DeleteMiss:
    case Load:
    case Replace:
    case Miss:
    case GetGetter:
    case IntrinsicGetter:
    case InHit:
    case InMiss:
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
        doesCalls = false;
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
            if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint())
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
    out.print("\n", m_type, ":(");

    CommaPrinter comma;

    out.print(comma, m_state);

    out.print(comma, "ident = '", m_identifier, "'");
    if (isValidOffset(m_offset))
        out.print(comma, "offset = ", m_offset);

    if (m_polyProtoAccessChain) {
        out.print(comma, "prototype access chain = ");
        m_polyProtoAccessChain->dump(structure(), out);
    } else {
        if (m_type == Transition || m_type == Delete)
            out.print(comma, "structure = ", pointerDump(structure()), " -> ", pointerDump(newStructure()));
        else if (m_structure)
            out.print(comma, "structure = ", pointerDump(m_structure.get()));
    }

    if (!m_conditionSet.isEmpty())
        out.print(comma, "conditions = ", m_conditionSet);

    dumpImpl(out, comma);
    out.print(")");
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

bool AccessCase::propagateTransitions(SlotVisitor& visitor) const
{
    bool result = true;

    if (m_structure)
        result &= m_structure->markIfCheap(visitor);

    if (m_polyProtoAccessChain) {
        for (StructureID structureID : m_polyProtoAccessChain->chain())
            result &= visitor.vm().getStructure(structureID)->markIfCheap(visitor);
    }

    switch (m_type) {
    case Transition:
    case Delete:
        if (visitor.vm().heap.isMarked(m_structure->previousID()))
            visitor.appendUnbarriered(m_structure.get());
        else
            result = false;
        break;
    default:
        break;
    }

    return result;
}

void AccessCase::visitAggregate(SlotVisitor& visitor) const
{
    m_identifier.visitAggregate(visitor);
}

void AccessCase::generateWithGuard(
    AccessGenerationState& state, CCallHelpers::JumpList& fallThrough)
{
    SuperSamplerScope superSamplerScope(false);

    checkConsistency(*state.stubInfo);

    RELEASE_ASSERT(m_state == Committed);
    m_state = Generated;

    CCallHelpers& jit = *state.jit;
    StructureStubInfo& stubInfo = *state.stubInfo;
    VM& vm = state.m_vm;
    JSValueRegs valueRegs = state.valueRegs;
    GPRReg baseGPR = state.baseGPR;
    GPRReg scratchGPR = state.scratchGPR;

    if (requiresIdentifierNameMatch() && !stubInfo.hasConstantIdentifier) {
        RELEASE_ASSERT(m_identifier);
        GPRReg propertyGPR = state.u.propertyGPR;
        // non-rope string check done inside polymorphic access.

        if (uid()->isSymbol())
            jit.loadPtr(MacroAssembler::Address(propertyGPR, Symbol::offsetOfSymbolImpl()), scratchGPR);
        else
            jit.loadPtr(MacroAssembler::Address(propertyGPR, JSString::offsetOfValue()), scratchGPR);
        fallThrough.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImmPtr(uid())));
    }

    auto emitDefaultGuard = [&] () {
        if (m_polyProtoAccessChain) {
            GPRReg baseForAccessGPR = state.scratchGPR;
            jit.move(state.baseGPR, baseForAccessGPR);
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
                        JSValue prototype = structure->prototypeForLookup(state.m_globalObject);
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
        this->as<ModuleNamespaceAccessCase>().emit(state, fallThrough);
        return;
    }

    case IndexedScopedArgumentsLoad: {
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = state.u.propertyGPR;

        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ScopedArgumentsType)));

        ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
        allocator.lock(stubInfo.baseRegs());
        allocator.lock(valueRegs);
        allocator.lock(stubInfo.propertyRegs());
        allocator.lock(scratchGPR);
        
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
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = state.u.propertyGPR;
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
        // This code is written such that the result could alias with the base or the property.

        TypedArrayType type = toTypedArrayType(m_type);

        GPRReg propertyGPR = state.u.propertyGPR;

        
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(typeForTypedArrayType(type))));

        jit.load32(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
        state.failAndRepatch.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratchGPR));

        ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
        allocator.lock(stubInfo.baseRegs());
        allocator.lock(valueRegs);
        allocator.lock(stubInfo.propertyRegs());
        allocator.lock(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        jit.loadPtr(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfVector()), scratch2GPR);
        jit.cageConditionally(Gigacage::Primitive, scratch2GPR, scratchGPR, scratchGPR);

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
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = state.u.propertyGPR;

        fallThrough.append(jit.branchIfNotString(baseGPR));

        ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
        allocator.lock(stubInfo.baseRegs());
        allocator.lock(valueRegs);
        allocator.lock(stubInfo.propertyRegs());
        allocator.lock(scratchGPR);
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

    case IndexedInt32Load:
    case IndexedDoubleLoad:
    case IndexedContiguousLoad:
    case IndexedArrayStorageLoad: {
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = state.u.propertyGPR;

        // int32 check done in polymorphic access.
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        jit.and32(CCallHelpers::TrustedImm32(IndexingShapeMask), scratchGPR);

        CCallHelpers::Jump isOutOfBounds;
        CCallHelpers::Jump isEmpty;

        ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
        allocator.lock(stubInfo.baseRegs());
        allocator.lock(valueRegs);
        allocator.lock(stubInfo.propertyRegs());
        allocator.lock(scratchGPR);
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
            isOutOfBounds = jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, ArrayStorage::vectorLengthOffset()));

            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
#if USE(JSVALUE64)
            jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs(scratchGPR));
            isEmpty = jit.branchIfEmpty(scratchGPR);
            jit.move(scratchGPR, valueRegs.payloadGPR());
#else
            jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs(scratch3GPR, scratchGPR));
            isEmpty = jit.branchIfEmpty(scratch3GPR);
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
            isOutOfBounds = jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength()));
            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
            if (m_type == IndexedDoubleLoad) {
                RELEASE_ASSERT(state.scratchFPR != InvalidFPRReg);
                jit.loadDouble(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), state.scratchFPR);
                isEmpty = jit.branchIfNaN(state.scratchFPR);
                jit.boxDouble(state.scratchFPR, valueRegs);
            } else {
#if USE(JSVALUE64)
                jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratchGPR));
                isEmpty = jit.branchIfEmpty(scratchGPR);
                jit.move(scratchGPR, valueRegs.payloadGPR());
#else
                jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratch3GPR, scratchGPR));
                isEmpty = jit.branchIfEmpty(scratch3GPR);
                jit.move(scratchGPR, valueRegs.payloadGPR());
                jit.move(scratch3GPR, valueRegs.tagGPR());
#endif
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.succeed();

        if (allocator.didReuseRegisters()) {
            isOutOfBounds.link(&jit);
            isEmpty.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            state.failAndIgnore.append(jit.jump());
        } else {
            state.failAndIgnore.append(isOutOfBounds);
            state.failAndIgnore.append(isEmpty);
        }

        return;
    }

    case InstanceOfHit:
    case InstanceOfMiss:
        emitDefaultGuard();
        
        fallThrough.append(
            jit.branchPtr(
                CCallHelpers::NotEqual, state.u.prototypeGPR,
                CCallHelpers::TrustedImmPtr(as<InstanceOfAccessCase>().prototype())));
        break;
        
    case InstanceOfGeneric: {
        GPRReg prototypeGPR = state.u.prototypeGPR;
        // Legend: value = `base instanceof prototypeGPR`.
        
        GPRReg valueGPR = valueRegs.payloadGPR();
        
        ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
        allocator.lock(stubInfo.baseRegs());
        allocator.lock(valueRegs);
        allocator.lock(stubInfo.propertyRegs());
        allocator.lock(scratchGPR);
        
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

        jit.emitLoadPrototype(vm, valueGPR, resultRegs, scratchGPR, failAndIgnore);
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
    JSGlobalObject* globalObject = state.m_globalObject;
    ECMAMode ecmaMode = state.m_ecmaMode;
    StructureStubInfo& stubInfo = *state.stubInfo;
    JSValueRegs valueRegs = state.valueRegs;
    GPRReg baseGPR = state.baseGPR;
    GPRReg thisGPR = stubInfo.thisValueIsInThisGPR() ? state.u.thisGPR : baseGPR;
    GPRReg scratchGPR = state.scratchGPR;

    for (const ObjectPropertyCondition& condition : m_conditionSet) {
        RELEASE_ASSERT(!m_polyProtoAccessChain);

        if (condition.isWatchableAssumingImpurePropertyWatchpoint(PropertyCondition::WatchabilityEffort::EnsureWatchability)) {
            state.installWatchpoint(condition);
            continue;
        }

        // For now, we only allow equivalence when it's watchable.
        RELEASE_ASSERT(condition.condition().kind() != PropertyCondition::Equivalence);

        if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint()) {
            // The reason why this cannot happen is that we require that PolymorphicAccess calls
            // AccessCase::generate() only after it has verified that
            // AccessCase::couldStillSucceed() returned true.

            dataLog("This condition is no longer met: ", condition, "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        // We will emit code that has a weak reference that isn't otherwise listed anywhere.
        Structure* structure = condition.object()->structure(vm);
        state.weakReferences.append(WriteBarrier<JSCell>(vm, codeBlock, structure));

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

        if (isValidOffset(m_offset)) {
            Structure* currStructure;
            if (!hasAlternateBase())
                currStructure = structure();
            else
                currStructure = alternateBase()->structure(vm);
            currStructure->startWatchingPropertyForReplacements(vm, offset());
        }

        GPRReg baseForGetGPR;
        if (viaProxy()) {
            ASSERT(m_type != CustomValueSetter || m_type != CustomAccessorSetter); // Because setters need to not trash valueRegsPayloadGPR.
            if (m_type == Getter || m_type == Setter)
                baseForGetGPR = scratchGPR;
            else
                baseForGetGPR = valueRegsPayloadGPR;

            ASSERT((m_type != Getter && m_type != Setter) || baseForGetGPR != baseGPR);
            ASSERT(m_type != Setter || baseForGetGPR != valueRegsPayloadGPR);

            jit.loadPtr(
                CCallHelpers::Address(baseGPR, JSProxy::targetOffset()),
                baseForGetGPR);
        } else
            baseForGetGPR = baseGPR;

        GPRReg baseForAccessGPR;
        if (m_polyProtoAccessChain) {
            // This isn't pretty, but we know we got here via generateWithGuard,
            // and it left the baseForAccess inside scratchGPR. We could re-derive the base,
            // but it'd require emitting the same code to load the base twice.
            baseForAccessGPR = scratchGPR;
        } else {
            if (hasAlternateBase()) {
                jit.move(
                    CCallHelpers::TrustedImmPtr(alternateBase()), scratchGPR);
                baseForAccessGPR = scratchGPR;
            } else
                baseForAccessGPR = baseForGetGPR;
        }

        GPRReg loadedValueGPR = InvalidGPRReg;
        if (m_type != CustomValueGetter && m_type != CustomAccessorGetter && m_type != CustomValueSetter && m_type != CustomAccessorSetter) {
            if (m_type == Load || m_type == GetGetter)
                loadedValueGPR = valueRegsPayloadGPR;
            else
                loadedValueGPR = scratchGPR;

            ASSERT((m_type != Getter && m_type != Setter) || loadedValueGPR != baseGPR);
            ASSERT(m_type != Setter || loadedValueGPR != valueRegsPayloadGPR);

            GPRReg storageGPR;
            if (isInlineOffset(m_offset))
                storageGPR = baseForAccessGPR;
            else {
                jit.loadPtr(
                    CCallHelpers::Address(baseForAccessGPR, JSObject::butterflyOffset()),
                    loadedValueGPR);
                storageGPR = loadedValueGPR;
            }

#if USE(JSVALUE64)
            jit.load64(
                CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset)), loadedValueGPR);
#else
            if (m_type == Load || m_type == GetGetter) {
                jit.load32(
                    CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset) + TagOffset),
                    valueRegs.tagGPR());
            }
            jit.load32(
                CCallHelpers::Address(storageGPR, offsetRelativeToBase(m_offset) + PayloadOffset),
                loadedValueGPR);
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
            if (!structure()->classInfo()->isSubClassOf(access.domAttribute()->classInfo)) {
                state.failAndIgnore.append(jit.jump());
                return;
            }

            if (Options::useDOMJIT() && access.domAttribute()->domJIT) {
                access.emitDOMJITGetter(state, access.domAttribute()->domJIT, baseForGetGPR);
                return;
            }
        }

        // Stuff for custom getters/setters.
        CCallHelpers::Call operationCall;

        // Stuff for JS getters/setters.
        CCallHelpers::DataLabelPtr addressOfLinkFunctionCheck;
        CCallHelpers::Call fastPathCall;
        CCallHelpers::Call slowPathCall;

        // This also does the necessary calculations of whether or not we're an
        // exception handling call site.
        AccessGenerationState::SpillState spillState = state.preserveLiveRegistersToStackForCall();

        auto restoreLiveRegistersFromStackForCall = [&](AccessGenerationState::SpillState& spillState, bool callHasReturnValue) {
            RegisterSet dontRestore;
            if (callHasReturnValue) {
                // This is the result value. We don't want to overwrite the result with what we stored to the stack.
                // We sometimes have to store it to the stack just in case we throw an exception and need the original value.
                dontRestore.set(valueRegs);
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

            state.setSpillStateForJSGetterSetter(spillState);

            RELEASE_ASSERT(!access.callLinkInfo());
            CallLinkInfo* callLinkInfo = state.m_callLinkInfos.add();
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

            callLinkInfo->setUpCall(CallLinkInfo::Call, stubInfo.codeOrigin, loadedValueGPR);

            CCallHelpers::JumpList done;

            // There is a "this" argument.
            unsigned numberOfParameters = 1;
            // ... and a value argument if we're calling a setter.
            if (m_type == Setter)
                numberOfParameters++;

            // Get the accessor; if there ain't one then the result is jsUndefined().
            // Note that GetterSetter always has cells for both. If it is not set (like, getter exits, but setter is not set), Null{Getter,Setter}Function is stored.
            Optional<CCallHelpers::Jump> returnUndefined;
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

            unsigned alignedNumberOfBytesForCall =
            WTF::roundUpToMultipleOf(stackAlignmentBytes(), numberOfBytesForCall);

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

            CCallHelpers::Jump slowCase = jit.branchPtrWithPatch(
                CCallHelpers::NotEqual, loadedValueGPR, addressOfLinkFunctionCheck,
                CCallHelpers::TrustedImmPtr(nullptr));

            fastPathCall = jit.nearCall();
            if (m_type == Getter)
                jit.setupResults(valueRegs);
            done.append(jit.jump());

            slowCase.link(&jit);
            jit.move(loadedValueGPR, GPRInfo::regT0);
#if USE(JSVALUE32_64)
            // We *always* know that the getter/setter, if non-null, is a cell.
            jit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), GPRInfo::regT1);
#endif
            jit.move(CCallHelpers::TrustedImmPtr(access.callLinkInfo()), GPRInfo::regT2);
            jit.move(CCallHelpers::TrustedImmPtr(globalObject), GPRInfo::regT3);
            slowPathCall = jit.nearCall();
            if (m_type == Getter)
                jit.setupResults(valueRegs);
            done.append(jit.jump());

            if (returnUndefined) {
                ASSERT(m_type == Getter);
                returnUndefined.value().link(&jit);
                jit.moveTrustedValue(jsUndefined(), valueRegs);
            }
            done.link(&jit);

            jit.addPtr(CCallHelpers::TrustedImm32((codeBlock->stackPointerOffset() * sizeof(Register)) - state.preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation),
                GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
            bool callHasReturnValue = isGetter();
            restoreLiveRegistersFromStackForCall(spillState, callHasReturnValue);

            jit.addLinkTask([=, &vm] (LinkBuffer& linkBuffer) {
                this->as<GetterSetterAccessCase>().callLinkInfo()->setCallLocations(
                    CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOfNearCall<JSInternalPtrTag>(slowPathCall)),
                    CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOf<JSInternalPtrTag>(addressOfLinkFunctionCheck)),
                    linkBuffer.locationOfNearCall<JSInternalPtrTag>(fastPathCall));

                linkBuffer.link(
                    slowPathCall,
                    CodeLocationLabel<JITThunkPtrTag>(vm.getCTIStub(linkCallThunkGenerator).code()));
            });
        } else {
            ASSERT(m_type == CustomValueGetter || m_type == CustomAccessorGetter || m_type == CustomValueSetter || m_type == CustomAccessorSetter);

            // Need to make room for the C call so any of our stack spillage isn't overwritten. It's
            // hard to track if someone did spillage or not, so we just assume that we always need
            // to make some space here.
            jit.makeSpaceOnStackForCCall();

            // Check if it is a super access
            GPRReg baseForCustomGetGPR = baseGPR != thisGPR ? thisGPR : baseForGetGPR;

            // getter: EncodedJSValue (*GetValueFunc)(JSGlobalObject*, EncodedJSValue thisValue, PropertyName);
            // setter: void (*PutValueFunc)(JSGlobalObject*, EncodedJSValue thisObject, EncodedJSValue value);
            // Custom values are passed the slotBase (the property holder), custom accessors are passed the thisVaule (reciever).
            // FIXME: Remove this differences in custom values and custom accessors.
            // https://bugs.webkit.org/show_bug.cgi?id=158014
            GPRReg baseForCustom = m_type == CustomValueGetter || m_type == CustomValueSetter ? baseForAccessGPR : baseForCustomGetGPR; 
            // We do not need to keep globalObject alive since the owner CodeBlock (even if JSGlobalObject* is one of CodeBlock that is inlined and held by DFG CodeBlock)
            // must keep it alive.
            if (m_type == CustomValueGetter || m_type == CustomAccessorGetter) {
                RELEASE_ASSERT(m_identifier);
                jit.setupArguments<PropertySlot::GetValueFunc>(
                    CCallHelpers::TrustedImmPtr(globalObject),
                    CCallHelpers::CellValue(baseForCustom),
                    CCallHelpers::TrustedImmPtr(uid()));
            } else {
                jit.setupArguments<PutPropertySlot::PutValueFunc>(
                    CCallHelpers::TrustedImmPtr(globalObject),
                    CCallHelpers::CellValue(baseForCustom),
                    valueRegs);
            }
            jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);

            operationCall = jit.call(OperationPtrTag);
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
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
                CCallHelpers::Address(
                    scratchGPR, offsetInButterfly(m_offset) * sizeof(JSValue)));
        }
        state.succeed();
        return;
    }

    case Transition: {
        // AccessCase::transition() should have returned null if this wasn't true.
        RELEASE_ASSERT(GPRInfo::numberOfRegisters >= 6 || !structure()->outOfLineCapacity() || structure()->outOfLineCapacity() == newStructure()->outOfLineCapacity());

        // NOTE: This logic is duplicated in AccessCase::doesCalls(). It's important that doesCalls() knows
        // exactly when this would make calls.
        bool allocating = newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity();
        bool reallocating = allocating && structure()->outOfLineCapacity();
        bool allocatingInline = allocating && !structure()->couldHaveIndexingHeader();

        ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
        allocator.lock(stubInfo.baseRegs());
        allocator.lock(valueRegs);
        allocator.lock(scratchGPR);

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
                Allocator allocator = vm.jsValueGigacageAuxiliarySpace.allocatorFor(newSize, AllocatorForMode::AllocatorIfExists);

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
                extraRegistersToPreserve.set(baseGPR);
                extraRegistersToPreserve.set(valueRegs);
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
                        linkBuffer.link(
                            operationCall,
                            FunctionPtr<OperationPtrTag>(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity));
                    });
                } else {
                    // Handle the case where we are reallocating (i.e. the old structure/butterfly
                    // already had out-of-line property storage).
                    jit.setupArguments<decltype(operationReallocateButterflyToGrowPropertyStorage)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR, CCallHelpers::TrustedImm32(newSize / sizeof(JSValue)));
                    jit.prepareCallOperation(vm);
                    
                    CCallHelpers::Call operationCall = jit.call(OperationPtrTag);
                    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                        linkBuffer.link(
                            operationCall,
                            FunctionPtr<OperationPtrTag>(operationReallocateButterflyToGrowPropertyStorage));
                    });
                }
                
                jit.reclaimSpaceOnStackForCCall();
                jit.move(GPRInfo::returnValueGPR, scratchGPR);
                
                CCallHelpers::Jump noException = jit.emitExceptionCheck(vm, CCallHelpers::InvertedExceptionCheck);
                
                state.restoreLiveRegistersFromStackForCallWithThrownException(spillState);
                state.emitExplicitExceptionHandler();
                
                noException.link(&jit);
                RegisterSet resultRegisterToExclude;
                resultRegisterToExclude.set(scratchGPR);
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
        ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
        allocator.lock(stubInfo.baseRegs());
        allocator.lock(valueRegs);
        allocator.lock(baseGPR);
        allocator.lock(scratchGPR);
        ASSERT(structure()->transitionWatchpointSetHasBeenInvalidated());
        ASSERT(newStructure()->isPropertyDeletionTransition());
        ASSERT(baseGPR != scratchGPR);
        ASSERT(!valueRegs.uses(baseGPR));
        ASSERT(!valueRegs.uses(scratchGPR));

        ScratchRegisterAllocator::PreservedState preservedState =
            allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

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

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
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
        
    case IntrinsicGetter: {
        RELEASE_ASSERT(isValidOffset(offset()));

        // We need to ensure the getter value does not move from under us. Note that GetterSetters
        // are immutable so we just need to watch the property not any value inside it.
        Structure* currStructure;
        if (!hasAlternateBase())
            currStructure = structure();
        else
            currStructure = alternateBase()->structure(vm);
        currStructure->startWatchingPropertyForReplacements(vm, offset());
        
        this->as<IntrinsicGetterAccessCase>().emitIntrinsicGetter(state);
        return;
    }
        
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
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
        return TypeInt8;
    case IndexedTypedArrayUint8Load:
        return TypeUint8;
    case IndexedTypedArrayUint8ClampedLoad:
        return TypeUint8Clamped;
    case IndexedTypedArrayInt16Load:
        return TypeInt16;
    case IndexedTypedArrayUint16Load:
        return TypeUint16;
    case IndexedTypedArrayInt32Load:
        return TypeInt32;
    case IndexedTypedArrayUint32Load:
        return TypeUint32;
    case IndexedTypedArrayFloat32Load:
        return TypeFloat32;
    case IndexedTypedArrayFloat64Load:
        return TypeFloat64;
    default:
        RELEASE_ASSERT_NOT_REACHED();
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

} // namespace JSC

#endif
