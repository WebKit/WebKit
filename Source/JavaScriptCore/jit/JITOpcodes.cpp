/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
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

#include "BasicBlockLocation.h"
#include "BytecodeGenerator.h"
#include "Exception.h"
#include "JITInlines.h"
#include "JSCast.h"
#include "JSFunction.h"
#include "JSPropertyNameEnumerator.h"
#include "LinkBuffer.h"
#include "SuperSampler.h"
#include "TypeLocation.h"
#include "TypeProfilerLog.h"
#include "VirtualRegister.h"

namespace JSC {

#if USE(JSVALUE64)

void JIT::emit_op_mov(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpMov>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;

    if (src.isConstant()) {
        JSValue value = m_codeBlock->getConstant(src);
        if (!value.isNumber())
            store64(TrustedImm64(JSValue::encode(value)), addressFor(dst));
        else
            store64(Imm64(JSValue::encode(value)), addressFor(dst));
        return;
    }

    load64(addressFor(src), regT0);
    store64(regT0, addressFor(dst));
}


void JIT::emit_op_end(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnd>();
    RELEASE_ASSERT(returnValueGPR != callFrameRegister);
    emitGetVirtualRegister(bytecode.m_value, returnValueGPR);
    emitRestoreCalleeSaves();
    emitFunctionEpilogue();
    ret();
}

void JIT::emit_op_jmp(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJmp>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    addJump(jump(), target);
}

void JIT::emit_op_new_object(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewObject>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    Structure* structure = metadata.m_objectAllocationProfile.structure();
    size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
    Allocator allocator = allocatorForNonVirtualConcurrently<JSFinalObject>(*m_vm, allocationSize, AllocatorForMode::AllocatorIfExists);

    RegisterID resultReg = regT0;
    RegisterID allocatorReg = regT1;
    RegisterID scratchReg = regT2;

    if (!allocator)
        addSlowCase(jump());
    else {
        JumpList slowCases;
        auto butterfly = TrustedImmPtr(nullptr);
        emitAllocateJSObject(resultReg, JITAllocator::constant(allocator), allocatorReg, TrustedImmPtr(structure), butterfly, scratchReg, slowCases);
        emitInitializeInlineStorage(resultReg, structure->inlineCapacity());
        addSlowCase(slowCases);
        emitPutVirtualRegister(bytecode.m_dst);
    }
}

void JIT::emitSlow_op_new_object(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpNewObject>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    Structure* structure = metadata.m_objectAllocationProfile.structure();
    callOperation(operationNewObject, TrustedImmPtr(&vm()), structure);
    emitStoreCell(dst, returnValueGPR);
}

void JIT::emit_op_overrides_has_instance(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpOverridesHasInstance>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister constructor = bytecode.m_constructor;
    VirtualRegister hasInstanceValue = bytecode.m_hasInstanceValue;

    emitGetVirtualRegister(hasInstanceValue, regT0);

    // We don't jump if we know what Symbol.hasInstance would do.
    Jump customhasInstanceValue = branchPtr(NotEqual, regT0, TrustedImmPtr(m_codeBlock->globalObject()->functionProtoHasInstanceSymbolFunction()));

    emitGetVirtualRegister(constructor, regT0);

    // Check that constructor 'ImplementsDefaultHasInstance' i.e. the object is not a C-API user nor a bound function.
    test8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance), regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });
    Jump done = jump();

    customhasInstanceValue.link(this);
    move(TrustedImm32(JSValue::ValueTrue), regT0);

    done.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_instanceof(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInstanceof>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_value;
    VirtualRegister proto = bytecode.m_prototype;

    // Load the operands (baseVal, proto, and value respectively) into registers.
    // We use regT0 for baseVal since we will be done with this first, and we can then use it for the result.
    emitGetVirtualRegister(value, regT2);
    emitGetVirtualRegister(proto, regT1);
    
    // Check that proto are cells. baseVal must be a cell - this is checked by the get_by_id for Symbol.hasInstance.
    emitJumpSlowCaseIfNotJSCell(regT2, value);
    emitJumpSlowCaseIfNotJSCell(regT1, proto);

    JITInstanceOfGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex),
        RegisterSet::stubUnavailableRegisters(),
        regT0, // result
        regT2, // value
        regT1, // proto
        regT3, regT4); // scratch
    gen.generateFastPath(*this);
    m_instanceOfs.append(gen);
    
    emitPutVirtualRegister(dst);
}

