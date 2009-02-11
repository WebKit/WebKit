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

#if !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

void JIT::compileGetByIdHotPath(int resultVReg, int baseVReg, Identifier* ident, unsigned)
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be patched.
    // Additionally, for get_by_id we need patch the offset of the branch to the slow case (we patch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.

    emitGetVirtualRegister(baseVReg, X86::eax);

    emitPutJITStubArg(X86::eax, 1);
    emitPutJITStubArgConstant(ident, 2);
    emitCTICall(Interpreter::cti_op_get_by_id_generic);
    emitPutVirtualRegister(resultVReg);
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

    emitGetVirtualRegisters(baseVReg, X86::eax, valueVReg, X86::edx);

    emitPutJITStubArgConstant(ident, 2);
    emitPutJITStubArg(X86::eax, 1);
    emitPutJITStubArg(X86::edx, 3);
    emitCTICall(Interpreter::cti_op_put_by_id_generic);
}

void JIT::compilePutByIdSlowCase(int, Identifier*, int, Vector<SlowCaseEntry>::iterator&, unsigned)
{
    ASSERT_NOT_REACHED();
}

#else

void JIT::compileGetByIdHotPath(int resultVReg, int baseVReg, Identifier*, unsigned propertyAccessInstructionIndex)
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be patched.
    // Additionally, for get_by_id we need patch the offset of the branch to the slow case (we patch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.

    emitGetVirtualRegister(baseVReg, X86::eax);

    emitJumpSlowCaseIfNotJSCell(X86::eax, baseVReg);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    DataLabelPtr structureToCompare;
    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    addSlowCase(structureCheck);
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetGetByIdStructure);
    ASSERT(differenceBetween(hotPathBegin, structureCheck) == patchOffsetGetByIdBranchToSlowCase);

    loadPtr(Address(X86::eax, FIELD_OFFSET(JSObject, m_propertyStorage)), X86::eax);
    DataLabel32 displacementLabel = loadPtrWithAddressOffsetPatch(Address(X86::eax, patchGetByIdDefaultOffset), X86::eax);
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
    emitPutJITStubArg(X86::eax, 1);
    emitPutJITStubArgConstant(ident, 2);
    Call call = emitCTICall(Interpreter::cti_op_get_by_id);
    emitPutVirtualRegister(resultVReg);

    ASSERT(differenceBetween(coldPathBegin, call) == patchOffsetGetByIdSlowCaseCall);

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].callReturnLocation = call;
}

void JIT::compilePutByIdHotPath(int baseVReg, Identifier*, int valueVReg, unsigned propertyAccessInstructionIndex)
{
    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    emitGetVirtualRegisters(baseVReg, X86::eax, valueVReg, X86::edx);

    // Jump to a slow case if either the base object is an immediate, or if the Structure does not match.
    emitJumpSlowCaseIfNotJSCell(X86::eax, baseVReg);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    // It is important that the following instruction plants a 32bit immediate, in order that it can be patched over.
    DataLabelPtr structureToCompare;
    addSlowCase(branchPtrWithPatch(NotEqual, Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure))));
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetPutByIdStructure);

    // Plant a load from a bogus ofset in the object's property map; we will patch this later, if it is to be used.
    loadPtr(Address(X86::eax, FIELD_OFFSET(JSObject, m_propertyStorage)), X86::eax);
    DataLabel32 displacementLabel = storePtrWithAddressOffsetPatch(X86::edx, Address(X86::eax, patchGetByIdDefaultOffset));
    ASSERT(differenceBetween(hotPathBegin, displacementLabel) == patchOffsetPutByIdPropertyMapOffset);
}

