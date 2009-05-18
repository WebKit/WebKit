/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "ResultType.h"
#include "SamplingTool.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

using namespace std;

namespace JSC {

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    compilePutByIdHotPath(currentInstruction[1].u.operand, &(m_codeBlock->identifier(currentInstruction[2].u.operand)), currentInstruction[3].u.operand, m_propertyAccessInstructionIndex++);
}

void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    compileGetByIdHotPath(currentInstruction[1].u.operand, currentInstruction[2].u.operand, &(m_codeBlock->identifier(currentInstruction[3].u.operand)), m_propertyAccessInstructionIndex++);
}

void JIT::emit_op_get_by_val(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT1);
#if USE(ALTERNATE_JSIMMEDIATE)
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
    loadPtr(Address(regT0, FIELD_OFFSET(JSArray, m_storage)), regT2);
    addSlowCase(branch32(AboveOrEqual, regT1, Address(regT0, FIELD_OFFSET(JSArray, m_fastAccessCutoff))));

    // Get the value from the vector
    loadPtr(BaseIndex(regT2, regT1, ScalePtr, FIELD_OFFSET(ArrayStorage, m_vector[0])), regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_put_by_val(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[1].u.operand, regT0, currentInstruction[2].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateInteger(regT1);
#if USE(ALTERNATE_JSIMMEDIATE)
    // See comment in op_get_by_val.
    zeroExtend32ToPtr(regT1, regT1);
#else
    emitFastArithImmToInt(regT1);
#endif
    emitJumpSlowCaseIfNotJSCell(regT0);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr)));

    // This is an array; get the m_storage pointer into ecx, then check if the index is below the fast cutoff
    loadPtr(Address(regT0, FIELD_OFFSET(JSArray, m_storage)), regT2);
    Jump inFastVector = branch32(Below, regT1, Address(regT0, FIELD_OFFSET(JSArray, m_fastAccessCutoff)));
    // No; oh well, check if the access if within the vector - if so, we may still be okay.
    addSlowCase(branch32(AboveOrEqual, regT1, Address(regT2, FIELD_OFFSET(ArrayStorage, m_vectorLength))));

    // This is a write to the slow part of the vector; first, we have to check if this would be the first write to this location.
    // FIXME: should be able to handle initial write to array; increment the the number of items in the array, and potentially update fast access cutoff. 
    addSlowCase(branchTestPtr(Zero, BaseIndex(regT2, regT1, ScalePtr, FIELD_OFFSET(ArrayStorage, m_vector[0]))));

    // All good - put the value into the array.
    inFastVector.link(this);
    emitGetVirtualRegister(currentInstruction[3].u.operand, regT0);
    storePtr(regT0, BaseIndex(regT2, regT1, ScalePtr, FIELD_OFFSET(ArrayStorage, m_vector[0])));
}

void JIT::emit_op_put_by_index(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, JITStubs::cti_op_put_by_index);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(Imm32(currentInstruction[2].u.operand));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_put_getter(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, JITStubs::cti_op_put_getter);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_put_setter(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, JITStubs::cti_op_put_setter);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_del_by_id(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, JITStubs::cti_op_del_by_id);
    stubCall.addArgument(currentInstruction[2].u.operand, regT2);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[3].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}


#if !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