void JIT::emitSlow_op_instanceof(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    
    auto bytecode = currentInstruction->as<OpInstanceof>();
    VirtualRegister resultVReg = bytecode.m_dst;
    
    JITInstanceOfGenerator& gen = m_instanceOfs[m_instanceOfIndex++];
    
    Label coldPathBegin = label();
    Call call = callOperation(operationInstanceOfOptimize, resultVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT2, regT1);
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_instanceof_custom(const Instruction*)
{
    // This always goes to slow path since we expect it to be rare.
    addSlowCase(jump());
}
    
void JIT::emit_op_is_empty(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsEmpty>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitGetVirtualRegister(value, regT0);
    compare64(Equal, regT0, TrustedImm32(JSValue::encode(JSValue())), regT0);

    boxBoolean(regT0, JSValueRegs { regT0 });
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_undefined(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsUndefined>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitGetVirtualRegister(value, regT0);
    Jump isCell = branchIfCell(regT0);

    compare64(Equal, regT0, TrustedImm32(JSValue::ValueUndefined), regT0);
    Jump done = jump();
    
    isCell.link(this);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT0);
    Jump notMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    emitLoadStructure(vm(), regT0, regT1, regT2);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT1, Structure::globalObjectOffset()), regT1);
    comparePtr(Equal, regT0, regT1, regT0);

    notMasqueradesAsUndefined.link(this);
    done.link(this);
    boxBoolean(regT0, JSValueRegs { regT0 });
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_undefined_or_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsUndefinedOrNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitGetVirtualRegister(value, regT0);

    and64(TrustedImm32(~JSValue::UndefinedTag), regT0);
    compare64(Equal, regT0, TrustedImm32(JSValue::ValueNull), regT0);

    boxBoolean(regT0, JSValueRegs { regT0 });
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_boolean(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsBoolean>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitGetVirtualRegister(value, regT0);
    xor64(TrustedImm32(JSValue::ValueFalse), regT0);
    test64(Zero, regT0, TrustedImm32(static_cast<int32_t>(~1)), regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_number(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsNumber>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitGetVirtualRegister(value, regT0);
    test64(NonZero, regT0, numberTagRegister, regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });
    emitPutVirtualRegister(dst);
}

#if USE(BIGINT32)
void JIT::emit_op_is_big_int(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsBigInt>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitGetVirtualRegister(value, regT0);
    Jump isCell = branchIfCell(regT0);

    move(TrustedImm64(JSValue::BigInt32Mask), regT1);
    and64(regT1, regT0);
    compare64(Equal, regT0, TrustedImm32(JSValue::BigInt32Tag), regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });
    Jump done = jump();

    isCell.link(this);
    compare8(Equal, Address(regT0, JSCell::typeInfoTypeOffset()), TrustedImm32(HeapBigIntType), regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });

    done.link(this);
    emitPutVirtualRegister(dst);
}
#else // if !USE(BIGINT32)
NO_RETURN void JIT::emit_op_is_big_int(const Instruction*)
{
    // If we only have HeapBigInts, then we emit isCellWithType instead of isBigInt.
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

void JIT::emit_op_is_cell_with_type(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsCellWithType>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    int type = bytecode.m_type;

    emitGetVirtualRegister(value, regT0);
    Jump isNotCell = branchIfNotCell(regT0);

    compare8(Equal, Address(regT0, JSCell::typeInfoTypeOffset()), TrustedImm32(type), regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });
    Jump done = jump();

    isNotCell.link(this);
    move(TrustedImm32(JSValue::ValueFalse), regT0);

    done.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_object(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsObject>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitGetVirtualRegister(value, regT0);
    Jump isNotCell = branchIfNotCell(regT0);

    compare8(AboveOrEqual, Address(regT0, JSCell::typeInfoTypeOffset()), TrustedImm32(ObjectType), regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });
    Jump done = jump();

    isNotCell.link(this);
    move(TrustedImm32(JSValue::ValueFalse), regT0);

    done.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_ret(const Instruction* currentInstruction)
{
    ASSERT(callFrameRegister != regT1);
    ASSERT(regT1 != returnValueGPR);
    ASSERT(returnValueGPR != callFrameRegister);

    // Return the result in %eax.
    auto bytecode = currentInstruction->as<OpRet>();
    emitGetVirtualRegister(bytecode.m_value, returnValueGPR);

    checkStackPointerAlignment();
    emitRestoreCalleeSaves();
    emitFunctionEpilogue();
    ret();
}

void JIT::emit_op_to_primitive(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToPrimitive>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;

    emitGetVirtualRegister(src, regT0);
    
    Jump isImm = branchIfNotCell(regT0);
    addSlowCase(branchIfObject(regT0));
    isImm.link(this);

    if (dst != src)
        emitPutVirtualRegister(dst);

}

void JIT::emit_op_to_property_key(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToPropertyKey>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;

    emitGetVirtualRegister(src, regT0);

    addSlowCase(branchIfNotCell(regT0));
    Jump done = branchIfSymbol(regT0);
    addSlowCase(branchIfNotString(regT0));

    done.link(this);
    if (src != dst)
        emitPutVirtualRegister(dst);
}

void JIT::emit_op_set_function_name(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSetFunctionName>();
    emitGetVirtualRegister(bytecode.m_function, regT0);
    emitGetVirtualRegister(bytecode.m_name, regT1);
    callOperation(operationSetFunctionName, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1);
}

void JIT::emit_op_not(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNot>();
    emitGetVirtualRegister(bytecode.m_operand, regT0);

    // Invert against JSValue(false); if the value was tagged as a boolean, then all bits will be
    // clear other than the low bit (which will be 0 or 1 for false or true inputs respectively).
    // Then invert against JSValue(true), which will add the tag back in, and flip the low bit.
    xor64(TrustedImm32(JSValue::ValueFalse), regT0);
    addSlowCase(branchTestPtr(NonZero, regT0, TrustedImm32(static_cast<int32_t>(~1))));
    xor64(TrustedImm32(JSValue::ValueTrue), regT0);

    emitPutVirtualRegister(bytecode.m_dst);
}

void JIT::emit_op_jfalse(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJfalse>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    GPRReg value = regT0;
    GPRReg scratch1 = regT1;
    GPRReg scratch2 = regT2;
    bool shouldCheckMasqueradesAsUndefined = true;

    emitGetVirtualRegister(bytecode.m_condition, value);
    addJump(branchIfFalsey(vm(), JSValueRegs(value), scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, m_codeBlock->globalObject()), target);
}

void JIT::emit_op_jeq_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeqNull>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(src, regT0);
    Jump isImmediate = branchIfNotCell(regT0);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    Jump isNotMasqueradesAsUndefined = branchTest8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    emitLoadStructure(vm(), regT0, regT2, regT1);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    addJump(branchPtr(Equal, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump masqueradesGlobalObjectIsForeign = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    and64(TrustedImm32(~JSValue::UndefinedTag), regT0);
    addJump(branch64(Equal, regT0, TrustedImm64(JSValue::encode(jsNull()))), target);            

    isNotMasqueradesAsUndefined.link(this);
    masqueradesGlobalObjectIsForeign.link(this);
};
void JIT::emit_op_jneq_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqNull>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(src, regT0);
    Jump isImmediate = branchIfNotCell(regT0);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    addJump(branchTest8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);
    emitLoadStructure(vm(), regT0, regT2, regT1);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    addJump(branchPtr(NotEqual, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    and64(TrustedImm32(~JSValue::UndefinedTag), regT0);
    addJump(branch64(NotEqual, regT0, TrustedImm64(JSValue::encode(jsNull()))), target);            

    wasNotImmediate.link(this);
}

void JIT::emit_op_jundefined_or_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJundefinedOrNull>();
    VirtualRegister value = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(value, regT0);

    and64(TrustedImm32(~JSValue::UndefinedTag), regT0);
    addJump(branch64(Equal, regT0, TrustedImm64(JSValue::encode(jsNull()))), target);
}

void JIT::emit_op_jnundefined_or_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJnundefinedOrNull>();
    VirtualRegister value = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(value, regT0);

    and64(TrustedImm32(~JSValue::UndefinedTag), regT0);
    addJump(branch64(NotEqual, regT0, TrustedImm64(JSValue::encode(jsNull()))), target);
}

void JIT::emit_op_jneq_ptr(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqPtr>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister src = bytecode.m_value;
    JSValue specialPointer = getConstantOperand(bytecode.m_specialPointer);
    ASSERT(specialPointer.isCell());
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    
    emitGetVirtualRegister(src, regT0);
    CCallHelpers::Jump equal = branchPtr(Equal, regT0, TrustedImmPtr(specialPointer.asCell()));
    store8(TrustedImm32(1), &metadata.m_hasJumped);
    addJump(jump(), target);
    equal.link(this);
}

void JIT::emit_op_eq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEq>();
    emitGetVirtualRegisters(bytecode.m_lhs, regT0, bytecode.m_rhs, regT1);
    emitJumpSlowCaseIfNotInt(regT0, regT1, regT2);
    compare32(Equal, regT1, regT0, regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });
    emitPutVirtualRegister(bytecode.m_dst);
}

