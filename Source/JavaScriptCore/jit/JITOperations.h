/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#include "JITMathICForwards.h"
#include "SlowPathReturnType.h"
#include <wtf/Platform.h>
#include <wtf/text/UniquedStringImpl.h>

namespace JSC {

typedef int64_t EncodedJSValue;
    
class ArrayAllocationProfile;
class ArrayProfile;
class UnaryArithProfile;
class BinaryArithProfile;
class Butterfly;
class CallFrame;
class CallLinkInfo;
class CodeBlock;
class JSArray;
class JSCell;
class JSFunction;
class JSGlobalObject;
class JSLexicalEnvironment;
class JSObject;
class JSScope;
class JSString;
class JSValue;
class RegExp;
class RegExpObject;
class Register;
class Structure;
class StructureStubInfo;
class Symbol;
class SymbolTable;
class VM;
class WatchpointSet;

struct ByValInfo;
struct ECMAMode;
struct InlineCallFrame;
struct Instruction;

extern "C" {

typedef char* UnusedPtr;

// These typedefs provide typechecking when generating calls out to helper routines;
// this helps prevent calling a helper routine with the wrong arguments!
/*
    Key:
    A: JSArray*
    Aap: ArrayAllocationProfile*
    Ap: ArrayProfile*
    Arp: BinaryArithProfile*
    B: Butterfly*
    By: ByValInfo*
    C: JSCell*
    Cb: CodeBlock*
    Cli: CallLinkInfo*
    D: double
    F: CallFrame*
    G: JSGlobalObject*
    I: UniquedStringImpl*
    Icf: InlineCallFrame*
    Idc: const Identifier*
    J: EncodedJSValue
    Mic: JITMathIC* (can be JITAddIC*, JITMulIC*, etc).
    Jcp: const JSValue*
    Jsc: JSScope*
    Jsf: JSFunction*
    Jss: JSString*
    L: JSLexicalEnvironment*
    O: JSObject*
    P: pointer (char*)
    Pc: Instruction* i.e. bytecode PC
    Q: int64_t
    R: Register
    Re: RegExp*
    Reo: RegExpObject*
    S: size_t
    Sprt: SlowPathReturnType
    Ssi: StructureStubInfo*
    St: Structure*
    Symtab: SymbolTable*
    Sym: Symbol*
    T: StringImpl*
    V: void
    Vm: VM*
    Ws: WatchpointSet*
    Z: int32_t
    Ui: uint32_t
*/

using J_JITOperation_GJMic = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue, void*);
using J_JITOperation_GP = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, void*);
using J_JITOperation_GPP = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, void*, void*);
using J_JITOperation_GPPP = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, void*, void*, void*);
using J_JITOperation_GJJ = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue, EncodedJSValue);
using J_JITOperation_GJJMic = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue, EncodedJSValue, void*);
using Z_JITOperation_GJZZ = int32_t(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue, int32_t, int32_t);
using F_JITOperation_GFJZZ = CallFrame*(JIT_OPERATION *)(JSGlobalObject*, CallFrame*, EncodedJSValue, int32_t, int32_t);
using Sprt_JITOperation_EGCli = SlowPathReturnType(JIT_OPERATION *)(CallFrame*, JSGlobalObject*, CallLinkInfo*);
using V_JITOperation_Cb = void(JIT_OPERATION *)(CodeBlock*);
using Z_JITOperation_G = int32_t (JIT_OPERATION *)(JSGlobalObject*);
using P_JITOperation_VmStZB = char*(JIT_OPERATION *)(VM*, Structure*, int32_t, Butterfly*);
using P_JITOperation_GStZB = char*(JIT_OPERATION *)(JSGlobalObject*, Structure*, int32_t, Butterfly*);
using J_JITOperation_GJ = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue);
using J_JITOperation_GJI = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue, UniquedStringImpl*);
using V_JITOperation_GSsiJJC = void(JIT_OPERATION *)(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, EncodedJSValue, uintptr_t);
using C_JITOperation_TT = uintptr_t(JIT_OPERATION *)(StringImpl*, StringImpl*);
using C_JITOperation_B_GJssJss = uintptr_t(JIT_OPERATION *)(JSGlobalObject*, JSString*, JSString*);
using S_JITOperation_GJJ = size_t(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue, EncodedJSValue);
using V_JITOperation_GJJJ = void(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue);
using J_JITOperation_GSsiJJ = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, EncodedJSValue);
using J_JITOperation_GSsiJJI = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, EncodedJSValue, UniquedStringImpl*);
using V_JITOperation_GCCJ = void(JIT_OPERATION *)(JSGlobalObject*, JSCell*, JSCell*, EncodedJSValue);
using J_JITOperation_GSsiJI = EncodedJSValue(JIT_OPERATION *)(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, UniquedStringImpl*);
using Z_JITOperation_GJZZ = int32_t(JIT_OPERATION *)(JSGlobalObject*, EncodedJSValue, int32_t, int32_t);
using F_JITOperation_GFJZZ = CallFrame*(JIT_OPERATION *)(JSGlobalObject*, CallFrame*, EncodedJSValue, int32_t, int32_t);
using D_JITOperation_DD = double(JIT_OPERATION *)(double, double);
using D_JITOperation_D = double(JIT_OPERATION *)(double);

