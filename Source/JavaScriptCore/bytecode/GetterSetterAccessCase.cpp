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
#include "GetterSetterAccessCase.h"

#if ENABLE(JIT)

#include "AccessCaseSnippetParams.h"
#include "DOMJITCallDOMGetterSnippet.h"
#include "DOMJITGetterSetter.h"
#include "HeapInlines.h"
#include "JSCJSValueInlines.h"
#include "PolymorphicAccess.h"
#include "StructureStubInfo.h"

namespace JSC {

namespace GetterSetterAccessCaseInternal {
static constexpr bool verbose = false;
}

GetterSetterAccessCase::GetterSetterAccessCase(VM& vm, JSCell* owner, AccessType accessType, CacheableIdentifier identifier, PropertyOffset offset, Structure* structure, const ObjectPropertyConditionSet& conditionSet, bool viaProxy, WatchpointSet* additionalSet, JSObject* customSlotBase, std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain)
    : Base(vm, owner, accessType, identifier, offset, structure, conditionSet, viaProxy, additionalSet, WTFMove(prototypeAccessChain))
{
    m_customSlotBase.setMayBeNull(vm, owner, customSlotBase);
}

std::unique_ptr<AccessCase> GetterSetterAccessCase::create(
    VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier, PropertyOffset offset, Structure* structure, const ObjectPropertyConditionSet& conditionSet,
    bool viaProxy, WatchpointSet* additionalSet, FunctionPtr<OperationPtrTag> customGetter, JSObject* customSlotBase,
    Optional<DOMAttributeAnnotation> domAttribute, std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain)
{
    switch (type) {
    case Getter:
    case CustomAccessorGetter:
    case CustomValueGetter:
        break;
    default:
        ASSERT_NOT_REACHED();
    };

    std::unique_ptr<GetterSetterAccessCase> result(new GetterSetterAccessCase(vm, owner, type, identifier, offset, structure, conditionSet, viaProxy, additionalSet, customSlotBase, WTFMove(prototypeAccessChain)));
    result->m_domAttribute = domAttribute;
    result->m_customAccessor = customGetter ? FunctionPtr<OperationPtrTag>(customGetter) : nullptr;
    return result;
}

std::unique_ptr<AccessCase> GetterSetterAccessCase::create(VM& vm, JSCell* owner, AccessType type, Structure* structure, CacheableIdentifier identifier, PropertyOffset offset,
    const ObjectPropertyConditionSet& conditionSet, std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain, FunctionPtr<OperationPtrTag> customSetter,
    JSObject* customSlotBase)
{
    ASSERT(type == Setter || type == CustomValueSetter || type == CustomAccessorSetter);
    std::unique_ptr<GetterSetterAccessCase> result(new GetterSetterAccessCase(vm, owner, type, identifier, offset, structure, conditionSet, false, nullptr, customSlotBase, WTFMove(prototypeAccessChain)));
    result->m_customAccessor = customSetter ? FunctionPtr<OperationPtrTag>(customSetter) : nullptr;
    return result;
}


GetterSetterAccessCase::~GetterSetterAccessCase()
{
}


GetterSetterAccessCase::GetterSetterAccessCase(const GetterSetterAccessCase& other)
    : Base(other)
    , m_customSlotBase(other.m_customSlotBase)
{
    m_customAccessor = other.m_customAccessor;
    m_domAttribute = other.m_domAttribute;
}

std::unique_ptr<AccessCase> GetterSetterAccessCase::clone() const
{
    std::unique_ptr<GetterSetterAccessCase> result(new GetterSetterAccessCase(*this));
    result->resetState();
    return result;
}

bool GetterSetterAccessCase::hasAlternateBase() const
{
    if (customSlotBase())
        return true;
    return Base::hasAlternateBase();
}

JSObject* GetterSetterAccessCase::alternateBase() const
{
    if (customSlotBase())
        return customSlotBase();
    return Base::alternateBase();
}

void GetterSetterAccessCase::dumpImpl(PrintStream& out, CommaPrinter& comma) const
{
    Base::dumpImpl(out, comma);
    out.print(comma, "customSlotBase = ", RawPointer(customSlotBase()));
    if (callLinkInfo())
        out.print(comma, "callLinkInfo = ", RawPointer(callLinkInfo()));
    out.print(comma, "customAccessor = ", RawPointer(m_customAccessor.executableAddress()));
}

void GetterSetterAccessCase::emitDOMJITGetter(AccessGenerationState& state, const DOMJIT::GetterSetter* domJIT, GPRReg baseForGetGPR)
{
    CCallHelpers& jit = *state.jit;
    StructureStubInfo& stubInfo = *state.stubInfo;
    JSValueRegs valueRegs = state.valueRegs;
    GPRReg baseGPR = state.baseGPR;
    GPRReg scratchGPR = state.scratchGPR;

    // We construct the environment that can execute the DOMJIT::Snippet here.
    Ref<DOMJIT::CallDOMGetterSnippet> snippet = domJIT->compiler()();

    Vector<GPRReg> gpScratch;
    Vector<FPRReg> fpScratch;
    Vector<SnippetParams::Value> regs;

    ScratchRegisterAllocator allocator(stubInfo.usedRegisters);
    allocator.lock(stubInfo.baseRegs());
    allocator.lock(valueRegs);
    allocator.lock(scratchGPR);

    GPRReg paramBaseGPR = InvalidGPRReg;
    GPRReg paramGlobalObjectGPR = InvalidGPRReg;
    JSValueRegs paramValueRegs = valueRegs;
    GPRReg remainingScratchGPR = InvalidGPRReg;

    // valueRegs and baseForGetGPR may be the same. For example, in Baseline JIT, we pass the same regT0 for baseGPR and valueRegs.
    // In FTL, there is no constraint that the baseForGetGPR interferes with the result. To make implementation simple in
    // Snippet, Snippet assumes that result registers always early interfere with input registers, in this case,
    // baseForGetGPR. So we move baseForGetGPR to the other register if baseForGetGPR == valueRegs.
    if (baseForGetGPR != valueRegs.payloadGPR()) {
        paramBaseGPR = baseForGetGPR;
        if (!snippet->requireGlobalObject)
            remainingScratchGPR = scratchGPR;
        else
            paramGlobalObjectGPR = scratchGPR;
    } else {
        jit.move(valueRegs.payloadGPR(), scratchGPR);
        paramBaseGPR = scratchGPR;
        if (snippet->requireGlobalObject)
            paramGlobalObjectGPR = allocator.allocateScratchGPR();
    }

    JSGlobalObject* globalObjectForDOMJIT = structure()->globalObject();

    regs.append(paramValueRegs);
    regs.append(paramBaseGPR);
    if (snippet->requireGlobalObject) {
        ASSERT(paramGlobalObjectGPR != InvalidGPRReg);
        regs.append(SnippetParams::Value(paramGlobalObjectGPR, globalObjectForDOMJIT));
    }

    if (snippet->numGPScratchRegisters) {
        unsigned i = 0;
        if (remainingScratchGPR != InvalidGPRReg) {
            gpScratch.append(remainingScratchGPR);
            ++i;
        }
        for (; i < snippet->numGPScratchRegisters; ++i)
            gpScratch.append(allocator.allocateScratchGPR());
    }

    for (unsigned i = 0; i < snippet->numFPScratchRegisters; ++i)
        fpScratch.append(allocator.allocateScratchFPR());

    // Let's store the reused registers to the stack. After that, we can use allocated scratch registers.
    ScratchRegisterAllocator::PreservedState preservedState =
    allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

    if (GetterSetterAccessCaseInternal::verbose) {
        dataLog("baseGPR = ", baseGPR, "\n");
        dataLog("valueRegs = ", valueRegs, "\n");
        dataLog("scratchGPR = ", scratchGPR, "\n");
        dataLog("paramBaseGPR = ", paramBaseGPR, "\n");
        if (paramGlobalObjectGPR != InvalidGPRReg)
            dataLog("paramGlobalObjectGPR = ", paramGlobalObjectGPR, "\n");
        dataLog("paramValueRegs = ", paramValueRegs, "\n");
        for (unsigned i = 0; i < snippet->numGPScratchRegisters; ++i)
            dataLog("gpScratch[", i, "] = ", gpScratch[i], "\n");
    }

    if (snippet->requireGlobalObject)
        jit.move(CCallHelpers::TrustedImmPtr(globalObjectForDOMJIT), paramGlobalObjectGPR);

    // We just spill the registers used in Snippet here. For not spilled registers here explicitly,
    // they must be in the used register set passed by the callers (Baseline, DFG, and FTL) if they need to be kept.
    // Some registers can be locked, but not in the used register set. For example, the caller could make baseGPR
    // same to valueRegs, and not include it in the used registers since it will be changed.
    RegisterSet registersToSpillForCCall;
    for (auto& value : regs) {
        SnippetReg reg = value.reg();
        if (reg.isJSValueRegs())
            registersToSpillForCCall.set(reg.jsValueRegs());
        else if (reg.isGPR())
            registersToSpillForCCall.set(reg.gpr());
        else
            registersToSpillForCCall.set(reg.fpr());
    }
    for (GPRReg reg : gpScratch)
        registersToSpillForCCall.set(reg);
    for (FPRReg reg : fpScratch)
        registersToSpillForCCall.set(reg);
    registersToSpillForCCall.exclude(RegisterSet::registersToNotSaveForCCall());

    AccessCaseSnippetParams params(state.m_vm, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
    snippet->generator()->run(jit, params);
    allocator.restoreReusedRegistersByPopping(jit, preservedState);
    state.succeed();
    
    CCallHelpers::JumpList exceptions = params.emitSlowPathCalls(state, registersToSpillForCCall, jit);
    if (!exceptions.empty()) {
        exceptions.link(&jit);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        state.emitExplicitExceptionHandler();
    }
}

} // namespace JSC

#endif // ENABLE(JIT)
