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

#define __ m_assembler.

using namespace std;

namespace JSC {

typedef X86Assembler::JmpSrc JmpSrc;
typedef X86Assembler::JmpDst JmpDst;

#if !ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

void JIT::compileGetByIdHotPath(int resultVReg, int baseVReg, Identifier* ident, unsigned)
{
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be repatched.
    // Additionally, for get_by_id we need repatch the offset of the branch to the slow case (we repatch this to jump
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
    // In order to be able to repatch both the Structure, and the object offset, we store one pointer,
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
    // As for put_by_id, get_by_id requires the offset of the Structure and the offset of the access to be repatched.
    // Additionally, for get_by_id we need repatch the offset of the branch to the slow case (we repatch this to jump
    // to array-length / prototype access tranpolines, and finally we also the the property-map access offset as a label
    // to jump back to if one of these trampolies finds a match.

    emitGetVirtualRegister(baseVReg, X86::eax);

    emitJumpSlowCaseIfNotJSCell(X86::eax, baseVReg);

    JmpDst hotPathBegin = __ label();
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    __ cmpl_im_force32(repatchGetByIdDefaultStructure, FIELD_OFFSET(JSCell, m_structure), X86::eax);
    ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, __ label()) == repatchOffsetGetByIdStructure);
    addSlowCase(__ jne());
    ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, __ label()) == repatchOffsetGetByIdBranchToSlowCase);

    __ movl_mr(FIELD_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    __ movl_mr(repatchGetByIdDefaultOffset, X86::eax, X86::eax);
    ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, __ label()) == repatchOffsetGetByIdPropertyMapOffset);
    emitPutVirtualRegister(resultVReg);
}


void JIT::compileGetByIdSlowCase(int resultVReg, int baseVReg, Identifier* ident, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex)
{
    // As for the hot path of get_by_id, above, we ensure that we can use an architecture specific offset
    // so that we only need track one pointer into the slow case code - we track a pointer to the location
    // of the call (which we can use to look up the repatch information), but should a array-length or
    // prototype access trampoline fail we want to bail out back to here.  To do so we can subtract back
    // the distance from the call to the head of the slow case.

    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

#ifndef NDEBUG
    JmpDst coldPathBegin = __ label();
#endif
    emitPutJITStubArg(X86::eax, 1);
    emitPutJITStubArgConstant(reinterpret_cast<unsigned>(ident), 2);
    JmpSrc call = emitCTICall(Interpreter::cti_op_get_by_id);
    ASSERT(X86Assembler::getDifferenceBetweenLabels(coldPathBegin, call) == repatchOffsetGetByIdSlowCaseCall);
    emitPutVirtualRegister(resultVReg);

    // Track the location of the call; this will be used to recover repatch information.
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].callReturnLocation = call;
}

void JIT::compilePutByIdHotPath(int baseVReg, Identifier*, int valueVReg, unsigned propertyAccessInstructionIndex)
{
    // In order to be able to repatch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    emitGetVirtualRegisters(baseVReg, X86::eax, valueVReg, X86::edx);

    // Jump to a slow case if either the base object is an immediate, or if the Structure does not match.
    emitJumpSlowCaseIfNotJSCell(X86::eax, baseVReg);

    JmpDst hotPathBegin = __ label();
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].hotPathBegin = hotPathBegin;

    // It is important that the following instruction plants a 32bit immediate, in order that it can be patched over.
    __ cmpl_im_force32(repatchGetByIdDefaultStructure, FIELD_OFFSET(JSCell, m_structure), X86::eax);
    ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, __ label()) == repatchOffsetPutByIdStructure);
    addSlowCase(__ jne());

    // Plant a load from a bogus ofset in the object's property map; we will patch this later, if it is to be used.
    __ movl_mr(FIELD_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    __ movl_rm(X86::edx, repatchGetByIdDefaultOffset, X86::eax);
    ASSERT(X86Assembler::getDifferenceBetweenLabels(hotPathBegin, __ label()) == repatchOffsetPutByIdPropertyMapOffset);
}