void JIT::compilePutByIdSlowCase(int baseVReg, Identifier* ident, int, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex)
{
    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

    emitPutJITStubArgConstant(ident, 2);
    emitPutJITStubArg(X86::eax, 1);
    emitPutJITStubArg(X86::edx, 3);
    Call call = emitCTICall(Interpreter::cti_op_put_by_id);

    // Track the location of the call; this will be used to recover patch information.
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].callReturnLocation = call;
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
    failureCases.append(emitJumpIfNotJSCell(X86::eax));
    failureCases.append(branchPtr(NotEqual, Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), ImmPtr(oldStructure)));
    JumpList successCases;

    //  ecx = baseObject
    loadPtr(Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
    // proto(ecx) = baseObject->structure()->prototype()
    failureCases.append(branch32(NotEqual, Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo) + FIELD_OFFSET(TypeInfo, m_type)), Imm32(ObjectType)));

    loadPtr(Address(X86::ecx, FIELD_OFFSET(Structure, m_prototype)), X86::ecx);
    
    // ecx = baseObject->m_structure
    for (RefPtr<Structure>* it = chain->head(); *it; ++it) {
        // null check the prototype
        successCases.append(branchPtr(Equal, X86::ecx, ImmPtr(JSValuePtr::encode(jsNull()))));

        // Check the structure id
        failureCases.append(branchPtr(NotEqual, Address(X86::ecx, FIELD_OFFSET(JSCell, m_structure)), ImmPtr(it->get())));
        
        loadPtr(Address(X86::ecx, FIELD_OFFSET(JSCell, m_structure)), X86::ecx);
        failureCases.append(branch32(NotEqual, Address(X86::ecx, FIELD_OFFSET(Structure, m_typeInfo) + FIELD_OFFSET(TypeInfo, m_type)), Imm32(ObjectType)));
        loadPtr(Address(X86::ecx, FIELD_OFFSET(Structure, m_prototype)), X86::ecx);
    }

    successCases.link(this);

    Call callTarget;

    // emit a call only if storage realloc is needed
    if (transitionWillNeedStorageRealloc(oldStructure, newStructure)) {
        pop(X86::ebx);
#if PLATFORM(X86_64)
        move(Imm32(newStructure->propertyStorageCapacity()), X86::edx);
        move(Imm32(oldStructure->propertyStorageCapacity()), X86::esi);
        move(X86::eax, X86::edi);
        callTarget = call();
#else
        push(Imm32(newStructure->propertyStorageCapacity()));
        push(Imm32(oldStructure->propertyStorageCapacity()));
        push(X86::eax);
        callTarget = call();
        addPtr(Imm32(3 * sizeof(void*)), X86::esp);
#endif
        emitGetJITStubArg(3, X86::edx);
        push(X86::ebx);
    }

    // Assumes m_refCount can be decremented easily, refcount decrement is safe as 
    // codeblock should ensure oldStructure->m_refCount > 0
    sub32(Imm32(1), AbsoluteAddress(oldStructure->addressOfCount()));
    add32(Imm32(1), AbsoluteAddress(newStructure->addressOfCount()));
    storePtr(ImmPtr(newStructure), Address(X86::eax, FIELD_OFFSET(JSCell, m_structure)));

    // write the value
    loadPtr(Address(X86::eax, FIELD_OFFSET(JSObject, m_propertyStorage)), X86::eax);
    storePtr(X86::edx, Address(X86::eax, cachedOffset * sizeof(JSValuePtr)));

    ret();
    
    Jump failureJump;
    bool plantedFailureJump = false;
    if (!failureCases.empty()) {
        failureCases.link(this);
        restoreArgumentReferenceForTrampoline();
        failureJump = jump();
        plantedFailureJump = true;
    }

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    if (plantedFailureJump)
        patchBuffer.linkTailRecursive(failureJump, Interpreter::cti_op_put_by_id_fail);

    if (transitionWillNeedStorageRealloc(oldStructure, newStructure))
        patchBuffer.link(callTarget, resizePropertyStorage);
    
    stubInfo->stubRoutine = patchBuffer.entry();

    returnAddress.relinkCallerToFunction(code);
}

