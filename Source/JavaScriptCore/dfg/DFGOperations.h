/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGArithMode.h"
#include "JITOperations.h"
#include "TypedArrayType.h"

namespace JSC {

class DateInstance;
class JSBigInt;

namespace DFG {

struct OSRExitBase;

extern "C" {

JSCell* JIT_OPERATION operationStringFromCharCode(JSGlobalObject*, int32_t)  WTF_INTERNAL; 
EncodedJSValue JIT_OPERATION operationStringFromCharCodeUntyped(JSGlobalObject*, EncodedJSValue)  WTF_INTERNAL;

// These routines provide callbacks out to C++ implementations of operations too complex to JIT.
JSCell* JIT_OPERATION operationCallObjectConstructor(JSGlobalObject*, EncodedJSValue encodedTarget) WTF_INTERNAL;
JSCell* JIT_OPERATION operationToObject(JSGlobalObject*, EncodedJSValue encodedTarget, UniquedStringImpl*) WTF_INTERNAL;
JSArray* JIT_OPERATION operationObjectKeys(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
JSArray* JIT_OPERATION operationObjectKeysObject(JSGlobalObject*, JSObject*) WTF_INTERNAL;
JSArray* JIT_OPERATION operationObjectGetOwnPropertyNames(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
JSArray* JIT_OPERATION operationObjectGetOwnPropertyNamesObject(JSGlobalObject*, JSObject*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationObjectCreate(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
JSCell* JIT_OPERATION operationObjectCreateObject(JSGlobalObject*, JSObject*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreateThis(JSGlobalObject*, JSObject* constructor, uint32_t inlineCapacity) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreatePromise(JSGlobalObject*, JSObject* constructor) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreateInternalPromise(JSGlobalObject*, JSObject* constructor) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreateGenerator(JSGlobalObject*, JSObject* constructor) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreateAsyncGenerator(JSGlobalObject*, JSObject* constructor) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToThis(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToThisStrict(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueMod(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueBitNot(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueBitAnd(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueBitOr(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueBitXor(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueBitLShift(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueBitRShift(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueBitURShift(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueAddNotNumber(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueDiv(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValuePow(JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationInc(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDec(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
double JIT_OPERATION operationArithAbs(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
UCPUStrictInt32 JIT_OPERATION operationArithClz32(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
double JIT_OPERATION operationArithFRound(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
double JIT_OPERATION operationArithSqrt(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;

#define DFG_ARITH_UNARY(capitalizedName, lowerName) \
double JIT_OPERATION operationArith##capitalizedName(JSGlobalObject*, EncodedJSValue encodedOp1) WTF_INTERNAL;
    FOR_EACH_DFG_ARITH_UNARY_OP(DFG_ARITH_UNARY)
#undef DFG_ARITH_UNARY

EncodedJSValue JIT_OPERATION operationArithRound(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArithFloor(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArithCeil(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArithTrunc(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValCell(JSGlobalObject*, JSCell*, EncodedJSValue encodedProperty) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValObjectInt(JSGlobalObject*, JSObject*, int32_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValStringInt(JSGlobalObject*, JSString*, int32_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValObjectString(JSGlobalObject*, JSCell*, JSCell* string) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValObjectSymbol(JSGlobalObject*, JSCell*, JSCell* symbol) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToPrimitive(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToPropertyKey(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToNumber(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToNumeric(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationCallNumberConstructor(JSGlobalObject*, EncodedJSValue);
EncodedJSValue JIT_OPERATION operationGetByValWithThis(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetPrototypeOf(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetPrototypeOfObject(JSGlobalObject*, JSObject*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationHasGenericProperty(JSGlobalObject*, EncodedJSValue, JSCell*);
EncodedJSValue JIT_OPERATION operationHasOwnStructureProperty(JSGlobalObject*, JSCell*, JSString*);
EncodedJSValue JIT_OPERATION operationInStructureProperty(JSGlobalObject*, JSCell*, JSString*);
size_t JIT_OPERATION operationHasIndexedPropertyByInt(JSGlobalObject*, JSCell*, int32_t, int32_t);
JSCell* JIT_OPERATION operationGetPropertyEnumerator(JSGlobalObject*, EncodedJSValue);
JSCell* JIT_OPERATION operationGetPropertyEnumeratorCell(JSGlobalObject*, JSCell*);
JSCell* JIT_OPERATION operationToIndexString(JSGlobalObject*, int32_t);
JSCell* JIT_OPERATION operationNewRegexpWithLastIndex(JSGlobalObject*, JSCell*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewArray(JSGlobalObject*, Structure*, void*, size_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewEmptyArray(VM*, Structure*) WTF_INTERNAL;
char* JIT_OPERATION operationNewArrayWithSize(JSGlobalObject*, Structure*, int32_t, Butterfly*) WTF_INTERNAL;
char* JIT_OPERATION operationNewArrayWithSizeAndHint(JSGlobalObject*, Structure*, int32_t, int32_t, Butterfly*) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt8ArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt8ArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt16ArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt16ArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt32ArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt32ArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint8ArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint8ArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint8ClampedArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint8ClampedArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint16ArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint16ArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint32ArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint32ArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewFloat32ArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewFloat32ArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewFloat64ArrayWithSize(JSGlobalObject*, Structure*, int32_t, char*) WTF_INTERNAL;
char* JIT_OPERATION operationNewFloat64ArrayWithOneArgument(JSGlobalObject*, Structure*, EncodedJSValue) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewArrayIterator(VM*, Structure*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewMapIterator(VM*, Structure*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewSetIterator(VM*, Structure*) WTF_INTERNAL;

void JIT_OPERATION operationPutByValStrict(JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValNonStrict(JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValCellStrict(JSGlobalObject*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValCellNonStrict(JSGlobalObject*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValCellStringStrict(JSGlobalObject*, JSCell*, JSCell* string, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValCellStringNonStrict(JSGlobalObject*, JSCell*, JSCell* string, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValCellSymbolStrict(JSGlobalObject*, JSCell*, JSCell* symbol, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValCellSymbolNonStrict(JSGlobalObject*, JSCell*, JSCell* symbol, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValBeyondArrayBoundsStrict(JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValBeyondArrayBoundsNonStrict(JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsNonStrict(JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectStrict(JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectNonStrict(JSGlobalObject*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectCellStrict(JSGlobalObject*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectCellNonStrict(JSGlobalObject*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectCellStringStrict(JSGlobalObject*, JSCell*, JSCell* string, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectCellStringNonStrict(JSGlobalObject*, JSCell*, JSCell* string, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectCellSymbolStrict(JSGlobalObject*, JSCell*, JSCell* symbol, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectCellSymbolNonStrict(JSGlobalObject*, JSCell*, JSCell* symbol, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsStrict(JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsNonStrict(JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutDoubleByValBeyondArrayBoundsStrict(JSGlobalObject*, JSObject*, int32_t index, double value) WTF_INTERNAL;
void JIT_OPERATION operationPutDoubleByValBeyondArrayBoundsNonStrict(JSGlobalObject*, JSObject*, int32_t index, double value) WTF_INTERNAL;
void JIT_OPERATION operationPutDoubleByValDirectBeyondArrayBoundsStrict(JSGlobalObject*, JSObject*, int32_t index, double value) WTF_INTERNAL;
void JIT_OPERATION operationPutDoubleByValDirectBeyondArrayBoundsNonStrict(JSGlobalObject*, JSObject*, int32_t index, double value) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdWithThis(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByIdWithThisStrict(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, uintptr_t) WTF_INTERNAL;
void JIT_OPERATION operationPutByValWithThis(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValWithThisStrict(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationDefineDataProperty(JSGlobalObject*, JSObject*, EncodedJSValue, EncodedJSValue, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationDefineDataPropertyString(JSGlobalObject*, JSObject*, JSString*, EncodedJSValue, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationDefineDataPropertyStringIdent(JSGlobalObject*, JSObject*, UniquedStringImpl*, EncodedJSValue, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationDefineDataPropertySymbol(JSGlobalObject*, JSObject*, Symbol*, EncodedJSValue, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationDefineAccessorProperty(JSGlobalObject*, JSObject*, EncodedJSValue, JSObject*, JSObject*, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationDefineAccessorPropertyString(JSGlobalObject*, JSObject*, JSString*, JSObject*, JSObject*, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationDefineAccessorPropertyStringIdent(JSGlobalObject*, JSObject*, UniquedStringImpl*, JSObject*, JSObject*, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationDefineAccessorPropertySymbol(JSGlobalObject*, JSObject*, Symbol*, JSObject*, JSObject*, int32_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPush(JSGlobalObject*, EncodedJSValue encodedValue, JSArray*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPushMultiple(JSGlobalObject*, JSArray*, void* buffer, int32_t elementCount) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPushDouble(JSGlobalObject*, double value, JSArray*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPushDoubleMultiple(JSGlobalObject*, JSArray*, void* buffer, int32_t elementCount) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPop(JSGlobalObject*, JSArray*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPopAndRecoverLength(JSGlobalObject*, JSArray*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationRegExpExecString(JSGlobalObject*, RegExpObject*, JSString*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationRegExpExec(JSGlobalObject*, RegExpObject*, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationRegExpExecGeneric(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationRegExpExecNonGlobalOrSticky(JSGlobalObject*, RegExp*, JSString*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationRegExpMatchFastGlobalString(JSGlobalObject*, RegExp*, JSString*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationRegExpMatchFastString(JSGlobalObject*, RegExpObject*, JSString*) WTF_INTERNAL;
// These comparisons return a boolean within a size_t such that the value is zero extended to fill the register.
size_t JIT_OPERATION operationRegExpTestString(JSGlobalObject*, RegExpObject*, JSString*) WTF_INTERNAL;
size_t JIT_OPERATION operationRegExpTest(JSGlobalObject*, RegExpObject*, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationRegExpTestGeneric(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareStrictEqCell(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationSubHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationMulHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationModHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDivHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationPowHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationBitAndHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationBitNotHeapBigInt(JSGlobalObject*, JSCell* op1) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationBitOrHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationBitLShiftHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationAddHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationBitRShiftHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationBitXorHeapBigInt(JSGlobalObject*, JSCell* op1, JSCell* op2) WTF_INTERNAL;
size_t JIT_OPERATION operationSameValue(JSGlobalObject*, EncodedJSValue, EncodedJSValue) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreateActivationDirect(VM*, Structure*, JSScope*, SymbolTable*, EncodedJSValue);
JSCell* JIT_OPERATION operationCreateDirectArguments(VM*, Structure*, uint32_t length, uint32_t minCapacity);
JSCell* JIT_OPERATION operationCreateDirectArgumentsDuringExit(VM*, InlineCallFrame*, JSFunction*, uint32_t argumentCount);
JSCell* JIT_OPERATION operationCreateScopedArguments(JSGlobalObject*, Structure*, Register* argumentStart, uint32_t length, JSFunction* callee, JSLexicalEnvironment*);
JSCell* JIT_OPERATION operationCreateClonedArgumentsDuringExit(VM*, InlineCallFrame*, JSFunction*, uint32_t argumentCount);
JSCell* JIT_OPERATION operationCreateClonedArguments(JSGlobalObject*, Structure*, Register* argumentStart, uint32_t length, JSFunction* callee);
JSCell* JIT_OPERATION operationCreateArgumentsButterfly(JSGlobalObject*, Register* argumentStart, uint32_t argumentCount);
JSCell* JIT_OPERATION operationCreateRest(JSGlobalObject*, Register* argumentStart, unsigned numberOfArgumentsToSkip, unsigned arraySize);
JSCell* JIT_OPERATION operationNewArrayBuffer(VM*, Structure*, JSCell*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationSetAdd(JSGlobalObject*, JSCell*, EncodedJSValue, int32_t) WTF_INTERNAL;
JSCell* JIT_OPERATION operationMapSet(JSGlobalObject*, JSCell*, EncodedJSValue, EncodedJSValue, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationWeakSetAdd(VM*, JSCell*, JSCell*, int32_t) WTF_INTERNAL;
void JIT_OPERATION operationWeakMapSet(VM*, JSCell*, JSCell*, EncodedJSValue, int32_t) WTF_INTERNAL;
double JIT_OPERATION operationFModOnInts(int32_t, int32_t) WTF_INTERNAL;
size_t JIT_OPERATION operationTypeOfIsObject(JSGlobalObject*, JSCell*) WTF_INTERNAL;
size_t JIT_OPERATION operationTypeOfIsFunction(JSGlobalObject*, JSCell*) WTF_INTERNAL;
size_t JIT_OPERATION operationObjectIsCallable(JSGlobalObject*, JSCell*) WTF_INTERNAL;
size_t JIT_OPERATION operationIsConstructor(JSGlobalObject*, EncodedJSValue) WTF_INTERNAL;
JSCell* JIT_OPERATION operationTypeOfObject(JSGlobalObject*, JSCell*) WTF_INTERNAL;
char* JIT_OPERATION operationAllocateSimplePropertyStorageWithInitialCapacity(VM*) WTF_INTERNAL;
char* JIT_OPERATION operationAllocateSimplePropertyStorage(VM*, size_t newSize) WTF_INTERNAL;
char* JIT_OPERATION operationAllocateComplexPropertyStorageWithInitialCapacity(VM*, JSObject*) WTF_INTERNAL;
char* JIT_OPERATION operationAllocateComplexPropertyStorage(VM*, JSObject*, size_t newSize) WTF_INTERNAL;
char* JIT_OPERATION operationEnsureInt32(VM*, JSCell*);
char* JIT_OPERATION operationEnsureDouble(VM*, JSCell*);
char* JIT_OPERATION operationEnsureContiguous(VM*, JSCell*);
char* JIT_OPERATION operationEnsureArrayStorage(VM*, JSCell*);
StringImpl* JIT_OPERATION operationResolveRope(JSGlobalObject*, JSString*);
JSString* JIT_OPERATION operationSingleCharacterString(VM*, int32_t);

JSCell* JIT_OPERATION operationStringSubstr(JSGlobalObject*, JSCell*, int32_t, int32_t);
JSCell* JIT_OPERATION operationStringSlice(JSGlobalObject*, JSCell*, int32_t, int32_t);
JSString* JIT_OPERATION operationStringValueOf(JSGlobalObject*, EncodedJSValue);
JSString* JIT_OPERATION operationToLowerCase(JSGlobalObject*, JSString*, uint32_t);

char* JIT_OPERATION operationInt32ToString(JSGlobalObject*, int32_t, int32_t);
char* JIT_OPERATION operationInt52ToString(JSGlobalObject*, int64_t, int32_t);
char* JIT_OPERATION operationDoubleToString(JSGlobalObject*, double, int32_t);
char* JIT_OPERATION operationInt32ToStringWithValidRadix(JSGlobalObject*, int32_t, int32_t);
char* JIT_OPERATION operationInt52ToStringWithValidRadix(JSGlobalObject*, int64_t, int32_t);
char* JIT_OPERATION operationDoubleToStringWithValidRadix(JSGlobalObject*, double, int32_t);

EncodedJSValue JIT_OPERATION operationNormalizeMapKeyHeapBigInt(VM*, JSBigInt*) WTF_INTERNAL;
UCPUStrictInt32 JIT_OPERATION operationMapHash(JSGlobalObject*, EncodedJSValue input);
UCPUStrictInt32 JIT_OPERATION operationMapHashHeapBigInt(VM*, JSBigInt*);
JSCell* JIT_OPERATION operationJSMapFindBucket(JSGlobalObject*, JSCell*, EncodedJSValue, int32_t);
JSCell* JIT_OPERATION operationJSSetFindBucket(JSGlobalObject*, JSCell*, EncodedJSValue, int32_t);

EncodedJSValue JIT_OPERATION operationParseIntNoRadixGeneric(JSGlobalObject*, EncodedJSValue);
EncodedJSValue JIT_OPERATION operationParseIntStringNoRadix(JSGlobalObject*, JSString*);
EncodedJSValue JIT_OPERATION operationParseIntString(JSGlobalObject*, JSString*, int32_t);
EncodedJSValue JIT_OPERATION operationParseIntGeneric(JSGlobalObject*, EncodedJSValue, int32_t);

Symbol* JIT_OPERATION operationNewSymbol(VM*);
Symbol* JIT_OPERATION operationNewSymbolWithDescription(JSGlobalObject*, JSString*);
JSCell* JIT_OPERATION operationNewStringObject(VM*, JSString*, Structure*);
JSString* JIT_OPERATION operationToStringOnCell(JSGlobalObject*, JSCell*);
JSString* JIT_OPERATION operationToString(JSGlobalObject*, EncodedJSValue);
JSString* JIT_OPERATION operationCallStringConstructorOnCell(JSGlobalObject*, JSCell*);
JSString* JIT_OPERATION operationCallStringConstructor(JSGlobalObject*, EncodedJSValue);
JSString* JIT_OPERATION operationMakeRope2(JSGlobalObject*, JSString*, JSString*);
JSString* JIT_OPERATION operationMakeRope3(JSGlobalObject*, JSString*, JSString*, JSString*);
JSString* JIT_OPERATION operationStrCat2(JSGlobalObject*, EncodedJSValue, EncodedJSValue);
JSString* JIT_OPERATION operationStrCat3(JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue);
char* JIT_OPERATION operationFindSwitchImmTargetForDouble(VM*, EncodedJSValue, size_t tableIndex);
char* JIT_OPERATION operationSwitchString(JSGlobalObject*, size_t tableIndex, JSString*);
uintptr_t JIT_OPERATION operationCompareStringImplLess(StringImpl*, StringImpl*);
uintptr_t JIT_OPERATION operationCompareStringImplLessEq(StringImpl*, StringImpl*);
uintptr_t JIT_OPERATION operationCompareStringImplGreater(StringImpl*, StringImpl*);
uintptr_t JIT_OPERATION operationCompareStringImplGreaterEq(StringImpl*, StringImpl*);
uintptr_t JIT_OPERATION operationCompareStringLess(JSGlobalObject*, JSString*, JSString*);
uintptr_t JIT_OPERATION operationCompareStringLessEq(JSGlobalObject*, JSString*, JSString*);
uintptr_t JIT_OPERATION operationCompareStringGreater(JSGlobalObject*, JSString*, JSString*);
uintptr_t JIT_OPERATION operationCompareStringGreaterEq(JSGlobalObject*, JSString*, JSString*);
void JIT_OPERATION operationNotifyWrite(VM*, WatchpointSet*);
void JIT_OPERATION operationThrowStackOverflowForVarargs(JSGlobalObject*) WTF_INTERNAL;
UCPUStrictInt32 JIT_OPERATION operationSizeOfVarargs(JSGlobalObject*, EncodedJSValue arguments, uint32_t firstVarArgOffset);
void JIT_OPERATION operationLoadVarargs(JSGlobalObject*, int32_t firstElementDest, EncodedJSValue arguments, uint32_t offset, uint32_t length, uint32_t mandatoryMinimum);
void JIT_OPERATION operationThrowDFG(JSGlobalObject*, EncodedJSValue);
void JIT_OPERATION operationThrowStaticError(JSGlobalObject*, JSString*, uint32_t);

size_t JIT_OPERATION operationHasOwnProperty(JSGlobalObject*, JSObject*, EncodedJSValue);

UCPUStrictInt32 JIT_OPERATION operationArrayIndexOfString(JSGlobalObject*, Butterfly*, JSString*, int32_t);
UCPUStrictInt32 JIT_OPERATION operationArrayIndexOfValueDouble(JSGlobalObject*, Butterfly*, EncodedJSValue, int32_t);
UCPUStrictInt32 JIT_OPERATION operationArrayIndexOfValueInt32OrContiguous(JSGlobalObject*, Butterfly*, EncodedJSValue, int32_t);

JSCell* JIT_OPERATION operationSpreadFastArray(JSGlobalObject*, JSCell*);
JSCell* JIT_OPERATION operationSpreadGeneric(JSGlobalObject*, JSCell*);
JSCell* JIT_OPERATION operationNewArrayWithSpreadSlow(JSGlobalObject*, void*, uint32_t);
JSCell* JIT_OPERATION operationCreateImmutableButterfly(JSGlobalObject*, unsigned length);

JSCell* JIT_OPERATION operationResolveScope(JSGlobalObject*, JSScope*, UniquedStringImpl*);
EncodedJSValue JIT_OPERATION operationResolveScopeForHoistingFuncDeclInEval(JSGlobalObject*, JSScope*, UniquedStringImpl*);
EncodedJSValue JIT_OPERATION operationGetDynamicVar(JSGlobalObject*, JSObject* scope, UniquedStringImpl*, unsigned);
void JIT_OPERATION operationPutDynamicVarStrict(JSGlobalObject*, JSObject* scope, EncodedJSValue, UniquedStringImpl*, unsigned);
void JIT_OPERATION operationPutDynamicVarNonStrict(JSGlobalObject*, JSObject* scope, EncodedJSValue, UniquedStringImpl*, unsigned);

int64_t JIT_OPERATION operationConvertBoxedDoubleToInt52(EncodedJSValue);
int64_t JIT_OPERATION operationConvertDoubleToInt52(double);

size_t JIT_OPERATION operationNumberIsInteger(JSGlobalObject*, EncodedJSValue);

char* JIT_OPERATION operationNewRawObject(VM*, Structure*, int32_t, Butterfly*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewObjectWithButterfly(VM*, Structure*, Butterfly*) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewObjectWithButterflyWithIndexingHeaderAndVectorLength(VM*, Structure*, unsigned length, Butterfly*) WTF_INTERNAL;

void JIT_OPERATION operationLinkDirectCall(CallLinkInfo*, JSFunction*) WTF_INTERNAL;

EncodedJSValue JIT_OPERATION operationDateGetFullYear(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetUTCFullYear(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetMonth(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetUTCMonth(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetDate(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetUTCDate(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetDay(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetUTCDay(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetHours(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetUTCHours(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetMinutes(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetUTCMinutes(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetSeconds(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetUTCSeconds(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetTimezoneOffset(VM*, DateInstance*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationDateGetYear(VM*, DateInstance*) WTF_INTERNAL;

void JIT_OPERATION operationProcessTypeProfilerLogDFG(VM*) WTF_INTERNAL;

void JIT_OPERATION operationTriggerReoptimizationNow(CodeBlock* baselineCodeBlock, CodeBlock* optiimzedCodeBlock, OSRExitBase*) WTF_INTERNAL;
void triggerReoptimizationNow(CodeBlock* baselineCodeBlock, CodeBlock* optiimzedCodeBlock, OSRExitBase*); // This is not JIT_OPERATION.

#if USE(JSVALUE32_64)
double JIT_OPERATION operationRandom(JSGlobalObject*);
#endif

#if ENABLE(FTL_JIT)
void JIT_OPERATION operationTriggerTierUpNow(VM*) WTF_INTERNAL;
void JIT_OPERATION operationTriggerTierUpNowInLoop(VM*, unsigned bytecodeIndexBits) WTF_INTERNAL;
char* JIT_OPERATION operationTriggerOSREntryNow(VM*, unsigned bytecodeIndexBits) WTF_INTERNAL;
#endif // ENABLE(FTL_JIT)

} // extern "C"

inline auto operationNewTypedArrayWithSizeForType(TypedArrayType type) -> decltype(&operationNewInt8ArrayWithSize)
{
    switch (type) {
    case TypeInt8:
        return operationNewInt8ArrayWithSize;
    case TypeInt16:
        return operationNewInt16ArrayWithSize;
    case TypeInt32:
        return operationNewInt32ArrayWithSize;
    case TypeUint8:
        return operationNewUint8ArrayWithSize;
    case TypeUint8Clamped:
        return operationNewUint8ClampedArrayWithSize;
    case TypeUint16:
        return operationNewUint16ArrayWithSize;
    case TypeUint32:
        return operationNewUint32ArrayWithSize;
    case TypeFloat32:
        return operationNewFloat32ArrayWithSize;
    case TypeFloat64:
        return operationNewFloat64ArrayWithSize;
    case NotTypedArray:
    case TypeDataView:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

inline auto operationNewTypedArrayWithOneArgumentForType(TypedArrayType type) -> decltype(&operationNewInt8ArrayWithOneArgument)
{
    switch (type) {
    case TypeInt8:
        return operationNewInt8ArrayWithOneArgument;
    case TypeInt16:
        return operationNewInt16ArrayWithOneArgument;
    case TypeInt32:
        return operationNewInt32ArrayWithOneArgument;
    case TypeUint8:
        return operationNewUint8ArrayWithOneArgument;
    case TypeUint8Clamped:
        return operationNewUint8ClampedArrayWithOneArgument;
    case TypeUint16:
        return operationNewUint16ArrayWithOneArgument;
    case TypeUint32:
        return operationNewUint32ArrayWithOneArgument;
    case TypeFloat32:
        return operationNewFloat32ArrayWithOneArgument;
    case TypeFloat64:
        return operationNewFloat64ArrayWithOneArgument;
    case NotTypedArray:
    case TypeDataView:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

} } // namespace JSC::DFG

#endif