void JIT::compilePutByIdSlowCase(int baseVReg, Identifier* ident, int, Vector<SlowCaseEntry>::iterator& iter, unsigned propertyAccessInstructionIndex)
{
    linkSlowCaseIfNotJSCell(iter, baseVReg);
    linkSlowCase(iter);

    emitPutJITStubArgConstant(reinterpret_cast<unsigned>(ident), 2);
    emitPutJITStubArg(X86::eax, 1);
    emitPutJITStubArg(X86::edx, 3);
    JmpSrc call = emitCTICall(Interpreter::cti_op_put_by_id);

    // Track the location of the call; this will be used to recover repatch information.
    m_propertyAccessCompilationInfo[propertyAccessInstructionIndex].callReturnLocation = call;
}

static JSObject* resizePropertyStorage(JSObject* baseObject, size_t oldSize, size_t newSize)
{
    baseObject->allocatePropertyStorageInline(oldSize, newSize);
    return baseObject;
}

static inline bool transitionWillNeedStorageRealloc(Structure* oldStructure, Structure* newStructure)
{
    return oldStructure->propertyStorageCapacity() != newStructure->propertyStorageCapacity();
}

void JIT::privateCompilePutByIdTransition(StructureStubInfo* stubInfo, Structure* oldStructure, Structure* newStructure, size_t cachedOffset, StructureChain* chain, void* returnAddress)
{
    Vector<JmpSrc, 16> failureCases;
    // Check eax is an object of the right Structure.
    __ testl_i32r(JSImmediate::TagMask, X86::eax);
    failureCases.append(__ jne());
    __ cmpl_im(reinterpret_cast<uint32_t>(oldStructure), FIELD_OFFSET(JSCell, m_structure), X86::eax);
    failureCases.append(__ jne());
    Vector<JmpSrc> successCases;

    //  ecx = baseObject
    __ movl_mr(FIELD_OFFSET(JSCell, m_structure), X86::eax, X86::ecx);
    // proto(ecx) = baseObject->structure()->prototype()
    __ cmpl_im(ObjectType, FIELD_OFFSET(Structure, m_typeInfo) + FIELD_OFFSET(TypeInfo, m_type), X86::ecx);
    failureCases.append(__ jne());
    __ movl_mr(FIELD_OFFSET(Structure, m_prototype), X86::ecx, X86::ecx);
    
    // ecx = baseObject->m_structure
    for (RefPtr<Structure>* it = chain->head(); *it; ++it) {
        // null check the prototype
        __ cmpl_ir(asInteger(jsNull()), X86::ecx);
        successCases.append(__ je());

        // Check the structure id
        __ cmpl_im(reinterpret_cast<uint32_t>(it->get()), FIELD_OFFSET(JSCell, m_structure), X86::ecx);
        failureCases.append(__ jne());
        
        __ movl_mr(FIELD_OFFSET(JSCell, m_structure), X86::ecx, X86::ecx);
        __ cmpl_im(ObjectType, FIELD_OFFSET(Structure, m_typeInfo) + FIELD_OFFSET(TypeInfo, m_type), X86::ecx);
        failureCases.append(__ jne());
        __ movl_mr(FIELD_OFFSET(Structure, m_prototype), X86::ecx, X86::ecx);
    }

    failureCases.append(__ jne());
    for (unsigned i = 0; i < successCases.size(); ++i)
        __ link(successCases[i], __ label());

    JmpSrc callTarget;

    // emit a call only if storage realloc is needed
    if (transitionWillNeedStorageRealloc(oldStructure, newStructure)) {
        __ push_r(X86::edx);
        __ push_i32(newStructure->propertyStorageCapacity());
        __ push_i32(oldStructure->propertyStorageCapacity());
        __ push_r(X86::eax);
        callTarget = __ call();
        __ addl_ir(3 * sizeof(void*), X86::esp);
        __ pop_r(X86::edx);
    }

    // Assumes m_refCount can be decremented easily, refcount decrement is safe as 
    // codeblock should ensure oldStructure->m_refCount > 0
    __ subl_im(1, reinterpret_cast<void*>(oldStructure));
    __ addl_im(1, reinterpret_cast<void*>(newStructure));
    __ movl_i32m(reinterpret_cast<uint32_t>(newStructure), FIELD_OFFSET(JSCell, m_structure), X86::eax);

    // write the value
    __ movl_mr(FIELD_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    __ movl_rm(X86::edx, cachedOffset * sizeof(JSValue*), X86::eax);

    __ ret();
    
    JmpSrc failureJump;
    if (failureCases.size()) {
        for (unsigned i = 0; i < failureCases.size(); ++i)
            __ link(failureCases[i], __ label());
        restoreArgumentReferenceForTrampoline();
        failureJump = __ jmp();
    }

    void* code = __ executableCopy(m_codeBlock->executablePool());

    if (failureCases.size())
        X86Assembler::link(code, failureJump, reinterpret_cast<void*>(Interpreter::cti_op_put_by_id_fail));

    if (transitionWillNeedStorageRealloc(oldStructure, newStructure))
        X86Assembler::link(code, callTarget, reinterpret_cast<void*>(resizePropertyStorage));
    
    stubInfo->stubRoutine = code;
    
    ctiRepatchCallByReturnAddress(returnAddress, code);
}

void JIT::patchGetByIdSelf(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, void* returnAddress)
{
    // We don't want to repatch more than once - in future go to cti_op_get_by_id_generic.
    // Should probably go to Interpreter::cti_op_get_by_id_fail, but that doesn't do anything interesting right now.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_self_fail));

    // Repatch the offset into the propoerty map to load from, then repatch the Structure to look for.
    X86Assembler::repatchDisplacement(reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset, cachedOffset * sizeof(JSValue*));
    X86Assembler::repatchImmediate(reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdStructure, reinterpret_cast<uint32_t>(structure));
}

void JIT::patchPutByIdReplace(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, void* returnAddress)
{
    // We don't want to repatch more than once - in future go to cti_op_put_by_id_generic.
    // Should probably go to Interpreter::cti_op_put_by_id_fail, but that doesn't do anything interesting right now.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Interpreter::cti_op_put_by_id_generic));

    // Repatch the offset into the propoerty map to load from, then repatch the Structure to look for.
    X86Assembler::repatchDisplacement(reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetPutByIdPropertyMapOffset, cachedOffset * sizeof(JSValue*));
    X86Assembler::repatchImmediate(reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetPutByIdStructure, reinterpret_cast<uint32_t>(structure));
}

void JIT::privateCompilePatchGetArrayLength(void* returnAddress)
{
    StructureStubInfo* stubInfo = &m_codeBlock->getStubInfo(returnAddress);

    // We don't want to repatch more than once - in future go to cti_op_put_by_id_generic.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_array_fail));

    // Check eax is an array
    __ cmpl_im(reinterpret_cast<unsigned>(m_interpreter->m_jsArrayVptr), 0, X86::eax);
    JmpSrc failureCases1 = __ jne();

    // Checks out okay! - get the length from the storage
    __ movl_mr(FIELD_OFFSET(JSArray, m_storage), X86::eax, X86::ecx);
    __ movl_mr(FIELD_OFFSET(ArrayStorage, m_length), X86::ecx, X86::ecx);

    __ cmpl_ir(JSImmediate::maxImmediateInt, X86::ecx);
    JmpSrc failureCases2 = __ ja();

    __ addl_rr(X86::ecx, X86::ecx);
    __ addl_ir(1, X86::ecx);
    __ movl_rr(X86::ecx, X86::eax);
    JmpSrc success = __ jmp();

    void* code = __ executableCopy(m_codeBlock->executablePool());

    // Use the repatch information to link the failure cases back to the original slow case routine.
    void* slowCaseBegin = reinterpret_cast<char*>(stubInfo->callReturnLocation) - repatchOffsetGetByIdSlowCaseCall;
    X86Assembler::link(code, failureCases1, slowCaseBegin);
    X86Assembler::link(code, failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    intptr_t successDest = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset;
    X86Assembler::link(code, success, reinterpret_cast<void*>(successDest));

    // Track the stub we have created so that it will be deleted later.
    stubInfo->stubRoutine = code;

    // Finally repatch the jump to sow case back in the hot path to jump here instead.
    intptr_t jmpLocation = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdBranchToSlowCase;
    X86Assembler::repatchBranchOffset(jmpLocation, code);
}

void JIT::privateCompileGetByIdSelf(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, void* returnAddress)
{
    // Check eax is an object of the right Structure.
    __ testl_i32r(JSImmediate::TagMask, X86::eax);
    JmpSrc failureCases1 = __ jne();
    JmpSrc failureCases2 = checkStructure(X86::eax, structure);

    // Checks out okay! - getDirectOffset
    __ movl_mr(FIELD_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    __ movl_mr(cachedOffset * sizeof(JSValue*), X86::eax, X86::eax);
    __ ret();

    void* code = __ executableCopy(m_codeBlock->executablePool());

    X86Assembler::link(code, failureCases1, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_self_fail));
    X86Assembler::link(code, failureCases2, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_self_fail));

    stubInfo->stubRoutine = code;

    ctiRepatchCallByReturnAddress(returnAddress, code);
}

void JIT::privateCompileGetByIdProto(StructureStubInfo* stubInfo, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, void* returnAddress, CallFrame* callFrame)
{
#if USE(CTI_REPATCH_PIC)
    // We don't want to repatch more than once - in future go to cti_op_put_by_id_generic.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_proto_list));

    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    __ movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);

    // Check eax is an object of the right Structure.
    JmpSrc failureCases1 = checkStructure(X86::eax, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
    __ cmpl_im(reinterpret_cast<uint32_t>(prototypeStructure), prototypeStructureAddress);
    JmpSrc failureCases2 = __ jne();

    // Checks out okay! - getDirectOffset
    __ movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::eax);

    JmpSrc success = __ jmp();

    void* code = __ executableCopy(m_codeBlock->executablePool());

    // Use the repatch information to link the failure cases back to the original slow case routine.
    void* slowCaseBegin = reinterpret_cast<char*>(stubInfo->callReturnLocation) - repatchOffsetGetByIdSlowCaseCall;
    X86Assembler::link(code, failureCases1, slowCaseBegin);
    X86Assembler::link(code, failureCases2, slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    intptr_t successDest = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset;
    X86Assembler::link(code, success, reinterpret_cast<void*>(successDest));

    // Track the stub we have created so that it will be deleted later.
    stubInfo->stubRoutine = code;

    // Finally repatch the jump to slow case back in the hot path to jump here instead.
    intptr_t jmpLocation = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdBranchToSlowCase;
    X86Assembler::repatchBranchOffset(jmpLocation, code);
#else
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    __ movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);

    // Check eax is an object of the right Structure.
    __ testl_i32r(JSImmediate::TagMask, X86::eax);
    JmpSrc failureCases1 = __ jne();
    JmpSrc failureCases2 = checkStructure(X86::eax, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
    __ cmpl_im(reinterpret_cast<uint32_t>(prototypeStructure), prototypeStructureAddress);
    JmpSrc failureCases3 = __ jne();

    // Checks out okay! - getDirectOffset
    __ movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::eax);

    __ ret();

    void* code = __ executableCopy(m_codeBlock->executablePool());

    X86Assembler::link(code, failureCases1, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_proto_fail));
    X86Assembler::link(code, failureCases2, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_proto_fail));
    X86Assembler::link(code, failureCases3, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_proto_fail));

    stubInfo->stubRoutine = code;

    ctiRepatchCallByReturnAddress(returnAddress, code);
