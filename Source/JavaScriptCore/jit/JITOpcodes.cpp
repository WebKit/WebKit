/*
 * Copyright (C) 2009-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
#if ENABLE(JIT)
#include "JIT.h"

#include "BaselineJITRegisters.h"
#include "BasicBlockLocation.h"
#include "BytecodeGenerator.h"
#include "Exception.h"
#include "JITInlines.h"
#include "JITThunks.h"
#include "JSCast.h"
#include "JSFunction.h"
#include "JSPropertyNameEnumerator.h"
#include "LinkBuffer.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include "TypeLocation.h"
#include "TypeProfilerLog.h"
#include "VirtualRegister.h"

namespace JSC {

void JIT::emit_op_mov(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpMov>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;

    if (src.isConstant()) {
        if (m_profiledCodeBlock->isConstantOwnedByUnlinkedCodeBlock(src)) {
            storeValue(m_unlinkedCodeBlock->getConstant(src), addressFor(dst), jsRegT10);
        } else {
            loadCodeBlockConstant(src, jsRegT10);
            storeValue(jsRegT10, addressFor(dst));
        }
        return;
    }

    loadValue(addressFor(src), jsRegT10);
    storeValue(jsRegT10, addressFor(dst));
}

void JIT::emit_op_end(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnd>();
    static_assert(noOverlap(returnValueJSR, callFrameRegister));
    emitGetVirtualRegister(bytecode.m_value, returnValueJSR);
    emitRestoreCalleeSaves();
    emitFunctionEpilogue();
    ret();
}

void JIT::emit_op_jmp(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJmp>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    addJump(jump(), target);
}


void JIT::emit_op_new_object(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewObject>();

    RegisterID resultReg = regT0;
    RegisterID allocatorReg = regT1;
    RegisterID scratchReg = regT2;
    RegisterID structureReg = regT3;

    loadPtrFromMetadata(bytecode, OpNewObject::Metadata::offsetOfObjectAllocationProfile() + ObjectAllocationProfile::offsetOfAllocator(), allocatorReg);
    loadPtrFromMetadata(bytecode, OpNewObject::Metadata::offsetOfObjectAllocationProfile() + ObjectAllocationProfile::offsetOfStructure(), structureReg);

    JumpList slowCases;
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject(resultReg, JITAllocator::variable(), allocatorReg, structureReg, butterfly, scratchReg, slowCases);
    load8(Address(structureReg, Structure::inlineCapacityOffset()), scratchReg);
    emitInitializeInlineStorage(resultReg, scratchReg);
    mutatorFence(*m_vm);
    boxCell(resultReg, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);

    addSlowCase(slowCases);
}

void JIT::emitSlow_op_new_object(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    RegisterID structureReg = regT3;

    auto bytecode = currentInstruction->as<OpNewObject>();
    VirtualRegister dst = bytecode.m_dst;
    callOperationNoExceptionCheck(operationNewObject, TrustedImmPtr(&vm()), structureReg);
    boxCell(returnValueGPR, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);
}


void JIT::emit_op_overrides_has_instance(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpOverridesHasInstance>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister constructor = bytecode.m_constructor;
    VirtualRegister hasInstanceValue = bytecode.m_hasInstanceValue;

    emitGetVirtualRegisterPayload(hasInstanceValue, regT2);

    // We don't jump if we know what Symbol.hasInstance would do.
    move(TrustedImm32(1), regT0);
    loadGlobalObject(regT1);
    Jump customHasInstanceValue = branchPtr(NotEqual, regT2, Address(regT1, JSGlobalObject::offsetOfFunctionProtoHasInstanceSymbolFunction()));
    // We know that constructor is an object from the way bytecode is emitted for instanceof expressions.
    emitGetVirtualRegisterPayload(constructor, regT2);
    // Check that constructor 'ImplementsDefaultHasInstance' i.e. the object is not a C-API user nor a bound function.
    test8(Zero, Address(regT2, JSCell::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance), regT0);
    customHasInstanceValue.link(this);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}


void JIT::emit_op_instanceof(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInstanceof>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_value;
    VirtualRegister proto = bytecode.m_prototype;

    using BaselineJITRegisters::Instanceof::resultJSR;
    using BaselineJITRegisters::Instanceof::valueJSR;
    using BaselineJITRegisters::Instanceof::protoJSR;
    using BaselineJITRegisters::Instanceof::FastPath::stubInfoGPR;

    emitGetVirtualRegister(value, valueJSR);
    emitGetVirtualRegister(proto, protoJSR);
    
    // Check that proto are cells. baseVal must be a cell - this is checked by the get_by_id for Symbol.hasInstance.
    emitJumpSlowCaseIfNotJSCell(valueJSR, value);
    emitJumpSlowCaseIfNotJSCell(protoJSR, proto);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITInstanceOfGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex),
        RegisterSetBuilder::stubUnavailableRegisters(),
        resultJSR.payloadGPR(),
        valueJSR.payloadGPR(),
        protoJSR.payloadGPR(),
        stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
#if USE(JSVALUE32_64)
    boxBoolean(resultJSR.payloadGPR(), resultJSR);
#endif
    addSlowCase();
    m_instanceOfs.append(gen);

    setFastPathResumePoint();
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_instanceof(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    JITInstanceOfGenerator& gen = m_instanceOfs[m_instanceOfIndex++];

    Label coldPathBegin = label();

    using BaselineJITRegisters::Instanceof::valueJSR;
    using BaselineJITRegisters::Instanceof::protoJSR;
    using BaselineJITRegisters::Instanceof::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::Instanceof::SlowPath::stubInfoGPR;

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    callOperation<decltype(operationInstanceOfOptimize)>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        globalObjectGPR, stubInfoGPR, valueJSR, protoJSR);
    static_assert(BaselineJITRegisters::Instanceof::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_is_empty(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsEmpty>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

#if USE(JSVALUE64)
    emitGetVirtualRegister(value, regT0);
#elif USE(JSVALUE32_64)
    emitGetVirtualRegisterTag(value, regT0);
#endif
    isEmpty(regT0, regT0);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_typeof_is_undefined(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTypeofIsUndefined>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitGetVirtualRegister(value, jsRegT10);
    Jump isCell = branchIfCell(jsRegT10);

    isUndefined(jsRegT10, regT0);
    Jump done = jump();
    
    isCell.link(this);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(jsRegT10.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT0);
    Jump notMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    emitLoadStructure(vm(), jsRegT10.payloadGPR(), regT1);
    loadGlobalObject(regT0);
    loadPtr(Address(regT1, Structure::globalObjectOffset()), regT1);
    comparePtr(Equal, regT0, regT1, regT0);

    notMasqueradesAsUndefined.link(this);
    done.link(this);
    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_is_undefined_or_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsUndefinedOrNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitGetVirtualRegister(value, jsRegT10);

    emitTurnUndefinedIntoNull(jsRegT10);
    isNull(jsRegT10, regT0);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}


void JIT::emit_op_is_boolean(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsBoolean>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

#if USE(JSVALUE64)
    emitGetVirtualRegister(value, regT0);
    xor64(TrustedImm32(JSValue::ValueFalse), regT0);
    test64(Zero, regT0, TrustedImm32(static_cast<int32_t>(~1)), regT0);
#elif USE(JSVALUE32_64)
    emitGetVirtualRegisterTag(value, regT0);
    compare32(Equal, regT0, TrustedImm32(JSValue::BooleanTag), regT0);
#endif

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_is_number(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsNumber>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

#if USE(JSVALUE64)
    emitGetVirtualRegister(value, regT0);
    test64(NonZero, regT0, numberTagRegister, regT0);
#elif USE(JSVALUE32_64)
    emitGetVirtualRegisterTag(value, regT0);
    add32(TrustedImm32(1), regT0);
    compare32(Below, regT0, TrustedImm32(JSValue::LowestTag + 1), regT0);
#endif

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

#if USE(BIGINT32)
void JIT::emit_op_is_big_int(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsBigInt>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitGetVirtualRegister(value, regT0);
    Jump isCell = branchIfCell(regT0);

    move(TrustedImm64(JSValue::BigInt32Mask), regT1);
    and64(regT1, regT0);
    compare64(Equal, regT0, TrustedImm32(JSValue::BigInt32Tag), regT0);
    boxBoolean(regT0, jsRegT10);
    Jump done = jump();

    isCell.link(this);
    compare8(Equal, Address(regT0, JSCell::typeInfoTypeOffset()), TrustedImm32(HeapBigIntType), regT0);
    boxBoolean(regT0, jsRegT10);

    done.link(this);
    emitPutVirtualRegister(dst, jsRegT10);
}
#else // if !USE(BIGINT32)
NO_RETURN void JIT::emit_op_is_big_int(const JSInstruction*)
{
    // If we only have HeapBigInts, then we emit isCellWithType instead of isBigInt.
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

void JIT::emit_op_is_cell_with_type(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsCellWithType>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    int type = bytecode.m_type;

    emitGetVirtualRegister(value, jsRegT32);

    move(TrustedImm32(0), regT0);
    Jump isNotCell = branchIfNotCell(jsRegT32);
    compare8(Equal, Address(jsRegT32.payloadGPR(), JSCell::typeInfoTypeOffset()), TrustedImm32(type), regT0);
    isNotCell.link(this);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_is_object(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsObject>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitGetVirtualRegister(value, jsRegT32);

    move(TrustedImm32(0), regT0);
    Jump isNotCell = branchIfNotCell(jsRegT32);
    compare8(AboveOrEqual, Address(jsRegT32.payloadGPR(), JSCell::typeInfoTypeOffset()), TrustedImm32(ObjectType), regT0);
    isNotCell.link(this);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_to_primitive(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToPrimitive>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;

    emitGetVirtualRegister(src, jsRegT10);
    
    Jump isImm = branchIfNotCell(jsRegT10);
    addSlowCase(branchIfObject(jsRegT10.payloadGPR()));
    isImm.link(this);

    if (dst != src)
        emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_to_property_key(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToPropertyKey>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;

    emitGetVirtualRegister(src, jsRegT10);

    addSlowCase(branchIfNotCell(jsRegT10));
    Jump done = branchIfSymbol(jsRegT10.payloadGPR());
    addSlowCase(branchIfNotString(jsRegT10.payloadGPR()));

    done.link(this);
    if (src != dst)
        emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_set_function_name(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSetFunctionName>();

    using SlowOperation = decltype(operationSetFunctionName);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg functionGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs nameJSR = preferredArgumentJSR<SlowOperation, 2>();

    emitGetVirtualRegisterPayload(bytecode.m_function, functionGPR);
    emitGetVirtualRegister(bytecode.m_name, nameJSR);
    loadGlobalObject(globalObjectGPR);
    callOperation(operationSetFunctionName, globalObjectGPR, functionGPR, nameJSR);
}

void JIT::emit_op_not(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNot>();
    emitGetVirtualRegister(bytecode.m_operand, jsRegT10);

    addSlowCase(branchIfNotBoolean(jsRegT10, regT2));
    xorPtr(TrustedImm32(1), jsRegT10.payloadGPR());

    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_jfalse(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJfalse>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    using BaselineJITRegisters::JFalse::valueJSR;

    emitGetVirtualRegister(bytecode.m_condition, valueJSR);
    emitNakedNearCall(vm().getCTIStub(valueIsFalseyGenerator).retaggedCode<NoPtrTag>());
    addJump(branchTest32(NonZero, regT0), target);
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::valueIsFalseyGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using BaselineJITRegisters::JFalse::valueJSR; // Incoming
    constexpr GPRReg scratch1GPR = regT1;
    constexpr GPRReg scratch2GPR = regT5;
    constexpr GPRReg globalObjectGPR = regT4;
    static_assert(noOverlap(valueJSR, scratch1GPR, scratch2GPR, globalObjectGPR));

    constexpr bool shouldCheckMasqueradesAsUndefined = true;

    jit.tagReturnAddress();

    loadGlobalObject(jit, globalObjectGPR);
    jit.move(TrustedImm32(1), regT0);
    auto isFalsey = jit.branchIfFalsey(vm, valueJSR, scratch1GPR, scratch2GPR, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, globalObjectGPR);
    jit.move(TrustedImm32(0), regT0);
    isFalsey.link(&jit);
    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: valueIsFalsey");
}

void JIT::emit_op_jeq_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeqNull>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(src, jsRegT10);
    Jump isImmediate = branchIfNotCell(jsRegT10);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    Jump isNotMasqueradesAsUndefined = branchTest8(Zero, Address(jsRegT10.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    emitLoadStructure(vm(), jsRegT10.payloadGPR(), regT2);
    loadGlobalObject(regT0);
    addJump(branchPtr(Equal, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump masqueradesGlobalObjectIsForeign = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    emitTurnUndefinedIntoNull(jsRegT10);
    addJump(branchIfNull(jsRegT10), target);

    isNotMasqueradesAsUndefined.link(this);
    masqueradesGlobalObjectIsForeign.link(this);
}

void JIT::emit_op_jneq_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqNull>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(src, jsRegT10);
    Jump isImmediate = branchIfNotCell(jsRegT10);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    addJump(branchTest8(Zero, Address(jsRegT10.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);
    emitLoadStructure(vm(), jsRegT10.payloadGPR(), regT2);
    loadGlobalObject(regT0);
    addJump(branchPtr(NotEqual, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    emitTurnUndefinedIntoNull(jsRegT10);
    addJump(branchIfNotNull(jsRegT10), target);

    wasNotImmediate.link(this);
}

void JIT::emit_op_jundefined_or_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJundefinedOrNull>();
    VirtualRegister value = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

#if USE(JSVALUE64)
    emitGetVirtualRegister(value, jsRegT10);
#elif USE(JSVALUE32_64)
    emitGetVirtualRegisterTag(value, jsRegT10.tagGPR());
#endif

    emitTurnUndefinedIntoNull(jsRegT10);
    addJump(branchIfNull(jsRegT10), target);
}

void JIT::emit_op_jnundefined_or_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJnundefinedOrNull>();
    VirtualRegister value = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

#if USE(JSVALUE64)
    emitGetVirtualRegister(value, jsRegT10);
#elif USE(JSVALUE32_64)
    emitGetVirtualRegisterTag(value, jsRegT10.tagGPR());
#endif

    emitTurnUndefinedIntoNull(jsRegT10);
    addJump(branchIfNotNull(jsRegT10), target);
}

void JIT::emit_op_jeq_ptr(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeqPtr>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(src, jsRegT10);
#if USE(JSVALUE32_64)
    // ON JSVALUE64 the pointer comparison below catches this case
    Jump notCell = branchIfNotCell(jsRegT10);
#endif
    loadCodeBlockConstantPayload(bytecode.m_specialPointer, regT2);
    addJump(branchPtr(Equal, jsRegT10.payloadGPR(), regT2), target);
#if USE(JSVALUE32_64)
    notCell.link(this);
#endif
}

void JIT::emit_op_jneq_ptr(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqPtr>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    
    emitGetVirtualRegister(src, jsRegT10);
#if USE(JSVALUE32_64)
    // ON JSVALUE64 the pointer comparison below catches this case
    Jump notCell = branchIfNotCell(jsRegT10);
#endif
    loadCodeBlockConstantPayload(bytecode.m_specialPointer, regT2);
    CCallHelpers::Jump equal = branchPtr(Equal, jsRegT10.payloadGPR(), regT2);
#if USE(JSVALUE32_64)
    notCell.link(this);
#endif
    store8ToMetadata(TrustedImm32(1), bytecode, OpJneqPtr::Metadata::offsetOfHasJumped());
    addJump(jump(), target);
    equal.link(this);
}

#if USE(JSVALUE64)

void JIT::emit_op_eq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEq>();
    emitGetVirtualRegister(bytecode.m_lhs, regT0);
    emitGetVirtualRegister(bytecode.m_rhs, regT1);
    emitJumpSlowCaseIfNotInt(regT0, regT1, regT2);
    compare32(Equal, regT1, regT0, regT0);
    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_jeq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    emitGetVirtualRegister(bytecode.m_lhs, regT0);
    emitGetVirtualRegister(bytecode.m_rhs, regT1);
    emitJumpSlowCaseIfNotInt(regT0, regT1, regT2);
    addJump(branch32(Equal, regT0, regT1), target);
}

#endif

void JIT::emit_op_jtrue(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJtrue>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    using BaselineJITRegisters::JTrue::valueJSR;

    emitGetVirtualRegister(bytecode.m_condition, valueJSR);
    emitNakedNearCall(vm().getCTIStub(valueIsTruthyGenerator).retaggedCode<NoPtrTag>());
    addJump(branchTest32(NonZero, regT0), target);
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::valueIsTruthyGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using BaselineJITRegisters::JTrue::valueJSR; // Incoming
    constexpr GPRReg scratch1GPR = regT1;
    constexpr GPRReg scratch2GPR = regT5;
    constexpr GPRReg globalObjectGPR = regT4;
    static_assert(noOverlap(valueJSR, scratch1GPR, scratch2GPR, globalObjectGPR));

    constexpr bool shouldCheckMasqueradesAsUndefined = true;

    jit.tagReturnAddress();

    loadGlobalObject(jit, globalObjectGPR);
    jit.move(TrustedImm32(1), regT0);
    auto isTruthy = jit.branchIfTruthy(vm, valueJSR, scratch1GPR, scratch2GPR, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, globalObjectGPR);
    jit.move(TrustedImm32(0), regT0);
    isTruthy.link(&jit);
    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: valueIsTruthy");
}

#if USE(JSVALUE64)

void JIT::emit_op_neq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNeq>();
    emitGetVirtualRegister(bytecode.m_lhs, regT0);
    emitGetVirtualRegister(bytecode.m_rhs, regT1);
    emitJumpSlowCaseIfNotInt(regT0, regT1, regT2);
    compare32(NotEqual, regT1, regT0, regT0);
    boxBoolean(regT0, jsRegT10);

    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_jneq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    emitGetVirtualRegister(bytecode.m_lhs, regT0);
    emitGetVirtualRegister(bytecode.m_rhs, regT1);
    emitJumpSlowCaseIfNotInt(regT0, regT1, regT2);
    addJump(branch32(NotEqual, regT0, regT1), target);
}

#endif

void JIT::emit_op_throw(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpThrow>();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();

    using BaselineJITRegisters::Throw::thrownValueJSR;
    using BaselineJITRegisters::Throw::bytecodeOffsetGPR;

    emitGetVirtualRegister(bytecode.m_value, thrownValueJSR);
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    emitNakedNearJump(vm().getCTIStub(op_throw_handlerGenerator).code());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::op_throw_handlerGenerator(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::Throw::globalObjectGPR;
    using BaselineJITRegisters::Throw::thrownValueJSR; // Incoming
    using BaselineJITRegisters::Throw::bytecodeOffsetGPR; // Incoming

#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
    {
        constexpr GPRReg scratchGPR = globalObjectGPR;
        static_assert(noOverlap(scratchGPR, thrownValueJSR, bytecodeOffsetGPR), "Should not clobber incoming parameters");
        jit.loadPtr(&vm.topEntryFrame, scratchGPR);
        jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(scratchGPR);
    }
#endif

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<decltype(operationThrow)>(globalObjectGPR, thrownValueJSR);
    Call operation = jit.call(OperationPtrTag);

    jit.jumpToExceptionHandler(vm);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link<OperationPtrTag>(operation, operationThrow);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: op_throw_handler");
}

#if USE(JSVALUE64)

template<typename Op>
void JIT::compileOpStrictEq(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitGetVirtualRegister(src1, regT0);
    emitGetVirtualRegister(src2, regT1);

#if USE(BIGINT32)
    /* At a high level we do (assuming 'type' to be StrictEq):
    If (left is Double || right is Double)
        goto slowPath;
    result = (left == right);
    if (result)
        goto done;
    if (left is Cell || right is Cell)
        goto slowPath;
    done:
    return result;
    */

    // This fragment implements (left is Double || right is Double), with a single branch instead of the 4 that would be naively required if we used branchIfInt32/branchIfNumber
    // The trick is that if a JSValue is an Int32, then adding 1<<49 to it will make it overflow, leaving all high bits at 0
    // If it is not a number at all, then 1<<49 will be its only high bit set
    // Leaving only doubles above or equal 1<<50.
    move(regT0, regT2);
    move(regT1, regT3);
    move(TrustedImm64(JSValue::LowestOfHighBits), regT5);
    add64(regT5, regT2);
    add64(regT5, regT3);
    lshift64(TrustedImm32(1), regT5);
    or64(regT2, regT3);
    addSlowCase(branch64(AboveOrEqual, regT3, regT5));

    compare64(Equal, regT0, regT1, regT5);
    Jump done = branchTest64(NonZero, regT5);

    move(regT0, regT2);
    // Jump slow if at least one is a cell (to cover strings and BigInts).
    and64(regT1, regT2);
    // FIXME: we could do something more precise: unless there is a BigInt32, we only need to do the slow path if both are strings
    addSlowCase(branchIfCell(regT2));

    done.link(this);
    if constexpr (std::is_same<Op, OpNstricteq>::value)
        xor64(TrustedImm64(1), regT5);
    boxBoolean(regT5, JSValueRegs { regT5 });
    emitPutVirtualRegister(dst, regT5);
