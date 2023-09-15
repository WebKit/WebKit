/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "JITOperationValidation.h"
#include "PrivateFieldPutKind.h"
#include "UGPRPair.h"
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
class JSRemoteFunction;
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

struct ECMAMode;
struct InlineCallFrame;

template<typename> struct BaseInstruction;
struct JSOpcodeTraits;
using JSInstruction = BaseInstruction<JSOpcodeTraits>;

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
    Sprt: UGPRPair
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

using J_JITOperation_GJMic = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, void*);
using J_JITOperation_GP = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, void*);
using J_JITOperation_GPP = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, void*, void*);
using J_JITOperation_GPPP = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, void*, void*, void*);
using J_JITOperation_GJJ = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, EncodedJSValue);
using J_JITOperation_GJJMic = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, EncodedJSValue, void*);
using Z_JITOperation_GJZZ = int32_t(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, int32_t, int32_t);
using F_JITOperation_GFJZZ = CallFrame*(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, CallFrame*, EncodedJSValue, int32_t, int32_t);
using Sprt_JITOperation_EGCli = UGPRPair(JIT_OPERATION_ATTRIBUTES *)(CallFrame*, JSGlobalObject*, CallLinkInfo*);
using V_JITOperation_Cb = void(JIT_OPERATION_ATTRIBUTES *)(CodeBlock*);
using Z_JITOperation_G = int32_t (JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*);
using P_JITOperation_VmStZB = char*(JIT_OPERATION_ATTRIBUTES *)(VM*, Structure*, int32_t, Butterfly*);
using P_JITOperation_GStZB = char*(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, Structure*, int32_t, Butterfly*);
using J_JITOperation_GJ = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue);
using J_JITOperation_GJI = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, UniquedStringImpl*);
using J_JITOperation_GJIP = EncodedJSValue(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, UniquedStringImpl*, void*);
using C_JITOperation_TT = uintptr_t(JIT_OPERATION_ATTRIBUTES *)(StringImpl*, StringImpl*);
using C_JITOperation_B_GJssJss = uintptr_t(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, JSString*, JSString*);
using S_JITOperation_GC = size_t(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, JSCell*);
using S_JITOperation_GCZ = size_t(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, JSCell*, int32_t);
using S_JITOperation_GJJ = size_t(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, EncodedJSValue);
using V_JITOperation_GJJJ = void(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue);
using V_JITOperation_GCCJ = void(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, JSCell*, JSCell*, EncodedJSValue);
using Z_JITOperation_GJZZ = int32_t(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, EncodedJSValue, int32_t, int32_t);
using F_JITOperation_GFJZZ = CallFrame*(JIT_OPERATION_ATTRIBUTES *)(JSGlobalObject*, CallFrame*, EncodedJSValue, int32_t, int32_t);

// This method is used to lookup an exception hander, keyed by faultLocation, which is
// the return location from one of the calls out to one of the helper operations above.

JSC_DECLARE_JIT_OPERATION(operationLookupExceptionHandler, void, (VM*));
JSC_DECLARE_JIT_OPERATION(operationLookupExceptionHandlerFromCallerFrame, void, (VM*));
JSC_DECLARE_JIT_OPERATION(operationVMHandleException, void, (VM*));
JSC_DECLARE_JIT_OPERATION(operationThrowStackOverflowErrorFromThunk, void, (JSGlobalObject*));
JSC_DECLARE_JIT_OPERATION(operationThrowIteratorResultIsNotObject, void, (JSGlobalObject*));
JSC_DECLARE_JIT_OPERATION(operationGetWrappedValueForCaller, EncodedJSValue, (JSRemoteFunction*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationGetWrappedValueForTarget, EncodedJSValue, (JSRemoteFunction*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationMaterializeRemoteFunctionTargetCode, UGPRPair, (JSRemoteFunction*));
JSC_DECLARE_JIT_OPERATION(operationThrowRemoteFunctionException, EncodedJSValue, (JSRemoteFunction*));

JSC_DECLARE_JIT_OPERATION(operationThrowStackOverflowError, void, (CodeBlock*));

// IC related functions and generic helpers.
//
//   1. Optimize suffixed ones are used for a normal fallback, which potentially set up IC.
//   2. Megamorphic suffixed ones are used for a fallback for a megamorphic IC case.
//   3. GaveUp suffixed ones are used for IC site which already gave up optimization.
//   4. Generic is unrelated to IC: used outside of IC, just a generic operation.
//
// So, 1-3 functions take StructureStubInfo* since it is IC slow path. Generic one does not.

JSC_DECLARE_JIT_OPERATION(operationTryGetByIdOptimize, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationTryGetByIdGaveUp, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationTryGetByIdGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, uintptr_t));

JSC_DECLARE_JIT_OPERATION(operationGetByIdOptimize, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetByIdMegamorphic, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetByIdGaveUp, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetByIdGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, uintptr_t));
JSC_DECLARE_JIT_OPERATION(operationGetByIdMegamorphicGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, uintptr_t));