void JIT::patchGetByIdSelf(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // We don't want to patch more than once - in future go to cti_op_get_by_id_generic.
    // Should probably go to Interpreter::cti_op_get_by_id_fail, but that doesn't do anything interesting right now.
    returnAddress.relinkCallerToFunction(Interpreter::cti_op_get_by_id_self_fail);

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetGetByIdStructure).repatch(structure);
    stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetGetByIdPropertyMapOffset).repatch(cachedOffset * sizeof(JSValuePtr));
}

void JIT::patchPutByIdReplace(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to Interpreter::cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    returnAddress.relinkCallerToFunction(Interpreter::cti_op_put_by_id_generic);

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetPutByIdStructure).repatch(structure);
    stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset).repatch(cachedOffset * sizeof(JSValuePtr));
}

void JIT::privateCompilePatchGetArrayLength(ProcessorReturnAddress returnAddress)
{
    StructureStubInfo* stubInfo = &m_codeBlock->getStubInfo(returnAddress);

    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    returnAddress.relinkCallerToFunction(Interpreter::cti_op_get_by_id_array_fail);

    // Check eax is an array
    Jump failureCases1 = branchPtr(NotEqual, Address(X86::eax), ImmPtr(m_interpreter->m_jsArrayVptr));

    // Checks out okay! - get the length from the storage
    loadPtr(Address(X86::eax, FIELD_OFFSET(JSArray, m_storage)), X86::ecx);
    load32(Address(X86::ecx, FIELD_OFFSET(ArrayStorage, m_length)), X86::ecx);

    Jump failureCases2 = branch32(Above, X86::ecx, Imm32(JSImmediate::maxImmediateInt));

    emitFastArithIntToImmNoCheck(X86::ecx, X86::eax);
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

void JIT::privateCompileGetByIdSelf(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // Check eax is an object of the right Structure.
    Jump failureCases1 = emitJumpIfNotJSCell(X86::eax);
    Jump failureCases2 = checkStructure(X86::eax, structure);

    // Checks out okay! - getDirectOffset
    loadPtr(Address(X86::eax, FIELD_OFFSET(JSObject, m_propertyStorage)), X86::eax);
    loadPtr(Address(X86::eax, cachedOffset * sizeof(JSValuePtr)), X86::eax);
    ret();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    patchBuffer.linkTailRecursive(failureCases1, Interpreter::cti_op_get_by_id_self_fail);
    patchBuffer.linkTailRecursive(failureCases2, Interpreter::cti_op_get_by_id_self_fail);

    stubInfo->stubRoutine = patchBuffer.entry();

    returnAddress.relinkCallerToFunction(code);
}

void JIT::privateCompileGetByIdProto(StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, ProcessorReturnAddress returnAddress, CallFrame* callFrame)
{
#if USE(CTI_REPATCH_PIC)
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    returnAddress.relinkCallerToFunction(Interpreter::cti_op_get_by_id_proto_list);

    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(static_cast<void*>(protoPropertyStorage), X86::edx);

    // Check eax is an object of the right Structure.
    Jump failureCases1 = checkStructure(X86::eax, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
    move(ImmPtr(prototypeStructure), X86::ebx);
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), X86::ebx);
#else
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));
#endif

    // Checks out okay! - getDirectOffset
    loadPtr(Address(X86::edx, cachedOffset * sizeof(JSValuePtr)), X86::eax);

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
#else
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, X86::edx);

    // Check eax is an object of the right Structure.
    Jump failureCases1 = emitJumpIfNotJSCell(X86::eax);
    Jump failureCases2 = checkStructure(X86::eax, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
    Jump failureCases3 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));

    // Checks out okay! - getDirectOffset
    loadPtr(Address(X86::edx, cachedOffset * sizeof(JSValuePtr)), X86::eax);

    ret();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    patchBuffer.link(failureCases1, Interpreter::cti_op_get_by_id_proto_fail);
    patchBuffer.link(failureCases2, Interpreter::cti_op_get_by_id_proto_fail);
    patchBuffer.link(failureCases3, Interpreter::cti_op_get_by_id_proto_fail);

    stubInfo->stubRoutine = patchBuffer.entry();

    returnAddress.relinkCallerToFunction(code);