// This method is used to lookup an exception hander, keyed by faultLocation, which is
// the return location from one of the calls out to one of the helper operations above.

void JIT_OPERATION operationLookupExceptionHandler(VM*) WTF_INTERNAL;
void JIT_OPERATION operationLookupExceptionHandlerFromCallerFrame(VM*) WTF_INTERNAL;
void JIT_OPERATION operationVMHandleException(VM*) WTF_INTERNAL;
void JIT_OPERATION operationThrowStackOverflowErrorFromThunk(JSGlobalObject*) WTF_INTERNAL;
void JIT_OPERATION operationThrowIteratorResultIsNotObject(JSGlobalObject*) WTF_INTERNAL;

void JIT_OPERATION operationThrowStackOverflowError(CodeBlock*) WTF_INTERNAL;
int32_t JIT_OPERATION operationCallArityCheck(JSGlobalObject*) WTF_INTERNAL;
int32_t JIT_OPERATION operationConstructArityCheck(JSGlobalObject*) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationTryGetById(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationTryGetByIdGeneric(JSGlobalObject*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationTryGetByIdOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetById(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdGeneric(JSGlobalObject*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdWithThis(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdWithThisGeneric(JSGlobalObject*, EncodedJSValue, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdWithThisOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdDirect(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdDirectGeneric(JSGlobalObject*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByIdDirectOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, uintptr_t) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationInById(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInByIdGeneric(JSGlobalObject*, EncodedJSValue, uintptr_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInByIdOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue, uintptr_t) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationInByVal(JSGlobalObject*, JSCell*, EncodedJSValue) WTF_INTERNAL;

void JIT_OPERATION operationPutByIdStrict(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdNonStrict(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectStrict(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectNonStrict(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdStrictOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdNonStrictOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectStrictOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDirectNonStrictOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDefinePrivateFieldStrict(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdDefinePrivateFieldStrictOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdPutPrivateFieldStrict(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdPutPrivateFieldStrictOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t) WTF_INTERNAL;

void JIT_OPERATION operationPutByValOptimize(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, ByValInfo*, ECMAMode) WTF_INTERNAL;
void JIT_OPERATION operationDirectPutByValOptimize(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, ByValInfo*, ECMAMode) WTF_INTERNAL;
void JIT_OPERATION operationPutByValGeneric(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, ByValInfo*, ECMAMode) WTF_INTERNAL;
void JIT_OPERATION operationDirectPutByValGeneric(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, ByValInfo*, ECMAMode) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationCallEval(JSGlobalObject*, CallFrame*, ECMAMode) WTF_INTERNAL;
SlowPathReturnType JIT_OPERATION operationLinkCall(CallFrame*, JSGlobalObject*, CallLinkInfo*) WTF_INTERNAL;
SlowPathReturnType JIT_OPERATION operationLinkPolymorphicCall(CallFrame*, JSGlobalObject*, CallLinkInfo*) WTF_INTERNAL;
SlowPathReturnType JIT_OPERATION operationVirtualCall(CallFrame*, JSGlobalObject*, CallLinkInfo*) WTF_INTERNAL;

size_t JIT_OPERATION operationCompareLess(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareLessEq(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareGreater(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareGreaterEq(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareEq(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareStrictEq(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
#if USE(JSVALUE64)
EncodedJSValue JIT_OPERATION operationCompareStringEq(JSGlobalObject*, JSCell* left, JSCell* right) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareEqHeapBigIntToInt32(JSGlobalObject*, JSCell* heapBigInt, int32_t smallInt) WTF_INTERNAL;
#else
size_t JIT_OPERATION operationCompareStringEq(JSGlobalObject*, JSCell* left, JSCell* right) WTF_INTERNAL;
#endif
EncodedJSValue JIT_OPERATION operationNewArrayWithProfile(JSGlobalObject*, ArrayAllocationProfile*, const JSValue* values, int32_t size) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewArrayWithSizeAndProfile(JSGlobalObject*, ArrayAllocationProfile*, EncodedJSValue size) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewFunction(VM*, JSScope*, JSCell*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewFunctionWithInvalidatedReallocationWatchpoint(VM*, JSScope*, JSCell*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewGeneratorFunction(VM*, JSScope*, JSCell*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint(VM*, JSScope*, JSCell*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewAsyncFunction(VM*, JSScope*, JSCell*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewAsyncFunctionWithInvalidatedReallocationWatchpoint(VM*, JSScope*, JSCell*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewAsyncGeneratorFunction(VM*, JSScope*, JSCell*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationNewAsyncGeneratorFunctionWithInvalidatedReallocationWatchpoint(VM*, JSScope*, JSCell*) WTF_INTERNAL;
void JIT_OPERATION operationSetFunctionName(JSGlobalObject*, JSCell*, EncodedJSValue) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewObject(VM*, Structure*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewPromise(VM*, Structure*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewInternalPromise(VM*, Structure*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewGenerator(VM*, Structure*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewAsyncGenerator(VM*, Structure*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewRegexp(JSGlobalObject*, JSCell*) WTF_INTERNAL;
UnusedPtr JIT_OPERATION operationHandleTraps(JSGlobalObject*) WTF_INTERNAL;
void JIT_OPERATION operationThrow(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationDebug(VM*, int32_t) WTF_INTERNAL;
#if ENABLE(DFG_JIT)
SlowPathReturnType JIT_OPERATION operationOptimize(VM*, uint32_t) WTF_INTERNAL;
char* JIT_OPERATION operationTryOSREnterAtCatch(VM*, uint32_t) WTF_INTERNAL;
char* JIT_OPERATION operationTryOSREnterAtCatchAndValueProfile(VM*, uint32_t) WTF_INTERNAL;
#endif
void JIT_OPERATION operationPutByIndex(JSGlobalObject*, EncodedJSValue, int32_t, EncodedJSValue);
void JIT_OPERATION operationPutGetterById(JSGlobalObject*, JSCell*, UniquedStringImpl*, int32_t options, JSCell*) WTF_INTERNAL;
void JIT_OPERATION operationPutSetterById(JSGlobalObject*, JSCell*, UniquedStringImpl*, int32_t options, JSCell*) WTF_INTERNAL;
void JIT_OPERATION operationPutGetterByVal(JSGlobalObject*, JSCell*, EncodedJSValue, int32_t attribute, JSCell*) WTF_INTERNAL;
void JIT_OPERATION operationPutSetterByVal(JSGlobalObject*, JSCell*, EncodedJSValue, int32_t attribute, JSCell*) WTF_INTERNAL;
#if USE(JSVALUE64)
void JIT_OPERATION operationPutGetterSetter(JSGlobalObject*, JSCell*, UniquedStringImpl*, int32_t attribute, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
#else
void JIT_OPERATION operationPutGetterSetter(JSGlobalObject*, JSCell*, UniquedStringImpl*, int32_t attribute, JSCell*, JSCell*) WTF_INTERNAL;
#endif
void JIT_OPERATION operationPushFunctionNameScope(JSGlobalObject*, int32_t, SymbolTable*, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationPopScope(JSGlobalObject*, int32_t) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationGetByValOptimize(JSGlobalObject*, StructureStubInfo*, ArrayProfile*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValGeneric(JSGlobalObject*, StructureStubInfo*, ArrayProfile*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByVal(JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationHasIndexedPropertyDefault(JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationHasIndexedPropertyGeneric(JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo*) WTF_INTERNAL;
size_t JIT_OPERATION operationDeleteByIdOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue base, uintptr_t, ECMAMode) WTF_INTERNAL;
size_t JIT_OPERATION operationDeleteByIdGeneric(JSGlobalObject*, StructureStubInfo*, EncodedJSValue base, uintptr_t, ECMAMode) WTF_INTERNAL;
size_t JIT_OPERATION operationDeleteByValOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue base, EncodedJSValue target, ECMAMode) WTF_INTERNAL;
size_t JIT_OPERATION operationDeleteByValGeneric(JSGlobalObject*, StructureStubInfo*, EncodedJSValue base, EncodedJSValue target, ECMAMode) WTF_INTERNAL;
JSCell* JIT_OPERATION operationPushWithScope(JSGlobalObject*, JSCell* currentScopeCell, EncodedJSValue object) WTF_INTERNAL;
JSCell* JIT_OPERATION operationPushWithScopeObject(JSGlobalObject* globalObject, JSCell* currentScopeCell, JSObject* object) WTF_INTERNAL;
JSCell* JIT_OPERATION operationGetPNames(JSGlobalObject*, JSObject*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInstanceOf(JSGlobalObject*, EncodedJSValue value, EncodedJSValue proto) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInstanceOfGeneric(JSGlobalObject*, StructureStubInfo*, EncodedJSValue value, EncodedJSValue proto) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInstanceOfOptimize(JSGlobalObject*, StructureStubInfo*, EncodedJSValue value, EncodedJSValue proto) WTF_INTERNAL;
int32_t JIT_OPERATION operationSizeFrameForForwardArguments(JSGlobalObject*, EncodedJSValue arguments, int32_t numUsedStackSlots, int32_t firstVarArgOffset) WTF_INTERNAL;
int32_t JIT_OPERATION operationSizeFrameForVarargs(JSGlobalObject*, EncodedJSValue arguments, int32_t numUsedStackSlots, int32_t firstVarArgOffset) WTF_INTERNAL;
CallFrame* JIT_OPERATION operationSetupForwardArgumentsFrame(JSGlobalObject*, CallFrame*, EncodedJSValue, int32_t, int32_t length) WTF_INTERNAL;
CallFrame* JIT_OPERATION operationSetupVarargsFrame(JSGlobalObject*, CallFrame*, EncodedJSValue arguments, int32_t firstVarArgOffset, int32_t length) WTF_INTERNAL;

char* JIT_OPERATION operationSwitchCharWithUnknownKeyType(JSGlobalObject*, EncodedJSValue key, size_t tableIndex) WTF_INTERNAL;
char* JIT_OPERATION operationSwitchImmWithUnknownKeyType(VM*, EncodedJSValue key, size_t tableIndex) WTF_INTERNAL;
char* JIT_OPERATION operationSwitchStringWithUnknownKeyType(JSGlobalObject*, EncodedJSValue key, size_t tableIndex) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetFromScope(JSGlobalObject*, const Instruction* bytecodePC) WTF_INTERNAL;
void JIT_OPERATION operationPutToScope(JSGlobalObject*, const Instruction* bytecodePC) WTF_INTERNAL;

char* JIT_OPERATION operationReallocateButterflyToHavePropertyStorageWithInitialCapacity(VM*, JSObject*) WTF_INTERNAL;
char* JIT_OPERATION operationReallocateButterflyToGrowPropertyStorage(VM*, JSObject*, size_t newSize) WTF_INTERNAL;

void JIT_OPERATION operationWriteBarrierSlowPath(VM*, JSCell*);
void JIT_OPERATION operationOSRWriteBarrier(VM*, JSCell*);

void JIT_OPERATION operationExceptionFuzz(JSGlobalObject*);
void JIT_OPERATION operationExceptionFuzzWithCallFrame(VM*);

int32_t JIT_OPERATION operationCheckIfExceptionIsUncatchableAndNotifyProfiler(VM*);
size_t JIT_OPERATION operationInstanceOfCustom(JSGlobalObject*, EncodedJSValue encodedValue, JSObject* constructor, EncodedJSValue encodedHasInstance) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationValueAdd(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueAddProfiled(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueAddProfiledOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueAddProfiledNoOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueAddOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueAddNoOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueMul(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueMulOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueMulNoOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueMulProfiledOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueMulProfiledNoOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueMulProfiled(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArithNegate(JSGlobalObject*, EncodedJSValue operand);
EncodedJSValue JIT_OPERATION operationArithNegateProfiled(JSGlobalObject*, EncodedJSValue operand, UnaryArithProfile*);
EncodedJSValue JIT_OPERATION operationArithNegateProfiledOptimize(JSGlobalObject*, EncodedJSValue encodedOperand, JITNegIC*);
EncodedJSValue JIT_OPERATION operationArithNegateOptimize(JSGlobalObject*, EncodedJSValue encodedOperand, JITNegIC*);
EncodedJSValue JIT_OPERATION operationValueSub(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueSubProfiled(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueSubOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueSubNoOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueSubProfiledOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueSubProfiledNoOptimize(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*) WTF_INTERNAL;

void JIT_OPERATION operationProcessTypeProfilerLog(VM*) WTF_INTERNAL;
void JIT_OPERATION operationProcessShadowChickenLog(VM*) WTF_INTERNAL;

} // extern "C"

} // namespace JSC

#endif // ENABLE(JIT)