#else // if !USE(BIGINT32)
    // Jump slow if both are cells (to cover strings).
    move(regT0, regT2);
    or64(regT1, regT2);
    addSlowCase(branchIfCell(regT2));

    // Jump slow if either is a double. First test if it's an integer, which is fine, and then test
    // if it's a double.
    Jump leftOK = branchIfInt32(regT0);
    addSlowCase(branchIfNumber(regT0));
    leftOK.link(this);
    Jump rightOK = branchIfInt32(regT1);
    addSlowCase(branchIfNumber(regT1));
    rightOK.link(this);

    if constexpr (std::is_same<Op, OpStricteq>::value)
        compare64(Equal, regT1, regT0, regT0);
    else
        compare64(NotEqual, regT1, regT0, regT0);
    boxBoolean(regT0, jsRegT10);

    emitPutVirtualRegister(dst, jsRegT10);
#endif
}

void JIT::emit_op_stricteq(const JSInstruction* currentInstruction)
{
    compileOpStrictEq<OpStricteq>(currentInstruction);
}

void JIT::emit_op_nstricteq(const JSInstruction* currentInstruction)
{
    compileOpStrictEq<OpNstricteq>(currentInstruction);
}

template<typename Op>
void JIT::compileOpStrictEqJump(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    int target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitGetVirtualRegister(src1, regT0);
    emitGetVirtualRegister(src2, regT1);

#if USE(BIGINT32)
    /* At a high level we do (assuming 'type' to be StrictEq):
    If (left is Double || right is Double)
       goto slowPath;
    if (left == right)
       goto taken;
    if (left is Cell || right is Cell)
       goto slowPath;
    goto notTaken;
    */

    // This fragment implements (left is Double || right is Double), with a single branch instead of the 4 that would be naively required if we used branchIfInt32/branchIfNumber
    // The trick is that if a JSValue is an Int32, then adding 1<<49 to it will make it overflow, leaving all high bits at 0
    // If it is not a number at all, then 1<<49 will be its only high bit set
    // Leaving only doubles above or equal 1<<50.
    move(regT0, regT2);
    move(regT1, regT3);
    move(TrustedImm64(JSValue::LowestOfHighBits), regT5);
    add64(regT5, regT2);
    add64(regT5, regT3);
    lshift64(TrustedImm32(1), regT5);
    or64(regT2, regT3);
    addSlowCase(branch64(AboveOrEqual, regT3, regT5));

    Jump areEqual = branch64(Equal, regT0, regT1);
    if constexpr (std::is_same<Op, OpJstricteq>::value)
        addJump(areEqual, target);

    move(regT0, regT2);
    // Jump slow if at least one is a cell (to cover strings and BigInts).
    and64(regT1, regT2);
    // FIXME: we could do something more precise: unless there is a BigInt32, we only need to do the slow path if both are strings
    addSlowCase(branchIfCell(regT2));

    if constexpr (std::is_same<Op, OpJnstricteq>::value) {
        addJump(jump(), target);
        areEqual.link(this);
    }
#else // if !USE(BIGINT32)
    // Jump slow if both are cells (to cover strings).
    move(regT0, regT2);
    or64(regT1, regT2);
    addSlowCase(branchIfCell(regT2));

    // Jump slow if either is a double. First test if it's an integer, which is fine, and then test
    // if it's a double.
    Jump leftOK = branchIfInt32(regT0);
    addSlowCase(branchIfNumber(regT0));
    leftOK.link(this);
    Jump rightOK = branchIfInt32(regT1);
    addSlowCase(branchIfNumber(regT1));
    rightOK.link(this);
    if constexpr (std::is_same<Op, OpJstricteq>::value)
        addJump(branch64(Equal, regT1, regT0), target);
    else
        addJump(branch64(NotEqual, regT1, regT0), target);
#endif
}