#endif
}

#if USE(CTI_REPATCH_PIC)
void JIT::privateCompileGetByIdSelfList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* polymorphicStructures, int currentIndex, Structure* structure, size_t cachedOffset)
{
    JmpSrc failureCase = checkStructure(X86::eax, structure);
    __ movl_mr(FIELD_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    __ movl_mr(cachedOffset * sizeof(JSValue*), X86::eax, X86::eax);
    JmpSrc success = __ jmp();

    void* code = __ executableCopy(m_codeBlock->executablePool());
    ASSERT(code);

    // Use the repatch information to link the failure cases back to the original slow case routine.
    void* lastProtoBegin = polymorphicStructures->list[currentIndex - 1].stubRoutine;
    if (!lastProtoBegin)
        lastProtoBegin = reinterpret_cast<char*>(stubInfo->callReturnLocation) - repatchOffsetGetByIdSlowCaseCall;

    X86Assembler::link(code, failureCase, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    intptr_t successDest = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset;
    X86Assembler::link(code, success, reinterpret_cast<void*>(successDest));

    structure->ref();
    polymorphicStructures->list[currentIndex].set(code, structure);

    // Finally repatch the jump to slow case back in the hot path to jump here instead.
    intptr_t jmpLocation = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdBranchToSlowCase;
    X86Assembler::repatchBranchOffset(jmpLocation, code);
}

void JIT::privateCompileGetByIdProtoList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, Structure* prototypeStructure, size_t cachedOffset, CallFrame* callFrame)
{
    // The prototype object definitely exists (if this stub exists the CodeBlock is referencing a Structure that is
    // referencing the prototype object - let's speculatively load it's table nice and early!)
    JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    __ movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);

    // Check eax is an object of the right Structure.
    JmpSrc failureCases1 = checkStructure(X86::eax, structure);

    // Check the prototype object's Structure had not changed.
    Structure** prototypeStructureAddress = &(protoObject->m_structure);
    __ cmpl_im(reinterpret_cast<uint32_t>(prototypeStructure), prototypeStructureAddress);
    JmpSrc failureCases2 = __ jne();

    // Checks out okay! - getDirectOffset
    __ movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::eax);

    JmpSrc success = __ jmp();

    void* code = __ executableCopy(m_codeBlock->executablePool());

    // Use the repatch information to link the failure cases back to the original slow case routine.
    void* lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;
    X86Assembler::link(code, failureCases1, lastProtoBegin);
    X86Assembler::link(code, failureCases2, lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    intptr_t successDest = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset;
    X86Assembler::link(code, success, reinterpret_cast<void*>(successDest));

    structure->ref();
    prototypeStructure->ref();
    prototypeStructures->list[currentIndex].set(code, structure, prototypeStructure);

    // Finally repatch the jump to slow case back in the hot path to jump here instead.
    intptr_t jmpLocation = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdBranchToSlowCase;
    X86Assembler::repatchBranchOffset(jmpLocation, code);
}

