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

#if ENABLE(JIT)
#include "JIT.h"

#include "CodeBlock.h"
#include "GetterSetter.h"
#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSPropertyNameIterator.h"
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
#if USE(JSVALUE64)

JIT::CodeRef JIT::stringGetByValStubGenerator(JSGlobalData* globalData)
{
    JSInterfaceJIT jit;
    JumpList failures;
    failures.append(jit.branchPtr(NotEqual, Address(regT0, JSCell::classInfoOffset()), TrustedImmPtr(&JSString::s_info)));

    // Load string length to regT2, and start the process of loading the data pointer into regT0
    jit.load32(Address(regT0, ThunkHelpers::jsStringLengthOffset()), regT2);
    jit.loadPtr(Address(regT0, ThunkHelpers::jsStringValueOffset()), regT0);
    failures.append(jit.branchTest32(Zero, regT0));

    // Do an unsigned compare to simultaneously filter negative indices as well as indices that are too large
    failures.append(jit.branch32(AboveOrEqual, regT1, regT2));
    
    // Load the character
    JumpList is16Bit;
    JumpList cont8Bit;
    // Load the string flags
    jit.loadPtr(Address(regT0, ThunkHelpers::stringImplFlagsOffset()), regT2);
    jit.loadPtr(Address(regT0, ThunkHelpers::stringImplDataOffset()), regT0);
    is16Bit.append(jit.branchTest32(Zero, regT2, TrustedImm32(ThunkHelpers::stringImpl8BitFlag())));
    jit.load8(BaseIndex(regT0, regT1, TimesOne, 0), regT0);
    cont8Bit.append(jit.jump());
    is16Bit.link(&jit);
    jit.load16(BaseIndex(regT0, regT1, TimesTwo, 0), regT0);
    cont8Bit.link(&jit);

    failures.append(jit.branch32(AboveOrEqual, regT0, TrustedImm32(0x100)));
    jit.move(TrustedImmPtr(globalData->smallStrings.singleCharacterStrings()), regT1);
    jit.loadPtr(BaseIndex(regT1, regT0, ScalePtr, 0), regT0);
    jit.ret();
    
    failures.link(&jit);
    jit.move(TrustedImm32(0), regT0);
    jit.ret();
    
    LinkBuffer patchBuffer(*globalData, &jit, GLOBAL_THUNK_ID);
    return patchBuffer.finalizeCode();
}

void JIT::emit_op_get_by_val(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;

    emitGetVirtualRegisters(base, regT0, property, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT1);

    // This is technically incorrect - we're zero-extending an int32.  On the hot path this doesn't matter.
    // We check the value as if it was a uint32 against the m_vectorLength - which will always fail if
    // number was signed since m_vectorLength is always less than intmax (since the total allocation
    // size is always less than 4Gb).  As such zero extending wil have been correct (and extending the value
    // to 64-bits is necessary since it's used in the address calculation.  We zero extend rather than sign
    // extending since it makes it easier to re-tag the value in the slow case.
    zeroExtend32ToPtr(regT1, regT1);

    emitJumpSlowCaseIfNotJSCell(regT0, base);
    addSlowCase(branchPtr(NotEqual, Address(regT0, JSCell::classInfoOffset()), TrustedImmPtr(&JSArray::s_info)));

    loadPtr(Address(regT0, JSArray::storageOffset()), regT2);
    addSlowCase(branch32(AboveOrEqual, regT1, Address(regT0, JSArray::vectorLengthOffset())));

    loadPtr(BaseIndex(regT2, regT1, ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])), regT0);
    addSlowCase(branchTestPtr(Zero, regT0));

    emitValueProfilingSite();
    emitPutVirtualRegister(dst);
}

void JIT::emitSlow_op_get_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    
    linkSlowCase(iter); // property int32 check
    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    Jump nonCell = jump();
    linkSlowCase(iter); // base array check
    Jump notString = branchPtr(NotEqual, Address(regT0, JSCell::classInfoOffset()), TrustedImmPtr(&JSString::s_info));
    emitNakedCall(CodeLocationLabel(m_globalData->getCTIStub(stringGetByValStubGenerator).code()));
    Jump failed = branchTestPtr(Zero, regT0);
    emitPutVirtualRegister(dst, regT0);
    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_get_by_val));
    failed.link(this);
    notString.link(this);
    nonCell.link(this);
    
    linkSlowCase(iter); // vector length check
    linkSlowCase(iter); // empty value
    
    JITStubCall stubCall(this, cti_op_get_by_val);
    stubCall.addArgument(base, regT2);
    stubCall.addArgument(property, regT2);
    stubCall.call(dst);

    emitValueProfilingSite();
}

void JIT::compileGetDirectOffset(RegisterID base, RegisterID result, RegisterID offset, RegisterID scratch)
{
    loadPtr(Address(base, JSObject::offsetOfPropertyStorage()), scratch);
    loadPtr(BaseIndex(scratch, offset, ScalePtr, 0), result);
}