#endif
}

#if USE(CTI_REPATCH_PIC)
void JIT::privateCompileGetByIdSelfList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, size_t cachedOffset)
{
    Jump failureCase = checkStructure(X86::eax, structure);
    loadPtr(Address(X86::eax, FIELD_OFFSET(JSObject, m_propertyStorage)), X86::eax);
    loadPtr(Address(X86::eax, cachedOffset * sizeof(JSValuePtr)), X86::eax);
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
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, X86::edx);

    // Check eax is an object of the right Structure.
    Jump failureCases1 = checkStructure(X86::eax, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
    move(ImmPtr(prototypeStructure), X86::ebx);
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), X86::ebx);
#else
    Jump failureCases2 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));
#endif

    // Checks out okay! - getDirectOffset
    loadPtr(Address(X86::edx, cachedOffset * sizeof(JSValuePtr)), X86::eax);

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
    Jump baseObjectCheck = checkStructure(X86::eax, structure);
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
        move(ImmPtr(currStructure), X86::ebx);
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), X86::ebx));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, X86::edx);
    loadPtr(Address(X86::edx, cachedOffset * sizeof(JSValuePtr)), X86::eax);
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
#endif

void JIT::privateCompileGetByIdChain(StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, ProcessorReturnAddress returnAddress, CallFrame* callFrame)
{
#if USE(CTI_REPATCH_PIC)
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    returnAddress.relinkCallerToFunction(Interpreter::cti_op_get_by_id_proto_list);

    ASSERT(count);
    
    JumpList bucketsOfFail;

    // Check eax is an object of the right Structure.
    bucketsOfFail.append(checkStructure(X86::eax, structure));

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
        move(ImmPtr(currStructure), X86::ebx);
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), X86::ebx));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, X86::edx);
    loadPtr(Address(X86::edx, cachedOffset * sizeof(JSValuePtr)), X86::eax);
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
#else
    ASSERT(count);
    
    JumpList bucketsOfFail;

    // Check eax is an object of the right Structure.
    bucketsOfFail.append(emitJumpIfNotJSCell(X86::eax));
    bucketsOfFail.append(checkStructure(X86::eax, structure));

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
#if PLATFORM(X86_64)
        move(ImmPtr(currStructure), X86::ebx);
        bucketsOfFail.append(branchPtr(NotEqual, X86::ebx, AbsoluteAddress(prototypeStructureAddress)));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, X86::edx);
    loadPtr(Address(X86::edx, cachedOffset * sizeof(JSValuePtr)), X86::eax);
    ret();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());

    patchBuffer.link(bucketsOfFail, Interpreter::cti_op_get_by_id_proto_fail);

    stubInfo->stubRoutine = patchBuffer.entry();

    returnAddress.relinkCallerToFunction(code);
#endif
}

void JIT::privateCompilePutByIdReplace(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // Check eax is an object of the right Structure.
    Jump failureCases1 = emitJumpIfNotJSCell(X86::eax);
    Jump failureCases2 = checkStructure(X86::eax, structure);

    // checks out okay! - putDirectOffset
    loadPtr(Address(X86::eax, FIELD_OFFSET(JSObject, m_propertyStorage)), X86::eax);
    storePtr(X86::edx, Address(X86::eax, cachedOffset * sizeof(JSValuePtr)));
    ret();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);
    
    patchBuffer.linkTailRecursive(failureCases1, Interpreter::cti_op_put_by_id_fail);
    patchBuffer.linkTailRecursive(failureCases2, Interpreter::cti_op_put_by_id_fail);

    stubInfo->stubRoutine = patchBuffer.entry();
    
    returnAddress.relinkCallerToFunction(code);
}

#endif

} // namespace JSC

#endif // ENABLE(JIT)
