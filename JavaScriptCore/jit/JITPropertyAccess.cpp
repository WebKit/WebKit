/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#include "JIT.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "LinkBuffer.h"
#include "RepatchBuffer.h"
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace JSC {

#if USE(JSVALUE32_64)

void JIT::emit_op_put_by_index(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_put_by_index);
    stubCall.addArgument(base);
    stubCall.addArgument(Imm32(property));
    stubCall.addArgument(value);
    stubCall.call();
}

void JIT::emit_op_put_getter(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned function = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_put_getter);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(property)));
    stubCall.addArgument(function);
    stubCall.call();
}

void JIT::emit_op_put_setter(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned function = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_put_setter);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(property)));
    stubCall.addArgument(function);
    stubCall.call();
}

void JIT::emit_op_del_by_id(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_del_by_id);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(property)));
    stubCall.call(dst);
}


#if !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

/* ------------------------------ BEGIN: !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

// Treat these as nops - the call will be handed as a regular get_by_id/op_call pair.
void JIT::emit_op_method_check(Instruction*) {}
void JIT::emitSlow_op_method_check(Instruction*, Vector<SlowCaseEntry>::iterator&) { ASSERT_NOT_REACHED(); }
#if ENABLE(JIT_OPTIMIZE_METHOD_CALLS)
#error "JIT_OPTIMIZE_METHOD_CALLS requires JIT_OPTIMIZE_PROPERTY_ACCESS"
#endif

void JIT::emit_op_get_by_val(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_get_by_val);
    stubCall.addArgument(base);
    stubCall.addArgument(property);
    stubCall.call(dst);
}

void JIT::emitSlow_op_get_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_put_by_val(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_put_by_val);
    stubCall.addArgument(base);
    stubCall.addArgument(property);
    stubCall.addArgument(value);
    stubCall.call();
}

void JIT::emitSlow_op_put_by_val(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int ident = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_get_by_id_generic);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&(m_codeBlock->identifier(ident))));
    stubCall.call(dst);

    m_propertyAccessInstructionIndex++;
}

void JIT::emitSlow_op_get_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    m_propertyAccessInstructionIndex++;
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    int base = currentInstruction[1].u.operand;
    int ident = currentInstruction[2].u.operand;
    int value = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_put_by_id_generic);
    stubCall.addArgument(base);
    stubCall.addArgument(ImmPtr(&(m_codeBlock->identifier(ident))));
    stubCall.addArgument(value);
    stubCall.call();

    m_propertyAccessInstructionIndex++;
}

void JIT::emitSlow_op_put_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    m_propertyAccessInstructionIndex++;
    ASSERT_NOT_REACHED();
}

#else // !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

/* ------------------------------ BEGIN: ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

#if ENABLE(JIT_OPTIMIZE_METHOD_CALLS)

void JIT::emit_op_method_check(Instruction* currentInstruction)
{
    // Assert that the following instruction is a get_by_id.
    ASSERT(m_interpreter->getOpcodeID((currentInstruction + OPCODE_LENGTH(op_method_check))->u.opcode) == op_get_by_id);

    currentInstruction += OPCODE_LENGTH(op_method_check);

    // Do the method check - check the object & its prototype's structure inline (this is the common case).
    m_methodCallCompilationInfo.append(MethodCallCompilationInfo(m_propertyAccessInstructionIndex));
    MethodCallCompilationInfo& info = m_methodCallCompilationInfo.last();

    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), info.structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    DataLabelPtr protoStructureToCompare, protoObj = moveWithPatch(ImmPtr(0), regT2);
    Jump protoStructureCheck = branchPtrWithPatch(NotEqual, Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), protoStructureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));

    // This will be relinked to load the function without doing a load.
    DataLabelPtr putFunction = moveWithPatch(ImmPtr(0), regT0);
    move(Imm32(JSValue::CellTag), regT1);
    Jump match = jump();

    ASSERT(differenceBetween(info.structureToCompare, protoObj) == patchOffsetMethodCheckProtoObj);
    ASSERT(differenceBetween(info.structureToCompare, protoStructureToCompare) == patchOffsetMethodCheckProtoStruct);
    ASSERT(differenceBetween(info.structureToCompare, putFunction) == patchOffsetMethodCheckPutFunction);

    // Link the failure cases here.
    structureCheck.link(this);
    protoStructureCheck.link(this);

    // Do a regular(ish) get_by_id (the slow case will be link to
    // cti_op_get_by_id_method_check instead of cti_op_get_by_id.
    compileGetByIdHotPath();

    match.link(this);
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_method_check), dst, regT1, regT0);

    // We've already generated the following get_by_id, so make sure it's skipped over.
    m_bytecodeIndex += OPCODE_LENGTH(op_get_by_id);
}

void JIT::emitSlow_op_method_check(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    currentInstruction += OPCODE_LENGTH(op_method_check);

    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int ident = currentInstruction[3].u.operand;

    compileGetByIdSlowCase(dst, base, &(m_codeBlock->identifier(ident)), iter, true);

    // We've already generated the following get_by_id, so make sure it's skipped over.
    m_bytecodeIndex += OPCODE_LENGTH(op_get_by_id);
}

#else //!ENABLE(JIT_OPTIMIZE_METHOD_CALLS)

// Treat these as nops - the call will be handed as a regular get_by_id/op_call pair.
void JIT::emit_op_method_check(Instruction*) {}
void JIT::emitSlow_op_method_check(Instruction*, Vector<SlowCaseEntry>::iterator&) { ASSERT_NOT_REACHED(); }

#endif

void JIT::emit_op_get_by_val(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    
    emitLoad2(base, regT1, regT0, property, regT3, regT2);

    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr)));
    addSlowCase(branch32(AboveOrEqual, regT2, Address(regT0, OBJECT_OFFSETOF(JSArray, m_fastAccessCutoff))));

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT0);
    load32(BaseIndex(regT0, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + 4), regT1); // tag
    load32(BaseIndex(regT0, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), regT0); // payload
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_get_by_val), dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;

    // The slow void JIT::emitSlow_that handles accesses to arrays (below) may jump back up to here. 
    Label callGetByValJITStub(this);

    linkSlowCase(iter); // property int32 check
    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    linkSlowCase(iter); // base array check

    JITStubCall stubCall(this, cti_op_get_by_val);
    stubCall.addArgument(base);
    stubCall.addArgument(property);
    stubCall.call(dst);

    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_get_by_val));

    linkSlowCase(iter); // array fast cut-off check

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT0);
    branch32(AboveOrEqual, regT2, Address(regT0, OBJECT_OFFSETOF(ArrayStorage, m_vectorLength)), callGetByValJITStub);

    // Missed the fast region, but it is still in the vector.
    load32(BaseIndex(regT0, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + 4), regT1); // tag
    load32(BaseIndex(regT0, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), regT0); // payload

    // FIXME: Maybe we can optimize this comparison to JSValue().
    Jump skip = branch32(NotEqual, regT0, Imm32(0));
    branch32(Equal, regT1, Imm32(JSValue::CellTag), callGetByValJITStub);

    skip.link(this);
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_put_by_val(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;

    emitLoad2(base, regT1, regT0, property, regT3, regT2);

    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr)));
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT3);

    Jump inFastVector = branch32(Below, regT2, Address(regT0, OBJECT_OFFSETOF(JSArray, m_fastAccessCutoff)));

    // Check if the access is within the vector.
    addSlowCase(branch32(AboveOrEqual, regT2, Address(regT3, OBJECT_OFFSETOF(ArrayStorage, m_vectorLength))));

    // This is a write to the slow part of the vector; first, we have to check if this would be the first write to this location.
    // FIXME: should be able to handle initial write to array; increment the the number of items in the array, and potentially update fast access cutoff. 
    Jump skip = branch32(NotEqual, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + 4), Imm32(JSValue::CellTag));
    addSlowCase(branch32(Equal, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), Imm32(0)));
    skip.link(this);

    inFastVector.link(this);

    emitLoad(value, regT1, regT0);
    store32(regT0, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]))); // payload
    store32(regT1, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]) + 4)); // tag
}

void JIT::emitSlow_op_put_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;

    linkSlowCase(iter); // property int32 check
    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    linkSlowCase(iter); // base not array check

    JITStubCall stubPutByValCall(this, cti_op_put_by_val);
    stubPutByValCall.addArgument(base);
    stubPutByValCall.addArgument(property);
    stubPutByValCall.addArgument(value);
    stubPutByValCall.call();

    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_get_by_val));

    // Slow cases for immediate int accesses to arrays.
    linkSlowCase(iter); // in vector check
    linkSlowCase(iter); // written to slot check

    JITStubCall stubCall(this, cti_op_put_by_val_array);
    stubCall.addArgument(regT1, regT0);
    stubCall.addArgument(regT2);
    stubCall.addArgument(value);
    stubCall.call();
}

void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    
    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    compileGetByIdHotPath();
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_get_by_id), dst, regT1, regT0);
}

void JIT::compileGetByIdHotPath()
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be patched.
    // Additionally, for get_by_id we need patch the offset of the branch to the slow case (we patch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.
    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;
    m_propertyAccessInstructionIndex++;

    DataLabelPtr structureToCompare;
    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    addSlowCase(structureCheck);
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetGetByIdStructure);
    ASSERT(differenceBetween(hotPathBegin, structureCheck) == patchOffsetGetByIdBranchToSlowCase);

    Label externalLoad = loadPtrWithPatchToLEA(Address(regT0, OBJECT_OFFSETOF(JSObject, m_externalStorage)), regT2);
    Label externalLoadComplete(this);
    ASSERT(differenceBetween(hotPathBegin, externalLoad) == patchOffsetGetByIdExternalLoad);
    ASSERT(differenceBetween(externalLoad, externalLoadComplete) == patchLengthGetByIdExternalLoad);

    DataLabel32 displacementLabel1 = loadPtrWithAddressOffsetPatch(Address(regT2, patchGetByIdDefaultOffset), regT0); // payload
    ASSERT(differenceBetween(hotPathBegin, displacementLabel1) == patchOffsetGetByIdPropertyMapOffset1);
    DataLabel32 displacementLabel2 = loadPtrWithAddressOffsetPatch(Address(regT2, patchGetByIdDefaultOffset), regT1); // tag
    ASSERT(differenceBetween(hotPathBegin, displacementLabel2) == patchOffsetGetByIdPropertyMapOffset2);

    Label putResult(this);
    ASSERT(differenceBetween(hotPathBegin, putResult) == patchOffsetGetByIdPutResult);
}

void JIT::emitSlow_op_get_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int ident = currentInstruction[3].u.operand;

    compileGetByIdSlowCase(dst, base, &(m_codeBlock->identifier(ident)), iter);
}

void JIT::compileGetByIdSlowCase(int dst, int base, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, bool isMethodCheck)
{
    // As for the hot path of get_by_id, above, we ensure that we can use an architecture specific offset
    // so that we only need track one pointer into the slow case code - we track a pointer to the location
    // of the call (which we can use to look up the patch information), but should a array-length or
    // prototype access trampoline fail we want to bail out back to here.  To do so we can subtract back
    // the distance from the call to the head of the slow case.
    linkSlowCaseIfNotJSCell(iter, base);
    linkSlowCase(iter);

    Label coldPathBegin(this);

    JITStubCall stubCall(this, isMethodCheck ? cti_op_get_by_id_method_check : cti_op_get_by_id);
    stubCall.addArgument(regT1, regT0);
    stubCall.addArgument(ImmPtr(ident));
    Call call = stubCall.call(dst);

    ASSERT(differenceBetween(coldPathBegin, call) == patchOffsetGetByIdSlowCaseCall);

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].callReturnLocation = call;
    m_propertyAccessInstructionIndex++;
}

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    int base = currentInstruction[1].u.operand;
    int value = currentInstruction[3].u.operand;

    emitLoad2(base, regT1, regT0, value, regT3, regT2);

    emitJumpSlowCaseIfNotJSCell(base, regT1);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;
    m_propertyAccessInstructionIndex++;

    // It is important that the following instruction plants a 32bit immediate, in order that it can be patched over.
    DataLabelPtr structureToCompare;
    addSlowCase(branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure))));
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetPutByIdStructure);

    // Plant a load from a bogus ofset in the object's property map; we will patch this later, if it is to be used.
    Label externalLoad = loadPtrWithPatchToLEA(Address(regT0, OBJECT_OFFSETOF(JSObject, m_externalStorage)), regT0);
    Label externalLoadComplete(this);
    ASSERT(differenceBetween(hotPathBegin, externalLoad) == patchOffsetPutByIdExternalLoad);
    ASSERT(differenceBetween(externalLoad, externalLoadComplete) == patchLengthPutByIdExternalLoad);

    DataLabel32 displacementLabel1 = storePtrWithAddressOffsetPatch(regT2, Address(regT0, patchGetByIdDefaultOffset)); // payload
    DataLabel32 displacementLabel2 = storePtrWithAddressOffsetPatch(regT3, Address(regT0, patchGetByIdDefaultOffset)); // tag
    ASSERT(differenceBetween(hotPathBegin, displacementLabel1) == patchOffsetPutByIdPropertyMapOffset1);
    ASSERT(differenceBetween(hotPathBegin, displacementLabel2) == patchOffsetPutByIdPropertyMapOffset2);
}

void JIT::emitSlow_op_put_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int base = currentInstruction[1].u.operand;
    int ident = currentInstruction[2].u.operand;

    linkSlowCaseIfNotJSCell(iter, base);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_put_by_id);
    stubCall.addArgument(regT1, regT0);
    stubCall.addArgument(ImmPtr(&(m_codeBlock->identifier(ident))));
    stubCall.addArgument(regT3, regT2); 
    Call call = stubCall.call();

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].callReturnLocation = call;
    m_propertyAccessInstructionIndex++;
}

// Compile a store into an object's property storage.  May overwrite base.
void JIT::compilePutDirectOffset(RegisterID base, RegisterID valueTag, RegisterID valuePayload, Structure* structure, size_t cachedOffset)
{
    int offset = cachedOffset;
    if (structure->isUsingInlineStorage())
        offset += OBJECT_OFFSETOF(JSObject, m_inlineStorage) /  sizeof(Register);
    else
        loadPtr(Address(base, OBJECT_OFFSETOF(JSObject, m_externalStorage)), base);
    emitStore(offset, valueTag, valuePayload, base);
}

// Compile a load from an object's property storage.  May overwrite base.
void JIT::compileGetDirectOffset(RegisterID base, RegisterID resultTag, RegisterID resultPayload, Structure* structure, size_t cachedOffset)
{
    int offset = cachedOffset;
    if (structure->isUsingInlineStorage())
        offset += OBJECT_OFFSETOF(JSObject, m_inlineStorage) / sizeof(Register);
    else
        loadPtr(Address(base, OBJECT_OFFSETOF(JSObject, m_externalStorage)), base);
    emitLoad(offset, resultTag, resultPayload, base);
}

void JIT::compileGetDirectOffset(JSObject* base, RegisterID temp, RegisterID resultTag, RegisterID resultPayload, size_t cachedOffset)
{
    if (base->isUsingInlineStorage()) {
        load32(reinterpret_cast<char*>(&base->m_inlineStorage[cachedOffset]), resultPayload);
        load32(reinterpret_cast<char*>(&base->m_inlineStorage[cachedOffset]) + 4, resultTag);
        return;
    }

    size_t offset = cachedOffset * sizeof(JSValue);

    PropertyStorage* protoPropertyStorage = &base->m_externalStorage;
    loadPtr(static_cast<void*>(protoPropertyStorage), temp);
    load32(Address(temp, offset), resultPayload);
    load32(Address(temp, offset + 4), resultTag);
}

void JIT::privateCompilePutByIdTransition(StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, ReturnAddressPtr returnAddress)
{
    // It is assumed that regT0 contains the basePayload and regT1 contains the baseTag.  The value can be found on the stack.

    JumpList failureCases;
    failureCases.append(branch32(NotEqual, regT1, Imm32(JSValue::CellTag)));

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    failureCases.append(branchPtr(NotEqual, regT2, ImmPtr(oldStructure)));

    // Verify that nothing in the prototype chain has a setter for this property. 
    for (RefPtr<Structure>* it = chain->head(); *it; ++it) {
        loadPtr(Address(regT2, OBJECT_OFFSETOF(Structure, m_prototype)), regT2);
        loadPtr(Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
        failureCases.append(branchPtr(NotEqual, regT2, ImmPtr(it->get())));
    }

    // Reallocate property storage if needed.
    Call callTarget;
    bool willNeedStorageRealloc = oldStructure->propertyStorageCapacity() != newStructure->propertyStorageCapacity();
    if (willNeedStorageRealloc) {
        // This trampoline was called to like a JIT stub; before we can can call again we need to
        // remove the return address from the stack, to prevent the stack from becoming misaligned.
        preserveReturnAddressAfterCall(regT3);
 
        JITStubCall stubCall(this, cti_op_put_by_id_transition_realloc);
        stubCall.skipArgument(); // base
        stubCall.skipArgument(); // ident
        stubCall.skipArgument(); // value
        stubCall.addArgument(Imm32(oldStructure->propertyStorageCapacity()));
        stubCall.addArgument(Imm32(newStructure->propertyStorageCapacity()));
        stubCall.call(regT0);

        restoreReturnAddressBeforeReturn(regT3);
    }

    sub32(Imm32(1), AbsoluteAddress(oldStructure->addressOfCount()));
    add32(Imm32(1), AbsoluteAddress(newStructure->addressOfCount()));
    storePtr(ImmPtr(newStructure), Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)));
 
    load32(Address(stackPointerRegister, offsetof(struct JITStackFrame, args[2]) + sizeof(void*)), regT3);
    load32(Address(stackPointerRegister, offsetof(struct JITStackFrame, args[2]) + sizeof(void*) + 4), regT2);

    // Write the value
    compilePutDirectOffset(regT0, regT2, regT3, newStructure, cachedOffset);

    ret();
    
    ASSERT(!failureCases.empty());
    failureCases.link(this);
    restoreArgumentReferenceForTrampoline();
    Call failureCall = tailRecursiveCall();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    patchBuffer.link(failureCall, FunctionPtr(cti_op_put_by_id_fail));

    if (willNeedStorageRealloc) {
        ASSERT(m_calls.size() == 1);
        patchBuffer.link(m_calls[0].from, FunctionPtr(cti_op_put_by_id_transition_realloc));
    }
    
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relinkCallerToTrampoline(returnAddress, entryLabel);
}

void JIT::patchGetByIdSelf(CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);

    // We don't want to patch more than once - in future go to cti_op_get_by_id_generic.
    // Should probably go to JITStubs::cti_op_get_by_id_fail, but that doesn't do anything interesting right now.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_self_fail));

    int offset = sizeof(JSValue) * cachedOffset;

    // If we're patching to use inline storage, convert the initial load to a lea; this avoids the extra load
    // and makes the subsequent load's offset automatically correct
    if (structure->isUsingInlineStorage())
        repatchBuffer.repatchLoadPtrToLEA(stubInfo->hotPathBegin.instructionAtOffset(patchOffsetGetByIdExternalLoad));

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetGetByIdStructure), structure);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetGetByIdPropertyMapOffset1), offset); // payload
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetGetByIdPropertyMapOffset2), offset + 4); // tag
}

void JIT::patchMethodCallProto(CodeBlock* codeBlock, MethodCallLinkInfo& methodCallLinkInfo, JSFunction* callee, Structure* structure, JSObject* proto, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);

    ASSERT(!methodCallLinkInfo.cachedStructure);
    methodCallLinkInfo.cachedStructure = structure;
    structure->ref();

    Structure* prototypeStructure = proto->structure();
    ASSERT(!methodCallLinkInfo.cachedPrototypeStructure);
    methodCallLinkInfo.cachedPrototypeStructure = prototypeStructure;
    prototypeStructure->ref();

    repatchBuffer.repatch(methodCallLinkInfo.structureLabel, structure);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckProtoObj), proto);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckProtoStruct), prototypeStructure);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckPutFunction), callee);

    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id));
}

void JIT::patchPutByIdReplace(CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_put_by_id_generic));

    int offset = sizeof(JSValue) * cachedOffset;

    // If we're patching to use inline storage, convert the initial load to a lea; this avoids the extra load
    // and makes the subsequent load's offset automatically correct
    if (structure->isUsingInlineStorage())
        repatchBuffer.repatchLoadPtrToLEA(stubInfo->hotPathBegin.instructionAtOffset(patchOffsetPutByIdExternalLoad));

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetPutByIdStructure), structure);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset1), offset); // payload
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset2), offset + 4); // tag
}

void JIT::privateCompilePatchGetArrayLength(ReturnAddressPtr returnAddress)
{
    StructureStubInfo* stubInfo = &m_codeBlock->getStubInfo(returnAddress);
    
    // regT0 holds a JSCell*

    // Check for array
    Jump failureCases1 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr));

    // Checks out okay! - get the length from the storage
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT2);
    load32(Address(regT2, OBJECT_OFFSETOF(ArrayStorage, m_length)), regT2);

    Jump failureCases2 = branch32(Above, regT2, Imm32(INT_MAX));
    move(regT2, regT0);
    move(Imm32(JSValue::Int32Tag), regT1);
    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_array_fail));
}

void JIT::privateCompileGetByIdProto(StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame)
{
    // regT0 holds a JSCell*

    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));

    Jump failureCases1 = checkStructure(regT0, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
    move(ImmPtr(prototypeStructure), regT3);
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3);
#else
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));
#endif

    // Checks out okay! - getDirectOffset
    compileGetDirectOffset(protoObject, regT2, regT1, regT0, cachedOffset);

    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_proto_list));
}


void JIT::privateCompileGetByIdSelfList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, size_t cachedOffset)
{
    // regT0 holds a JSCell*
    
    Jump failureCase = checkStructure(regT0, structure);
    compileGetDirectOffset(regT0, regT1, regT0, structure, cachedOffset);
    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = polymorphicStructures->list[currentIndex - 1].stubRoutine;
    if (!lastProtoBegin)
        lastProtoBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);

    patchBuffer.link(failureCase, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();

    structure->ref();
    polymorphicStructures->list[currentIndex].set(entryLabel, structure);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdProtoList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, CallFrame* callFrame)
{
    // regT0 holds a JSCell*
    
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));

    // Check eax is an object of the right Structure.
    Jump failureCases1 = checkStructure(regT0, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
    move(ImmPtr(prototypeStructure), regT3);
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3);
#else
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));
#endif

    compileGetDirectOffset(protoObject, regT2, regT1, regT0, cachedOffset);

    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;
    patchBuffer.link(failureCases1, lastProtoBegin);
    patchBuffer.link(failureCases2, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();

    structure->ref();
    prototypeStructure->ref();
    prototypeStructures->list[currentIndex].set(entryLabel, structure, prototypeStructure);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdChainList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, CallFrame* callFrame)
{
    // regT0 holds a JSCell*
    
    ASSERT(count);
    
    JumpList bucketsOfFail;

    // Check eax is an object of the right Structure.
    bucketsOfFail.append(checkStructure(regT0, structure));

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
        move(ImmPtr(currStructure), regT3);
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);

    compileGetDirectOffset(protoObject, regT2, regT1, regT0, cachedOffset);
    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;

    patchBuffer.link(bucketsOfFail, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();

    // Track the stub we have created so that it will be deleted later.
    structure->ref();
    chain->ref();
    prototypeStructures->list[currentIndex].set(entryLabel, structure, chain);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdChain(StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame)
{
    // regT0 holds a JSCell*
    
    ASSERT(count);
    
    JumpList bucketsOfFail;

    // Check eax is an object of the right Structure.
    bucketsOfFail.append(checkStructure(regT0, structure));

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
        move(ImmPtr(currStructure), regT3);
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);

    compileGetDirectOffset(protoObject, regT2, regT1, regT0, cachedOffset);
    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    patchBuffer.link(bucketsOfFail, stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall));

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_proto_list));
}

/* ------------------------------ END: !ENABLE / ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

#endif // !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

#else // USE(JSVALUE32_64)

void JIT::emit_op_get_by_val(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT1);
#if USE(JSVALUE64)
    // This is technically incorrect - we're zero-extending an int32.  On the hot path this doesn't matter.
    // We check the value as if it was a uint32 against the m_fastAccessCutoff - which will always fail if
    // number was signed since m_fastAccessCutoff is always less than intmax (since the total allocation
    // size is always less than 4Gb).  As such zero extending wil have been correct (and extending the value
    // to 64-bits is necessary since it's used in the address calculation.  We zero extend rather than sign
    // extending since it makes it easier to re-tag the value in the slow case.
    zeroExtend32ToPtr(regT1, regT1);
#else
    emitFastArithImmToInt(regT1);
#endif
    emitJumpSlowCaseIfNotJSCell(regT0);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr)));

    // This is an array; get the m_storage pointer into ecx, then check if the index is below the fast cutoff
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT2);
    addSlowCase(branch32(AboveOrEqual, regT1, Address(regT0, OBJECT_OFFSETOF(JSArray, m_fastAccessCutoff))));

    // Get the value from the vector
    loadPtr(BaseIndex(regT2, regT1, ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_put_by_val(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[1].u.operand, regT0, currentInstruction[2].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT1);
#if USE(JSVALUE64)
    // See comment in op_get_by_val.
    zeroExtend32ToPtr(regT1, regT1);
#else
    emitFastArithImmToInt(regT1);
#endif
    emitJumpSlowCaseIfNotJSCell(regT0);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr)));

    // This is an array; get the m_storage pointer into ecx, then check if the index is below the fast cutoff
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT2);
    Jump inFastVector = branch32(Below, regT1, Address(regT0, OBJECT_OFFSETOF(JSArray, m_fastAccessCutoff)));
    // No; oh well, check if the access if within the vector - if so, we may still be okay.
    addSlowCase(branch32(AboveOrEqual, regT1, Address(regT2, OBJECT_OFFSETOF(ArrayStorage, m_vectorLength))));

    // This is a write to the slow part of the vector; first, we have to check if this would be the first write to this location.
    // FIXME: should be able to handle initial write to array; increment the the number of items in the array, and potentially update fast access cutoff. 
    addSlowCase(branchTestPtr(Zero, BaseIndex(regT2, regT1, ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0]))));

    // All good - put the value into the array.
    inFastVector.link(this);
    emitGetVirtualRegister(currentInstruction[3].u.operand, regT0);
    storePtr(regT0, BaseIndex(regT2, regT1, ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
}

void JIT::emit_op_put_by_index(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_put_by_index);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(Imm32(currentInstruction[2].u.operand));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_put_getter(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_put_getter);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_put_setter(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_put_setter);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_del_by_id(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_del_by_id);
    stubCall.addArgument(currentInstruction[2].u.operand, regT2);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[3].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}


#if !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

/* ------------------------------ BEGIN: !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

// Treat these as nops - the call will be handed as a regular get_by_id/op_call pair.
void JIT::emit_op_method_check(Instruction*) {}
void JIT::emitSlow_op_method_check(Instruction*, Vector<SlowCaseEntry>::iterator&) { ASSERT_NOT_REACHED(); }
#if ENABLE(JIT_OPTIMIZE_METHOD_CALLS)
#error "JIT_OPTIMIZE_METHOD_CALLS requires JIT_OPTIMIZE_PROPERTY_ACCESS"
#endif

void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    unsigned resultVReg = currentInstruction[1].u.operand;
    unsigned baseVReg = currentInstruction[2].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    emitGetVirtualRegister(baseVReg, regT0);
    JITStubCall stubCall(this, cti_op_get_by_id_generic);
    stubCall.addArgument(regT0);
    stubCall.addArgument(ImmPtr(ident));
    stubCall.call(resultVReg);

    m_propertyAccessInstructionIndex++;
}

void JIT::emitSlow_op_get_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    unsigned baseVReg = currentInstruction[1].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));
    unsigned valueVReg = currentInstruction[3].u.operand;

    emitGetVirtualRegisters(baseVReg, regT0, valueVReg, regT1);

    JITStubCall stubCall(this, cti_op_put_by_id_generic);
    stubCall.addArgument(regT0);
    stubCall.addArgument(ImmPtr(ident));
    stubCall.addArgument(regT1);
    stubCall.call();

    m_propertyAccessInstructionIndex++;
}

void JIT::emitSlow_op_put_by_id(Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    ASSERT_NOT_REACHED();
}

#else // !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

/* ------------------------------ BEGIN: ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

#if ENABLE(JIT_OPTIMIZE_METHOD_CALLS)

void JIT::emit_op_method_check(Instruction* currentInstruction)
{
    // Assert that the following instruction is a get_by_id.
    ASSERT(m_interpreter->getOpcodeID((currentInstruction + OPCODE_LENGTH(op_method_check))->u.opcode) == op_get_by_id);

    currentInstruction += OPCODE_LENGTH(op_method_check);
    unsigned resultVReg = currentInstruction[1].u.operand;
    unsigned baseVReg = currentInstruction[2].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    emitGetVirtualRegister(baseVReg, regT0);

    // Do the method check - check the object & its prototype's structure inline (this is the common case).
    m_methodCallCompilationInfo.append(MethodCallCompilationInfo(m_propertyAccessInstructionIndex));
    MethodCallCompilationInfo& info = m_methodCallCompilationInfo.last();

    Jump notCell = emitJumpIfNotJSCell(regT0);

    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceMethodCheck);

    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), info.structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    DataLabelPtr protoStructureToCompare, protoObj = moveWithPatch(ImmPtr(0), regT1);
    Jump protoStructureCheck = branchPtrWithPatch(NotEqual, Address(regT1, OBJECT_OFFSETOF(JSCell, m_structure)), protoStructureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));

    // This will be relinked to load the function without doing a load.
    DataLabelPtr putFunction = moveWithPatch(ImmPtr(0), regT0);

    END_UNINTERRUPTED_SEQUENCE(sequenceMethodCheck);

    Jump match = jump();

    ASSERT(differenceBetween(info.structureToCompare, protoObj) == patchOffsetMethodCheckProtoObj);
    ASSERT(differenceBetween(info.structureToCompare, protoStructureToCompare) == patchOffsetMethodCheckProtoStruct);
    ASSERT(differenceBetween(info.structureToCompare, putFunction) == patchOffsetMethodCheckPutFunction);

    // Link the failure cases here.
    notCell.link(this);
    structureCheck.link(this);
    protoStructureCheck.link(this);

    // Do a regular(ish) get_by_id (the slow case will be link to
    // cti_op_get_by_id_method_check instead of cti_op_get_by_id.
    compileGetByIdHotPath(resultVReg, baseVReg, ident, m_propertyAccessInstructionIndex++);

    match.link(this);
    emitPutVirtualRegister(resultVReg);

    // We've already generated the following get_by_id, so make sure it's skipped over.
    m_bytecodeIndex += OPCODE_LENGTH(op_get_by_id);
}

void JIT::emitSlow_op_method_check(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    currentInstruction += OPCODE_LENGTH(op_method_check);
    unsigned resultVReg = currentInstruction[1].u.operand;
    unsigned baseVReg = currentInstruction[2].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    compileGetByIdSlowCase(resultVReg, baseVReg, ident, iter, true);

    // We've already generated the following get_by_id, so make sure it's skipped over.
    m_bytecodeIndex += OPCODE_LENGTH(op_get_by_id);
}

#else //!ENABLE(JIT_OPTIMIZE_METHOD_CALLS)

// Treat these as nops - the call will be handed as a regular get_by_id/op_call pair.
void JIT::emit_op_method_check(Instruction*) {}
void JIT::emitSlow_op_method_check(Instruction*, Vector<SlowCaseEntry>::iterator&) { ASSERT_NOT_REACHED(); }

#endif

void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    unsigned resultVReg = currentInstruction[1].u.operand;
    unsigned baseVReg = currentInstruction[2].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    emitGetVirtualRegister(baseVReg, regT0);
    compileGetByIdHotPath(resultVReg, baseVReg, ident, m_propertyAccessInstructionIndex++);
    emitPutVirtualRegister(resultVReg);
}

void JIT::compileGetByIdHotPath(int, int baseVReg, Identifier*, unsigned propertyAccessInstructionIndex)
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be patched.
    // Additionally, for get_by_id we need patch the offset of the branch to the slow case (we patch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceGetByIdHotPath);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    DataLabelPtr structureToCompare;
    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    addSlowCase(structureCheck);
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetGetByIdStructure);
    ASSERT(differenceBetween(hotPathBegin, structureCheck) == patchOffsetGetByIdBranchToSlowCase);

    Label externalLoad = loadPtrWithPatchToLEA(Address(regT0, OBJECT_OFFSETOF(JSObject, m_externalStorage)), regT0);
    Label externalLoadComplete(this);
    ASSERT(differenceBetween(hotPathBegin, externalLoad) == patchOffsetGetByIdExternalLoad);
    ASSERT(differenceBetween(externalLoad, externalLoadComplete) == patchLengthGetByIdExternalLoad);

    DataLabel32 displacementLabel = loadPtrWithAddressOffsetPatch(Address(regT0, patchGetByIdDefaultOffset), regT0);
    ASSERT(differenceBetween(hotPathBegin, displacementLabel) == patchOffsetGetByIdPropertyMapOffset);

    Label putResult(this);

    END_UNINTERRUPTED_SEQUENCE(sequenceGetByIdHotPath);

    ASSERT(differenceBetween(hotPathBegin, putResult) == patchOffsetGetByIdPutResult);
}

void JIT::emitSlow_op_get_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned resultVReg = currentInstruction[1].u.operand;
    unsigned baseVReg = currentInstruction[2].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    compileGetByIdSlowCase(resultVReg, baseVReg, ident, iter, false);
}

void JIT::compileGetByIdSlowCase(int resultVReg, int baseVReg, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, bool isMethodCheck)
{
    // As for the hot path of get_by_id, above, we ensure that we can use an architecture specific offset
    // so that we only need track one pointer into the slow case code - we track a pointer to the location
    // of the call (which we can use to look up the patch information), but should a array-length or
    // prototype access trampoline fail we want to bail out back to here.  To do so we can subtract back
    // the distance from the call to the head of the slow case.

    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceGetByIdSlowCase);

#ifndef NDEBUG
    Label coldPathBegin(this);
#endif
    JITStubCall stubCall(this, isMethodCheck ? cti_op_get_by_id_method_check : cti_op_get_by_id);
    stubCall.addArgument(regT0);
    stubCall.addArgument(ImmPtr(ident));
    Call call = stubCall.call(resultVReg);

    END_UNINTERRUPTED_SEQUENCE(sequenceGetByIdSlowCase);

    ASSERT(differenceBetween(coldPathBegin, call) == patchOffsetGetByIdSlowCaseCall);

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex].callReturnLocation = call;
    m_propertyAccessInstructionIndex++;
}

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    unsigned baseVReg = currentInstruction[1].u.operand;
    unsigned valueVReg = currentInstruction[3].u.operand;

    unsigned propertyAccessInstructionIndex = m_propertyAccessInstructionIndex++;

    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    emitGetVirtualRegisters(baseVReg, regT0, valueVReg, regT1);

    // Jump to a slow case if either the base object is an immediate, or if the Structure does not match.
    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    BEGIN_UNINTERRUPTED_SEQUENCE(sequencePutById);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    // It is important that the following instruction plants a 32bit immediate, in order that it can be patched over.
    DataLabelPtr structureToCompare;
    addSlowCase(branchPtrWithPatch(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure))));
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetPutByIdStructure);

    // Plant a load from a bogus ofset in the object's property map; we will patch this later, if it is to be used.
    Label externalLoad = loadPtrWithPatchToLEA(Address(regT0, OBJECT_OFFSETOF(JSObject, m_externalStorage)), regT0);
    Label externalLoadComplete(this);
    ASSERT(differenceBetween(hotPathBegin, externalLoad) == patchOffsetPutByIdExternalLoad);
    ASSERT(differenceBetween(externalLoad, externalLoadComplete) == patchLengthPutByIdExternalLoad);

    DataLabel32 displacementLabel = storePtrWithAddressOffsetPatch(regT1, Address(regT0, patchGetByIdDefaultOffset));

    END_UNINTERRUPTED_SEQUENCE(sequencePutById);

    ASSERT(differenceBetween(hotPathBegin, displacementLabel) == patchOffsetPutByIdPropertyMapOffset);
}

void JIT::emitSlow_op_put_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned baseVReg = currentInstruction[1].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));

    unsigned propertyAccessInstructionIndex = m_propertyAccessInstructionIndex++;

    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_put_by_id);
    stubCall.addArgument(regT0);
    stubCall.addArgument(ImmPtr(ident));
    stubCall.addArgument(regT1);
    Call call = stubCall.call();

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].callReturnLocation = call;
}

// Compile a store into an object's property storage.  May overwrite the
// value in objectReg.
void JIT::compilePutDirectOffset(RegisterID base, RegisterID value, Structure* structure, size_t cachedOffset)
{
    int offset = cachedOffset * sizeof(JSValue);
    if (structure->isUsingInlineStorage())
        offset += OBJECT_OFFSETOF(JSObject, m_inlineStorage);
    else
        loadPtr(Address(base, OBJECT_OFFSETOF(JSObject, m_externalStorage)), base);
    storePtr(value, Address(base, offset));
}

// Compile a load from an object's property storage.  May overwrite base.
void JIT::compileGetDirectOffset(RegisterID base, RegisterID result, Structure* structure, size_t cachedOffset)
{
    int offset = cachedOffset * sizeof(JSValue);
    if (structure->isUsingInlineStorage())
        offset += OBJECT_OFFSETOF(JSObject, m_inlineStorage);
    else
        loadPtr(Address(base, OBJECT_OFFSETOF(JSObject, m_externalStorage)), base);
    loadPtr(Address(base, offset), result);
}

void JIT::compileGetDirectOffset(JSObject* base, RegisterID temp, RegisterID result, size_t cachedOffset)
{
    if (base->isUsingInlineStorage())
        loadPtr(static_cast<void*>(&base->m_inlineStorage[cachedOffset]), result);
    else {
        PropertyStorage* protoPropertyStorage = &base->m_externalStorage;
        loadPtr(static_cast<void*>(protoPropertyStorage), temp);
        loadPtr(Address(temp, cachedOffset * sizeof(JSValue)), result);
    } 
}

void JIT::privateCompilePutByIdTransition(StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, ReturnAddressPtr returnAddress)
{
    JumpList failureCases;
    // Check eax is an object of the right Structure.
    failureCases.append(emitJumpIfNotJSCell(regT0));
    failureCases.append(branchPtr(NotEqual, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), ImmPtr(oldStructure)));
    JumpList successCases;

    // ecx = baseObject
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    // proto(ecx) = baseObject->structure()->prototype()
    failureCases.append(branch32(NotEqual, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo) + OBJECT_OFFSETOF(TypeInfo, m_type)), Imm32(ObjectType)));

    loadPtr(Address(regT2, OBJECT_OFFSETOF(Structure, m_prototype)), regT2);
    
    // ecx = baseObject->m_structure
    for (RefPtr<Structure>* it = chain->head(); *it; ++it) {
        // null check the prototype
        successCases.append(branchPtr(Equal, regT2, ImmPtr(JSValue::encode(jsNull()))));

        // Check the structure id
        failureCases.append(branchPtr(NotEqual, Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), ImmPtr(it->get())));
        
        loadPtr(Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
        failureCases.append(branch32(NotEqual, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo) + OBJECT_OFFSETOF(TypeInfo, m_type)), Imm32(ObjectType)));
        loadPtr(Address(regT2, OBJECT_OFFSETOF(Structure, m_prototype)), regT2);
    }

    successCases.link(this);

    Call callTarget;

    // emit a call only if storage realloc is needed
    bool willNeedStorageRealloc = oldStructure->propertyStorageCapacity() != newStructure->propertyStorageCapacity();
    if (willNeedStorageRealloc) {
        // This trampoline was called to like a JIT stub; before we can can call again we need to
        // remove the return address from the stack, to prevent the stack from becoming misaligned.
        preserveReturnAddressAfterCall(regT3);
 
        JITStubCall stubCall(this, cti_op_put_by_id_transition_realloc);
        stubCall.skipArgument(); // base
        stubCall.skipArgument(); // ident
        stubCall.skipArgument(); // value
        stubCall.addArgument(Imm32(oldStructure->propertyStorageCapacity()));
        stubCall.addArgument(Imm32(newStructure->propertyStorageCapacity()));
        stubCall.call(regT0);
        emitGetJITStubArg(2, regT1);

        restoreReturnAddressBeforeReturn(regT3);
    }

    // Assumes m_refCount can be decremented easily, refcount decrement is safe as 
    // codeblock should ensure oldStructure->m_refCount > 0
    sub32(Imm32(1), AbsoluteAddress(oldStructure->addressOfCount()));
    add32(Imm32(1), AbsoluteAddress(newStructure->addressOfCount()));
    storePtr(ImmPtr(newStructure), Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)));

    // write the value
    compilePutDirectOffset(regT0, regT1, newStructure, cachedOffset);

    ret();
    
    ASSERT(!failureCases.empty());
    failureCases.link(this);
    restoreArgumentReferenceForTrampoline();
    Call failureCall = tailRecursiveCall();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    patchBuffer.link(failureCall, FunctionPtr(cti_op_put_by_id_fail));

    if (willNeedStorageRealloc) {
        ASSERT(m_calls.size() == 1);
        patchBuffer.link(m_calls[0].from, FunctionPtr(cti_op_put_by_id_transition_realloc));
    }
    
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relinkCallerToTrampoline(returnAddress, entryLabel);
}

void JIT::patchGetByIdSelf(CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);

    // We don't want to patch more than once - in future go to cti_op_get_by_id_generic.
    // Should probably go to cti_op_get_by_id_fail, but that doesn't do anything interesting right now.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_self_fail));

    int offset = sizeof(JSValue) * cachedOffset;

    // If we're patching to use inline storage, convert the initial load to a lea; this avoids the extra load
    // and makes the subsequent load's offset automatically correct
    if (structure->isUsingInlineStorage())
        repatchBuffer.repatchLoadPtrToLEA(stubInfo->hotPathBegin.instructionAtOffset(patchOffsetGetByIdExternalLoad));

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetGetByIdStructure), structure);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetGetByIdPropertyMapOffset), offset);
}

void JIT::patchMethodCallProto(CodeBlock* codeBlock, MethodCallLinkInfo& methodCallLinkInfo, JSFunction* callee, Structure* structure, JSObject* proto, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);

    ASSERT(!methodCallLinkInfo.cachedStructure);
    methodCallLinkInfo.cachedStructure = structure;
    structure->ref();

    Structure* prototypeStructure = proto->structure();
    ASSERT(!methodCallLinkInfo.cachedPrototypeStructure);
    methodCallLinkInfo.cachedPrototypeStructure = prototypeStructure;
    prototypeStructure->ref();

    repatchBuffer.repatch(methodCallLinkInfo.structureLabel, structure);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckProtoObj), proto);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckProtoStruct), prototypeStructure);
    repatchBuffer.repatch(methodCallLinkInfo.structureLabel.dataLabelPtrAtOffset(patchOffsetMethodCheckPutFunction), callee);

    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id));
}

void JIT::patchPutByIdReplace(CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_put_by_id_generic));

    int offset = sizeof(JSValue) * cachedOffset;

    // If we're patching to use inline storage, convert the initial load to a lea; this avoids the extra load
    // and makes the subsequent load's offset automatically correct
    if (structure->isUsingInlineStorage())
        repatchBuffer.repatchLoadPtrToLEA(stubInfo->hotPathBegin.instructionAtOffset(patchOffsetPutByIdExternalLoad));

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetPutByIdStructure), structure);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset), offset);
}

void JIT::privateCompilePatchGetArrayLength(ReturnAddressPtr returnAddress)
{
    StructureStubInfo* stubInfo = &m_codeBlock->getStubInfo(returnAddress);

    // Check eax is an array
    Jump failureCases1 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr));

    // Checks out okay! - get the length from the storage
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSArray, m_storage)), regT2);
    load32(Address(regT2, OBJECT_OFFSETOF(ArrayStorage, m_length)), regT2);

    Jump failureCases2 = branch32(Above, regT2, Imm32(JSImmediate::maxImmediateInt));

    emitFastArithIntToImmNoCheck(regT2, regT0);
    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_array_fail));
}

void JIT::privateCompileGetByIdProto(StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame)
{
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));

    // Check eax is an object of the right Structure.
    Jump failureCases1 = checkStructure(regT0, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
    move(ImmPtr(prototypeStructure), regT3);
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3);
#else
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));
#endif

    // Checks out okay! - getDirectOffset
    compileGetDirectOffset(protoObject, regT1, regT0, cachedOffset);

    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_proto_list));
}

void JIT::privateCompileGetByIdSelfList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, size_t cachedOffset)
{
    Jump failureCase = checkStructure(regT0, structure);
    compileGetDirectOffset(regT0, regT0, structure, cachedOffset);
    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = polymorphicStructures->list[currentIndex - 1].stubRoutine;
    if (!lastProtoBegin)
        lastProtoBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);

    patchBuffer.link(failureCase, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();

    structure->ref();
    polymorphicStructures->list[currentIndex].set(entryLabel, structure);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdProtoList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, CallFrame* callFrame)
{
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));

    // Check eax is an object of the right Structure.
    Jump failureCases1 = checkStructure(regT0, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
    move(ImmPtr(prototypeStructure), regT3);
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3);
#else
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));
#endif

    // Checks out okay! - getDirectOffset
    compileGetDirectOffset(protoObject, regT1, regT0, cachedOffset);

    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;
    patchBuffer.link(failureCases1, lastProtoBegin);
    patchBuffer.link(failureCases2, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();

    structure->ref();
    prototypeStructure->ref();
    prototypeStructures->list[currentIndex].set(entryLabel, structure, prototypeStructure);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdChainList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, CallFrame* callFrame)
{
    ASSERT(count);
    
    JumpList bucketsOfFail;

    // Check eax is an object of the right Structure.
    Jump baseObjectCheck = checkStructure(regT0, structure);
    bucketsOfFail.append(baseObjectCheck);

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
        move(ImmPtr(currStructure), regT3);
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);

    compileGetDirectOffset(protoObject, regT1, regT0, cachedOffset);
    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;

    patchBuffer.link(bucketsOfFail, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();

    // Track the stub we have created so that it will be deleted later.
    structure->ref();
    chain->ref();
    prototypeStructures->list[currentIndex].set(entryLabel, structure, chain);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);
}

void JIT::privateCompileGetByIdChain(StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame)
{
    ASSERT(count);
    
    JumpList bucketsOfFail;

    // Check eax is an object of the right Structure.
    bucketsOfFail.append(checkStructure(regT0, structure));

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
        move(ImmPtr(currStructure), regT3);
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), regT3));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);

    compileGetDirectOffset(protoObject, regT1, regT0, cachedOffset);
    Jump success = jump();

    LinkBuffer patchBuffer(this, m_codeBlock->executablePool());

    // Use the patch information to link the failure cases back to the original slow case routine.
    patchBuffer.link(bucketsOfFail, stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall));

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, entryLabel);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_proto_list));
}

/* ------------------------------ END: !ENABLE / ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

#endif // !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

#endif // USE(JSVALUE32_64)

} // namespace JSC

#endif // ENABLE(JIT)