void JIT::emit_op_jeq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    emitGetVirtualRegisters(bytecode.m_lhs, regT0, bytecode.m_rhs, regT1);
    emitJumpSlowCaseIfNotInt(regT0, regT1, regT2);
    addJump(branch32(Equal, regT0, regT1), target);
}

void JIT::emit_op_jtrue(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJtrue>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    GPRReg value = regT0;
    GPRReg scratch1 = regT1;
    GPRReg scratch2 = regT2;
    bool shouldCheckMasqueradesAsUndefined = true;
    emitGetVirtualRegister(bytecode.m_condition, value);
    addJump(branchIfTruthy(vm(), JSValueRegs(value), scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, m_codeBlock->globalObject()), target);
}

void JIT::emit_op_neq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNeq>();
    emitGetVirtualRegisters(bytecode.m_lhs, regT0, bytecode.m_rhs, regT1);
    emitJumpSlowCaseIfNotInt(regT0, regT1, regT2);
    compare32(NotEqual, regT1, regT0, regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });

    emitPutVirtualRegister(bytecode.m_dst);
}

void JIT::emit_op_jneq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    emitGetVirtualRegisters(bytecode.m_lhs, regT0, bytecode.m_rhs, regT1);
    emitJumpSlowCaseIfNotInt(regT0, regT1, regT2);
    addJump(branch32(NotEqual, regT0, regT1), target);
}

void JIT::emit_op_throw(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpThrow>();
    ASSERT(regT0 == returnValueGPR);
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);
    emitGetVirtualRegister(bytecode.m_value, regT0);
    callOperationNoExceptionCheck(operationThrow, TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    jumpToExceptionHandler(vm());
}

template<typename Op>
void JIT::compileOpStrictEq(const Instruction* currentInstruction, CompileOpStrictEqType type)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitGetVirtualRegisters(src1, regT0, src2, regT1);

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
    if (type == CompileOpStrictEqType::NStrictEq)
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

    if (type == CompileOpStrictEqType::StrictEq)
        compare64(Equal, regT1, regT0, regT0);
    else
        compare64(NotEqual, regT1, regT0, regT0);
    boxBoolean(regT0, JSValueRegs { regT0 });

    emitPutVirtualRegister(dst);
#endif
}

void JIT::emit_op_stricteq(const Instruction* currentInstruction)
{
    compileOpStrictEq<OpStricteq>(currentInstruction, CompileOpStrictEqType::StrictEq);
}

void JIT::emit_op_nstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEq<OpNstricteq>(currentInstruction, CompileOpStrictEqType::NStrictEq);
}

template<typename Op>
void JIT::compileOpStrictEqJump(const Instruction* currentInstruction, CompileOpStrictEqType type)
{
    auto bytecode = currentInstruction->as<Op>();
    int target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitGetVirtualRegisters(src1, regT0, src2, regT1);

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
    if (type == CompileOpStrictEqType::StrictEq)
        addJump(areEqual, target);

    move(regT0, regT2);
    // Jump slow if at least one is a cell (to cover strings and BigInts).
    and64(regT1, regT2);
    // FIXME: we could do something more precise: unless there is a BigInt32, we only need to do the slow path if both are strings
    addSlowCase(branchIfCell(regT2));

    if (type == CompileOpStrictEqType::NStrictEq) {
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
    if (type == CompileOpStrictEqType::StrictEq)
        addJump(branch64(Equal, regT1, regT0), target);
    else
        addJump(branch64(NotEqual, regT1, regT0), target);
#endif
}

void JIT::emit_op_jstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEqJump<OpJstricteq>(currentInstruction, CompileOpStrictEqType::StrictEq);
}

void JIT::emit_op_jnstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEqJump<OpJnstricteq>(currentInstruction, CompileOpStrictEqType::NStrictEq);
}

void JIT::emitSlow_op_jstricteq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    callOperation(operationCompareStrictEq, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1);
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emitSlow_op_jnstricteq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJnstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    callOperation(operationCompareStrictEq, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1);
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);
}

void JIT::emit_op_to_number(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumber>();
    VirtualRegister dstVReg = bytecode.m_dst;
    VirtualRegister srcVReg = bytecode.m_operand;
    emitGetVirtualRegister(srcVReg, regT0);
    
    addSlowCase(branchIfNotNumber(regT0));

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    if (srcVReg != dstVReg)
        emitPutVirtualRegister(dstVReg);
}

void JIT::emit_op_to_numeric(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumeric>();
    VirtualRegister dstVReg = bytecode.m_dst;
    VirtualRegister srcVReg = bytecode.m_operand;
    emitGetVirtualRegister(srcVReg, regT0);

    Jump isNotCell = branchIfNotCell(regT0);
    addSlowCase(branchIfNotHeapBigInt(regT0));
    Jump isBigInt = jump();

    isNotCell.link(this);
    addSlowCase(branchIfNotNumber(regT0));
    isBigInt.link(this);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    if (srcVReg != dstVReg)
        emitPutVirtualRegister(dstVReg);
}

void JIT::emit_op_to_string(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToString>();
    VirtualRegister srcVReg = bytecode.m_operand;
    emitGetVirtualRegister(srcVReg, regT0);

    addSlowCase(branchIfNotCell(regT0));
    addSlowCase(branchIfNotString(regT0));

    emitPutVirtualRegister(bytecode.m_dst);
}

void JIT::emit_op_to_object(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToObject>();
    VirtualRegister dstVReg = bytecode.m_dst;
    VirtualRegister srcVReg = bytecode.m_operand;
    emitGetVirtualRegister(srcVReg, regT0);

    addSlowCase(branchIfNotCell(regT0));
    addSlowCase(branchIfNotObject(regT0));

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    if (srcVReg != dstVReg)
        emitPutVirtualRegister(dstVReg);
}