void JIT::emit_op_get_by_pname(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;
    unsigned expected = currentInstruction[4].u.operand;
    unsigned iter = currentInstruction[5].u.operand;
    unsigned i = currentInstruction[6].u.operand;

    emitGetVirtualRegister(property, regT0);
    addSlowCase(branchPtr(NotEqual, regT0, addressFor(expected)));
    emitGetVirtualRegisters(base, regT0, iter, regT1);
    emitJumpSlowCaseIfNotJSCell(regT0, base);

    // Test base's structure
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    addSlowCase(branchPtr(NotEqual, regT2, Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedStructure))));
    load32(addressFor(i), regT3);
    sub32(TrustedImm32(1), regT3);
    addSlowCase(branch32(AboveOrEqual, regT3, Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_numCacheableSlots))));
    compileGetDirectOffset(regT0, regT0, regT3, regT1);

    emitPutVirtualRegister(dst, regT0);
}

void JIT::emitSlow_op_get_by_pname(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned base = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;

    linkSlowCase(iter);
    linkSlowCaseIfNotJSCell(iter, base);
    linkSlowCase(iter);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_get_by_val);
    stubCall.addArgument(base, regT2);
    stubCall.addArgument(property, regT2);
    stubCall.call(dst);
}

void JIT::emit_op_put_by_val(Instruction* currentInstruction)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;

    emitGetVirtualRegisters(base, regT0, property, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT1);
    // See comment in op_get_by_val.
    zeroExtend32ToPtr(regT1, regT1);
    emitJumpSlowCaseIfNotJSCell(regT0, base);
    addSlowCase(branchPtr(NotEqual, Address(regT0, JSCell::classInfoOffset()), TrustedImmPtr(&JSArray::s_info)));
    addSlowCase(branch32(AboveOrEqual, regT1, Address(regT0, JSArray::vectorLengthOffset())));

    loadPtr(Address(regT0, JSArray::storageOffset()), regT2);
    Jump empty = branchTestPtr(Zero, BaseIndex(regT2, regT1, ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));

    Label storeResult(this);
    emitGetVirtualRegister(value, regT3);
    storePtr(regT3, BaseIndex(regT2, regT1, ScalePtr, OBJECT_OFFSETOF(ArrayStorage, m_vector[0])));
    Jump end = jump();
    
    empty.link(this);
    add32(TrustedImm32(1), Address(regT2, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
    branch32(Below, regT1, Address(regT2, OBJECT_OFFSETOF(ArrayStorage, m_length))).linkTo(storeResult, this);

    add32(TrustedImm32(1), regT1);
    store32(regT1, Address(regT2, OBJECT_OFFSETOF(ArrayStorage, m_length)));
    sub32(TrustedImm32(1), regT1);
    jump().linkTo(storeResult, this);

    end.link(this);

    emitWriteBarrier(regT0, regT3, regT1, regT3, ShouldFilterImmediates, WriteBarrierForPropertyAccess);
}

void JIT::emitSlow_op_put_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;

    linkSlowCase(iter); // property int32 check
    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    linkSlowCase(iter); // base not array check
    linkSlowCase(iter); // in vector check

    JITStubCall stubPutByValCall(this, cti_op_put_by_val);
    stubPutByValCall.addArgument(regT0);
    stubPutByValCall.addArgument(property, regT2);
    stubPutByValCall.addArgument(value, regT2);
    stubPutByValCall.call();
}

void JIT::emit_op_put_by_index(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_put_by_index);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(TrustedImm32(currentInstruction[2].u.operand));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_put_getter(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_put_getter);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_put_setter(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_put_setter);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_del_by_id(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_del_by_id);
    stubCall.addArgument(currentInstruction[2].u.operand, regT2);
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[3].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}

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
    m_methodCallCompilationInfo.append(MethodCallCompilationInfo(m_bytecodeOffset, m_propertyAccessCompilationInfo.size()));
    MethodCallCompilationInfo& info = m_methodCallCompilationInfo.last();

    Jump notCell = emitJumpIfNotJSCell(regT0);

    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceMethodCheck);

    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, JSCell::structureOffset()), info.structureToCompare, TrustedImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    DataLabelPtr protoStructureToCompare, protoObj = moveWithPatch(TrustedImmPtr(0), regT1);
    Jump protoStructureCheck = branchPtrWithPatch(NotEqual, Address(regT1, JSCell::structureOffset()), protoStructureToCompare, TrustedImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));

    // This will be relinked to load the function without doing a load.
    DataLabelPtr putFunction = moveWithPatch(TrustedImmPtr(0), regT0);

    END_UNINTERRUPTED_SEQUENCE(sequenceMethodCheck);

    Jump match = jump();

    ASSERT_JIT_OFFSET_UNUSED(protoObj, differenceBetween(info.structureToCompare, protoObj), patchOffsetMethodCheckProtoObj);
    ASSERT_JIT_OFFSET(differenceBetween(info.structureToCompare, protoStructureToCompare), patchOffsetMethodCheckProtoStruct);
    ASSERT_JIT_OFFSET_UNUSED(putFunction, differenceBetween(info.structureToCompare, putFunction), patchOffsetMethodCheckPutFunction);

    // Link the failure cases here.
    notCell.link(this);
    structureCheck.link(this);
    protoStructureCheck.link(this);

    // Do a regular(ish) get_by_id (the slow case will be link to
    // cti_op_get_by_id_method_check instead of cti_op_get_by_id.
    compileGetByIdHotPath(baseVReg, ident);

    match.link(this);
    emitValueProfilingSite(m_bytecodeOffset + OPCODE_LENGTH(op_method_check));
    emitPutVirtualRegister(resultVReg);

    // We've already generated the following get_by_id, so make sure it's skipped over.
    m_bytecodeOffset += OPCODE_LENGTH(op_get_by_id);
}

