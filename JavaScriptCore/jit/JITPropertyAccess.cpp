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

    emitGetVirtualRegister(baseVReg, regT0);

    emitPutJITStubArg(regT0, 1);
    emitPutJITStubArgConstant(ident, 2);
    emitCTICall(JITStubs::cti_op_get_by_id_generic);
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

    emitGetVirtualRegisters(baseVReg, regT0, valueVReg, regT1);

    emitPutJITStubArgConstant(ident, 2);
    emitPutJITStubArg(regT0, 1);
    emitPutJITStubArg(regT1, 3);
    emitCTICall(JITStubs::cti_op_put_by_id_generic);
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

    emitGetVirtualRegister(baseVReg, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    Label hotPathBegin(this);
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    DataLabelPtr structureToCompare;
    Jump structureCheck = branchPtrWithPatch(NotEqual, Address(regT0, FIELD_OFFSET(JSCell, m_structure)), structureToCompare, ImmPtr(reinterpret_cast<void*>(patchGetByIdDefaultStructure)));
    addSlowCase(structureCheck);
    ASSERT(differenceBetween(hotPathBegin, structureToCompare) == patchOffsetGetByIdStructure);
    ASSERT(differenceBetween(hotPathBegin, structureCheck) == patchOffsetGetByIdBranchToSlowCase);

    loadPtr(Address(regT0, FIELD_OFFSET(JSObject, m_propertyStorage)), regT0);
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
    emitPutJITStubArg(regT0, 1);
    emitPutJITStubArgConstant(ident, 2);
    Call call = emitCTICall(JITStubs::cti_op_get_by_id);
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
    loadPtr(Address(regT0, FIELD_OFFSET(JSObject, m_propertyStorage)), regT0);
    DataLabel32 displacementLabel = storePtrWithAddressOffsetPatch(regT1, Address(regT0, patchGetByIdDefaultOffset));
    ASSERT(differenceBetween(hotPathBegin, displacementLabel) == patchOffsetPutByIdPropertyMapOffset);
}

void JIT::compilePutByIdSlowCase(int baseVReg, Identifier* ident, int, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex)
{
    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

    emitPutJITStubArgConstant(ident, 2);
    emitPutJITStubArg(regT0, 1);
    emitPutJITStubArg(regT1, 3);
    Call call = emitCTICall(JITStubs::cti_op_put_by_id);

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
        successCases.append(branchPtr(Equal, regT2, ImmPtr(JSValuePtr::encode(jsNull()))));

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
        move(Imm32(newStructure->propertyStorageCapacity()), regT1);
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
    loadPtr(Address(regT0, FIELD_OFFSET(JSObject, m_propertyStorage)), regT0);
    storePtr(regT1, Address(regT0, cachedOffset * sizeof(JSValuePtr)));

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

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetGetByIdStructure).repatch(structure);
    stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetGetByIdPropertyMapOffset).repatch(cachedOffset * sizeof(JSValuePtr));
}