void JIT::privateCompileGetByIdChainList(StructureStubInfo* stubInfo, PolymorphicAccessStructureList* prototypeStructures, int currentIndex, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, CallFrame* callFrame)
{
    ASSERT(count);
    
    Vector<JmpSrc> bucketsOfFail;

    // Check eax is an object of the right Structure.
    JmpSrc baseObjectCheck = checkStructure(X86::eax, structure);
    bucketsOfFail.append(baseObjectCheck);

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
        __ cmpl_im(reinterpret_cast<uint32_t>(currStructure), prototypeStructureAddress);
        bucketsOfFail.append(__ jne());
    }
    ASSERT(protoObject);

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    __ movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);
    __ movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::eax);
    JmpSrc success = __ jmp();

    void* code = __ executableCopy(m_codeBlock->executablePool());

    // Use the repatch information to link the failure cases back to the original slow case routine.
    void* lastProtoBegin = prototypeStructures->list[currentIndex - 1].stubRoutine;

    for (unsigned i = 0; i < bucketsOfFail.size(); ++i)
        X86Assembler::link(code, bucketsOfFail[i], lastProtoBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    intptr_t successDest = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset;
    X86Assembler::link(code, success, reinterpret_cast<void*>(successDest));

    // Track the stub we have created so that it will be deleted later.
    structure->ref();
    chain->ref();
    prototypeStructures->list[currentIndex].set(code, structure, chain);

    // Finally repatch the jump to slow case back in the hot path to jump here instead.
    intptr_t jmpLocation = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdBranchToSlowCase;
    X86Assembler::repatchBranchOffset(jmpLocation, code);
}
#endif