void JIT::emitSlow_op_method_check(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    currentInstruction += OPCODE_LENGTH(op_method_check);
    unsigned resultVReg = currentInstruction[1].u.operand;
    unsigned baseVReg = currentInstruction[2].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    compileGetByIdSlowCase(resultVReg, baseVReg, ident, iter, true);
    emitValueProfilingSite(m_bytecodeOffset + OPCODE_LENGTH(op_method_check));

    // We've already generated the following get_by_id, so make sure it's skipped over.
    m_bytecodeOffset += OPCODE_LENGTH(op_get_by_id);
}

void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    unsigned resultVReg = currentInstruction[1].u.operand;
    unsigned baseVReg = currentInstruction[2].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    emitGetVirtualRegister(baseVReg, regT0);
    compileGetByIdHotPath(baseVReg, ident);
    emitValueProfilingSite();
    emitPutVirtualRegister(resultVReg);
}

void JIT::compileGetByIdHotPath(int baseVReg, Identifier*)
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be patched.
    // Additionally, for get_by_id we need patch the offset of the branch to the slow case (we patch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    BEGIN_UNINTERRUPTED_SEQUENCE(sequenceGetByIdHotPath);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo.append(PropertyStubCompilationInfo());
    m_propertyAccessCompilationInfo.last().bytecodeIndex = m_bytecodeOffset;
    m_propertyAccessCompilationInfo.last().hotPathBegin = hotPathBegin;

    DataLabelPtr structureToCompare;
    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, JSCell::structureOffset()), structureToCompare, TrustedImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    addSlowCase(structureCheck);
    ASSERT_JIT_OFFSET(differenceBetween(hotPathBegin, structureToCompare), patchOffsetGetByIdStructure);
    ASSERT_JIT_OFFSET(differenceBetween(hotPathBegin, structureCheck), patchOffsetGetByIdBranchToSlowCase)

    loadPtr(Address(regT0, JSObject::offsetOfPropertyStorage()), regT0);
    DataLabelCompact displacementLabel = loadPtrWithCompactAddressOffsetPatch(Address(regT0, patchGetByIdDefaultOffset), regT0);
    ASSERT_JIT_OFFSET_UNUSED(displacementLabel, differenceBetween(hotPathBegin, displacementLabel), patchOffsetGetByIdPropertyMapOffset);

    Label putResult(this);

    END_UNINTERRUPTED_SEQUENCE(sequenceGetByIdHotPath);

    ASSERT_JIT_OFFSET(differenceBetween(hotPathBegin, putResult), patchOffsetGetByIdPutResult);
}

void JIT::emitSlow_op_get_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned resultVReg = currentInstruction[1].u.operand;
    unsigned baseVReg = currentInstruction[2].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    compileGetByIdSlowCase(resultVReg, baseVReg, ident, iter, false);
    emitValueProfilingSite();
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
    stubCall.addArgument(TrustedImmPtr(ident));
    Call call = stubCall.call(resultVReg);

    END_UNINTERRUPTED_SEQUENCE(sequenceGetByIdSlowCase);

    ASSERT_JIT_OFFSET(differenceBetween(coldPathBegin, call), patchOffsetGetByIdSlowCaseCall);

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex++].callReturnLocation = call;
}

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    unsigned baseVReg = currentInstruction[1].u.operand;
    unsigned valueVReg = currentInstruction[3].u.operand;

    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    emitGetVirtualRegisters(baseVReg, regT0, valueVReg, regT1);

    // Jump to a slow case if either the base object is an immediate, or if the Structure does not match.
    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    BEGIN_UNINTERRUPTED_SEQUENCE(sequencePutById);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo.append(PropertyStubCompilationInfo());
    m_propertyAccessCompilationInfo.last().bytecodeIndex = m_bytecodeOffset;
    m_propertyAccessCompilationInfo.last().hotPathBegin = hotPathBegin;

    // It is important that the following instruction plants a 32bit immediate, in order that it can be patched over.
    DataLabelPtr structureToCompare;
    addSlowCase(branchPtrWithPatch(NotEqual, Address(regT0, JSCell::structureOffset()), structureToCompare, TrustedImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure))));
    ASSERT_JIT_OFFSET(differenceBetween(hotPathBegin, structureToCompare), patchOffsetPutByIdStructure);

    loadPtr(Address(regT0, JSObject::offsetOfPropertyStorage()), regT2);
    DataLabel32 displacementLabel = storePtrWithAddressOffsetPatch(regT1, Address(regT2, patchPutByIdDefaultOffset));

    END_UNINTERRUPTED_SEQUENCE(sequencePutById);

    emitWriteBarrier(regT0, regT1, regT2, regT3, ShouldFilterImmediates, WriteBarrierForPropertyAccess);

    ASSERT_JIT_OFFSET_UNUSED(displacementLabel, differenceBetween(hotPathBegin, displacementLabel), patchOffsetPutByIdPropertyMapOffset);
}