void JIT::emit_op_jstricteq(const JSInstruction* currentInstruction)
{
    compileOpStrictEqJump<OpJstricteq>(currentInstruction);
}

void JIT::emit_op_jnstricteq(const JSInstruction* currentInstruction)
{
    compileOpStrictEqJump<OpJnstricteq>(currentInstruction);
}

void JIT::emitSlow_op_jstricteq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    loadGlobalObject(regT2);
    callOperation(operationCompareStrictEq, regT2, regT0, regT1);
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emitSlow_op_jnstricteq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJnstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    loadGlobalObject(regT2);
    callOperation(operationCompareStrictEq, regT2, regT0, regT1);
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);
}

#endif

void JIT::emit_op_to_number(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumber>();
    VirtualRegister dstVReg = bytecode.m_dst;
    VirtualRegister srcVReg = bytecode.m_operand;

    emitGetVirtualRegister(srcVReg, jsRegT10);
    
    addSlowCase(branchIfNotNumber(jsRegT10, regT2));

    emitValueProfilingSite(bytecode, jsRegT10);
    if (srcVReg != dstVReg)
        emitPutVirtualRegister(dstVReg, jsRegT10);
}

void JIT::emit_op_to_numeric(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumeric>();
    VirtualRegister dstVReg = bytecode.m_dst;
    VirtualRegister srcVReg = bytecode.m_operand;

    emitGetVirtualRegister(srcVReg, jsRegT10);

    Jump isNotCell = branchIfNotCell(jsRegT10);
    addSlowCase(branchIfNotHeapBigInt(jsRegT10.payloadGPR()));
    Jump isBigInt = jump();

    isNotCell.link(this);
    addSlowCase(branchIfNotNumber(jsRegT10, regT2));
    isBigInt.link(this);

    emitValueProfilingSite(bytecode, jsRegT10);
    if (srcVReg != dstVReg)
        emitPutVirtualRegister(dstVReg, jsRegT10);
}

