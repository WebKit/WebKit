/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "CallLinkInfo.h"
#include "DOMJITGetterSetter.h"
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
static const bool verbose = false;
}

AccessCase::AccessCase(VM& vm, JSCell* owner, AccessType type, PropertyOffset offset, Structure* structure, const ObjectPropertyConditionSet& conditionSet, std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain)
    : m_type(type)
    , m_offset(offset)
    , m_polyProtoAccessChain(WTFMove(prototypeAccessChain))
{
    m_structure.setMayBeNull(vm, owner, structure);
    m_conditionSet = conditionSet;
}

std::unique_ptr<AccessCase> AccessCase::create(VM& vm, JSCell* owner, AccessType type, PropertyOffset offset, Structure* structure, const ObjectPropertyConditionSet& conditionSet, std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain)
{
    switch (type) {
    case InHit:
    case InMiss:
        break;
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
    case ModuleNamespaceLoad:
    case Replace:
    case InstanceOfGeneric:
        RELEASE_ASSERT(!prototypeAccessChain);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    };

    return std::unique_ptr<AccessCase>(new AccessCase(vm, owner, type, offset, structure, conditionSet, WTFMove(prototypeAccessChain)));
}

std::unique_ptr<AccessCase> AccessCase::create(
    VM& vm, JSCell* owner, PropertyOffset offset, Structure* oldStructure, Structure* newStructure,
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

    return std::unique_ptr<AccessCase>(new AccessCase(vm, owner, Transition, offset, newStructure, conditionSet, WTFMove(prototypeAccessChain)));
}

AccessCase::~AccessCase()
{
}