void JIT::emitSlow_op_put_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned baseVReg = currentInstruction[1].u.operand;
    Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));
    unsigned direct = currentInstruction[8].u.operand;

    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

    JITStubCall stubCall(this, direct ? cti_op_put_by_id_direct : cti_op_put_by_id);
    stubCall.addArgument(regT0);
    stubCall.addArgument(TrustedImmPtr(ident));
    stubCall.addArgument(regT1);
    Call call = stubCall.call();

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[m_propertyAccessInstructionIndex++].callReturnLocation = call;
}

// Compile a store into an object's property storage.  May overwrite the
// value in objectReg.
void JIT::compilePutDirectOffset(RegisterID base, RegisterID value, size_t cachedOffset)
{
    int offset = cachedOffset * sizeof(JSValue);
    loadPtr(Address(base, JSObject::offsetOfPropertyStorage()), base);
    storePtr(value, Address(base, offset));
}

// Compile a load from an object's property storage.  May overwrite base.
void JIT::compileGetDirectOffset(RegisterID base, RegisterID result, size_t cachedOffset)
{
    int offset = cachedOffset * sizeof(JSValue);
    loadPtr(Address(base, JSObject::offsetOfPropertyStorage()), result);
    loadPtr(Address(result, offset), result);
}

void JIT::compileGetDirectOffset(JSObject* base, RegisterID result, size_t cachedOffset)
{
    loadPtr(base->addressOfPropertyStorage(), result);
    loadPtr(Address(result, cachedOffset * sizeof(WriteBarrier<Unknown>)), result);
}

void JIT::privateCompilePutByIdTransition(StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, ReturnAddressPtr returnAddress, bool direct)
{
    JumpList failureCases;
    // Check eax is an object of the right Structure.
    failureCases.append(emitJumpIfNotJSCell(regT0));
    failureCases.append(branchPtr(NotEqual, Address(regT0, JSCell::structureOffset()), TrustedImmPtr(oldStructure)));
    
    testPrototype(oldStructure->storedPrototype(), failureCases);
    
    ASSERT(oldStructure->storedPrototype().isNull() || oldStructure->storedPrototype().asCell()->structure() == chain->head()->get());

    // ecx = baseObject->m_structure
    if (!direct) {
        for (WriteBarrier<Structure>* it = chain->head(); *it; ++it) {
            ASSERT((*it)->storedPrototype().isNull() || (*it)->storedPrototype().asCell()->structure() == it[1].get());
            testPrototype((*it)->storedPrototype(), failureCases);
        }
    }

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
        stubCall.addArgument(TrustedImm32(oldStructure->propertyStorageCapacity()));
        stubCall.addArgument(TrustedImm32(newStructure->propertyStorageCapacity()));
        stubCall.call(regT0);
        emitGetJITStubArg(2, regT1);

        restoreReturnAddressBeforeReturn(regT3);
    }

    // Planting the new structure triggers the write barrier so we need
    // an unconditional barrier here.
    emitWriteBarrier(regT0, regT1, regT2, regT3, UnconditionalWriteBarrier, WriteBarrierForPropertyAccess);

    ASSERT(newStructure->classInfo() == oldStructure->classInfo());
    storePtr(TrustedImmPtr(newStructure), Address(regT0, JSCell::structureOffset()));
    compilePutDirectOffset(regT0, regT1, cachedOffset);

    ret();
    
    ASSERT(!failureCases.empty());
    failureCases.link(this);
    restoreArgumentReferenceForTrampoline();
    Call failureCall = tailRecursiveCall();

    LinkBuffer patchBuffer(*m_globalData, this, m_codeBlock);

    patchBuffer.link(failureCall, FunctionPtr(direct ? cti_op_put_by_id_direct_fail : cti_op_put_by_id_fail));

    if (willNeedStorageRealloc) {
        ASSERT(m_calls.size() == 1);
        patchBuffer.link(m_calls[0].from, FunctionPtr(cti_op_put_by_id_transition_realloc));
    }
    
    stubInfo->stubRoutine = patchBuffer.finalizeCode();
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relinkCallerToTrampoline(returnAddress, CodeLocationLabel(stubInfo->stubRoutine.code()));
}