/* ------------------------------ BEGIN: !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

void JIT::compileGetByIdHotPath(int resultVReg, int baseVReg, Identifier* ident, unsigned)
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be patched.
    // Additionally, for get_by_id we need patch the offset of the branch to the slow case (we patch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.

    emitGetVirtualRegister(baseVReg, regT0);

    JITStubCall stubCall(this, JITStubs::cti_op_get_by_id_generic);
    stubCall.addArgument(regT0);
    stubCall.addArgument(ImmPtr(ident));
    stubCall.call(resultVReg);
}

void JIT::compileGetByIdSlowCase(int, int, Identifier*, Vector<SlowCaseEntry>::iterator&, unsigned)
{
    ASSERT_NOT_REACHED();
}

void JIT::compilePutByIdHotPath(int baseVReg, Identifier* ident, int valueVReg, unsigned)
{
    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    emitGetVirtualRegisters(baseVReg, regT0, valueVReg, regT1);

    JITStubCall stubCall(this, JITStubs::cti_op_put_by_id_generic);
    stubCall.addArgument(regT0);
    stubCall.addArgument(ImmPtr(ident));
    stubCall.addArgument(regT1);
    stubCall.call();
}

void JIT::compilePutByIdSlowCase(int, Identifier*, int, Vector<SlowCaseEntry>::iterator&, unsigned)
{
    ASSERT_NOT_REACHED();
}

#else // !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

/* ------------------------------ BEGIN: ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

void JIT::compileGetByIdHotPath(int resultVReg, int baseVReg, Identifier*, unsigned propertyAccessInstructionIndex)
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be patched.
    // Additionally, for get_by_id we need patch the offset of the branch to the slow case (we patch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.

    emitGetVirtualRegister(baseVReg, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    DataLabelPtr structureToCompare;
    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, FIELD_OFFSET(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    addSlowCase(structureCheck);
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetGetByIdStructure);
    ASSERT(differenceBetween(hotPathBegin, structureCheck) == patchOffsetGetByIdBranchToSlowCase);

    Label externalLoad(this);
    loadPtr(Address(regT0, FIELD_OFFSET(JSObject, m_externalStorage)), regT0);
    Label externalLoadComplete(this);
    ASSERT(differenceBetween(hotPathBegin, externalLoad) == patchOffsetGetByIdExternalLoad);
    ASSERT(differenceBetween(externalLoad, externalLoadComplete) == patchLengthGetByIdExternalLoad);

    DataLabel32 displacementLabel = loadPtrWithAddressOffsetPatch(Address(regT0, patchGetByIdDefaultOffset), regT0);
    ASSERT(differenceBetween(hotPathBegin, displacementLabel) == patchOffsetGetByIdPropertyMapOffset);

    Label putResult(this);
    ASSERT(differenceBetween(hotPathBegin, putResult) == patchOffsetGetByIdPutResult);
    emitPutVirtualRegister(resultVReg);
}


void JIT::compileGetByIdSlowCase(int resultVReg, int baseVReg, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex)
{
    // As for the hot path of get_by_id, above, we ensure that we can use an architecture specific offset
    // so that we only need track one pointer into the slow case code - we track a pointer to the location
    // of the call (which we can use to look up the patch information), but should a array-length or
    // prototype access trampoline fail we want to bail out back to here.  To do so we can subtract back
    // the distance from the call to the head of the slow case.

    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

#ifndef NDEBUG
    Label coldPathBegin(this);
#endif
    JITStubCall stubCall(this, JITStubs::cti_op_get_by_id);
    stubCall.addArgument(regT0);
    stubCall.addArgument(ImmPtr(ident));
    Call call = stubCall.call(resultVReg);

    ASSERT(differenceBetween(coldPathBegin, call) == patchOffsetGetByIdSlowCaseCall);

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].callReturnLocation = call;
}

void JIT::compilePutByIdHotPath(int baseVReg, Identifier*, int valueVReg, unsigned propertyAccessInstructionIndex)
{
    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    emitGetVirtualRegisters(baseVReg, regT0, valueVReg, regT1);

    // Jump to a slow case if either the base object is an immediate, or if the Structure does not match.
    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    // It is important that the following instruction plants a 32bit immediate, in order that it can be patched over.
    DataLabelPtr structureToCompare;
    addSlowCase(branchPtrWithPatch(NotEqual, Address(regT0, FIELD_OFFSET(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure))));
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetPutByIdStructure);

    // Plant a load from a bogus ofset in the object's property map; we will patch this later, if it is to be used.
    Label externalLoad(this);
    loadPtr(Address(regT0, FIELD_OFFSET(JSObject, m_externalStorage)), regT0);
    Label externalLoadComplete(this);
    ASSERT(differenceBetween(hotPathBegin, externalLoad) == patchOffsetPutByIdExternalLoad);
    ASSERT(differenceBetween(externalLoad, externalLoadComplete) == patchLengthPutByIdExternalLoad);

    DataLabel32 displacementLabel = storePtrWithAddressOffsetPatch(regT1, Address(regT0, patchGetByIdDefaultOffset));
    ASSERT(differenceBetween(hotPathBegin, displacementLabel) == patchOffsetPutByIdPropertyMapOffset);
}

void JIT::compilePutByIdSlowCase(int baseVReg, Identifier* ident, int, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex)
{
    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

    JITStubCall stubCall(this, JITStubs::cti_op_put_by_id);
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
        offset += FIELD_OFFSET(JSObject, m_inlineStorage);
    else
        loadPtr(Address(base, FIELD_OFFSET(JSObject, m_externalStorage)), base);
    storePtr(value, Address(base, offset));
}

// Compile a load from an object's property storage.  May overwrite base.
void JIT::compileGetDirectOffset(RegisterID base, RegisterID result, Structure* structure, size_t cachedOffset)
{
    int offset = cachedOffset * sizeof(JSValue);
    if (structure->isUsingInlineStorage())
        offset += FIELD_OFFSET(JSObject, m_inlineStorage);
    else
        loadPtr(Address(base, FIELD_OFFSET(JSObject, m_externalStorage)), base);
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

static JSObject* resizePropertyStorage(JSObject* baseObject, int32_t oldSize, int32_t newSize)
{
    baseObject->allocatePropertyStorage(oldSize, newSize);
    return baseObject;
}

static inline bool transitionWillNeedStorageRealloc(Structure* oldStructure, Structure* newStructure)
{
    return oldStructure->propertyStorageCapacity() != newStructure->propertyStorageCapacity();
}

void JIT::privateCompilePutByIdTransition(StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, ProcessorReturnAddress returnAddress)
{
    JumpList failureCases;
    // Check eax is an object of the right Structure.
    failureCases.append(emitJumpIfNotJSCell(regT0));
    failureCases.append(branchPtr(NotEqual, Address(regT0, FIELD_OFFSET(JSCell, m_structure)), ImmPtr(oldStructure)));
    JumpList successCases;

    //  ecx = baseObject
    loadPtr(Address(regT0, FIELD_OFFSET(JSCell, m_structure)), regT2);
    // proto(ecx) = baseObject->structure()->prototype()
    failureCases.append(branch32(NotEqual, Address(regT2, FIELD_OFFSET(Structure, m_typeInfo) + FIELD_OFFSET(TypeInfo, m_type)), Imm32(ObjectType)));

    loadPtr(Address(regT2, FIELD_OFFSET(Structure, m_prototype)), regT2);
    
    // ecx = baseObject->m_structure
    for (RefPtr<Structure>* it = chain->head(); *it; ++it) {
        // null check the prototype
        successCases.append(branchPtr(Equal, regT2, ImmPtr(JSValue::encode(jsNull()))));

        // Check the structure id
        failureCases.append(branchPtr(NotEqual, Address(regT2, FIELD_OFFSET(JSCell, m_structure)), ImmPtr(it->get())));
        
        loadPtr(Address(regT2, FIELD_OFFSET(JSCell, m_structure)), regT2);
        failureCases.append(branch32(NotEqual, Address(regT2, FIELD_OFFSET(Structure, m_typeInfo) + FIELD_OFFSET(TypeInfo, m_type)), Imm32(ObjectType)));
        loadPtr(Address(regT2, FIELD_OFFSET(Structure, m_prototype)), regT2);
    }

    successCases.link(this);

    Call callTarget;

    // emit a call only if storage realloc is needed
    bool willNeedStorageRealloc = transitionWillNeedStorageRealloc(oldStructure, newStructure);
    if (willNeedStorageRealloc) {
        pop(X86::ebx);
#if PLATFORM(X86_64)
        // Setup arguments in edi, esi, edx.  Since baseObject is in regT0,
        // regT0 had better not be any of these registers.
        ASSERT(regT0 != X86::edx);
        ASSERT(regT0 != X86::esi);
        ASSERT(regT0 != X86::edi);
        move(Imm32(newStructure->propertyStorageCapacity()), X86::edx);
        move(Imm32(oldStructure->propertyStorageCapacity()), X86::esi);
        move(regT0, X86::edi);
        callTarget = call();
#else
        push(Imm32(newStructure->propertyStorageCapacity()));
        push(Imm32(oldStructure->propertyStorageCapacity()));
        push(regT0);
        callTarget = call();
        addPtr(Imm32(3 * sizeof(void*)), X86::esp);
#endif
        emitGetJITStubArg(3, regT1);
        push(X86::ebx);
    }

    // Assumes m_refCount can be decremented easily, refcount decrement is safe as 
    // codeblock should ensure oldStructure->m_refCount > 0
    sub32(Imm32(1), AbsoluteAddress(oldStructure->addressOfCount()));
    add32(Imm32(1), AbsoluteAddress(newStructure->addressOfCount()));
    storePtr(ImmPtr(newStructure), Address(regT0, FIELD_OFFSET(JSCell, m_structure)));

    // write the value
    compilePutDirectOffset(regT0, regT1, newStructure, cachedOffset);

    ret();
    
    ASSERT(!failureCases.empty());
    failureCases.link(this);
    restoreArgumentReferenceForTrampoline();
    Call failureCall = tailRecursiveCall();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    patchBuffer.link(failureCall, JITStubs::cti_op_put_by_id_fail);

    if (willNeedStorageRealloc)
        patchBuffer.link(callTarget, resizePropertyStorage);
    
    stubInfo->stubRoutine = patchBuffer.entry();

    returnAddress.relinkCallerToFunction(code);
}

void JIT::patchGetByIdSelf(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // We don't want to patch more than once - in future go to cti_op_get_by_id_generic.
    // Should probably go to JITStubs::cti_op_get_by_id_fail, but that doesn't do anything interesting right now.
    returnAddress.relinkCallerToFunction(JITStubs::cti_op_get_by_id_self_fail);

    int offset = sizeof(JSValue) * cachedOffset;

    // If we're patching to use inline storage, convert the initial load to a lea; this avoids the extra load
    // and makes the subsequent load's offset automatically correct
    if (structure->isUsingInlineStorage())
        stubInfo->hotPathBegin.instructionAtOffset(patchOffsetGetByIdExternalLoad + patchLengthGetByIdExternalLoadPrefix).patchLoadToLEA();

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetGetByIdStructure).repatch(structure);
    stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetGetByIdPropertyMapOffset).repatch(offset);
}

void JIT::patchPutByIdReplace(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to JITStubs::cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    returnAddress.relinkCallerToFunction(JITStubs::cti_op_put_by_id_generic);

    int offset = sizeof(JSValue) * cachedOffset;

    // If we're patching to use inline storage, convert the initial load to a lea; this avoids the extra load
    // and makes the subsequent load's offset automatically correct
    if (structure->isUsingInlineStorage())
        stubInfo->hotPathBegin.instructionAtOffset(patchOffsetPutByIdExternalLoad + patchLengthPutByIdExternalLoadPrefix).patchLoadToLEA();

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetPutByIdStructure).repatch(structure);
    stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset).repatch(offset);
}

void JIT::privateCompilePatchGetArrayLength(ProcessorReturnAddress returnAddress)
{
    StructureStubInfo* stubInfo = &m_codeBlock->getStubInfo(returnAddress);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    returnAddress.relinkCallerToFunction(JITStubs::cti_op_get_by_id_array_fail);

    // Check eax is an array
    Jump failureCases1 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsArrayVPtr));

    // Checks out okay! - get the length from the storage
    loadPtr(Address(regT0, FIELD_OFFSET(JSArray, m_storage)), regT2);
    load32(Address(regT2, FIELD_OFFSET(ArrayStorage, m_length)), regT2);

    Jump failureCases2 = branch32(Above, regT2, Imm32(JSImmediate::maxImmediateInt));

    emitFastArithIntToImmNoCheck(regT2, regT0);
    Jump success = jump();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.entry();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    jumpLocation.relink(entryLabel);
}

void JIT::privateCompileGetByIdProto(StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, ProcessorReturnAddress returnAddress, CallFrame* callFrame)
{
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    returnAddress.relinkCallerToFunction(JITStubs::cti_op_get_by_id_proto_list);

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

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel slowCaseBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);
    patchBuffer.link(failureCases1, slowCaseBegin);
    patchBuffer.link(failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.entry();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    jumpLocation.relink(entryLabel);
}

void JIT::privateCompileGetByIdSelfList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, size_t cachedOffset)
{
    Jump failureCase = checkStructure(regT0, structure);
    compileGetDirectOffset(regT0, regT0, structure, cachedOffset);
    Jump success = jump();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    ASSERT(code);
    PatchBuffer patchBuffer(code);

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = polymorphicStructures->list[currentIndex - 1].stubRoutine;
    if (!lastProtoBegin)
        lastProtoBegin = stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall);

    patchBuffer.link(failureCase, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.entry();

    structure->ref();
    polymorphicStructures->list[currentIndex].set(entryLabel, structure);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    jumpLocation.relink(entryLabel);
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

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;
    patchBuffer.link(failureCases1, lastProtoBegin);
    patchBuffer.link(failureCases2, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.entry();

    structure->ref();
    prototypeStructure->ref();
    prototypeStructures->list[currentIndex].set(entryLabel, structure, prototypeStructure);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    jumpLocation.relink(entryLabel);
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

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    // Use the patch information to link the failure cases back to the original slow case routine.
    CodeLocationLabel lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;

    patchBuffer.link(bucketsOfFail, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    CodeLocationLabel entryLabel = patchBuffer.entry();

    // Track the stub we have created so that it will be deleted later.
    structure->ref();
    chain->ref();
    prototypeStructures->list[currentIndex].set(entryLabel, structure, chain);

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    jumpLocation.relink(entryLabel);
}

void JIT::privateCompileGetByIdChain(StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, ProcessorReturnAddress returnAddress, CallFrame* callFrame)
{
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    returnAddress.relinkCallerToFunction(JITStubs::cti_op_get_by_id_proto_list);

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

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    // Use the patch information to link the failure cases back to the original slow case routine.
    patchBuffer.link(bucketsOfFail, stubInfo->callReturnLocation.labelAtOffset(-patchOffsetGetByIdSlowCaseCall));

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    patchBuffer.link(success, stubInfo->hotPathBegin.labelAtOffset(patchOffsetGetByIdPutResult));

    // Track the stub we have created so that it will be deleted later.
    CodeLocationLabel entryLabel = patchBuffer.entry();
    stubInfo->stubRoutine = entryLabel;

    // Finally patch the jump to slow case back in the hot path to jump here instead.
    CodeLocationJump jumpLocation = stubInfo->hotPathBegin.jumpAtOffset(patchOffsetGetByIdBranchToSlowCase);
    jumpLocation.relink(entryLabel);
}

/* ------------------------------ END: !ENABLE / ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS) ------------------------------ */

#endif // !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

} // namespace JSC

#endif // ENABLE(JIT)