void JIT::emit_op_catch(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCatch>();

    restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

    move(TrustedImmPtr(m_vm), regT3);
    load64(Address(regT3, VM::callFrameForCatchOffset()), callFrameRegister);
    storePtr(TrustedImmPtr(nullptr), Address(regT3, VM::callFrameForCatchOffset()));

    addPtr(TrustedImm32(stackPointerOffsetFor(codeBlock()) * sizeof(Register)), callFrameRegister, stackPointerRegister);

    callOperationNoExceptionCheck(operationCheckIfExceptionIsUncatchableAndNotifyProfiler, TrustedImmPtr(&vm()));
    Jump isCatchableException = branchTest32(Zero, returnValueGPR);
    jumpToExceptionHandler(vm());
    isCatchableException.link(this);

    move(TrustedImmPtr(m_vm), regT3);
    load64(Address(regT3, VM::exceptionOffset()), regT0);
    store64(TrustedImm64(JSValue::encode(JSValue())), Address(regT3, VM::exceptionOffset()));
    emitPutVirtualRegister(bytecode.m_exception);

    load64(Address(regT0, Exception::valueOffset()), regT0);
    emitPutVirtualRegister(bytecode.m_thrownValue);

#if ENABLE(DFG_JIT)
    // FIXME: consider inline caching the process of doing OSR entry, including
    // argument type proofs, storing locals to the buffer, etc
    // https://bugs.webkit.org/show_bug.cgi?id=175598

    auto& metadata = bytecode.metadata(m_codeBlock);
    ValueProfileAndVirtualRegisterBuffer* buffer = metadata.m_buffer;
    if (buffer || !shouldEmitProfiling())
        callOperation(operationTryOSREnterAtCatch, &vm(), m_bytecodeIndex.asBits());
    else
        callOperation(operationTryOSREnterAtCatchAndValueProfile, &vm(), m_bytecodeIndex.asBits());
    auto skipOSREntry = branchTestPtr(Zero, returnValueGPR);
    emitRestoreCalleeSaves();
    farJump(returnValueGPR, ExceptionHandlerPtrTag);
    skipOSREntry.link(this);
    if (buffer && shouldEmitProfiling()) {
        buffer->forEach([&] (ValueProfileAndVirtualRegister& profile) {
            JSValueRegs regs(regT0);
            emitGetVirtualRegister(profile.m_operand, regs);
            emitValueProfilingSite(static_cast<ValueProfile&>(profile));
        });
    }
#endif // ENABLE(DFG_JIT)
}

void JIT::emit_op_identity_with_profile(const Instruction*)
{
    // We don't need to do anything here...
}

void JIT::emit_op_get_parent_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetParentScope>();
    VirtualRegister currentScope = bytecode.m_scope;
    emitGetVirtualRegister(currentScope, regT0);
    loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    emitStoreCell(bytecode.m_dst, regT0);
}

void JIT::emit_op_switch_imm(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSwitchImm>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    SimpleJumpTable* jumpTable = &m_codeBlock->switchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset, SwitchRecord::Immediate));
    jumpTable->ensureCTITable();

    emitGetVirtualRegister(scrutinee, regT0);
    callOperation(operationSwitchImmWithUnknownKeyType, TrustedImmPtr(&vm()), regT0, tableIndex);
    farJump(returnValueGPR, JSSwitchPtrTag);
}

void JIT::emit_op_switch_char(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSwitchChar>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    SimpleJumpTable* jumpTable = &m_codeBlock->switchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset, SwitchRecord::Character));
    jumpTable->ensureCTITable();

    emitGetVirtualRegister(scrutinee, regT0);
    callOperation(operationSwitchCharWithUnknownKeyType, TrustedImmPtr(m_codeBlock->globalObject()), regT0, tableIndex);
    farJump(returnValueGPR, JSSwitchPtrTag);
}

void JIT::emit_op_switch_string(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSwitchString>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    StringJumpTable* jumpTable = &m_codeBlock->stringSwitchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset));

    emitGetVirtualRegister(scrutinee, regT0);
    callOperation(operationSwitchStringWithUnknownKeyType, TrustedImmPtr(m_codeBlock->globalObject()), regT0, tableIndex);
    farJump(returnValueGPR, JSSwitchPtrTag);
}

void JIT::emit_op_debug(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDebug>();
    load32(codeBlock()->debuggerRequestsAddress(), regT0);
    Jump noDebuggerRequests = branchTest32(Zero, regT0);
    callOperation(operationDebug, &vm(), static_cast<int>(bytecode.m_debugHookType));
    noDebuggerRequests.link(this);
}

void JIT::emit_op_eq_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEqNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_operand;

    emitGetVirtualRegister(src1, regT0);
    Jump isImmediate = branchIfNotCell(regT0);

    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT0);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    emitLoadStructure(vm(), regT0, regT2, regT1);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT2, Structure::globalObjectOffset()), regT2);
    comparePtr(Equal, regT0, regT2, regT0);
    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    and64(TrustedImm32(~JSValue::UndefinedTag), regT0);
    compare64(Equal, regT0, TrustedImm32(JSValue::ValueNull), regT0);

    wasNotImmediate.link(this);
    wasNotMasqueradesAsUndefined.link(this);

    boxBoolean(regT0, JSValueRegs { regT0 });
    emitPutVirtualRegister(dst);

}

void JIT::emit_op_neq_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNeqNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_operand;

    emitGetVirtualRegister(src1, regT0);
    Jump isImmediate = branchIfNotCell(regT0);

    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(1), regT0);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    emitLoadStructure(vm(), regT0, regT2, regT1);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT2, Structure::globalObjectOffset()), regT2);
    comparePtr(NotEqual, regT0, regT2, regT0);
    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    and64(TrustedImm32(~JSValue::UndefinedTag), regT0);
    compare64(NotEqual, regT0, TrustedImm32(JSValue::ValueNull), regT0);

    wasNotImmediate.link(this);
    wasNotMasqueradesAsUndefined.link(this);

    boxBoolean(regT0, JSValueRegs { regT0 });
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_enter(const Instruction*)
{
    // Even though CTI doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.
    size_t count = m_codeBlock->numVars();
    for (size_t j = CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters(); j < count; ++j)
        emitInitRegister(virtualRegisterForLocal(j));

    emitWriteBarrier(m_codeBlock);

    emitEnterOptimizationCheck();
}

void JIT::emit_op_get_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetScope>();
    VirtualRegister dst = bytecode.m_dst;
    emitGetFromCallFrameHeaderPtr(CallFrameSlot::callee, regT0);
    loadPtr(Address(regT0, JSFunction::offsetOfScopeChain()), regT0);
    emitStoreCell(dst, regT0);
}