void JIT::patchGetByIdSelf(CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);

    // We don't want to patch more than once - in future go to cti_op_get_by_id_generic.
    // Should probably go to cti_op_get_by_id_fail, but that doesn't do anything interesting right now.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_self_fail));

    int offset = sizeof(JSValue) * cachedOffset;

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetGetByIdStructure), structure);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelCompactAtOffset(patchOffsetGetByIdPropertyMapOffset), offset);
}

void JIT::patchPutByIdReplace(CodeBlock* codeBlock, StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ReturnAddressPtr returnAddress, bool direct)
{
    RepatchBuffer repatchBuffer(codeBlock);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(direct ? cti_op_put_by_id_direct_generic : cti_op_put_by_id_generic));

    int offset = sizeof(JSValue) * cachedOffset;

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetPutByIdStructure), structure);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset), offset);
}

void JIT::privateCompilePatchGetArrayLength(ReturnAddressPtr returnAddress)
{
    StructureStubInfo* stubInfo = &m_codeBlock->getStubInfo(returnAddress);

    // Check eax is an array
    Jump failureCases1 = branchPtr(NotEqual, Address(regT0, JSCell::classInfoOffset()), TrustedImmPtr(&JSArray::s_info));

    // Checks out okay! - get the length from the storage
    loadPtr(Address(regT0, JSArray::storageOffset()), regT3);
    load32(Address(regT3, OBJECT_OFFSETOF(ArrayStorage, m_length)), regT2);
    Jump failureCases2 = branch32(LessThan, regT2, TrustedImm32(0));

    emitFastArithIntToImmNoCheck(regT2, regT0);
    Jump success = jump();

    LinkBuffer patchBuffer(*m_globalData, this, m_codeBlock);

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    stubInfo->stubRoutine = patchBuffer.finalizeCode();

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, CodeLocationLabel(stubInfo->stubRoutine.code()));

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_array_fail));
}

void JIT::privateCompileGetByIdProto(StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame)
{
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));

    // Check eax is an object of the right Structure.
    Jump failureCases1 = checkStructure(regT0, structure);

    // Check the prototype object's Structure had not changed.
    move(TrustedImmPtr(protoObject), regT3);
    Jump failureCases2 = branchPtr(NotEqual, Address(regT3, JSCell::structureOffset()), TrustedImmPtr(prototypeStructure));

    bool needsStubLink = false;
    
    // Checks out okay!
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(protoObject, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(TrustedImmPtr(protoObject));
        stubCall.addArgument(TrustedImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(TrustedImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else
        compileGetDirectOffset(protoObject, regT0, cachedOffset);
    Jump success = jump();
    LinkBuffer patchBuffer(*m_globalData, this, m_codeBlock);

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }
    // Track the stub we have created so that it will be deleted later.
    stubInfo->stubRoutine = patchBuffer.finalizeCode();

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, CodeLocationLabel(stubInfo->stubRoutine.code()));

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_proto_list));
}