void JIT::patchPutByIdReplace(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // We don't want to patch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to JITStubs::cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    returnAddress.relinkCallerToFunction(JITStubs::cti_op_put_by_id_generic);

    // Patch the offset into the propoerty map to load from, then patch the Structure to look for.
    stubInfo->hotPathBegin.dataLabelPtrAtOffset(patchOffsetPutByIdStructure).repatch(structure);
    stubInfo->hotPathBegin.dataLabel32AtOffset(patchOffsetPutByIdPropertyMapOffset).repatch(cachedOffset * sizeof(JSValuePtr));
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

void JIT::privateCompileGetByIdSelf(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // Check eax is an object of the right Structure.
    Jump failureCases1 = emitJumpIfNotJSCell(regT0);
    Jump failureCases2 = checkStructure(regT0, structure);

    // Checks out okay! - getDirectOffset
    loadPtr(Address(regT0, FIELD_OFFSET(JSObject, m_propertyStorage)), regT0);
    loadPtr(Address(regT0, cachedOffset * sizeof(JSValuePtr)), regT0);
    ret();

    Call failureCases1Call = makeTailRecursiveCall(failureCases1);
    Call failureCases2Call = makeTailRecursiveCall(failureCases2);

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    patchBuffer.link(failureCases1Call, JITStubs::cti_op_get_by_id_self_fail);
    patchBuffer.link(failureCases2Call, JITStubs::cti_op_get_by_id_self_fail);

    stubInfo->stubRoutine = patchBuffer.entry();

    returnAddress.relinkCallerToFunction(code);
}

void JIT::privateCompileGetByIdProto(StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, ProcessorReturnAddress returnAddress, CallFrame* callFrame)
{
#if USE(CTI_REPATCH_PIC)
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
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(static_cast<void*>(protoPropertyStorage), regT1);
    loadPtr(Address(regT1, cachedOffset * sizeof(JSValuePtr)), regT0);

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

    // Check eax is an object of the right Structure.
    Jump failureCases1 = emitJumpIfNotJSCell(regT0);
    Jump failureCases2 = checkStructure(regT0, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
    Jump failureCases3 = branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(prototypeStructure));

    // Checks out okay! - getDirectOffset
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, regT1);
    loadPtr(Address(regT1, cachedOffset * sizeof(JSValuePtr)), regT0);

    ret();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);

    patchBuffer.link(failureCases1, JITStubs::cti_op_get_by_id_proto_fail);
    patchBuffer.link(failureCases2, JITStubs::cti_op_get_by_id_proto_fail);
    patchBuffer.link(failureCases3, JITStubs::cti_op_get_by_id_proto_fail);

    stubInfo->stubRoutine = patchBuffer.entry();

    returnAddress.relinkCallerToFunction(code);
#endif
}

#if USE(CTI_REPATCH_PIC)
void JIT::privateCompileGetByIdSelfList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, size_t cachedOffset)
{
    Jump failureCase = checkStructure(regT0, structure);
    loadPtr(Address(regT0, FIELD_OFFSET(JSObject, m_propertyStorage)), regT0);
    loadPtr(Address(regT0, cachedOffset * sizeof(JSValuePtr)), regT0);
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
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, regT1);
    loadPtr(Address(regT1, cachedOffset * sizeof(JSValuePtr)), regT0);

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

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, regT1);
    loadPtr(Address(regT1, cachedOffset * sizeof(JSValuePtr)), regT0);
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

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, regT1);
    loadPtr(Address(regT1, cachedOffset * sizeof(JSValuePtr)), regT0);
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
    bucketsOfFail.append(emitJumpIfNotJSCell(regT0));
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
        bucketsOfFail.append(branchPtr(NotEqual, regT3, AbsoluteAddress(prototypeStructureAddress)));
#else
        bucketsOfFail.append(branchPtr(NotEqual, AbsoluteAddress(prototypeStructureAddress), ImmPtr(currStructure)));
#endif
    }
    ASSERT(protoObject);

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    loadPtr(protoPropertyStorage, regT1);
    loadPtr(Address(regT1, cachedOffset * sizeof(JSValuePtr)), regT0);
    ret();

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());

    patchBuffer.link(bucketsOfFail, JITStubs::cti_op_get_by_id_proto_fail);

    stubInfo->stubRoutine = patchBuffer.entry();

    returnAddress.relinkCallerToFunction(code);
#endif
}

void JIT::privateCompilePutByIdReplace(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, ProcessorReturnAddress returnAddress)
{
    // Check eax is an object of the right Structure.
    Jump failureCases1 = emitJumpIfNotJSCell(regT0);
    Jump failureCases2 = checkStructure(regT0, structure);

    // checks out okay! - putDirectOffset
    loadPtr(Address(regT0, FIELD_OFFSET(JSObject, m_propertyStorage)), regT0);
    storePtr(regT1, Address(regT0, cachedOffset * sizeof(JSValuePtr)));
    ret();

    Call failureCases1Call = makeTailRecursiveCall(failureCases1);
    Call failureCases2Call = makeTailRecursiveCall(failureCases2);

    void* code = m_assembler.executableCopy(m_codeBlock->executablePool());
    PatchBuffer patchBuffer(code);
    
    patchBuffer.link(failureCases1Call, JITStubs::cti_op_put_by_id_fail);
    patchBuffer.link(failureCases2Call, JITStubs::cti_op_put_by_id_fail);

    stubInfo->stubRoutine = patchBuffer.entry();
    
    returnAddress.relinkCallerToFunction(code);
}

#endif

} // namespace JSC

#endif // ENABLE(JIT)