void JIT::privateCompileGetByIdChain(StructureStubInfo* stubInfo, Structure* structure, StructureChain* chain, size_t count, size_t cachedOffset, void* returnAddress, CallFrame* callFrame)
{
#if USE(CTI_REPATCH_PIC)
    // We don't want to repatch more than once - in future go to cti_op_put_by_id_generic.
    ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_proto_list));

    ASSERT(count);
    
    Vector<JmpSrc> bucketsOfFail;

    // Check eax is an object of the right Structure.
    JmpSrc baseObjectCheck = checkStructure(X86::eax, structure);
    bucketsOfFail.append(baseObjectCheck);

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
        __ cmpl_im(reinterpret_cast<uint32_t>(currStructure), prototypeStructureAddress);
        bucketsOfFail.append(__ jne());
    }
    ASSERT(protoObject);

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    __ movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);
    __ movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::eax);
    JmpSrc success = __ jmp();

    void* code = __ executableCopy(m_codeBlock->executablePool());

    // Use the repatch information to link the failure cases back to the original slow case routine.
    void* slowCaseBegin = reinterpret_cast<char*>(stubInfo->callReturnLocation) - repatchOffsetGetByIdSlowCaseCall;

    for (unsigned i = 0; i < bucketsOfFail.size(); ++i)
        X86Assembler::link(code, bucketsOfFail[i], slowCaseBegin);

    // On success return back to the hot patch code, at a point it will perform the store to dest for us.
    intptr_t successDest = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdPropertyMapOffset;
    X86Assembler::link(code, success, reinterpret_cast<void*>(successDest));

    // Track the stub we have created so that it will be deleted later.
    stubInfo->stubRoutine = code;

    // Finally repatch the jump to slow case back in the hot path to jump here instead.
    intptr_t jmpLocation = reinterpret_cast<intptr_t>(stubInfo->hotPathBegin) + repatchOffsetGetByIdBranchToSlowCase;
    X86Assembler::repatchBranchOffset(jmpLocation, code);