void JIT::privateCompileGetByIdSelfList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset)
{
    Jump failureCase = checkStructure(regT0, structure);
    bool needsStubLink = false;
    bool isDirect = false;
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(regT0, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(regT0);
        stubCall.addArgument(TrustedImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(TrustedImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else {
        isDirect = true;
        compileGetDirectOffset(regT0, regT0, cachedOffset);
    }
    Jump success = jump();

    LinkBuffer patchBuffer(*m_globalData, this, m_codeBlock);

    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = CodeLocationLabel(polymorphicStructures->list[currentIndex - 1].stubRoutine.code());
    if (!lastProtoBegin)
        lastProtoBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);

    patchBuffer.link(failureCase, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    MacroAssemblerCodeRef stubCode = patchBuffer.finalizeCode();

    polymorphicStructures->list[currentIndex].set(*m_globalData, m_codeBlock->ownerExecutable(), stubCode, structure, isDirect);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, CodeLocationLabel(stubCode.code()));
}

void JIT::privateCompileGetByIdProtoList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, Structure* prototypeStructure, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset, CallFrame* callFrame)
{
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));

    // Check eax is an object of the right Structure.
    Jump failureCases1 = checkStructure(regT0, structure);

    // Check the prototype object's Structure had not changed.
    move(TrustedImmPtr(protoObject), regT3);
    Jump failureCases2 = branchPtr(NotEqual, Address(regT3, JSCell::structureOffset()), TrustedImmPtr(prototypeStructure));

    // Checks out okay!
    bool needsStubLink = false;
    bool isDirect = false;
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(protoObject, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(TrustedImmPtr(protoObject));
        stubCall.addArgument(TrustedImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(TrustedImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else {
        isDirect = true;
        compileGetDirectOffset(protoObject, regT0, cachedOffset);
    }

    Jump success = jump();

    LinkBuffer patchBuffer(*m_globalData, this, m_codeBlock);

    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = CodeLocationLabel(prototypeStructures->list[currentIndex - 1].stubRoutine.code());
    patchBuffer.link(failureCases1, lastProtoBegin);
    patchBuffer.link(failureCases2, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    MacroAssemblerCodeRef stubCode = patchBuffer.finalizeCode();
    prototypeStructures->list[currentIndex].set(*m_globalData, m_codeBlock->ownerExecutable(), stubCode, structure, prototypeStructure, isDirect);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, CodeLocationLabel(stubCode.code()));
}

void JIT::privateCompileGetByIdChainList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, StructureChain* chain, size_t count, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset, CallFrame* callFrame)
{
    ASSERT(count);
    JumpList bucketsOfFail;

    // Check eax is an object of the right Structure.
    Jump baseObjectCheck = checkStructure(regT0, structure);
    bucketsOfFail.append(baseObjectCheck);

    Structure* currStructure = structure;
    WriteBarrier<Structure>* it = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i, ++it) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = it->get();
        testPrototype(protoObject, bucketsOfFail);
    }
    ASSERT(protoObject);
    
    bool needsStubLink = false;
    bool isDirect = false;
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(protoObject, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(TrustedImmPtr(protoObject));
        stubCall.addArgument(TrustedImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(TrustedImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else {
        isDirect = true;
        compileGetDirectOffset(protoObject, regT0, cachedOffset);
    }
    Jump success = jump();

    LinkBuffer patchBuffer(*m_globalData, this, m_codeBlock);
    
    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = CodeLocationLabel(prototypeStructures->list[currentIndex - 1].stubRoutine.code());

    patchBuffer.link(bucketsOfFail, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeRef stubRoutine = patchBuffer.finalizeCode();

    // Track the stub we have created so that it will be deleted later.
    prototypeStructures->list[currentIndex].set(callFrame->globalData(), m_codeBlock->ownerExecutable(), stubRoutine, structure, chain, isDirect);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, CodeLocationLabel(stubRoutine.code()));
}

void JIT::privateCompileGetByIdChain(StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, const Identifier& ident, const PropertySlot& slot, size_t cachedOffset, ReturnAddressPtr returnAddress, CallFrame* callFrame)
{
    ASSERT(count);

    JumpList bucketsOfFail;

    // Check eax is an object of the right Structure.
    bucketsOfFail.append(checkStructure(regT0, structure));

    Structure* currStructure = structure;
    WriteBarrier<Structure>* it = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i, ++it) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = it->get();
        testPrototype(protoObject, bucketsOfFail);
    }
    ASSERT(protoObject);

    bool needsStubLink = false;
    if (slot.cachedPropertyType() == PropertySlot::Getter) {
        needsStubLink = true;
        compileGetDirectOffset(protoObject, regT1, cachedOffset);
        JITStubCall stubCall(this, cti_op_get_by_id_getter_stub);
        stubCall.addArgument(regT1);
        stubCall.addArgument(regT0);
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else if (slot.cachedPropertyType() == PropertySlot::Custom) {
        needsStubLink = true;
        JITStubCall stubCall(this, cti_op_get_by_id_custom_stub);
        stubCall.addArgument(TrustedImmPtr(protoObject));
        stubCall.addArgument(TrustedImmPtr(FunctionPtr(slot.customGetter()).executableAddress()));
        stubCall.addArgument(TrustedImmPtr(const_cast<Identifier*>(&ident)));
        stubCall.addArgument(TrustedImmPtr(stubInfo->callReturnLocation.executableAddress()));
        stubCall.call();
    } else
        compileGetDirectOffset(protoObject, regT0, cachedOffset);
    Jump success = jump();

    LinkBuffer patchBuffer(*m_globalData, this, m_codeBlock);

    if (needsStubLink) {
        for (Vector<CallRecord>::iterator iter = m_calls.begin(); iter != m_calls.end(); ++iter) {
            if (iter->to)
                patchBuffer.link(iter->from, FunctionPtr(iter->to));
        }
    }

    // Use the patch information to link the failure cases back to the original slow case routine.
    patchBuffer.link(bucketsOfFail, stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall));

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeRef stubRoutine = patchBuffer.finalizeCode();
    stubInfo->stubRoutine = stubRoutine;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(jumpLocation, CodeLocationLabel(stubRoutine.code()));

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_proto_list));
}

