/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JITOperations_h
#define JITOperations_h

#if ENABLE(JIT)

#include "CallFrame.h"
#include "CommonSlowPaths.h"
#include "JITExceptions.h"
#include "JSArray.h"
#include "JSCJSValue.h"
#include "MacroAssembler.h"
#include "PutKind.h"
#include "StructureStubInfo.h"
#include "VariableWatchpointSet.h"

namespace JSC {

class ArrayAllocationProfile;

#if CALLING_CONVENTION_IS_STDCALL
#define JIT_OPERATION CDECL
#else
#define JIT_OPERATION
#endif

extern "C" {

// These typedefs provide typechecking when generating calls out to helper routines;
// this helps prevent calling a helper routine with the wrong arguments!
/*
    Key:
    A: JSArray*
    Aap: ArrayAllocationProfile*
    C: JSCell*
    Cb: CodeBlock*
    D: double
    E: ExecState*
    F: CallFrame*
    I: StringImpl*
    Icf: InlineCalLFrame*
    Idc: const Identifier*
    J: EncodedJSValue
    Jcp: const JSValue*
    Jsa: JSActivation*
    Jss: JSString*
    O: JSObject*
    P: pointer (char*)
    Pc: Instruction* i.e. bytecode PC
    R: Register
    S: size_t
    Sprt: SlowPathReturnType
    Ssi: StructureStubInfo*
    St: Structure*
    V: void
    Vm: VM*
    Vws: VariableWatchpointSet*
    Z: int32_t
*/

typedef CallFrame* JIT_OPERATION (*F_JITOperation_EFJJ)(ExecState*, CallFrame*, EncodedJSValue, EncodedJSValue);
typedef CallFrame* JIT_OPERATION (*F_JITOperation_EJZ)(ExecState*, EncodedJSValue, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_E)(ExecState*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EA)(ExecState*, JSArray*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EAZ)(ExecState*, JSArray*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EAapJ)(ExecState*, ArrayAllocationProfile*, EncodedJSValue);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EAapJcpZ)(ExecState*, ArrayAllocationProfile*, const JSValue*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EC)(ExecState*, JSCell*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ECI)(ExecState*, JSCell*, StringImpl*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ECJ)(ExecState*, JSCell*, EncodedJSValue);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EDA)(ExecState*, double, JSArray*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EE)(ExecState*, ExecState*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EI)(ExecState*, StringImpl*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJA)(ExecState*, EncodedJSValue, JSArray*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJIdc)(ExecState*, EncodedJSValue, const Identifier*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJJ)(ExecState*, EncodedJSValue, EncodedJSValue);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJssZ)(ExecState*, JSString*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EJP)(ExecState*, EncodedJSValue, void*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EP)(ExecState*, void*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EPP)(ExecState*, void*, void*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EPS)(ExecState*, void*, size_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EPc)(ExecState*, Instruction*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ESS)(ExecState*, size_t, size_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ESsiCI)(ExecState*, StructureStubInfo*, JSCell*, StringImpl*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_ESsiJI)(ExecState*, StructureStubInfo*, EncodedJSValue, StringImpl*);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EZ)(ExecState*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EZIcfZ)(ExecState*, int32_t, InlineCallFrame*, int32_t);
typedef EncodedJSValue JIT_OPERATION (*J_JITOperation_EZZ)(ExecState*, int32_t, int32_t);
typedef JSCell* JIT_OPERATION (*C_JITOperation_E)(ExecState*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EZ)(ExecState*, int32_t);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EC)(ExecState*, JSCell*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EIcf)(ExecState*, InlineCallFrame*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EJssSt)(ExecState*, JSString*, Structure*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EJssJss)(ExecState*, JSString*, JSString*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EJssJssJss)(ExecState*, JSString*, JSString*, JSString*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EO)(ExecState*, JSObject*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EOZ)(ExecState*, JSObject*, int32_t);
typedef JSCell* JIT_OPERATION (*C_JITOperation_ESt)(ExecState*, Structure*);
typedef JSCell* JIT_OPERATION (*C_JITOperation_EZ)(ExecState*, int32_t);
typedef double JIT_OPERATION (*D_JITOperation_D)(double);
typedef double JIT_OPERATION (*D_JITOperation_DD)(double, double);
typedef double JIT_OPERATION (*D_JITOperation_ZZ)(int32_t, int32_t);
typedef double JIT_OPERATION (*D_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef int32_t JIT_OPERATION (*Z_JITOperation_D)(double);
typedef int32_t JIT_OPERATION (*Z_JITOperation_E)(ExecState*);
typedef size_t JIT_OPERATION (*S_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef size_t JIT_OPERATION (*S_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef size_t JIT_OPERATION (*S_JITOperation_EJJ)(ExecState*, EncodedJSValue, EncodedJSValue);
typedef size_t JIT_OPERATION (*S_JITOperation_EOJss)(ExecState*, JSObject*, JSString*);
typedef size_t JIT_OPERATION (*S_JITOperation_J)(EncodedJSValue);
typedef SlowPathReturnType JIT_OPERATION (*Sprt_JITOperation_EZ)(ExecState*, int32_t);
typedef void JIT_OPERATION (*V_JITOperation_E)(ExecState*);
typedef void JIT_OPERATION (*V_JITOperation_EC)(ExecState*, JSCell*);
typedef void JIT_OPERATION (*V_JITOperation_ECb)(ExecState*, CodeBlock*);
typedef void JIT_OPERATION (*V_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef void JIT_OPERATION (*V_JITOperation_ECIcf)(ExecState*, JSCell*, InlineCallFrame*);
typedef void JIT_OPERATION (*V_JITOperation_ECICC)(ExecState*, JSCell*, Identifier*, JSCell*, JSCell*);
typedef void JIT_OPERATION (*V_JITOperation_ECCIcf)(ExecState*, JSCell*, JSCell*, InlineCallFrame*);
typedef void JIT_OPERATION (*V_JITOperation_ECJJ)(ExecState*, JSCell*, EncodedJSValue, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_ECPSPS)(ExecState*, JSCell*, void*, size_t, void*, size_t);
typedef void JIT_OPERATION (*V_JITOperation_ECZ)(ExecState*, JSCell*, int32_t);
typedef void JIT_OPERATION (*V_JITOperation_ECC)(ExecState*, JSCell*, JSCell*);
typedef void JIT_OPERATION (*V_JITOperation_EIdJZ)(ExecState*, Identifier*, EncodedJSValue, int32_t);
typedef void JIT_OPERATION (*V_JITOperation_EJ)(ExecState*, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_EJCI)(ExecState*, EncodedJSValue, JSCell*, StringImpl*);
typedef void JIT_OPERATION (*V_JITOperation_EJIdJJ)(ExecState*, EncodedJSValue, Identifier*, EncodedJSValue, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_EJJJ)(ExecState*, EncodedJSValue, EncodedJSValue, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_EJPP)(ExecState*, EncodedJSValue, void*, void*);
typedef void JIT_OPERATION (*V_JITOperation_EJZJ)(ExecState*, EncodedJSValue, int32_t, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_EJZ)(ExecState*, EncodedJSValue, int32_t);
typedef void JIT_OPERATION (*V_JITOperation_EOZD)(ExecState*, JSObject*, int32_t, double);
typedef void JIT_OPERATION (*V_JITOperation_EOZJ)(ExecState*, JSObject*, int32_t, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_EPc)(ExecState*, Instruction*);
typedef void JIT_OPERATION (*V_JITOperation_EPZJ)(ExecState*, void*, int32_t, EncodedJSValue);
typedef void JIT_OPERATION (*V_JITOperation_ESsiJJI)(ExecState*, StructureStubInfo*, EncodedJSValue, EncodedJSValue, StringImpl*);
typedef void JIT_OPERATION (*V_JITOperation_EVws)(ExecState*, VariableWatchpointSet*);
typedef void JIT_OPERATION (*V_JITOperation_EZ)(ExecState*, int32_t);
typedef void JIT_OPERATION (*V_JITOperation_EVm)(ExecState*, VM*);
typedef char* JIT_OPERATION (*P_JITOperation_E)(ExecState*);
typedef char* JIT_OPERATION (*P_JITOperation_EC)(ExecState*, JSCell*);
typedef char* JIT_OPERATION (*P_JITOperation_EJS)(ExecState*, EncodedJSValue, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_EO)(ExecState*, JSObject*);
typedef char* JIT_OPERATION (*P_JITOperation_EOS)(ExecState*, JSObject*, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_EOZ)(ExecState*, JSObject*, int32_t);
typedef char* JIT_OPERATION (*P_JITOperation_EPS)(ExecState*, void*, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_ES)(ExecState*, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_ESJss)(ExecState*, size_t, JSString*);
typedef char* JIT_OPERATION (*P_JITOperation_ESt)(ExecState*, Structure*);
typedef char* JIT_OPERATION (*P_JITOperation_EStJ)(ExecState*, Structure*, EncodedJSValue);
typedef char* JIT_OPERATION (*P_JITOperation_EStPS)(ExecState*, Structure*, void*, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_EStSS)(ExecState*, Structure*, size_t, size_t);
typedef char* JIT_OPERATION (*P_JITOperation_EStZ)(ExecState*, Structure*, int32_t);
typedef char* JIT_OPERATION (*P_JITOperation_EZZ)(ExecState*, int32_t, int32_t);
typedef StringImpl* JIT_OPERATION (*I_JITOperation_EJss)(ExecState*, JSString*);
typedef JSString* JIT_OPERATION (*Jss_JITOperation_EZ)(ExecState*, int32_t);

// This method is used to lookup an exception hander, keyed by faultLocation, which is
// the return location from one of the calls out to one of the helper operations above.

void JIT_OPERATION lookupExceptionHandler(VM*, ExecState*) WTF_INTERNAL;
void JIT_OPERATION operationVMHandleException(ExecState*) WTF_INTERNAL;

void JIT_OPERATION operationThrowStackOverflowError(ExecState*, CodeBlock*) WTF_INTERNAL;
int32_t JIT_OPERATION operationCallArityCheck(ExecState*) WTF_INTERNAL;
int32_t JIT_OPERATION operationConstructArityCheck(ExecState*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetById(ExecState*, StructureStubInfo*, EncodedJSValue, StringImpl*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdBuildList(ExecState*, StructureStubInfo*, EncodedJSValue, StringImpl*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdOptimize(ExecState*, StructureStubInfo*, EncodedJSValue, StringImpl*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInOptimize(ExecState*, StructureStubInfo*, JSCell*, StringImpl*);
EncodedJSValue JIT_OPERATION operationIn(ExecState*, StructureStubInfo*, JSCell*, StringImpl*);
EncodedJSValue JIT_OPERATION operationGenericIn(ExecState*, JSCell*, EncodedJSValue);
EncodedJSValue JIT_OPERATION operationCallCustomGetter(ExecState*, JSCell*, PropertySlot::GetValueFunc, StringImpl*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationCallGetter(ExecState*, JSCell*, JSCell*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdStrict(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdNonStrict(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectStrict(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectNonStrict(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdStrictOptimize(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdNonStrictOptimize(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectStrictOptimize(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectNonStrictOptimize(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdStrictBuildList(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdNonStrictBuildList(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectStrictBuildList(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectNonStrictBuildList(ExecState*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl*) WTF_INTERNAL;
void JIT_OPERATION operationReallocateStorageAndFinishPut(ExecState*, JSObject*, Structure*, PropertyOffset, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByVal(ExecState*, EncodedJSValue, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationDirectPutByVal(ExecState*, EncodedJSValue, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValGeneric(ExecState*, EncodedJSValue, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationDirectPutByValGeneric(ExecState*, EncodedJSValue, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationCallEval(ExecState*, ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkCall(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkClosureCall(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationVirtualCall(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationVirtualConstruct(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkConstruct(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkCallThatPreservesRegs(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkClosureCallThatPreservesRegs(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationVirtualCallThatPreservesRegs(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationVirtualConstructThatPreservesRegs(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationLinkConstructThatPreservesRegs(ExecState*) WTF_INTERNAL;

size_t JIT_OPERATION operationCompareLess(ExecState*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareLessEq(ExecState*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareGreater(ExecState*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareGreaterEq(ExecState*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationConvertJSValueToBoolean(ExecState*, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareEq(ExecState*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
#if USE(JSVALUE64)
EncodedJSValue JIT_OPERATION operationCompareStringEq(ExecState*, JSCell* left, JSCell* right) WTF_INTERNAL;
#else
size_t JIT_OPERATION operationCompareStringEq(ExecState*, JSCell* left, JSCell* right) WTF_INTERNAL;
#endif
size_t JIT_OPERATION operationHasProperty(ExecState*, JSObject*, JSString*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewArrayWithProfile(ExecState*, ArrayAllocationProfile*, const JSValue* values, int32_t size) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewArrayBufferWithProfile(ExecState*, ArrayAllocationProfile*, const JSValue* values, int32_t size) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewArrayWithSizeAndProfile(ExecState*, ArrayAllocationProfile*, EncodedJSValue size) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewFunction(ExecState*, JSCell*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewObject(ExecState*, Structure*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewRegexp(ExecState*, void*) WTF_INTERNAL;
void JIT_OPERATION operationHandleWatchdogTimer(ExecState*) WTF_INTERNAL;
void JIT_OPERATION operationThrowStaticError(ExecState*, EncodedJSValue, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationThrow(ExecState*, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationDebug(ExecState*, int32_t) WTF_INTERNAL;
#if ENABLE(DFG_JIT)
SlowPathReturnType JIT_OPERATION operationOptimize(ExecState*, int32_t) WTF_INTERNAL;
#endif
void JIT_OPERATION operationPutByIndex(ExecState*, EncodedJSValue, int32_t, EncodedJSValue);
#if USE(JSVALUE64)
void JIT_OPERATION operationPutGetterSetter(ExecState*, EncodedJSValue, Identifier*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
#else
void JIT_OPERATION operationPutGetterSetter(ExecState*, JSCell*, Identifier*, JSCell*, JSCell*) WTF_INTERNAL;
#endif
void JIT_OPERATION operationPushNameScope(ExecState*, Identifier*, EncodedJSValue, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationPushWithScope(ExecState*, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationPopScope(ExecState*) WTF_INTERNAL;
void JIT_OPERATION operationProfileDidCall(ExecState*, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationProfileWillCall(ExecState*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationCheckHasInstance(ExecState*, EncodedJSValue, EncodedJSValue baseVal) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreateActivation(ExecState*, int32_t offset) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreateArguments(ExecState*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetArgumentsLength(ExecState*, int32_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValDefault(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValGeneric(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValString(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript) WTF_INTERNAL;
void JIT_OPERATION operationTearOffActivation(ExecState*, JSCell*) WTF_INTERNAL;
void JIT_OPERATION operationTearOffArguments(ExecState*, JSCell*, JSCell*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDeleteById(ExecState*, EncodedJSValue base, const Identifier*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationGetPNames(ExecState*, JSObject*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInstanceOf(ExecState*, EncodedJSValue, EncodedJSValue proto) WTF_INTERNAL;
CallFrame* JIT_OPERATION operationSizeFrameForVarargs(ExecState*, EncodedJSValue arguments, int32_t firstFreeRegister) WTF_INTERNAL;
CallFrame* JIT_OPERATION operationLoadVarargs(ExecState*, CallFrame*, EncodedJSValue thisValue, EncodedJSValue arguments) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToObject(ExecState*, EncodedJSValue) WTF_INTERNAL;

char* JIT_OPERATION operationSwitchCharWithUnknownKeyType(ExecState*, EncodedJSValue key, size_t tableIndex) WTF_INTERNAL;
char* JIT_OPERATION operationSwitchImmWithUnknownKeyType(ExecState*, EncodedJSValue key, size_t tableIndex) WTF_INTERNAL;
char* JIT_OPERATION operationSwitchStringWithUnknownKeyType(ExecState*, EncodedJSValue key, size_t tableIndex) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationResolveScope(ExecState*, int32_t identifierIndex) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetFromScope(ExecState*, Instruction* bytecodePC) WTF_INTERNAL;
void JIT_OPERATION operationPutToScope(ExecState*, Instruction* bytecodePC) WTF_INTERNAL;

void JIT_OPERATION operationFlushWriteBarrierBuffer(ExecState*, JSCell*);
void JIT_OPERATION operationWriteBarrier(ExecState*, JSCell*, JSCell*);
void JIT_OPERATION operationUnconditionalWriteBarrier(ExecState*, JSCell*);
void JIT_OPERATION operationOSRWriteBarrier(ExecState*, JSCell*);

void JIT_OPERATION operationInitGlobalConst(ExecState*, Instruction*);

} // extern "C"

inline P_JITOperation_E operationLinkFor(
    CodeSpecializationKind kind, RegisterPreservationMode registers)
{
    switch (kind) {
    case CodeForCall:
        switch (registers) {
        case RegisterPreservationNotRequired:
            return operationLinkCall;
        case MustPreserveRegisters:
            return operationLinkCallThatPreservesRegs;
        }
        break;
    case CodeForConstruct:
        switch (registers) {
        case RegisterPreservationNotRequired:
            return operationLinkConstruct;
        case MustPreserveRegisters:
            return operationLinkConstructThatPreservesRegs;
        }
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

inline P_JITOperation_E operationVirtualFor(
    CodeSpecializationKind kind, RegisterPreservationMode registers)
{
    switch (kind) {
    case CodeForCall:
        switch (registers) {
        case RegisterPreservationNotRequired:
            return operationVirtualCall;
        case MustPreserveRegisters:
            return operationVirtualCallThatPreservesRegs;
        }
        break;
    case CodeForConstruct:
        switch (registers) {
        case RegisterPreservationNotRequired:
            return operationVirtualConstruct;
        case MustPreserveRegisters:
            return operationVirtualConstructThatPreservesRegs;
        }
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

inline P_JITOperation_E operationLinkClosureCallFor(RegisterPreservationMode registers)
{
    switch (registers) {
    case RegisterPreservationNotRequired:
        return operationLinkClosureCall;
    case MustPreserveRegisters:
        return operationLinkClosureCallThatPreservesRegs;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JITOperations_h