JSC_DECLARE_JIT_OPERATION(operationGetByIdDirectOptimize, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetByIdDirectGaveUp, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetByIdDirectGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, uintptr_t));

JSC_DECLARE_JIT_OPERATION(operationGetByIdWithThisOptimize, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetByIdWithThisMegamorphic, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetByIdWithThisGaveUp, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetByIdWithThisGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, uintptr_t));
JSC_DECLARE_JIT_OPERATION(operationGetByIdWithThisMegamorphicGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, uintptr_t));

JSC_DECLARE_JIT_OPERATION(operationInByIdOptimize, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationInByIdGaveUp, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationInByValOptimize, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationInByValGaveUp, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));

JSC_DECLARE_JIT_OPERATION(operationHasPrivateNameOptimize, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationHasPrivateNameGaveUp, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationHasPrivateBrandOptimize, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationHasPrivateBrandGaveUp, EncodedJSValue, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationPutByIdStrictOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdStrictMegamorphic, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdStrictGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdStrictMegamorphicGeneric, void, (JSGlobalObject*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t));

JSC_DECLARE_JIT_OPERATION(operationPutByIdSloppyOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdSloppyMegamorphic, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdSloppyGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdSloppyMegamorphicGeneric, void, (JSGlobalObject*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, uintptr_t));

JSC_DECLARE_JIT_OPERATION(operationPutByIdDirectStrictOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdDirectStrictGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationPutByIdDirectSloppyOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdDirectSloppyGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationPutByIdDefinePrivateFieldStrictOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdDefinePrivateFieldStrictGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationPutByIdSetPrivateFieldStrictOptimize, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationPutByIdSetPrivateFieldStrictGaveUp, void, (EncodedJSValue encodedValue, EncodedJSValue encodedBase, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationSetPrivateBrandOptimize, void, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationSetPrivateBrandGaveUp, void, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationCheckPrivateBrandOptimize, void, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationCheckPrivateBrandGaveUp, void, (EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationPutByValSloppyOptimize, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValSloppyMegamorphic, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValSloppyGaveUp, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValSloppyGeneric, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValSloppyMegamorphicGeneric, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationPutByValStrictOptimize, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValStrictMegamorphic, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValStrictGaveUp, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValStrictGeneric, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValStrictMegamorphicGeneric, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationDirectPutByValSloppyOptimize, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationDirectPutByValStrictGaveUp, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationDirectPutByValStrictGeneric, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationDirectPutByValStrictOptimize, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationDirectPutByValSloppyGaveUp, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationDirectPutByValSloppyGeneric, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationPutByValDefinePrivateFieldOptimize, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValDefinePrivateFieldGaveUp, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValDefinePrivateFieldGeneric, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationPutByValSetPrivateFieldOptimize, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValSetPrivateFieldGaveUp, void, (EncodedJSValue, EncodedJSValue, EncodedJSValue, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationPutByValSetPrivateFieldGeneric, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationGetByValOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationGetByValMegamorphic, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationGetByValGaveUp, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationGetByValGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty));
JSC_DECLARE_JIT_OPERATION(operationGetByValMegamorphicGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript));

JSC_DECLARE_JIT_OPERATION(operationGetByValWithThisOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, EncodedJSValue encodedThis, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationGetByValWithThisMegamorphic, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, EncodedJSValue encodedThis, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationGetByValWithThisGaveUp, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, EncodedJSValue encodedThis, JSGlobalObject*, StructureStubInfo*, ArrayProfile*));
JSC_DECLARE_JIT_OPERATION(operationGetByValWithThisGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedThis));
JSC_DECLARE_JIT_OPERATION(operationGetByValWithThisMegamorphicGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, EncodedJSValue encodedThis));