std::unique_ptr<AccessCase> AccessCase::fromStructureStubInfo(
    VM& vm, JSCell* owner, StructureStubInfo& stubInfo)
{
    switch (stubInfo.cacheType) {
    case CacheType::GetByIdSelf:
        return ProxyableAccessCase::create(vm, owner, Load, stubInfo.u.byIdSelf.offset, stubInfo.u.byIdSelf.baseObjectStructure.get());

    case CacheType::PutByIdReplace:
        return AccessCase::create(vm, owner, Replace, stubInfo.u.byIdSelf.offset, stubInfo.u.byIdSelf.baseObjectStructure.get());

    case CacheType::InByIdSelf:
        return AccessCase::create(vm, owner, InHit, stubInfo.u.byIdSelf.offset, stubInfo.u.byIdSelf.baseObjectStructure.get());

    case CacheType::ArrayLength:
        return AccessCase::create(vm, owner, AccessCase::ArrayLength);

    case CacheType::StringLength:
        return AccessCase::create(vm, owner, AccessCase::StringLength);

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

Vector<WatchpointSet*, 2> AccessCase::commit(VM& vm, const Identifier& ident)
{
    // It's fine to commit something that is already committed. That arises when we switch to using
    // newly allocated watchpoints. When it happens, it's not efficient - but we think that's OK
    // because most AccessCases have no extra watchpoints anyway.
    RELEASE_ASSERT(m_state == Primordial || m_state == Committed);

    Vector<WatchpointSet*, 2> result;
    Structure* structure = this->structure();

    if (!ident.isNull()) {
        if ((structure && structure->needImpurePropertyWatchpoint())
            || m_conditionSet.needImpurePropertyWatchpoint()
            || (m_polyProtoAccessChain && m_polyProtoAccessChain->needImpurePropertyWatchpoint()))
            result.append(vm.ensureWatchpointSetForImpureProperty(ident));
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

bool AccessCase::guardedByStructureCheck() const
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
        return false;
    default:
        return true;
    }
}

bool AccessCase::doesCalls(Vector<JSCell*>* cellsToMark) const
{
    switch (type()) {
    case Getter:
    case Setter:
    case CustomValueGetter:
    case CustomAccessorGetter:
    case CustomValueSetter:
    case CustomAccessorSetter:
        return true;
    case Transition:
        if (newStructure()->outOfLineCapacity() != structure()->outOfLineCapacity()
            && structure()->couldHaveIndexingHeader()) {
            if (cellsToMark)
                cellsToMark->append(newStructure());
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool AccessCase::couldStillSucceed() const
{
    return m_conditionSet.structuresEnsureValidityAssumingImpurePropertyWatchpoint();
}

bool AccessCase::canReplace(const AccessCase& other) const
{
    // This puts in a good effort to try to figure out if 'other' is made superfluous by '*this'.
    // It's fine for this to return false if it's in doubt.
    //
    // Note that if A->guardedByStructureCheck() && B->guardedByStructureCheck() then
    // A->canReplace(B) == B->canReplace(A).
    
    switch (type()) {
    case ArrayLength:
    case StringLength:
    case DirectArgumentsLength:
    case ScopedArgumentsLength:
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
    default:
        if (other.type() != type())
            return false;

        if (m_polyProtoAccessChain) {
            if (!other.m_polyProtoAccessChain)
                return false;
            // This is the only check we need since PolyProtoAccessChain contains the base structure.
            // If we ever change it to contain only the prototype chain, we'll also need to change
            // this to check the base structure.
            return structure() == other.structure()
                && *m_polyProtoAccessChain == *other.m_polyProtoAccessChain;
        }

        if (!guardedByStructureCheck() || !other.guardedByStructureCheck())
            return false;

        return structure() == other.structure();
    }
}

void AccessCase::dump(PrintStream& out) const
{
    out.print("\n", m_type, ":(");

    CommaPrinter comma;

    out.print(comma, m_state);

    if (isValidOffset(m_offset))
        out.print(comma, "offset = ", m_offset);
    if (!m_conditionSet.isEmpty())
        out.print(comma, "conditions = ", m_conditionSet);

    if (m_polyProtoAccessChain) {
        out.print(comma, "prototype access chain = ");
        m_polyProtoAccessChain->dump(structure(), out);
    } else {
        if (m_type == Transition)
            out.print(comma, "structure = ", pointerDump(structure()), " -> ", pointerDump(newStructure()));
        else if (m_structure)
            out.print(comma, "structure = ", pointerDump(m_structure.get()));
    }

    dumpImpl(out, comma);
    out.print(")");
}

bool AccessCase::visitWeak(VM& vm) const
{
    if (m_structure && !vm.heap.isMarked(m_structure.get()))
        return false;
    if (m_polyProtoAccessChain) {
        for (Structure* structure : m_polyProtoAccessChain->chain()) {
            if (!vm.heap.isMarked(structure))
                return false;
        }
    }
    if (!m_conditionSet.areStillLive(vm))
        return false;
    if (isAccessor()) {
        auto& accessor = this->as<GetterSetterAccessCase>();
        if (accessor.callLinkInfo())
            accessor.callLinkInfo()->visitWeak(vm);
        if (accessor.customSlotBase() && !vm.heap.isMarked(accessor.customSlotBase()))
            return false;
    } else if (type() == IntrinsicGetter) {
        auto& intrinsic = this->as<IntrinsicGetterAccessCase>();
        if (intrinsic.intrinsicFunction() && !vm.heap.isMarked(intrinsic.intrinsicFunction()))
            return false;
    } else if (type() == ModuleNamespaceLoad) {
        auto& accessCase = this->as<ModuleNamespaceAccessCase>();
        if (accessCase.moduleNamespaceObject() && !vm.heap.isMarked(accessCase.moduleNamespaceObject()))
            return false;
        if (accessCase.moduleEnvironment() && !vm.heap.isMarked(accessCase.moduleEnvironment()))
            return false;
    } else if (type() == InstanceOfHit || type() == InstanceOfMiss) {
        if (as<InstanceOfAccessCase>().prototype() && !vm.heap.isMarked(as<InstanceOfAccessCase>().prototype()))
            return false;
    }

    return true;
}

bool AccessCase::propagateTransitions(SlotVisitor& visitor) const
{
    bool result = true;

    if (m_structure)
        result &= m_structure->markIfCheap(visitor);

    if (m_polyProtoAccessChain) {
        for (Structure* structure : m_polyProtoAccessChain->chain())
            result &= structure->markIfCheap(visitor);
    }

    switch (m_type) {
    case Transition:
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

void AccessCase::generateWithGuard(
    AccessGenerationState& state, CCallHelpers::JumpList& fallThrough)
{
    SuperSamplerScope superSamplerScope(false);

    RELEASE_ASSERT(m_state == Committed);
    m_state = Generated;

    CCallHelpers& jit = *state.jit;
    StructureStubInfo& stubInfo = *state.stubInfo;
    VM& vm = state.m_vm;
    JSValueRegs valueRegs = state.valueRegs;
    GPRReg baseGPR = state.baseGPR;
    GPRReg thisGPR = state.thisGPR != InvalidGPRReg ? state.thisGPR : baseGPR;
    GPRReg scratchGPR = state.scratchGPR;

    UNUSED_PARAM(vm);

    auto emitDefaultGuard = [&] () {
        if (m_polyProtoAccessChain) {
            GPRReg baseForAccessGPR = state.scratchGPR;
            jit.move(state.baseGPR, baseForAccessGPR);
            m_polyProtoAccessChain->forEach(structure(), [&] (Structure* structure, bool atEnd) {
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
                        fallThrough.append(jit.branch64(CCallHelpers::NotEqual, baseForAccessGPR, CCallHelpers::TrustedImm64(ValueNull)));
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
                        fallThrough.append(jit.branch64(CCallHelpers::Equal, baseForAccessGPR, CCallHelpers::TrustedImm64(ValueNull)));
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

        jit.loadPtr(
            CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfStorage()),
            scratchGPR);
        fallThrough.append(
            jit.branchTest8(
                CCallHelpers::NonZero,
                CCallHelpers::Address(scratchGPR, ScopedArguments::offsetOfOverrodeThingsInStorage())));
        jit.load32(
            CCallHelpers::Address(scratchGPR, ScopedArguments::offsetOfTotalLengthInStorage()),
            valueRegs.payloadGPR());
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        state.succeed();
        return;
    }

    case ModuleNamespaceLoad: {
        this->as<ModuleNamespaceAccessCase>().emit(state, fallThrough);
        return;
    }

    case InstanceOfHit:
    case InstanceOfMiss:
        emitDefaultGuard();
        
        fallThrough.append(
            jit.branchPtr(
                CCallHelpers::NotEqual, thisGPR,
                CCallHelpers::TrustedImmPtr(as<InstanceOfAccessCase>().prototype())));
        break;
        
    case InstanceOfGeneric: {
        // Legend: value = `base instanceof this`.
        
        GPRReg valueGPR = valueRegs.payloadGPR();
        
        ScratchRegisterAllocator allocator(stubInfo.patch.usedRegisters);
        allocator.lock(baseGPR);
        allocator.lock(valueGPR);
        allocator.lock(thisGPR);
        allocator.lock(scratchGPR);
        
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        
        if (!state.stubInfo->prototypeIsKnownObject)
            state.failAndIgnore.append(jit.branchIfNotObject(thisGPR));
        
        ScratchRegisterAllocator::PreservedState preservedState =
            allocator.preserveReusedRegistersByPushing(
                jit,
                ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        CCallHelpers::Jump failAndIgnore;

        jit.move(baseGPR, valueGPR);
        
        CCallHelpers::Label loop(&jit);
        failAndIgnore = jit.branchIfType(valueGPR, ProxyObjectType);
        
        jit.emitLoadStructure(vm, valueGPR, scratch2GPR, scratchGPR);
#if USE(JSVALUE64)
        jit.load64(CCallHelpers::Address(scratch2GPR, Structure::prototypeOffset()), scratch2GPR);
        CCallHelpers::Jump hasMonoProto = jit.branchTest64(CCallHelpers::NonZero, scratch2GPR);
        jit.load64(
            CCallHelpers::Address(valueGPR, offsetRelativeToBase(knownPolyProtoOffset)),
            scratch2GPR);
        hasMonoProto.link(&jit);
#else
        jit.load32(
            CCallHelpers::Address(scratch2GPR, Structure::prototypeOffset() + TagOffset),
            scratchGPR);
        jit.load32(
            CCallHelpers::Address(scratch2GPR, Structure::prototypeOffset() + PayloadOffset),
            scratch2GPR);
        CCallHelpers::Jump hasMonoProto = jit.branch32(
            CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(JSValue::EmptyValueTag));
        jit.load32(
            CCallHelpers::Address(
                valueGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset),
            scratch2GPR);
        hasMonoProto.link(&jit);
#endif
        jit.move(scratch2GPR, valueGPR);
        
        CCallHelpers::Jump isInstance = jit.branchPtr(CCallHelpers::Equal, valueGPR, thisGPR);

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
    m_state = Generated;

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
    StructureStubInfo& stubInfo = *state.stubInfo;
    const Identifier& ident = *state.ident;
    JSValueRegs valueRegs = state.valueRegs;
    GPRReg baseGPR = state.baseGPR;
    GPRReg thisGPR = state.thisGPR != InvalidGPRReg ? state.thisGPR : baseGPR;
    GPRReg scratchGPR = state.scratchGPR;

    ASSERT(m_conditionSet.structuresEnsureValidityAssumingImpurePropertyWatchpoint());

    for (const ObjectPropertyCondition& condition : m_conditionSet) {
        RELEASE_ASSERT(!m_polyProtoAccessChain);

        Structure* structure = condition.object()->structure(vm);

        if (condition.isWatchableAssumingImpurePropertyWatchpoint()) {
            structure->addTransitionWatchpoint(state.addWatchpoint(condition));
            continue;
        }

        if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint(structure)) {
            // The reason why this cannot happen is that we require that PolymorphicAccess calls
            // AccessCase::generate() only after it has verified that
            // AccessCase::couldStillSucceed() returned true.

            dataLog("This condition is no longer met: ", condition, "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        // We will emit code that has a weak reference that isn't otherwise listed anywhere.
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
            CCallHelpers::tagFor(static_cast<VirtualRegister>(CallFrameSlot::argumentCount)));

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
            access.m_callLinkInfo = makeUnique<CallLinkInfo>();

            // FIXME: If we generated a polymorphic call stub that jumped back to the getter
            // stub, which then jumped back to the main code, then we'd have a reachability
            // situation that the GC doesn't know about. The GC would ensure that the polymorphic
            // call stub stayed alive, and it would ensure that the main code stayed alive, but
            // it wouldn't know that the getter stub was alive. Ideally JIT stub routines would
            // be GC objects, and then we'd be able to say that the polymorphic call stub has a
            // reference to the getter stub.
            // https://bugs.webkit.org/show_bug.cgi?id=148914
            access.callLinkInfo()->disallowStubs();

            access.callLinkInfo()->setUpCall(
                CallLinkInfo::Call, stubInfo.codeOrigin, loadedValueGPR);

            CCallHelpers::JumpList done;

            // There is a "this" argument.
            unsigned numberOfParameters = 1;
            // ... and a value argument if we're calling a setter.
            if (m_type == Setter)
                numberOfParameters++;

            // Get the accessor; if there ain't one then the result is jsUndefined().
            if (m_type == Setter) {
                jit.loadPtr(
                    CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfSetter()),
                    loadedValueGPR);
            } else {
                jit.loadPtr(
                    CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfGetter()),
                    loadedValueGPR);
            }

            CCallHelpers::Jump returnUndefined = jit.branchTestPtr(
                CCallHelpers::Zero, loadedValueGPR);

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
                calleeFrame.withOffset(CallFrameSlot::argumentCount * sizeof(Register) + PayloadOffset));

            jit.storeCell(
                loadedValueGPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));

            jit.storeCell(
                thisGPR,
                calleeFrame.withOffset(virtualRegisterForArgument(0).offset() * sizeof(Register)));

            if (m_type == Setter) {
                jit.storeValue(
                    valueRegs,
                    calleeFrame.withOffset(
                        virtualRegisterForArgument(1).offset() * sizeof(Register)));
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
            slowPathCall = jit.nearCall();
            if (m_type == Getter)
                jit.setupResults(valueRegs);
            done.append(jit.jump());

            returnUndefined.link(&jit);
            if (m_type == Getter)
                jit.moveTrustedValue(jsUndefined(), valueRegs);

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

            // getter: EncodedJSValue (*GetValueFunc)(ExecState*, EncodedJSValue thisValue, PropertyName);
            // setter: void (*PutValueFunc)(ExecState*, EncodedJSValue thisObject, EncodedJSValue value);
            // Custom values are passed the slotBase (the property holder), custom accessors are passed the thisVaule (reciever).
            // FIXME: Remove this differences in custom values and custom accessors.
            // https://bugs.webkit.org/show_bug.cgi?id=158014
            GPRReg baseForCustom = m_type == CustomValueGetter || m_type == CustomValueSetter ? baseForAccessGPR : baseForCustomGetGPR; 
            if (m_type == CustomValueGetter || m_type == CustomAccessorGetter) {
                jit.setupArguments<PropertySlot::GetValueFunc>(
                    CCallHelpers::CellValue(baseForCustom),
                    CCallHelpers::TrustedImmPtr(ident.impl()));
            } else {
                jit.setupArguments<PutPropertySlot::PutValueFunc>(
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

        ScratchRegisterAllocator allocator(stubInfo.patch.usedRegisters);
        allocator.lock(baseGPR);
#if USE(JSVALUE32_64)
        allocator.lock(stubInfo.patch.baseTagGPR);
#endif
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
                    CCallHelpers::tagFor(static_cast<VirtualRegister>(CallFrameSlot::argumentCount)));
                
                jit.makeSpaceOnStackForCCall();
                
                if (!reallocating) {
                    jit.setupArguments<decltype(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity)>(baseGPR);
                    
                    CCallHelpers::Call operationCall = jit.call(OperationPtrTag);
                    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                        linkBuffer.link(
                            operationCall,
                            FunctionPtr<OperationPtrTag>(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity));
                    });
                } else {
                    // Handle the case where we are reallocating (i.e. the old structure/butterfly
                    // already had out-of-line property storage).
                    jit.setupArguments<decltype(operationReallocateButterflyToGrowPropertyStorage)>(
                        baseGPR, CCallHelpers::TrustedImm32(newSize / sizeof(JSValue)));
                    
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
        // These need to be handled by generateWithGuard(), since the guard is part of the
        // algorithm. We can be sure that nobody will call generate() directly for these since they
        // are not guarded by structure checks.
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

#endif