void JIT::emit_op_to_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToThis>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    StructureID* cachedStructureID = &metadata.m_cachedStructureID;
    emitGetVirtualRegister(bytecode.m_srcDst, regT1);

    emitJumpSlowCaseIfNotJSCell(regT1);

    addSlowCase(branchIfNotType(regT1, FinalObjectType));
    load32(cachedStructureID, regT2);
    addSlowCase(branch32(NotEqual, Address(regT1, JSCell::structureIDOffset()), regT2));
}

void JIT::emit_op_create_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCreateThis>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister callee = bytecode.m_callee;
    WriteBarrierBase<JSCell>* cachedFunction = &metadata.m_cachedCallee;
    RegisterID calleeReg = regT0;
    RegisterID rareDataReg = regT4;
    RegisterID resultReg = regT0;
    RegisterID allocatorReg = regT1;
    RegisterID structureReg = regT2;
    RegisterID cachedFunctionReg = regT4;
    RegisterID scratchReg = regT3;

    emitGetVirtualRegister(callee, calleeReg);
    addSlowCase(branchIfNotFunction(calleeReg));
    loadPtr(Address(calleeReg, JSFunction::offsetOfExecutableOrRareData()), rareDataReg);
    addSlowCase(branchTestPtr(Zero, rareDataReg, TrustedImm32(JSFunction::rareDataTag)));
    loadPtr(Address(rareDataReg, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfAllocator() - JSFunction::rareDataTag), allocatorReg);
    loadPtr(Address(rareDataReg, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfStructure() - JSFunction::rareDataTag), structureReg);

    loadPtr(cachedFunction, cachedFunctionReg);
    Jump hasSeenMultipleCallees = branchPtr(Equal, cachedFunctionReg, TrustedImmPtr(JSCell::seenMultipleCalleeObjects()));
    addSlowCase(branchPtr(NotEqual, calleeReg, cachedFunctionReg));
    hasSeenMultipleCallees.link(this);

    JumpList slowCases;
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject(resultReg, JITAllocator::variable(), allocatorReg, structureReg, butterfly, scratchReg, slowCases);
    load8(Address(structureReg, Structure::inlineCapacityOffset()), scratchReg);
    emitInitializeInlineStorage(resultReg, scratchReg);
    addSlowCase(slowCases);
    emitPutVirtualRegister(bytecode.m_dst);
}

void JIT::emit_op_check_tdz(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCheckTdz>();
    emitGetVirtualRegister(bytecode.m_targetVirtualRegister, regT0);
    addSlowCase(branchIfEmpty(regT0));
}


// Slow cases

void JIT::emitSlow_op_eq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpEq>();
    callOperation(operationCompareEq, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1);
    boxBoolean(returnValueGPR, JSValueRegs { returnValueGPR });
    emitPutVirtualRegister(bytecode.m_dst, returnValueGPR);
}

void JIT::emitSlow_op_neq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpNeq>();
    callOperation(operationCompareEq, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1);
    xor32(TrustedImm32(0x1), regT0);
    boxBoolean(returnValueGPR, JSValueRegs { returnValueGPR });
    emitPutVirtualRegister(bytecode.m_dst, returnValueGPR);
}

void JIT::emitSlow_op_jeq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    callOperation(operationCompareEq, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1);
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emitSlow_op_jneq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    callOperation(operationCompareEq, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1);
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);
}

void JIT::emitSlow_op_instanceof_custom(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpInstanceofCustom>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_value;
    VirtualRegister constructor = bytecode.m_constructor;
    VirtualRegister hasInstanceValue = bytecode.m_hasInstanceValue;

    emitGetVirtualRegister(value, regT0);
    emitGetVirtualRegister(constructor, regT1);
    emitGetVirtualRegister(hasInstanceValue, regT2);
    callOperation(operationInstanceOfCustom, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1, regT2);
    boxBoolean(returnValueGPR, JSValueRegs { returnValueGPR });
    emitPutVirtualRegister(dst, returnValueGPR);
}

#endif // USE(JSVALUE64)

void JIT::emit_op_loop_hint(const Instruction*)
{
    // Emit the JIT optimization check: 
    if (canBeOptimized()) {
        addSlowCase(branchAdd32(PositiveOrZero, TrustedImm32(Options::executionCounterIncrementForLoop()),
            AbsoluteAddress(m_codeBlock->addressOfJITExecuteCounter())));
    }
}