void JIT::emit_op_to_string(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToString>();
    VirtualRegister dstVReg = bytecode.m_dst;
    VirtualRegister srcVReg = bytecode.m_operand;

    emitGetVirtualRegister(srcVReg, jsRegT10);

    addSlowCase(branchIfNotCell(jsRegT10));
    addSlowCase(branchIfNotString(jsRegT10.payloadGPR()));

    if (srcVReg != dstVReg)
        emitPutVirtualRegister(dstVReg, jsRegT10);
}

void JIT::emit_op_to_object(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToObject>();
    VirtualRegister dstVReg = bytecode.m_dst;
    VirtualRegister srcVReg = bytecode.m_operand;

    emitGetVirtualRegister(srcVReg, jsRegT10);

    addSlowCase(branchIfNotCell(jsRegT10));
    addSlowCase(branchIfNotObject(jsRegT10.payloadGPR()));

    emitValueProfilingSite(bytecode, jsRegT10);
    if (srcVReg != dstVReg)
        emitPutVirtualRegister(dstVReg, jsRegT10);
}

void JIT::emit_op_catch(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCatch>();

    restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

    move(TrustedImmPtr(m_vm), regT3);
    loadPtr(Address(regT3, VM::callFrameForCatchOffset()), callFrameRegister);
    storePtr(TrustedImmPtr(nullptr), Address(regT3, VM::callFrameForCatchOffset()));

    addPtr(TrustedImm32(stackPointerOffsetFor(m_unlinkedCodeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);

    // When the LLInt throws an exception, there is a chance that we've already tiered up
    // the same CodeBlock to baseline, and we'll catch the exception in the baseline JIT (because
    // we updated the exception handlers to point here). Because the LLInt uses a different value
    // inside s_constantsGPR, the callee saves we restore above may not contain the correct register.
    // So we replenish it here.
    {
        loadPtr(addressFor(CallFrameSlot::codeBlock), regT0);
        loadPtr(Address(regT0, CodeBlock::offsetOfJITData()), s_constantsGPR);
    }

    callOperationNoExceptionCheck(operationRetrieveAndClearExceptionIfCatchable, TrustedImmPtr(&vm()));
    Jump isCatchableException = branchTest32(NonZero, returnValueGPR);
    jumpToExceptionHandler(vm());
    isCatchableException.link(this);

    boxCell(returnValueGPR, jsRegT10);
    emitPutVirtualRegister(bytecode.m_exception, jsRegT10);

    loadValue(Address(jsRegT10.payloadGPR(), Exception::valueOffset()), jsRegT10);
    emitPutVirtualRegister(bytecode.m_thrownValue, jsRegT10);

#if ENABLE(DFG_JIT)
    // FIXME: consider inline caching the process of doing OSR entry, including
    // argument type proofs, storing locals to the buffer, etc
    // https://bugs.webkit.org/show_bug.cgi?id=175598

    callOperationNoExceptionCheck(operationTryOSREnterAtCatchAndValueProfile, TrustedImmPtr(&vm()), m_bytecodeIndex.asBits());
    auto skipOSREntry = branchTestPtr(Zero, returnValueGPR);
    emitPutToCallFrameHeader(returnValueGPR2, CallFrameSlot::codeBlock);
    emitRestoreCalleeSaves();
    farJump(returnValueGPR, ExceptionHandlerPtrTag);
    skipOSREntry.link(this);
#endif // ENABLE(DFG_JIT)
}

void JIT::emit_op_identity_with_profile(const JSInstruction*)
{
    // We don't need to do anything here...
}

void JIT::emit_op_get_parent_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetParentScope>();
    VirtualRegister currentScope = bytecode.m_scope;
    emitGetVirtualRegisterPayload(currentScope, regT0);
    loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    boxCell(regT0, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_switch_imm(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSwitchImm>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    const UnlinkedSimpleJumpTable& unlinkedTable = m_unlinkedCodeBlock->unlinkedSwitchJumpTable(tableIndex);
    SimpleJumpTable& linkedTable = m_switchJumpTables[tableIndex];
    m_switches.append(SwitchRecord(tableIndex, m_bytecodeIndex, defaultOffset, SwitchRecord::Immediate));
    linkedTable.ensureCTITable(unlinkedTable);

    emitGetVirtualRegister(scrutinee, jsRegT10);
    auto notInt32 = branchIfNotInt32(jsRegT10);
    sub32(Imm32(unlinkedTable.m_min), jsRegT10.payloadGPR());

    addJump(branch32(AboveOrEqual, jsRegT10.payloadGPR(), Imm32(linkedTable.m_ctiOffsets.size())), defaultOffset);
    move(TrustedImmPtr(linkedTable.m_ctiOffsets.data()), regT2);
    loadPtr(BaseIndex(regT2, jsRegT10.payloadGPR(), ScalePtr), regT2);
    farJump(regT2, JSSwitchPtrTag);

    notInt32.link(this);
    callOperationNoExceptionCheck(operationSwitchImmWithUnknownKeyType, TrustedImmPtr(&vm()), jsRegT10, tableIndex, unlinkedTable.m_min);
    farJump(returnValueGPR, JSSwitchPtrTag);
}

void JIT::emit_op_switch_char(const JSInstruction* currentInstruction)
{
    // FIXME: We should have a fast path.
    // https://bugs.webkit.org/show_bug.cgi?id=224521
    auto bytecode = currentInstruction->as<OpSwitchChar>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    const UnlinkedSimpleJumpTable& unlinkedTable = m_unlinkedCodeBlock->unlinkedSwitchJumpTable(tableIndex);
    SimpleJumpTable& linkedTable = m_switchJumpTables[tableIndex];
    m_switches.append(SwitchRecord(tableIndex, m_bytecodeIndex, defaultOffset, SwitchRecord::Character));
    linkedTable.ensureCTITable(unlinkedTable);

    using SlowOperation = decltype(operationSwitchCharWithUnknownKeyType);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr JSValueRegs scrutineeJSR = preferredArgumentJSR<SlowOperation, 1>();

    emitGetVirtualRegister(scrutinee, scrutineeJSR);
    loadGlobalObject(globalObjectGPR);
    callOperation(operationSwitchCharWithUnknownKeyType, globalObjectGPR, scrutineeJSR, tableIndex, unlinkedTable.m_min);
    farJump(returnValueGPR, JSSwitchPtrTag);
}

void JIT::emit_op_switch_string(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSwitchString>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    const UnlinkedStringJumpTable& unlinkedTable = m_unlinkedCodeBlock->unlinkedStringSwitchJumpTable(tableIndex);
    StringJumpTable& linkedTable = m_stringSwitchJumpTables[tableIndex];
    m_switches.append(SwitchRecord(tableIndex, m_bytecodeIndex, defaultOffset, SwitchRecord::String));
    linkedTable.ensureCTITable(unlinkedTable);

    using SlowOperation = decltype(operationSwitchStringWithUnknownKeyType);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr JSValueRegs scrutineeJSR = preferredArgumentJSR<SlowOperation, 1>();

    emitGetVirtualRegister(scrutinee, scrutineeJSR);
    loadGlobalObject(globalObjectGPR);
    callOperation(operationSwitchStringWithUnknownKeyType, globalObjectGPR, scrutineeJSR, tableIndex);
    farJump(returnValueGPR, JSSwitchPtrTag);
}

void JIT::emit_op_eq_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEqNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_operand;

    emitGetVirtualRegister(src1, jsRegT10);
    Jump isImmediate = branchIfNotCell(jsRegT10);

    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(jsRegT10.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT0);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    emitLoadStructure(vm(), jsRegT10.payloadGPR(), regT2);
    loadGlobalObject(regT0);
    loadPtr(Address(regT2, Structure::globalObjectOffset()), regT2);
    comparePtr(Equal, regT0, regT2, regT0);
    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    emitTurnUndefinedIntoNull(jsRegT10);
    isNull(jsRegT10, regT0);

    wasNotImmediate.link(this);
    wasNotMasqueradesAsUndefined.link(this);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_neq_null(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNeqNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_operand;

    emitGetVirtualRegister(src1, jsRegT10);
    Jump isImmediate = branchIfNotCell(jsRegT10);

    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(jsRegT10.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(1), regT0);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    emitLoadStructure(vm(), jsRegT10.payloadGPR(), regT2);
    loadGlobalObject(regT0);
    loadPtr(Address(regT2, Structure::globalObjectOffset()), regT2);
    comparePtr(NotEqual, regT0, regT2, regT0);
    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    emitTurnUndefinedIntoNull(jsRegT10);
    isNotNull(jsRegT10, regT0);

    wasNotImmediate.link(this);
    wasNotMasqueradesAsUndefined.link(this);

    boxBoolean(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_enter(const JSInstruction*)
{
    // Even though CTI doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.

    ASSERT(m_bytecodeIndex.offset() == 0);
    size_t count = m_unlinkedCodeBlock->numVars();
    uint32_t localsToInit = count - CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters();
    RELEASE_ASSERT(localsToInit < count);

    using BaselineJITRegisters::Enter::canBeOptimizedGPR;
    using BaselineJITRegisters::Enter::localsToInitGPR;

    move(TrustedImm32(canBeOptimized()), canBeOptimizedGPR);
    move(TrustedImm32(localsToInit), localsToInitGPR);
    emitNakedNearCall(vm().getCTIStub(op_enter_handlerGenerator).retaggedCode<NoPtrTag>());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::op_enter_handlerGenerator(VM& vm)
{
    CCallHelpers jit;

    jit.emitCTIThunkPrologue();

    using BaselineJITRegisters::Enter::canBeOptimizedGPR; // Incoming

    {
        using BaselineJITRegisters::Enter::localsToInitGPR; // Incoming
        constexpr GPRReg iteratorGPR = regT4;
        constexpr GPRReg endGPR = regT5;
        constexpr JSValueRegs undefinedJSR = jsRegT32;
        static_assert(noOverlap(localsToInitGPR, canBeOptimizedGPR, iteratorGPR, undefinedJSR));

        size_t startLocal = CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters();
        int startOffset = virtualRegisterForLocal(startLocal).offset();
        ASSERT(startOffset <= 0);
        jit.subPtr(GPRInfo::callFrameRegister, TrustedImm32((-startOffset - 1) * sizeof(Register)), endGPR);
        jit.mul32(TrustedImm32(sizeof(Register)), localsToInitGPR, localsToInitGPR);
        jit.subPtr(endGPR, localsToInitGPR, iteratorGPR);
        jit.moveTrustedValue(jsUndefined(), undefinedJSR);

#if CPU(ARM64)
        auto evenCase = jit.branchTest32(Zero, localsToInitGPR, TrustedImm32(sizeof(Register)));
        jit.store64(undefinedJSR.payloadGPR(), PostIndexAddress(iteratorGPR, sizeof(Register)));
        evenCase.link(&jit);
        auto initLoop = jit.label();
        Jump initDone = jit.branch32(GreaterThanOrEqual, iteratorGPR, endGPR);
        {
            jit.storePair64(undefinedJSR.payloadGPR(), undefinedJSR.payloadGPR(), PostIndexAddress(iteratorGPR, sizeof(Register) * 2));
            jit.jump(initLoop);
        }
        initDone.link(&jit);
#else
        auto initLoop = jit.label();
        Jump initDone = jit.branch32(GreaterThanOrEqual, iteratorGPR, endGPR);
        {
            jit.storeValue(undefinedJSR, Address(iteratorGPR));
            jit.addPtr(TrustedImm32(sizeof(Register)), iteratorGPR);
            jit.jump(initLoop);
        }
        initDone.link(&jit);
#endif
    }

    // emitWriteBarrier(m_codeBlock).
    static_assert(noOverlap(canBeOptimizedGPR, argumentGPR1, argumentGPR2));
    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), argumentGPR1);
    Jump ownerIsRememberedOrInEden = jit.barrierBranch(vm, argumentGPR1, argumentGPR2);

    // op_enter is always at bytecodeOffset 0.
    jit.store32(TrustedImm32(0), tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);

    // save canBeOptimizedGPR (arguments to call below are in registers on all platforms, so ok to stack this).
    // Note: we will do a call, so can't use pushToSave, as it does not maintain ABI stack alignment.
    jit.subPtr(TrustedImmPtr(16), stackPointerRegister);
    jit.storePtr(canBeOptimizedGPR, Address(stackPointerRegister));

    jit.setupArguments<decltype(operationWriteBarrierSlowPath)>(TrustedImmPtr(&vm), argumentGPR1);
    Call operationWriteBarrierCall = jit.call(OperationPtrTag);

    jit.loadPtr(Address(stackPointerRegister), canBeOptimizedGPR); // Restore canBeOptimizedGPR
    jit.addPtr(TrustedImmPtr(16), stackPointerRegister); // Restore stack pointer

    ownerIsRememberedOrInEden.link(&jit);

#if ENABLE(DFG_JIT)
    Call operationOptimizeCall;
    if (Options::useDFGJIT()) {
        // emitEnterOptimizationCheck().
        JumpList skipOptimize;

        skipOptimize.append(jit.branchTest32(Zero, canBeOptimizedGPR));

        jit.loadPtr(addressFor(CallFrameSlot::codeBlock), argumentGPR1);
        skipOptimize.append(jit.branchAdd32(Signed, TrustedImm32(Options::executionCounterIncrementForEntry()), Address(argumentGPR1, CodeBlock::offsetOfJITExecuteCounter())));

        jit.copyLLIntBaselineCalleeSavesFromFrameOrRegisterToEntryFrameCalleeSavesBuffer(vm.topEntryFrame);
        jit.prepareCallOperation(vm);

#if OS(WINDOWS) && CPU(X86_64)
        // On Windows, return values larger than 8 bytes are retuened via an implicit pointer passed as
        // the first argument, and remaining arguments are shifted to the right. Make space for this.
        static_assert(sizeof(SlowPathReturnType) == 16, "Assumed by generated call site below");
        jit.subPtr(MacroAssembler::TrustedImm32(16), MacroAssembler::stackPointerRegister);
        jit.move(MacroAssembler::stackPointerRegister, JIT::argumentGPR0);
        constexpr GPRReg vmPointerArgGPR { GPRInfo::argumentGPR1 };
        constexpr GPRReg bytecodeIndexBitsArgGPR { GPRInfo::argumentGPR2 };
#else
        constexpr GPRReg vmPointerArgGPR { GPRInfo::argumentGPR0 };
        constexpr GPRReg bytecodeIndexBitsArgGPR { GPRInfo::argumentGPR1 };
#endif
        jit.move(TrustedImmPtr(&vm), vmPointerArgGPR);
        jit.move(TrustedImm32(0), bytecodeIndexBitsArgGPR);

        operationOptimizeCall = jit.call(OperationPtrTag);

#if OS(WINDOWS) && CPU(X86_64)
        jit.pop(GPRInfo::returnValueGPR); // targetPC
        jit.pop(GPRInfo::returnValueGPR2); // dataBuffer (unused, but needs popping to restore stack)
#endif

        skipOptimize.append(jit.branchTestPtr(Zero, returnValueGPR));
        jit.farJump(returnValueGPR, GPRInfo::callFrameRegister);

        skipOptimize.link(&jit);
    }
#endif // ENABLE(DFG_JIT)

    jit.emitCTIThunkEpilogue();
    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link<OperationPtrTag>(operationWriteBarrierCall, operationWriteBarrierSlowPath);
#if ENABLE(DFG_JIT)
    if (Options::useDFGJIT())
        patchBuffer.link<OperationPtrTag>(operationOptimizeCall, operationOptimize);
#endif
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: op_enter_handler");
}

void JIT::emit_op_get_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetScope>();
    VirtualRegister dst = bytecode.m_dst;
    emitGetFromCallFrameHeaderPtr(CallFrameSlot::callee, regT0);
    loadPtr(Address(regT0, JSFunction::offsetOfScopeChain()), regT0);
    boxCell(regT0, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_to_this(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToThis>();
    VirtualRegister srcDst = bytecode.m_srcDst;

    emitGetVirtualRegister(srcDst, jsRegT10);

    emitJumpSlowCaseIfNotJSCell(jsRegT10, srcDst);

    addSlowCase(branchIfNotType(jsRegT10.payloadGPR(), FinalObjectType));
    load32FromMetadata(bytecode, OpToThis::Metadata::offsetOfCachedStructureID(), regT2);
    addSlowCase(branch32(NotEqual, Address(jsRegT10.payloadGPR(), JSCell::structureIDOffset()), regT2));
}

void JIT::emit_op_create_this(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCreateThis>();
    VirtualRegister callee = bytecode.m_callee;
    RegisterID calleeReg = regT0;
    RegisterID rareDataReg = regT4;
    RegisterID resultReg = regT0;
    RegisterID allocatorReg = regT1;
    RegisterID structureReg = regT2;
    RegisterID cachedFunctionReg = regT4;
    RegisterID scratchReg = regT3;

    emitGetVirtualRegisterPayload(callee, calleeReg);
    addSlowCase(branchIfNotFunction(calleeReg));
    loadPtr(Address(calleeReg, JSFunction::offsetOfExecutableOrRareData()), rareDataReg);
    addSlowCase(branchTestPtr(Zero, rareDataReg, TrustedImm32(JSFunction::rareDataTag)));
    loadPtr(Address(rareDataReg, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfAllocator() - JSFunction::rareDataTag), allocatorReg);
    loadPtr(Address(rareDataReg, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfStructure() - JSFunction::rareDataTag), structureReg);

    loadPtrFromMetadata(bytecode, OpCreateThis::Metadata::offsetOfCachedCallee(), cachedFunctionReg);
    Jump hasSeenMultipleCallees = branchPtr(Equal, cachedFunctionReg, TrustedImmPtr(JSCell::seenMultipleCalleeObjects()));
    addSlowCase(branchPtr(NotEqual, calleeReg, cachedFunctionReg));
    hasSeenMultipleCallees.link(this);

    JumpList slowCases;
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject(resultReg, JITAllocator::variable(), allocatorReg, structureReg, butterfly, scratchReg, slowCases);
    load8(Address(structureReg, Structure::inlineCapacityOffset()), scratchReg);
    emitInitializeInlineStorage(resultReg, scratchReg);
    mutatorFence(*m_vm);
    addSlowCase(slowCases);
    boxCell(resultReg, jsRegT10);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT10);
}

void JIT::emit_op_check_tdz(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCheckTdz>();
#if USE(JSVALUE64)
    emitGetVirtualRegister(bytecode.m_targetVirtualRegister, regT0);
#elif USE(JSVALUE32_64)
    emitGetVirtualRegisterTag(bytecode.m_targetVirtualRegister, regT0);
#endif
    addSlowCase(branchIfEmpty(regT0));
}

#if USE(JSVALUE64)

// Slow cases

void JIT::emitSlow_op_eq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpEq>();
    loadGlobalObject(regT2);
    callOperation(operationCompareEq, regT2, regT0, regT1);
    boxBoolean(returnValueGPR, JSValueRegs { returnValueGPR });
    emitPutVirtualRegister(bytecode.m_dst, returnValueGPR);
}

void JIT::emitSlow_op_neq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpNeq>();
    loadGlobalObject(regT2);
    callOperation(operationCompareEq, regT2, regT0, regT1);
    xor32(TrustedImm32(0x1), regT0);
    boxBoolean(returnValueGPR, JSValueRegs { returnValueGPR });
    emitPutVirtualRegister(bytecode.m_dst, returnValueGPR);
}

void JIT::emitSlow_op_jeq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    loadGlobalObject(regT2);
    callOperation(operationCompareEq, regT2, regT0, regT1);
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emitSlow_op_jneq(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    loadGlobalObject(regT2);
    callOperation(operationCompareEq, regT2, regT0, regT1);
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);
}

#endif // USE(JSVALUE64)

void JIT::emit_op_debug(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDebug>();
    loadPtr(addressFor(CallFrameSlot::codeBlock), regT0);
    load32(Address(regT0, CodeBlock::offsetOfDebuggerRequests()), regT0);
    Jump noDebuggerRequests = branchTest32(Zero, regT0);
    callOperation(operationDebug, TrustedImmPtr(&vm()), static_cast<int>(bytecode.m_debugHookType));
    noDebuggerRequests.link(this);
}

void JIT::emit_op_loop_hint(const JSInstruction* instruction)
{
    if (UNLIKELY(Options::returnEarlyFromInfiniteLoopsForFuzzing() && m_unlinkedCodeBlock->loopHintsAreEligibleForFuzzingEarlyReturn())) {
        uintptr_t* ptr = vm().getLoopHintExecutionCounter(instruction);
        loadPtr(ptr, regT0);
        auto skipEarlyReturn = branchPtr(Below, regT0, TrustedImmPtr(Options::earlyReturnFromInfiniteLoopsLimit()));

        loadGlobalObject(returnValueJSR.payloadGPR());
        loadPtr(Address(returnValueJSR.payloadGPR(), JSGlobalObject::offsetOfGlobalThis()), returnValueJSR.payloadGPR());
        boxCell(returnValueJSR.payloadGPR(), returnValueJSR);

        checkStackPointerAlignment();
        emitRestoreCalleeSaves();
        emitFunctionEpilogue();
        ret();

        skipEarlyReturn.link(this);
        addPtr(TrustedImm32(1), regT0);
        storePtr(regT0, ptr);
    }

    // Emit the JIT optimization check: 
    if (canBeOptimized()) {
        loadPtr(addressFor(CallFrameSlot::codeBlock), regT0);
        addSlowCase(branchAdd32(PositiveOrZero, TrustedImm32(Options::executionCounterIncrementForLoop()),
            Address(regT0, CodeBlock::offsetOfJITExecuteCounter())));
    }
}

void JIT::emitSlow_op_loop_hint(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
#if ENABLE(DFG_JIT)
    // Emit the slow path for the JIT optimization check:
    if (canBeOptimized()) {
        linkAllSlowCases(iter);

        copyLLIntBaselineCalleeSavesFromFrameOrRegisterToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

        callOperationNoExceptionCheck(operationOptimize, TrustedImmPtr(&vm()), m_bytecodeIndex.asBits());
        Jump noOptimizedEntry = branchTestPtr(Zero, returnValueGPR);
        if (ASSERT_ENABLED) {
            Jump ok = branchPtr(MacroAssembler::Above, returnValueGPR, TrustedImmPtr(bitwise_cast<void*>(static_cast<intptr_t>(1000))));
            abortWithReason(JITUnreasonableLoopHintJumpTarget);
            ok.link(this);
        }
        farJump(returnValueGPR, GPRInfo::callFrameRegister);
        noOptimizedEntry.link(this);

        emitJumpSlowToHot(jump(), currentInstruction->size());
    }
#else
    UNUSED_PARAM(currentInstruction);
    UNUSED_PARAM(iter);
#endif
}

void JIT::emit_op_check_traps(const JSInstruction*)
{
    addSlowCase(branchTest32(NonZero, AbsoluteAddress(m_vm->traps().trapBitsAddress()), TrustedImm32(VMTraps::AsyncEvents)));
}

void JIT::emit_op_nop(const JSInstruction*)
{
}

void JIT::emit_op_super_sampler_begin(const JSInstruction*)
{
    add32(TrustedImm32(1), AbsoluteAddress(bitwise_cast<void*>(&g_superSamplerCount)));
}

void JIT::emit_op_super_sampler_end(const JSInstruction*)
{
    sub32(TrustedImm32(1), AbsoluteAddress(bitwise_cast<void*>(&g_superSamplerCount)));
}

void JIT::emitSlow_op_check_traps(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();

    using BaselineJITRegisters::CheckTraps::bytecodeOffsetGPR;

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    emitNakedNearCall(vm().getCTIStub(op_check_traps_handlerGenerator).retaggedCode<NoPtrTag>());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::op_check_traps_handlerGenerator(VM& vm)
{
    CCallHelpers jit;

    using BaselineJITRegisters::CheckTraps::bytecodeOffsetGPR; // Incoming
    constexpr GPRReg globalObjectGPR = argumentGPR0;
    static_assert(noOverlap(bytecodeOffsetGPR, globalObjectGPR));

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<decltype(operationHandleTraps)>(globalObjectGPR);
    CCallHelpers::Call operation = jit.call(OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link<OperationPtrTag>(operation, operationHandleTraps);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: op_check_traps_handler");
}

void JIT::emit_op_new_regexp(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewRegexp>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister regexp = bytecode.m_regexp;
    GPRReg globalGPR = argumentGPR0;
    loadGlobalObject(globalGPR);
    callOperation(operationNewRegexp, globalGPR, TrustedImmPtr(jsCast<RegExp*>(m_unlinkedCodeBlock->getConstant(regexp))));
    boxCell(returnValueGPR, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);
}

template<typename Op>
void JIT::emitNewFuncCommon(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;

    emitGetVirtualRegisterPayload(bytecode.m_scope, argumentGPR1);
    auto constant = addToConstantPool(JITConstantPool::Type::FunctionDecl, bitwise_cast<void*>(static_cast<uintptr_t>(bytecode.m_functionDecl)));
    loadConstant(constant, argumentGPR2);

    OpcodeID opcodeID = Op::opcodeID;
    if (opcodeID == op_new_func)
        callOperation(operationNewFunction, dst, TrustedImmPtr(&vm()), argumentGPR1, argumentGPR2);
    else if (opcodeID == op_new_generator_func)
        callOperation(operationNewGeneratorFunction, dst, TrustedImmPtr(&vm()), argumentGPR1, argumentGPR2);
    else if (opcodeID == op_new_async_func)
        callOperation(operationNewAsyncFunction, dst, TrustedImmPtr(&vm()), argumentGPR1, argumentGPR2);
    else {
        ASSERT(opcodeID == op_new_async_generator_func);
        callOperation(operationNewAsyncGeneratorFunction, dst, TrustedImmPtr(&vm()), argumentGPR1, argumentGPR2);
    }
}

void JIT::emit_op_new_func(const JSInstruction* currentInstruction)
{
    emitNewFuncCommon<OpNewFunc>(currentInstruction);
}

void JIT::emit_op_new_generator_func(const JSInstruction* currentInstruction)
{
    emitNewFuncCommon<OpNewGeneratorFunc>(currentInstruction);
}

void JIT::emit_op_new_async_generator_func(const JSInstruction* currentInstruction)
{
    emitNewFuncCommon<OpNewAsyncGeneratorFunc>(currentInstruction);
}

void JIT::emit_op_new_async_func(const JSInstruction* currentInstruction)
{
    emitNewFuncCommon<OpNewAsyncFunc>(currentInstruction);
}

template<typename Op>
void JIT::emitNewFuncExprCommon(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;

    emitGetVirtualRegisterPayload(bytecode.m_scope, argumentGPR1);
    auto constant = addToConstantPool(JITConstantPool::Type::FunctionExpr, bitwise_cast<void*>(static_cast<uintptr_t>(bytecode.m_functionDecl)));
    loadConstant(constant, argumentGPR2);
    OpcodeID opcodeID = Op::opcodeID;

    if (opcodeID == op_new_func_exp)
        callOperation(operationNewFunction, dst, TrustedImmPtr(&vm()), argumentGPR1, argumentGPR2);
    else if (opcodeID == op_new_generator_func_exp)
        callOperation(operationNewGeneratorFunction, dst, TrustedImmPtr(&vm()), argumentGPR1, argumentGPR2);
    else if (opcodeID == op_new_async_func_exp)
        callOperation(operationNewAsyncFunction, dst, TrustedImmPtr(&vm()), argumentGPR1, argumentGPR2);
    else {
        ASSERT(opcodeID == op_new_async_generator_func_exp);
        callOperation(operationNewAsyncGeneratorFunction, dst, TrustedImmPtr(&vm()), argumentGPR1, argumentGPR2);
    }
}

void JIT::emit_op_new_func_exp(const JSInstruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewFuncExp>(currentInstruction);
}

void JIT::emit_op_new_generator_func_exp(const JSInstruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewGeneratorFuncExp>(currentInstruction);
}

void JIT::emit_op_new_async_func_exp(const JSInstruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewAsyncFuncExp>(currentInstruction);
}

void JIT::emit_op_new_async_generator_func_exp(const JSInstruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewAsyncGeneratorFuncExp>(currentInstruction);
}

void JIT::emit_op_new_array(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewArray>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister valuesStart = bytecode.m_argv;
    int size = bytecode.m_argc;
    addPtr(TrustedImm32(valuesStart.offset() * sizeof(Register)), callFrameRegister, argumentGPR2);
    materializePointerIntoMetadata(bytecode, OpNewArray::Metadata::offsetOfArrayAllocationProfile(), argumentGPR1);
    loadGlobalObject(argumentGPR0);
    callOperation(operationNewArrayWithProfile, dst, argumentGPR0, argumentGPR1, argumentGPR2, size);
}

void JIT::emit_op_new_array_with_size(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewArrayWithSize>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister sizeIndex = bytecode.m_length;

    using Operation = decltype(operationNewArrayWithSizeAndProfile);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<Operation, 0>();
    constexpr GPRReg profileGPR = preferredArgumentGPR<Operation, 1>();
    constexpr JSValueRegs sizeJSR = preferredArgumentJSR<Operation, 2>();

    materializePointerIntoMetadata(bytecode, OpNewArrayWithSize::Metadata::offsetOfArrayAllocationProfile(), profileGPR);
    emitGetVirtualRegister(sizeIndex, sizeJSR);
    loadGlobalObject(globalObjectGPR);
    callOperation(operationNewArrayWithSizeAndProfile, dst, globalObjectGPR, profileGPR, sizeJSR);
}

void JIT::emit_op_profile_type(const JSInstruction* currentInstruction)
{
    m_isShareable = false;

    auto bytecode = currentInstruction->as<OpProfileType>();
    auto& metadata = bytecode.metadata(m_profiledCodeBlock);
    TypeLocation* cachedTypeLocation = metadata.m_typeLocation;
    VirtualRegister valueToProfile = bytecode.m_targetVirtualRegister;

    emitGetVirtualRegister(valueToProfile, jsRegT10);

    JumpList jumpToEnd;

    jumpToEnd.append(branchIfEmpty(jsRegT10));

    // Compile in a predictive type check, if possible, to see if we can skip writing to the log.
    // These typechecks are inlined to match those of the 64-bit JSValue type checks.
    if (cachedTypeLocation->m_lastSeenType == TypeUndefined)
        jumpToEnd.append(branchIfUndefined(jsRegT10));
    else if (cachedTypeLocation->m_lastSeenType == TypeNull)
        jumpToEnd.append(branchIfNull(jsRegT10));
    else if (cachedTypeLocation->m_lastSeenType == TypeBoolean)
        jumpToEnd.append(branchIfBoolean(jsRegT10, regT2));
    else if (cachedTypeLocation->m_lastSeenType == TypeAnyInt)
        jumpToEnd.append(branchIfInt32(jsRegT10));
    else if (cachedTypeLocation->m_lastSeenType == TypeNumber)
        jumpToEnd.append(branchIfNumber(jsRegT10, regT2));
    else if (cachedTypeLocation->m_lastSeenType == TypeString) {
        Jump isNotCell = branchIfNotCell(jsRegT10);
        jumpToEnd.append(branchIfString(jsRegT10.payloadGPR()));
        isNotCell.link(this);
    }

    // Load the type profiling log into T2.
    TypeProfilerLog* cachedTypeProfilerLog = m_vm->typeProfilerLog();
    move(TrustedImmPtr(cachedTypeProfilerLog), regT2);
    // Load the next log entry into T3.
    loadPtr(Address(regT2, TypeProfilerLog::currentLogEntryOffset()), regT3);

    // Store the JSValue onto the log entry.
    storeValue(jsRegT10, Address(regT3, TypeProfilerLog::LogEntry::valueOffset()));

    // Store the structureID of the cell if jsRegT10 is a cell, otherwise, store 0 on the log entry.
    Jump notCell = branchIfNotCell(jsRegT10);
    load32(Address(jsRegT10.payloadGPR(), JSCell::structureIDOffset()), regT0);
    store32(regT0, Address(regT3, TypeProfilerLog::LogEntry::structureIDOffset()));
    Jump skipIsCell = jump();
    notCell.link(this);
    store32(TrustedImm32(0), Address(regT3, TypeProfilerLog::LogEntry::structureIDOffset()));
    skipIsCell.link(this);

    // Store the typeLocation on the log entry.
    move(TrustedImmPtr(cachedTypeLocation), regT0);
    storePtr(regT0, Address(regT3, TypeProfilerLog::LogEntry::locationOffset()));

    // Increment the current log entry.
    addPtr(TrustedImm32(sizeof(TypeProfilerLog::LogEntry)), regT3);
    storePtr(regT3, Address(regT2, TypeProfilerLog::currentLogEntryOffset()));
    Jump skipClearLog = branchPtr(NotEqual, regT3, TrustedImmPtr(cachedTypeProfilerLog->logEndPtr()));
    // Clear the log if we're at the end of the log.
    callOperationNoExceptionCheck(operationProcessTypeProfilerLog, TrustedImmPtr(&vm()));
    skipClearLog.link(this);

    jumpToEnd.link(this);
}

void JIT::emit_op_log_shadow_chicken_prologue(const JSInstruction* currentInstruction)
{
    RELEASE_ASSERT(vm().shadowChicken());
    updateTopCallFrame();
    static_assert(noOverlap(regT0, nonArgGPR0, regT2), "we will have problems if this is true.");
    auto bytecode = currentInstruction->as<OpLogShadowChickenPrologue>();
    GPRReg shadowPacketReg = regT0;
    GPRReg scratch1Reg = nonArgGPR0; // This must be a non-argument register.
    GPRReg scratch2Reg = regT2;
    ensureShadowChickenPacket(vm(), shadowPacketReg, scratch1Reg, scratch2Reg);
    emitGetVirtualRegisterPayload(bytecode.m_scope, regT3);
    logShadowChickenProloguePacket(shadowPacketReg, scratch1Reg, regT3);
}

void JIT::emit_op_log_shadow_chicken_tail(const JSInstruction* currentInstruction)
{
    RELEASE_ASSERT(vm().shadowChicken());
    updateTopCallFrame();
    static_assert(noOverlap(regT0, nonArgGPR0, regT2), "we will have problems if this is true.");
    static_assert(noOverlap(regT0, regT1, jsRegT32, regT4), "we will have problems if this is true.");
    auto bytecode = currentInstruction->as<OpLogShadowChickenTail>();
    GPRReg shadowPacketReg = regT0;
    {
        GPRReg scratch1Reg = nonArgGPR0; // This must be a non-argument register.
        GPRReg scratch2Reg = regT2;
        ensureShadowChickenPacket(vm(), shadowPacketReg, scratch1Reg, scratch2Reg);
    }
    emitGetVirtualRegister(bytecode.m_thisValue, jsRegT32);
    emitGetVirtualRegisterPayload(bytecode.m_scope, regT4);
    loadPtr(addressFor(CallFrameSlot::codeBlock), regT1);
    logShadowChickenTailPacket(shadowPacketReg, jsRegT32, regT4, regT1, CallSiteIndex(m_bytecodeIndex));
}

void JIT::emit_op_profile_control_flow(const JSInstruction* currentInstruction)
{
    m_isShareable = false;

    auto bytecode = currentInstruction->as<OpProfileControlFlow>();
    auto& metadata = bytecode.metadata(m_profiledCodeBlock);
    BasicBlockLocation* basicBlockLocation = metadata.m_basicBlockLocation;
#if USE(JSVALUE64)
    basicBlockLocation->emitExecuteCode(*this);
#else
    basicBlockLocation->emitExecuteCode(*this, regT0);
#endif
}

void JIT::emit_op_argument_count(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpArgumentCount>();
    VirtualRegister dst = bytecode.m_dst;
    load32(payloadFor(CallFrameSlot::argumentCountIncludingThis), regT0);
    sub32(TrustedImm32(1), regT0);
    JSValueRegs result = JSValueRegs::withTwoAvailableRegs(regT0, regT1);
    boxInt32(regT0, result);
    emitPutVirtualRegister(dst, result);
}

void JIT::emit_op_get_rest_length(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetRestLength>();
    VirtualRegister dst = bytecode.m_dst;
    unsigned numParamsToSkip = bytecode.m_numParametersToSkip;

    load32(payloadFor(CallFrameSlot::argumentCountIncludingThis), regT0);
    sub32(TrustedImm32(1), regT0);
    Jump zeroLength = branch32(LessThanOrEqual, regT0, Imm32(numParamsToSkip));
    sub32(Imm32(numParamsToSkip), regT0);
    boxInt32(regT0, jsRegT10);
    Jump done = jump();

    zeroLength.link(this);
    moveTrustedValue(jsNumber(0), jsRegT10);

    done.link(this);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_get_argument(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetArgument>();
    VirtualRegister dst = bytecode.m_dst;
    int index = bytecode.m_index;

    load32(payloadFor(CallFrameSlot::argumentCountIncludingThis), regT2);
    Jump argumentOutOfBounds = branch32(LessThanOrEqual, regT2, TrustedImm32(index));
    loadValue(addressFor(VirtualRegister(CallFrameSlot::thisArgument + index)), jsRegT10);
    Jump done = jump();

    argumentOutOfBounds.link(this);
    moveValue(jsUndefined(), jsRegT10);

    done.link(this);
    emitValueProfilingSite(bytecode, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_get_prototype_of(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetPrototypeOf>();

    emitGetVirtualRegister(bytecode.m_value, jsRegT10);

    JumpList slowCases;
    slowCases.append(branchIfNotCell(jsRegT10));
    slowCases.append(branchIfNotObject(jsRegT10.payloadGPR()));

    emitLoadPrototype(vm(), jsRegT10.payloadGPR(), jsRegT32, slowCases);
    addSlowCase(slowCases);

    emitValueProfilingSite(bytecode, jsRegT32);
    emitPutVirtualRegister(bytecode.m_dst, jsRegT32);
}

} // namespace JSC

#endif // ENABLE(JIT)