#else
    ASSERT(count);
    
    Vector<JmpSrc> bucketsOfFail;

    // Check eax is an object of the right Structure.
    __ testl_i32r(JSImmediate::TagMask, X86::eax);
    bucketsOfFail.append(__ jne());
    bucketsOfFail.append(checkStructure(X86::eax, structure));

    Structure* currStructure = structure;
    RefPtr<Structure>* chainEntries = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i) {
        protoObject = asObject(currStructure->prototypeForLookup(callFrame));
        currStructure = chainEntries[i].get();

        // Check the prototype object's Structure had not changed.
        Structure** prototypeStructureAddress = &(protoObject->m_structure);
        __ cmpl_im(reinterpret_cast<uint32_t>(currStructure), prototypeStructureAddress);
        bucketsOfFail.append(__ jne());
    }
    ASSERT(protoObject);

    PropertyStorage* protoPropertyStorage = &protoObject->m_propertyStorage;
    __ movl_mr(static_cast<void*>(protoPropertyStorage), X86::edx);
    __ movl_mr(cachedOffset * sizeof(JSValue*), X86::edx, X86::eax);
    __ ret();

    void* code = __ executableCopy(m_codeBlock->executablePool());

    for (unsigned i = 0; i < bucketsOfFail.size(); ++i)
        X86Assembler::link(code, bucketsOfFail[i], reinterpret_cast<void*>(Interpreter::cti_op_get_by_id_proto_fail));

    stubInfo->stubRoutine = code;

    ctiRepatchCallByReturnAddress(returnAddress, code);
#endif
}

void JIT::privateCompilePutByIdReplace(StructureStubInfo* stubInfo, Structure* structure, size_t cachedOffset, void* returnAddress)
{
    // Check eax is an object of the right Structure.
    __ testl_i32r(JSImmediate::TagMask, X86::eax);
    JmpSrc failureCases1 = __ jne();
    JmpSrc failureCases2 = checkStructure(X86::eax, structure);

    // checks out okay! - putDirectOffset
    __ movl_mr(FIELD_OFFSET(JSObject, m_propertyStorage), X86::eax, X86::eax);
    __ movl_rm(X86::edx, cachedOffset * sizeof(JSValue*), X86::eax);
    __ ret();

    void* code = __ executableCopy(m_codeBlock->executablePool());
    
    X86Assembler::link(code, failureCases1, reinterpret_cast<void*>(Interpreter::cti_op_put_by_id_fail));
    X86Assembler::link(code, failureCases2, reinterpret_cast<void*>(Interpreter::cti_op_put_by_id_fail));

    stubInfo->stubRoutine = code;
    
    ctiRepatchCallByReturnAddress(returnAddress, code);
}

#endif

} // namespace JSC

#endif // ENABLE(JIT)