void JIT::emitSlow_op_loop_hint(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
#if ENABLE(DFG_JIT)
    // Emit the slow path for the JIT optimization check:
    if (canBeOptimized()) {
        linkAllSlowCases(iter);

        copyCalleeSavesFromFrameOrRegisterToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

        callOperation(operationOptimize, &vm(), m_bytecodeIndex.asBits());
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

void JIT::emit_op_check_traps(const Instruction*)
{
    addSlowCase(branchTest8(NonZero, AbsoluteAddress(m_vm->needTrapHandlingAddress())));
}

void JIT::emit_op_nop(const Instruction*)
{
}

void JIT::emit_op_super_sampler_begin(const Instruction*)
{
    add32(TrustedImm32(1), AbsoluteAddress(bitwise_cast<void*>(&g_superSamplerCount)));
}

void JIT::emit_op_super_sampler_end(const Instruction*)
{
    sub32(TrustedImm32(1), AbsoluteAddress(bitwise_cast<void*>(&g_superSamplerCount)));
}

void JIT::emitSlow_op_check_traps(const Instruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    callOperation(operationHandleTraps, TrustedImmPtr(m_codeBlock->globalObject()));
}

void JIT::emit_op_new_regexp(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewRegexp>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister regexp = bytecode.m_regexp;
    callOperation(operationNewRegexp, TrustedImmPtr(m_codeBlock->globalObject()), jsCast<RegExp*>(m_codeBlock->getConstant(regexp)));
    emitStoreCell(dst, returnValueGPR);
}

template<typename Op>
void JIT::emitNewFuncCommon(const Instruction* currentInstruction)
{
    Jump lazyJump;
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;

#if USE(JSVALUE64)
    emitGetVirtualRegister(bytecode.m_scope, regT0);
#else
    emitLoadPayload(bytecode.m_scope, regT0);
#endif
    FunctionExecutable* funcExec = m_codeBlock->functionDecl(bytecode.m_functionDecl);

    OpcodeID opcodeID = Op::opcodeID;
    if (opcodeID == op_new_func)
        callOperation(operationNewFunction, dst, &vm(), regT0, funcExec);
    else if (opcodeID == op_new_generator_func)
        callOperation(operationNewGeneratorFunction, dst, &vm(), regT0, funcExec);
    else if (opcodeID == op_new_async_func)
        callOperation(operationNewAsyncFunction, dst, &vm(), regT0, funcExec);
    else {
        ASSERT(opcodeID == op_new_async_generator_func);
        callOperation(operationNewAsyncGeneratorFunction, dst, &vm(), regT0, funcExec);
    }
}

void JIT::emit_op_new_func(const Instruction* currentInstruction)
{
    emitNewFuncCommon<OpNewFunc>(currentInstruction);
}

void JIT::emit_op_new_generator_func(const Instruction* currentInstruction)
{
    emitNewFuncCommon<OpNewGeneratorFunc>(currentInstruction);
}

void JIT::emit_op_new_async_generator_func(const Instruction* currentInstruction)
{
    emitNewFuncCommon<OpNewAsyncGeneratorFunc>(currentInstruction);
}

void JIT::emit_op_new_async_func(const Instruction* currentInstruction)
{
    emitNewFuncCommon<OpNewAsyncFunc>(currentInstruction);
}
    
template<typename Op>
void JIT::emitNewFuncExprCommon(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;
#if USE(JSVALUE64)
    emitGetVirtualRegister(bytecode.m_scope, regT0);
#else
    emitLoadPayload(bytecode.m_scope, regT0);
#endif

    FunctionExecutable* function = m_codeBlock->functionExpr(bytecode.m_functionDecl);
    OpcodeID opcodeID = Op::opcodeID;

    if (opcodeID == op_new_func_exp)
        callOperation(operationNewFunction, dst, &vm(), regT0, function);
    else if (opcodeID == op_new_generator_func_exp)
        callOperation(operationNewGeneratorFunction, dst, &vm(), regT0, function);
    else if (opcodeID == op_new_async_func_exp)
        callOperation(operationNewAsyncFunction, dst, &vm(), regT0, function);
    else {
        ASSERT(opcodeID == op_new_async_generator_func_exp);
        callOperation(operationNewAsyncGeneratorFunction, dst, &vm(), regT0, function);
    }
}

void JIT::emit_op_new_func_exp(const Instruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewFuncExp>(currentInstruction);
}

void JIT::emit_op_new_generator_func_exp(const Instruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewGeneratorFuncExp>(currentInstruction);
}

void JIT::emit_op_new_async_func_exp(const Instruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewAsyncFuncExp>(currentInstruction);
}
    
void JIT::emit_op_new_async_generator_func_exp(const Instruction* currentInstruction)
{
    emitNewFuncExprCommon<OpNewAsyncGeneratorFuncExp>(currentInstruction);
}
    
void JIT::emit_op_new_array(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewArray>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister valuesStart = bytecode.m_argv;
    int size = bytecode.m_argc;
    addPtr(TrustedImm32(valuesStart.offset() * sizeof(Register)), callFrameRegister, regT0);
    callOperation(operationNewArrayWithProfile, dst, TrustedImmPtr(m_codeBlock->globalObject()),
        &metadata.m_arrayAllocationProfile, regT0, size);
}

void JIT::emit_op_new_array_with_size(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewArrayWithSize>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister sizeIndex = bytecode.m_length;
#if USE(JSVALUE64)
    emitGetVirtualRegister(sizeIndex, regT0);
    callOperation(operationNewArrayWithSizeAndProfile, dst, TrustedImmPtr(m_codeBlock->globalObject()),
        &metadata.m_arrayAllocationProfile, regT0);
#else
    emitLoad(sizeIndex, regT1, regT0);
    callOperation(operationNewArrayWithSizeAndProfile, dst, TrustedImmPtr(m_codeBlock->globalObject()),
        &metadata.m_arrayAllocationProfile, JSValueRegs(regT1, regT0));
#endif
}

#if USE(JSVALUE64)
void JIT::emit_op_has_structure_property(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasStructureProperty>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister enumerator = bytecode.m_enumerator;

    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(enumerator, regT1);
    emitJumpSlowCaseIfNotJSCell(regT0, base);

    load32(Address(regT0, JSCell::structureIDOffset()), regT0);
    addSlowCase(branch32(NotEqual, regT0, Address(regT1, JSPropertyNameEnumerator::cachedStructureIDOffset())));
    
    move(TrustedImm64(JSValue::encode(jsBoolean(true))), regT0);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_in_structure_property(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInStructureProperty>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister enumerator = bytecode.m_enumerator;

    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(enumerator, regT1);
    emitJumpSlowCaseIfNotJSCell(regT0, base);

    load32(Address(regT0, JSCell::structureIDOffset()), regT0);
    addSlowCase(branch32(NotEqual, regT0, Address(regT1, JSPropertyNameEnumerator::cachedStructureIDOffset())));
    
    move(TrustedImm64(JSValue::encode(jsBoolean(true))), regT0);
    emitPutVirtualRegister(dst);
}

void JIT::privateCompileHasIndexedProperty(ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
{
    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    
    PatchableJump badType;
    
    // FIXME: Add support for other types like TypedArrays and Arguments.
    // See https://bugs.webkit.org/show_bug.cgi?id=135033 and https://bugs.webkit.org/show_bug.cgi?id=135034.
    JumpList slowCases = emitLoadForArrayMode(currentInstruction, arrayMode, badType);
    move(TrustedImm64(JSValue::encode(jsBoolean(true))), regT0);
    Jump done = jump();

    LinkBuffer patchBuffer(*this, m_codeBlock);
    
    patchBuffer.link(badType, byValInfo->slowPathTarget);
    patchBuffer.link(slowCases, byValInfo->slowPathTarget);

    patchBuffer.link(done, byValInfo->doneTarget);

    byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
        m_codeBlock, patchBuffer, JITStubRoutinePtrTag,
        "Baseline has_indexed_property stub for %s, return point %p", toCString(*m_codeBlock).data(), returnAddress.value());
    
    MacroAssembler::repatchJump(byValInfo->badTypeJump, CodeLocationLabel<JITStubRoutinePtrTag>(byValInfo->stubRoutine->code().code()));
    MacroAssembler::repatchCall(CodeLocationCall<NoPtrTag>(MacroAssemblerCodePtr<NoPtrTag>(returnAddress)), FunctionPtr<OperationPtrTag>(operationHasIndexedPropertyGeneric));
}

void JIT::emit_op_has_indexed_property(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasIndexedProperty>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    ArrayProfile* profile = &metadata.m_arrayProfile;
    ByValInfo* byValInfo = m_codeBlock->addByValInfo();
    
    emitGetVirtualRegisters(base, regT0, property, regT1);

    emitJumpSlowCaseIfNotInt(regT1);

    // This is technically incorrect - we're zero-extending an int32. On the hot path this doesn't matter.
    // We check the value as if it was a uint32 against the m_vectorLength - which will always fail if
    // number was signed since m_vectorLength is always less than intmax (since the total allocation
    // size is always less than 4Gb). As such zero extending will have been correct (and extending the value
    // to 64-bits is necessary since it's used in the address calculation. We zero extend rather than sign
    // extending since it makes it easier to re-tag the value in the slow case.
    zeroExtend32ToPtr(regT1, regT1);

    emitJumpSlowCaseIfNotJSCell(regT0, base);
    emitArrayProfilingSiteWithCell(regT0, regT2, profile);
    and32(TrustedImm32(IndexingShapeMask), regT2);

    JITArrayMode mode = chooseArrayMode(profile);
    PatchableJump badType;

    // FIXME: Add support for other types like TypedArrays and Arguments.
    // See https://bugs.webkit.org/show_bug.cgi?id=135033 and https://bugs.webkit.org/show_bug.cgi?id=135034.
    JumpList slowCases = emitLoadForArrayMode(currentInstruction, mode, badType);
    
    move(TrustedImm64(JSValue::encode(jsBoolean(true))), regT0);

    addSlowCase(badType);
    addSlowCase(slowCases);
    
    Label done = label();
    
    emitPutVirtualRegister(dst);

    Label nextHotPath = label();
    
    m_byValCompilationInfo.append(ByValCompilationInfo(byValInfo, m_bytecodeIndex, PatchableJump(), badType, mode, profile, done, nextHotPath));
}

void JIT::emitSlow_op_has_indexed_property(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpHasIndexedProperty>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    ByValInfo* byValInfo = m_byValCompilationInfo[m_byValInstructionIndex].byValInfo;

    Label slowPath = label();
    
    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(property, regT1);
    Call call = callOperation(operationHasIndexedPropertyDefault, dst, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1, byValInfo);

    m_byValCompilationInfo[m_byValInstructionIndex].slowPathTarget = slowPath;
    m_byValCompilationInfo[m_byValInstructionIndex].returnAddress = call;
    m_byValInstructionIndex++;
}

void JIT::emit_op_get_direct_pname(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetDirectPname>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister index = bytecode.m_index;
    VirtualRegister enumerator = bytecode.m_enumerator;

    // Check that base is a cell
    emitGetVirtualRegister(base, regT0);
    emitJumpSlowCaseIfNotJSCell(regT0, base);

    // Check the structure
    emitGetVirtualRegister(enumerator, regT2);
    load32(Address(regT0, JSCell::structureIDOffset()), regT1);
    addSlowCase(branch32(NotEqual, regT1, Address(regT2, JSPropertyNameEnumerator::cachedStructureIDOffset())));

    // Compute the offset
    emitGetVirtualRegister(index, regT1);
    // If index is less than the enumerator's cached inline storage, then it's an inline access
    Jump outOfLineAccess = branch32(AboveOrEqual, regT1, Address(regT2, JSPropertyNameEnumerator::cachedInlineCapacityOffset()));
    addPtr(TrustedImm32(JSObject::offsetOfInlineStorage()), regT0);
    signExtend32ToPtr(regT1, regT1);
    load64(BaseIndex(regT0, regT1, TimesEight), regT0);
    
    Jump done = jump();

    // Otherwise it's out of line
    outOfLineAccess.link(this);
    loadPtr(Address(regT0, JSObject::butterflyOffset()), regT0);
    sub32(Address(regT2, JSPropertyNameEnumerator::cachedInlineCapacityOffset()), regT1);
    neg32(regT1);
    signExtend32ToPtr(regT1, regT1);
    int32_t offsetOfFirstProperty = static_cast<int32_t>(offsetInButterfly(firstOutOfLineOffset)) * sizeof(EncodedJSValue);
    load64(BaseIndex(regT0, regT1, TimesEight, offsetOfFirstProperty), regT0);
    
    done.link(this);
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(dst, regT0);
}

void JIT::emit_op_enumerator_structure_pname(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnumeratorStructurePname>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister enumerator = bytecode.m_enumerator;
    VirtualRegister index = bytecode.m_index;

    emitGetVirtualRegister(index, regT0);
    emitGetVirtualRegister(enumerator, regT1);
    Jump inBounds = branch32(Below, regT0, Address(regT1, JSPropertyNameEnumerator::endStructurePropertyIndexOffset()));

    move(TrustedImm64(JSValue::encode(jsNull())), regT0);

    Jump done = jump();
    inBounds.link(this);

    loadPtr(Address(regT1, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset()), regT1);
    signExtend32ToPtr(regT0, regT0);
    load64(BaseIndex(regT1, regT0, TimesEight), regT0);

    done.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_enumerator_generic_pname(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnumeratorGenericPname>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister enumerator = bytecode.m_enumerator;
    VirtualRegister index = bytecode.m_index;

    emitGetVirtualRegister(index, regT0);
    emitGetVirtualRegister(enumerator, regT1);
    Jump inBounds = branch32(Below, regT0, Address(regT1, JSPropertyNameEnumerator::endGenericPropertyIndexOffset()));

    move(TrustedImm64(JSValue::encode(jsNull())), regT0);

    Jump done = jump();
    inBounds.link(this);

    loadPtr(Address(regT1, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset()), regT1);
    signExtend32ToPtr(regT0, regT0);
    load64(BaseIndex(regT1, regT0, TimesEight), regT0);
    
    done.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_profile_type(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpProfileType>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    TypeLocation* cachedTypeLocation = metadata.m_typeLocation;
    VirtualRegister valueToProfile = bytecode.m_targetVirtualRegister;

    emitGetVirtualRegister(valueToProfile, regT0);

    JumpList jumpToEnd;

    jumpToEnd.append(branchIfEmpty(regT0));

    // Compile in a predictive type check, if possible, to see if we can skip writing to the log.
    // These typechecks are inlined to match those of the 64-bit JSValue type checks.
    if (cachedTypeLocation->m_lastSeenType == TypeUndefined)
        jumpToEnd.append(branchIfUndefined(regT0));
    else if (cachedTypeLocation->m_lastSeenType == TypeNull)
        jumpToEnd.append(branchIfNull(regT0));
    else if (cachedTypeLocation->m_lastSeenType == TypeBoolean)
        jumpToEnd.append(branchIfBoolean(regT0, regT1));
    else if (cachedTypeLocation->m_lastSeenType == TypeAnyInt)
        jumpToEnd.append(branchIfInt32(regT0));
    else if (cachedTypeLocation->m_lastSeenType == TypeNumber)
        jumpToEnd.append(branchIfNumber(regT0));
    else if (cachedTypeLocation->m_lastSeenType == TypeString) {
        Jump isNotCell = branchIfNotCell(regT0);
        jumpToEnd.append(branchIfString(regT0));
        isNotCell.link(this);
    }

    // Load the type profiling log into T2.
    TypeProfilerLog* cachedTypeProfilerLog = m_vm->typeProfilerLog();
    move(TrustedImmPtr(cachedTypeProfilerLog), regT2);
    // Load the next log entry into T1.
    loadPtr(Address(regT2, TypeProfilerLog::currentLogEntryOffset()), regT1);

    // Store the JSValue onto the log entry.
    store64(regT0, Address(regT1, TypeProfilerLog::LogEntry::valueOffset()));

    // Store the structureID of the cell if T0 is a cell, otherwise, store 0 on the log entry.
    Jump notCell = branchIfNotCell(regT0);
    load32(Address(regT0, JSCell::structureIDOffset()), regT0);
    store32(regT0, Address(regT1, TypeProfilerLog::LogEntry::structureIDOffset()));
    Jump skipIsCell = jump();
    notCell.link(this);
    store32(TrustedImm32(0), Address(regT1, TypeProfilerLog::LogEntry::structureIDOffset()));
    skipIsCell.link(this);

    // Store the typeLocation on the log entry.
    move(TrustedImmPtr(cachedTypeLocation), regT0);
    store64(regT0, Address(regT1, TypeProfilerLog::LogEntry::locationOffset()));

    // Increment the current log entry.
    addPtr(TrustedImm32(sizeof(TypeProfilerLog::LogEntry)), regT1);
    store64(regT1, Address(regT2, TypeProfilerLog::currentLogEntryOffset()));
    Jump skipClearLog = branchPtr(NotEqual, regT1, TrustedImmPtr(cachedTypeProfilerLog->logEndPtr()));
    // Clear the log if we're at the end of the log.
    callOperation(operationProcessTypeProfilerLog, &vm());
    skipClearLog.link(this);

    jumpToEnd.link(this);
}

void JIT::emit_op_log_shadow_chicken_prologue(const Instruction* currentInstruction)
{
    RELEASE_ASSERT(vm().shadowChicken());
    updateTopCallFrame();
    static_assert(nonArgGPR0 != regT0 && nonArgGPR0 != regT2, "we will have problems if this is true.");
    auto bytecode = currentInstruction->as<OpLogShadowChickenPrologue>();
    GPRReg shadowPacketReg = regT0;
    GPRReg scratch1Reg = nonArgGPR0; // This must be a non-argument register.
    GPRReg scratch2Reg = regT2;
    ensureShadowChickenPacket(vm(), shadowPacketReg, scratch1Reg, scratch2Reg);
    emitGetVirtualRegister(bytecode.m_scope, regT3);
    logShadowChickenProloguePacket(shadowPacketReg, scratch1Reg, regT3);
}

void JIT::emit_op_log_shadow_chicken_tail(const Instruction* currentInstruction)
{
    RELEASE_ASSERT(vm().shadowChicken());
    updateTopCallFrame();
    static_assert(nonArgGPR0 != regT0 && nonArgGPR0 != regT2, "we will have problems if this is true.");
    auto bytecode = currentInstruction->as<OpLogShadowChickenTail>();
    GPRReg shadowPacketReg = regT0;
    GPRReg scratch1Reg = nonArgGPR0; // This must be a non-argument register.
    GPRReg scratch2Reg = regT2;
    ensureShadowChickenPacket(vm(), shadowPacketReg, scratch1Reg, scratch2Reg);
    emitGetVirtualRegister(bytecode.m_thisValue, regT2);
    emitGetVirtualRegister(bytecode.m_scope, regT3);
    logShadowChickenTailPacket(shadowPacketReg, JSValueRegs(regT2), regT3, m_codeBlock, CallSiteIndex(m_bytecodeIndex));
}

#endif // USE(JSVALUE64)

void JIT::emit_op_profile_control_flow(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpProfileControlFlow>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    BasicBlockLocation* basicBlockLocation = metadata.m_basicBlockLocation;
#if USE(JSVALUE64)
    basicBlockLocation->emitExecuteCode(*this);
#else
    basicBlockLocation->emitExecuteCode(*this, regT0);
#endif
}

void JIT::emit_op_argument_count(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpArgumentCount>();
    VirtualRegister dst = bytecode.m_dst;
    load32(payloadFor(CallFrameSlot::argumentCountIncludingThis), regT0);
    sub32(TrustedImm32(1), regT0);
    JSValueRegs result = JSValueRegs::withTwoAvailableRegs(regT0, regT1);
    boxInt32(regT0, result);
    emitPutVirtualRegister(dst, result);
}

void JIT::emit_op_get_rest_length(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetRestLength>();
    VirtualRegister dst = bytecode.m_dst;
    unsigned numParamsToSkip = bytecode.m_numParametersToSkip;
    load32(payloadFor(CallFrameSlot::argumentCountIncludingThis), regT0);
    sub32(TrustedImm32(1), regT0);
    Jump zeroLength = branch32(LessThanOrEqual, regT0, Imm32(numParamsToSkip));
    sub32(Imm32(numParamsToSkip), regT0);
#if USE(JSVALUE64)
    boxInt32(regT0, JSValueRegs(regT0));
#endif
    Jump done = jump();

    zeroLength.link(this);
#if USE(JSVALUE64)
    move(TrustedImm64(JSValue::encode(jsNumber(0))), regT0);
#else
    move(TrustedImm32(0), regT0);
#endif

    done.link(this);
#if USE(JSVALUE64)
    emitPutVirtualRegister(dst, regT0);
#else
    move(TrustedImm32(JSValue::Int32Tag), regT1);
    emitPutVirtualRegister(dst, JSValueRegs(regT1, regT0));
#endif
}

void JIT::emit_op_get_argument(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetArgument>();
    VirtualRegister dst = bytecode.m_dst;
    int index = bytecode.m_index;
#if USE(JSVALUE64)
    JSValueRegs resultRegs(regT0);
#else
    JSValueRegs resultRegs(regT1, regT0);
#endif

    load32(payloadFor(CallFrameSlot::argumentCountIncludingThis), regT2);
    Jump argumentOutOfBounds = branch32(LessThanOrEqual, regT2, TrustedImm32(index));
    loadValue(addressFor(VirtualRegister(CallFrameSlot::thisArgument + index)), resultRegs);
    Jump done = jump();

    argumentOutOfBounds.link(this);
    moveValue(jsUndefined(), resultRegs);

    done.link(this);
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(dst, resultRegs);
}

} // namespace JSC

#endif // ENABLE(JIT)