JSC_DECLARE_JIT_OPERATION(operationDeleteByIdSloppyOptimize, size_t, (EncodedJSValue base, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationDeleteByIdSloppyGaveUp, size_t, (EncodedJSValue base, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationDeleteByIdSloppyGeneric, size_t, (JSGlobalObject*, EncodedJSValue base, uintptr_t));

JSC_DECLARE_JIT_OPERATION(operationDeleteByIdStrictOptimize, size_t, (EncodedJSValue base, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationDeleteByIdStrictGaveUp, size_t, (EncodedJSValue base, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationDeleteByIdStrictGeneric, size_t, (JSGlobalObject*, EncodedJSValue base, uintptr_t));

JSC_DECLARE_JIT_OPERATION(operationDeleteByValSloppyOptimize, size_t, (EncodedJSValue base, EncodedJSValue target, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationDeleteByValSloppyGaveUp, size_t, (EncodedJSValue base, EncodedJSValue target, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationDeleteByValSloppyGeneric, size_t, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue target));

JSC_DECLARE_JIT_OPERATION(operationDeleteByValStrictOptimize, size_t, (EncodedJSValue base, EncodedJSValue target, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationDeleteByValStrictGaveUp, size_t, (EncodedJSValue base, EncodedJSValue target, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationDeleteByValStrictGeneric, size_t, (JSGlobalObject*, EncodedJSValue base, EncodedJSValue target));

JSC_DECLARE_JIT_OPERATION(operationInstanceOfOptimize, EncodedJSValue, (EncodedJSValue value, EncodedJSValue proto, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationInstanceOfGaveUp, EncodedJSValue, (EncodedJSValue value, EncodedJSValue proto, JSGlobalObject*, StructureStubInfo*));

JSC_DECLARE_JIT_OPERATION(operationGetPrivateNameOptimize, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedFieldName, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetPrivateNameGaveUp, EncodedJSValue, (EncodedJSValue encodedBase, EncodedJSValue encodedFieldName, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetPrivateNameGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedFieldName));

JSC_DECLARE_JIT_OPERATION(operationGetPrivateNameByIdOptimize, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetPrivateNameByIdGaveUp, EncodedJSValue, (EncodedJSValue, JSGlobalObject*, StructureStubInfo*));
JSC_DECLARE_JIT_OPERATION(operationGetPrivateNameByIdGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, uintptr_t));

// End of IC related functions and generic helpers.

JSC_DECLARE_JIT_OPERATION(operationCallDirectEvalSloppy, EncodedJSValue, (void*, JSScope*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCallDirectEvalStrict, EncodedJSValue, (void*, JSScope*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationLinkCall, UGPRPair, (CallFrame*, JSGlobalObject*, CallLinkInfo*));
JSC_DECLARE_JIT_OPERATION(operationLinkPolymorphicCall, UGPRPair, (CallFrame*, JSGlobalObject*, CallLinkInfo*));
JSC_DECLARE_JIT_OPERATION(operationVirtualCall, UGPRPair, (CallFrame*, JSGlobalObject*, CallLinkInfo*));

JSC_DECLARE_JIT_OPERATION(operationCompareLess, size_t, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCompareLessEq, size_t, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCompareGreater, size_t, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCompareGreaterEq, size_t, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCompareEq, size_t, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCompareStrictEq, size_t, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
#if USE(JSVALUE64)
JSC_DECLARE_JIT_OPERATION(operationCompareStringEq, EncodedJSValue, (JSGlobalObject*, JSCell* left, JSCell* right));
JSC_DECLARE_JIT_OPERATION(operationCompareEqHeapBigIntToInt32, size_t, (JSGlobalObject*, JSCell* heapBigInt, int32_t smallInt));
#else
JSC_DECLARE_JIT_OPERATION(operationCompareStringEq, size_t, (JSGlobalObject*, JSCell* left, JSCell* right));
#endif
JSC_DECLARE_JIT_OPERATION(operationNewArrayWithProfile, EncodedJSValue, (JSGlobalObject*, ArrayAllocationProfile*, const JSValue* values, int32_t size));
JSC_DECLARE_JIT_OPERATION(operationNewArrayWithSizeAndProfile, EncodedJSValue, (JSGlobalObject*, ArrayAllocationProfile*, EncodedJSValue size));
JSC_DECLARE_JIT_OPERATION(operationNewFunction, EncodedJSValue, (VM*, JSScope*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationNewFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (VM*, JSScope*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationNewGeneratorFunction, EncodedJSValue, (VM*, JSScope*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (VM*, JSScope*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationNewAsyncFunction, EncodedJSValue, (VM*, JSScope*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationNewAsyncFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (VM*, JSScope*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationNewAsyncGeneratorFunction, EncodedJSValue, (VM*, JSScope*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationNewAsyncGeneratorFunctionWithInvalidatedReallocationWatchpoint, EncodedJSValue, (VM*, JSScope*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationSetFunctionName, void, (JSGlobalObject*, JSCell*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewObject, JSCell*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewPromise, JSCell*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewInternalPromise, JSCell*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewGenerator, JSCell*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewAsyncGenerator, JSCell*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewRegexp, JSCell*, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationHandleTraps, UnusedPtr, (JSGlobalObject*));
JSC_DECLARE_JIT_OPERATION(operationThrow, void, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationDebug, void, (VM*, int32_t));
#if ENABLE(DFG_JIT)
JSC_DECLARE_JIT_OPERATION(operationOptimize, UGPRPair, (VM*, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationTryOSREnterAtCatchAndValueProfile, UGPRPair, (VM*, uint32_t));
#endif
JSC_DECLARE_JIT_OPERATION(operationPutGetterById, void, (JSGlobalObject*, JSCell*, UniquedStringImpl*, int32_t options, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationPutSetterById, void, (JSGlobalObject*, JSCell*, UniquedStringImpl*, int32_t options, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationPutGetterByVal, void, (JSGlobalObject*, JSCell*, EncodedJSValue, int32_t attribute, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationPutSetterByVal, void, (JSGlobalObject*, JSCell*, EncodedJSValue, int32_t attribute, JSCell*));
#if USE(JSVALUE64)
JSC_DECLARE_JIT_OPERATION(operationPutGetterSetter, void, (JSGlobalObject*, JSCell*, UniquedStringImpl*, int32_t attribute, EncodedJSValue, EncodedJSValue));
#else
JSC_DECLARE_JIT_OPERATION(operationPutGetterSetter, void, (JSGlobalObject*, JSCell*, UniquedStringImpl*, int32_t attribute, JSCell*, JSCell*));
#endif

JSC_DECLARE_JIT_OPERATION(operationPushWithScope, JSCell*, (JSGlobalObject*, JSCell* currentScopeCell, EncodedJSValue object));
JSC_DECLARE_JIT_OPERATION(operationPushWithScopeObject, JSCell*, (JSGlobalObject* globalObject, JSCell* currentScopeCell, JSObject* object));
JSC_DECLARE_JIT_OPERATION(operationSizeFrameForForwardArguments, int32_t, (JSGlobalObject*, EncodedJSValue arguments, int32_t numUsedStackSlots, int32_t firstVarArgOffset));
JSC_DECLARE_JIT_OPERATION(operationSizeFrameForVarargs, int32_t, (JSGlobalObject*, EncodedJSValue arguments, int32_t numUsedStackSlots, int32_t firstVarArgOffset));
JSC_DECLARE_JIT_OPERATION(operationSetupForwardArgumentsFrame, CallFrame*, (JSGlobalObject*, CallFrame*, EncodedJSValue, int32_t, int32_t length));
JSC_DECLARE_JIT_OPERATION(operationSetupVarargsFrame, CallFrame*, (JSGlobalObject*, CallFrame*, EncodedJSValue arguments, int32_t firstVarArgOffset, int32_t length));

JSC_DECLARE_JIT_OPERATION(operationSwitchCharWithUnknownKeyType, char*, (JSGlobalObject*, EncodedJSValue key, size_t tableIndex, int32_t min));
JSC_DECLARE_JIT_OPERATION(operationSwitchImmWithUnknownKeyType, char*, (VM*, EncodedJSValue key, size_t tableIndex, int32_t min));
JSC_DECLARE_JIT_OPERATION(operationSwitchStringWithUnknownKeyType, char*, (JSGlobalObject*, EncodedJSValue key, size_t tableIndex));
JSC_DECLARE_JIT_OPERATION(operationResolveScopeForBaseline, EncodedJSValue, (JSGlobalObject*, const JSInstruction* bytecodePC));
JSC_DECLARE_JIT_OPERATION(operationGetFromScope, EncodedJSValue, (JSGlobalObject*, const JSInstruction* bytecodePC));
JSC_DECLARE_JIT_OPERATION(operationPutToScope, void, (JSGlobalObject*, const JSInstruction* bytecodePC));

JSC_DECLARE_JIT_OPERATION(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity, char*, (VM*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationReallocateButterflyToGrowPropertyStorage, char*, (VM*, JSObject*, size_t newSize));

JSC_DECLARE_JIT_OPERATION(operationWriteBarrierSlowPath, void, (VM*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationOSRWriteBarrier, void, (VM*, JSCell*));

JSC_DECLARE_JIT_OPERATION(operationExceptionFuzz, void, (JSGlobalObject*));
JSC_DECLARE_JIT_OPERATION(operationExceptionFuzzWithCallFrame, void, (VM*));

JSC_DECLARE_JIT_OPERATION(operationRetrieveAndClearExceptionIfCatchable, JSCell*, (VM*));
JSC_DECLARE_JIT_OPERATION(operationInstanceOfCustom, size_t, (JSGlobalObject*, EncodedJSValue encodedValue, JSObject* constructor, EncodedJSValue encodedHasInstance));

JSC_DECLARE_JIT_OPERATION(operationValueAdd, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueAddProfiled, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile*));
JSC_DECLARE_JIT_OPERATION(operationValueAddProfiledOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*));
JSC_DECLARE_JIT_OPERATION(operationValueAddProfiledNoOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*));
JSC_DECLARE_JIT_OPERATION(operationValueAddOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*));
JSC_DECLARE_JIT_OPERATION(operationValueAddNoOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITAddIC*));
JSC_DECLARE_JIT_OPERATION(operationValueMul, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueMulOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*));
JSC_DECLARE_JIT_OPERATION(operationValueMulNoOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*));
JSC_DECLARE_JIT_OPERATION(operationValueMulProfiledOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*));
JSC_DECLARE_JIT_OPERATION(operationValueMulProfiledNoOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITMulIC*));
JSC_DECLARE_JIT_OPERATION(operationValueMulProfiled, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile*));
JSC_DECLARE_JIT_OPERATION(operationArithNegate, EncodedJSValue, (JSGlobalObject*, EncodedJSValue operand));
JSC_DECLARE_JIT_OPERATION(operationArithNegateProfiled, EncodedJSValue, (JSGlobalObject*, EncodedJSValue operand, UnaryArithProfile*));
JSC_DECLARE_JIT_OPERATION(operationArithNegateProfiledOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOperand, JITNegIC*));
JSC_DECLARE_JIT_OPERATION(operationArithNegateOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOperand, JITNegIC*));
JSC_DECLARE_JIT_OPERATION(operationValueSub, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueSubProfiled, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, BinaryArithProfile*));
JSC_DECLARE_JIT_OPERATION(operationValueSubOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*));
JSC_DECLARE_JIT_OPERATION(operationValueSubNoOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*));
JSC_DECLARE_JIT_OPERATION(operationValueSubProfiledOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*));
JSC_DECLARE_JIT_OPERATION(operationValueSubProfiledNoOptimize, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2, JITSubIC*));

JSC_DECLARE_JIT_OPERATION(operationDebuggerWillCallNativeExecutable, void, (CallFrame*));

JSC_DECLARE_JIT_OPERATION(operationProcessTypeProfilerLog, void, (VM*));
JSC_DECLARE_JIT_OPERATION(operationProcessShadowChickenLog, void, (VM*));

} // namespace JSC

#endif // ENABLE(JIT)