void JIT::emit_op_get_scoped_var(Instruction* currentInstruction)
{
    int skip = currentInstruction[3].u.operand;

    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT0);
    bool checkTopLevel = m_codeBlock->codeType() == FunctionCode && m_codeBlock->needsFullScopeChain();
    ASSERT(skip || !checkTopLevel);
    if (checkTopLevel && skip--) {
        Jump activationNotCreated;
        if (checkTopLevel)
            activationNotCreated = branchTestPtr(Zero, addressFor(m_codeBlock->activationRegister()));
        loadPtr(Address(regT0, OBJECT_OFFSETOF(ScopeChainNode, next)), regT0);
        activationNotCreated.link(this);
    }
    while (skip--)
        loadPtr(Address(regT0, OBJECT_OFFSETOF(ScopeChainNode, next)), regT0);

    loadPtr(Address(regT0, OBJECT_OFFSETOF(ScopeChainNode, object)), regT0);
    loadPtr(Address(regT0, JSVariableObject::offsetOfRegisters()), regT0);
    loadPtr(Address(regT0, currentInstruction[2].u.operand * sizeof(Register)), regT0);
    emitValueProfilingSite();
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_put_scoped_var(Instruction* currentInstruction)
{
    int skip = currentInstruction[2].u.operand;

    emitGetVirtualRegister(currentInstruction[3].u.operand, regT0);

    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1);
    bool checkTopLevel = m_codeBlock->codeType() == FunctionCode && m_codeBlock->needsFullScopeChain();
    ASSERT(skip || !checkTopLevel);
    if (checkTopLevel && skip--) {
        Jump activationNotCreated;
        if (checkTopLevel)
            activationNotCreated = branchTestPtr(Zero, addressFor(m_codeBlock->activationRegister()));
        loadPtr(Address(regT1, OBJECT_OFFSETOF(ScopeChainNode, next)), regT1);
        activationNotCreated.link(this);
    }
    while (skip--)
        loadPtr(Address(regT1, OBJECT_OFFSETOF(ScopeChainNode, next)), regT1);
    loadPtr(Address(regT1, OBJECT_OFFSETOF(ScopeChainNode, object)), regT1);

    emitWriteBarrier(regT1, regT0, regT2, regT3, ShouldFilterImmediates, WriteBarrierForVariableAccess);

    loadPtr(Address(regT1, JSVariableObject::offsetOfRegisters()), regT1);
    storePtr(regT0, Address(regT1, currentInstruction[1].u.operand * sizeof(Register)));
}

void JIT::emit_op_get_global_var(Instruction* currentInstruction)
{
    JSVariableObject* globalObject = m_codeBlock->globalObject();
    loadPtr(&globalObject->m_registers, regT0);
    loadPtr(Address(regT0, currentInstruction[2].u.operand * sizeof(Register)), regT0);
    emitValueProfilingSite();
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_put_global_var(Instruction* currentInstruction)
{
    JSGlobalObject* globalObject = m_codeBlock->globalObject();

    emitGetVirtualRegister(currentInstruction[2].u.operand, regT0);

    move(TrustedImmPtr(globalObject), regT1);
    loadPtr(Address(regT1, JSVariableObject::offsetOfRegisters()), regT1);
    storePtr(regT0, Address(regT1, currentInstruction[1].u.operand * sizeof(Register)));
    emitWriteBarrier(globalObject, regT0, regT2, ShouldFilterImmediates, WriteBarrierForVariableAccess);
}

void JIT::resetPatchGetById(RepatchBuffer& repatchBuffer, StructureStubInfo* stubInfo)
{
    repatchBuffer.relink(stubInfo->callReturnLocation, cti_op_get_by_id);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetGetByIdStructure), reinterpret_cast<void*>(-1));
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelCompactAtOffset(patchOffsetGetByIdPropertyMapOffset), 0);
    repatchBuffer.relink(stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase), stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall));
}

void JIT::resetPatchPutById(RepatchBuffer& repatchBuffer, StructureStubInfo* stubInfo)
{
    if (isDirectPutById(stubInfo))
        repatchBuffer.relink(stubInfo->callReturnLocation, cti_op_put_by_id_direct);
    else
        repatchBuffer.relink(stubInfo->callReturnLocation, cti_op_put_by_id);
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetPutByIdStructure), reinterpret_cast<void*>(-1));
    repatchBuffer.repatch(stubInfo->hotPathBegin.dataLabelCompactAtOffset(patchOffsetPutByIdPropertyMapOffset), 0);
}

#endif // USE(JSVALUE64)

void JIT::emitWriteBarrier(RegisterID owner, RegisterID value, RegisterID scratch, RegisterID scratch2, WriteBarrierMode mode, WriteBarrierUseKind useKind)
{
    UNUSED_PARAM(owner);
    UNUSED_PARAM(scratch);
    UNUSED_PARAM(scratch2);
    UNUSED_PARAM(useKind);
    UNUSED_PARAM(value);
    UNUSED_PARAM(mode);
    ASSERT(owner != scratch);
    ASSERT(owner != scratch2);
    
#if ENABLE(WRITE_BARRIER_PROFILING)
    emitCount(WriteBarrierCounters::jitCounterFor(useKind));
#endif
    
#if ENABLE(GGC)
    Jump filterCells;
    if (mode == ShouldFilterImmediates)
        filterCells = emitJumpIfNotJSCell(value);
    move(owner, scratch);
    andPtr(TrustedImm32(static_cast<int32_t>(MarkedBlock::blockMask)), scratch);
    move(owner, scratch2);
    // consume additional 8 bits as we're using an approximate filter
    rshift32(TrustedImm32(MarkedBlock::atomShift + 8), scratch2);
    andPtr(TrustedImm32(MarkedBlock::atomMask >> 8), scratch2);
    Jump filter = branchTest8(Zero, BaseIndex(scratch, scratch2, TimesOne, MarkedBlock::offsetOfMarks()));
    move(owner, scratch2);
    rshift32(TrustedImm32(MarkedBlock::cardShift), scratch2);
    andPtr(TrustedImm32(MarkedBlock::cardMask), scratch2);
    store8(TrustedImm32(1), BaseIndex(scratch, scratch2, TimesOne, MarkedBlock::offsetOfCards()));
    filter.link(this);
    if (mode == ShouldFilterImmediates)
        filterCells.link(this);
#endif
}

void JIT::emitWriteBarrier(JSCell* owner, RegisterID value, RegisterID scratch, WriteBarrierMode mode, WriteBarrierUseKind useKind)
{
    UNUSED_PARAM(owner);
    UNUSED_PARAM(scratch);
    UNUSED_PARAM(useKind);
    UNUSED_PARAM(value);
    UNUSED_PARAM(mode);
    
#if ENABLE(WRITE_BARRIER_PROFILING)
    emitCount(WriteBarrierCounters::jitCounterFor(useKind));
#endif
    
#if ENABLE(GGC)
    Jump filterCells;
    if (mode == ShouldFilterImmediates)
        filterCells = emitJumpIfNotJSCell(value);
    uint8_t* cardAddress = Heap::addressOfCardFor(owner);
    move(TrustedImmPtr(cardAddress), scratch);
    store8(TrustedImm32(1), Address(scratch));
    if (mode == ShouldFilterImmediates)
        filterCells.link(this);
#endif
}

void JIT::testPrototype(JSValue prototype, JumpList& failureCases)
{
    if (prototype.isNull())
        return;

    ASSERT(prototype.isCell());
    move(TrustedImmPtr(prototype.asCell()), regT3);
    failureCases.append(branchPtr(NotEqual, Address(regT3, JSCell::structureOffset()), TrustedImmPtr(prototype.asCell()->structure())));
}

void JIT::patchMethodCallProto(JSGlobalData& globalData, CodeBlock* codeBlock, MethodCallLinkInfo& methodCallLinkInfo, JSObject* callee, Structure* structure, JSObject* proto, ReturnAddressPtr returnAddress)
{
    RepatchBuffer repatchBuffer(codeBlock);
    
    CodeLocationDataLabelPtr structureLocation = methodCallLinkInfo.cachedStructure.location();
    methodCallLinkInfo.cachedStructure.set(globalData, structureLocation, codeBlock->ownerExecutable(), structure);
    
    Structure* prototypeStructure = proto->structure();
    methodCallLinkInfo.cachedPrototypeStructure.set(globalData, structureLocation.dataLabelPtrAtOffset(patchOffsetMethodCheckProtoStruct), codeBlock->ownerExecutable(), prototypeStructure);
    methodCallLinkInfo.cachedPrototype.set(globalData, structureLocation.dataLabelPtrAtOffset(patchOffsetMethodCheckProtoObj), codeBlock->ownerExecutable(), proto);
    methodCallLinkInfo.cachedFunction.set(globalData, structureLocation.dataLabelPtrAtOffset(patchOffsetMethodCheckPutFunction), codeBlock->ownerExecutable(), callee);
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(cti_op_get_by_id_method_check_update));
}

bool JIT::isDirectPutById(StructureStubInfo* stubInfo)
{
    switch (stubInfo->accessType) {
    case access_put_by_id_transition_normal:
        return false;
    case access_put_by_id_transition_direct:
        return true;
    case access_put_by_id_replace:
    case access_put_by_id_generic: {
        void* oldCall = MacroAssembler::readCallTarget(stubInfo->callReturnLocation).executableAddress();
        if (oldCall == bitwise_cast<void*>(cti_op_put_by_id_direct)
            || oldCall == bitwise_cast<void*>(cti_op_put_by_id_direct_generic)
            || oldCall == bitwise_cast<void*>(cti_op_put_by_id_direct_fail))
            return true;
        ASSERT(oldCall == bitwise_cast<void*>(cti_op_put_by_id)
               || oldCall == bitwise_cast<void*>(cti_op_put_by_id_generic)
               || oldCall == bitwise_cast<void*>(cti_op_put_by_id_fail));
        return false;
    }
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

} // namespace JSC

#endif // ENABLE(JIT)
