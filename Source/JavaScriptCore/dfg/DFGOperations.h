/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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
#include "DFGNodeType.h"
#include "IndexingType.h"
#include "JITOperations.h"
#include "TypedArrayType.h"
#include "UGPRPair.h"
#include <wtf/text/StringSearch.h>

namespace JSC {

class DateInstance;
class DirectCallLinkInfo;
class JSBigInt;
class JSBoundFunction;
class JSPropertyNameEnumerator;
class JSRopeString;
class JSMap;
class JSSet;
class JSWeakMap;
class JSWeakSet;
class NativeExecutable;
class OptimizingCallLinkInfo;
struct UnlinkedStringJumpTable;

namespace DFG {

struct OSRExitBase;

JSC_DECLARE_JIT_OPERATION(operationStringFromCharCode, JSCell*, (JSGlobalObject*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationStringFromCharCodeUntyped, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));

// These routines provide callbacks out to C++ implementations of operations too complex to JIT.
JSC_DECLARE_JIT_OPERATION(operationCallObjectConstructor, JSCell*, (JSGlobalObject*, EncodedJSValue encodedTarget));
JSC_DECLARE_JIT_OPERATION(operationToObject, JSCell*, (JSGlobalObject*, EncodedJSValue encodedTarget, UniquedStringImpl*));
JSC_DECLARE_JIT_OPERATION(operationObjectKeys, JSArray*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationObjectKeysObject, JSArray*, (JSGlobalObject*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationObjectGetOwnPropertyNames, JSArray*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationObjectGetOwnPropertyNamesObject, JSArray*, (JSGlobalObject*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationObjectGetOwnPropertySymbols, JSArray*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationObjectGetOwnPropertySymbolsObject, JSArray*, (JSGlobalObject*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationObjectCreate, JSCell*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationObjectCreateObject, JSCell*, (JSGlobalObject*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationObjectAssignObject, void, (JSGlobalObject*, JSObject*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationObjectAssignUntyped, void, (JSGlobalObject*, JSObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationObjectToStringUntyped, JSString*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationObjectToStringObjectSlow, JSString*, (JSGlobalObject*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationReflectOwnKeys, JSArray*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationReflectOwnKeysObject, JSArray*, (JSGlobalObject*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationCreateThis, JSCell*, (JSGlobalObject*, JSObject* constructor, uint32_t inlineCapacity));
JSC_DECLARE_JIT_OPERATION(operationCreatePromise, JSCell*, (JSGlobalObject*, JSObject* constructor));
JSC_DECLARE_JIT_OPERATION(operationCreateInternalPromise, JSCell*, (JSGlobalObject*, JSObject* constructor));
JSC_DECLARE_JIT_OPERATION(operationCreateGenerator, JSCell*, (JSGlobalObject*, JSObject* constructor));
JSC_DECLARE_JIT_OPERATION(operationCreateAsyncGenerator, JSCell*, (JSGlobalObject*, JSObject* constructor));
JSC_DECLARE_JIT_OPERATION(operationToThis, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationToThisStrict, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationValueMod, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueBitNot, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationValueBitAnd, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueBitOr, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueBitXor, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueBitLShift, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueBitRShift, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueBitURShift, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueAddNotNumber, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValueDiv, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationValuePow, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2));
JSC_DECLARE_JIT_OPERATION(operationInc, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationDec, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationArithAbs, double, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationArithClz32, UCPUStrictInt32, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationArithFRound, double, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationArithF16Round, double, (JSGlobalObject*, EncodedJSValue encodedOp1));
JSC_DECLARE_JIT_OPERATION(operationArithSqrt, double, (JSGlobalObject*, EncodedJSValue encodedOp1));

#define DFG_ARITH_UNARY(capitalizedName, lowerName) \
    JSC_DECLARE_JIT_OPERATION(operationArith##capitalizedName, double, (JSGlobalObject*, EncodedJSValue encodedOp1));
FOR_EACH_ARITH_UNARY_OP(DFG_ARITH_UNARY)
#undef DFG_ARITH_UNARY

JSC_DECLARE_JIT_OPERATION(operationArithRound, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationArithFloor, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationArithCeil, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationArithTrunc, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationArithMinMultipleDouble, double, (const double* buffer, unsigned elementCount));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationArithMaxMultipleDouble, double, (const double* buffer, unsigned elementCount));
JSC_DECLARE_JIT_OPERATION(operationGetByValObjectInt, EncodedJSValue, (JSGlobalObject*, JSObject*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationGetByValStringInt, EncodedJSValue, (JSGlobalObject*, JSString*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationGetByValObjectString, EncodedJSValue, (JSGlobalObject*, JSCell*, JSCell* string));
JSC_DECLARE_JIT_OPERATION(operationGetByValObjectSymbol, EncodedJSValue, (JSGlobalObject*, JSCell*, JSCell* symbol));
JSC_DECLARE_JIT_OPERATION(operationToPrimitive, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationToPropertyKey, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationToPropertyKeyOrNumber, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationToNumber, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationToNumberString, EncodedJSValue, (JSGlobalObject*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationToNumeric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCallNumberConstructor, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationGetPrototypeOf, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationGetPrototypeOfObject, EncodedJSValue, (JSGlobalObject*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationHasIndexedProperty, size_t, (JSGlobalObject*, JSCell*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationHasEnumerableIndexedProperty, size_t, (JSGlobalObject*, JSCell*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationGetPropertyEnumerator, JSCell*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationGetPropertyEnumeratorCell, JSCell*, (JSGlobalObject*, JSCell*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationEnumeratorNextUpdateIndexAndMode, UGPRPair, (JSGlobalObject*, EncodedJSValue, uint32_t, int32_t, JSPropertyNameEnumerator*));
JSC_DECLARE_JIT_OPERATION(operationEnumeratorNextUpdatePropertyName, JSString*, (JSGlobalObject*, uint32_t, int32_t, JSPropertyNameEnumerator*));
JSC_DECLARE_JIT_OPERATION(operationEnumeratorInByVal, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, uint32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationEnumeratorHasOwnProperty, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, uint32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationEnumeratorRecoverNameAndGetByVal, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, uint32_t, JSPropertyNameEnumerator*));
JSC_DECLARE_JIT_OPERATION(operationEnumeratorRecoverNameAndPutByVal, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, bool, uint32_t, JSPropertyNameEnumerator*));

JSC_DECLARE_JIT_OPERATION(operationNewRegexpWithLastIndex, JSCell*, (JSGlobalObject*, JSCell*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewArray, char*, (JSGlobalObject*, Structure*, void*, size_t));
JSC_DECLARE_JIT_OPERATION(operationNewEmptyArray, char*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewArrayWithSize, char*, (JSGlobalObject*, Structure*, int32_t, Butterfly*));
JSC_DECLARE_JIT_OPERATION(operationNewArrayWithSizeAndHint, char*, (JSGlobalObject*, Structure*, int32_t, int32_t, Butterfly*));
JSC_DECLARE_JIT_OPERATION(operationNewInt8ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewInt8ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewInt16ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewInt16ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewInt32ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewInt32ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewUint8ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewUint8ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewUint8ClampedArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewUint8ClampedArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewUint16ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewUint16ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewUint32ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewUint32ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewFloat16ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewFloat16ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewFloat32ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewFloat32ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewFloat64ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewFloat64ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewBigInt64ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewBigInt64ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewBigUint64ArrayWithSize, char*, (JSGlobalObject*, Structure*, intptr_t, char*));
JSC_DECLARE_JIT_OPERATION(operationNewBigUint64ArrayWithOneArgument, char*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewArrayIterator, JSCell*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewMapIterator, JSCell*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewSetIterator, JSCell*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewIteratorHelper, JSCell*, (VM*, Structure*));

JSC_DECLARE_JIT_OPERATION(operationPutByValCellStringStrict, void, (JSGlobalObject*, JSCell*, JSCell* string, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValCellStringSloppy, void, (JSGlobalObject*, JSCell*, JSCell* string, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValCellSymbolStrict, void, (JSGlobalObject*, JSCell*, JSCell* symbol, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValCellSymbolSloppy, void, (JSGlobalObject*, JSCell*, JSCell* symbol, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValBeyondArrayBoundsStrict, void, (JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValBeyondArrayBoundsSloppy, void, (JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValDirectCellStringStrict, void, (JSGlobalObject*, JSCell*, JSCell* string, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValDirectCellStringSloppy, void, (JSGlobalObject*, JSCell*, JSCell* string, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValDirectCellSymbolStrict, void, (JSGlobalObject*, JSCell*, JSCell* symbol, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValDirectCellSymbolSloppy, void, (JSGlobalObject*, JSCell*, JSCell* symbol, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValDirectBeyondArrayBoundsStrict, void, (JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValDirectBeyondArrayBoundsSloppy, void, (JSGlobalObject*, JSObject*, int32_t index, EncodedJSValue encodedValue));
JSC_DECLARE_JIT_OPERATION(operationPutDoubleByValBeyondArrayBoundsStrict, void, (JSGlobalObject*, JSObject*, int32_t index, double value));
JSC_DECLARE_JIT_OPERATION(operationPutDoubleByValBeyondArrayBoundsSloppy, void, (JSGlobalObject*, JSObject*, int32_t index, double value));
JSC_DECLARE_JIT_OPERATION(operationPutDoubleByValDirectBeyondArrayBoundsStrict, void, (JSGlobalObject*, JSObject*, int32_t index, double value));
JSC_DECLARE_JIT_OPERATION(operationPutDoubleByValDirectBeyondArrayBoundsSloppy, void, (JSGlobalObject*, JSObject*, int32_t index, double value));
JSC_DECLARE_JIT_OPERATION(operationPutByIdWithThis, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, uintptr_t));
JSC_DECLARE_JIT_OPERATION(operationPutByIdWithThisStrict, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, uintptr_t));
JSC_DECLARE_JIT_OPERATION(operationPutByValWithThis, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationPutByValWithThisStrict, void, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationDefineDataProperty, void, (JSGlobalObject*, JSObject*, EncodedJSValue, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDefineDataPropertyString, void, (JSGlobalObject*, JSObject*, JSString*, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDefineDataPropertyStringIdent, void, (JSGlobalObject*, JSObject*, UniquedStringImpl*, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDefineDataPropertySymbol, void, (JSGlobalObject*, JSObject*, Symbol*, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDefineAccessorProperty, void, (JSGlobalObject*, JSObject*, EncodedJSValue, JSObject*, JSObject*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDefineAccessorPropertyString, void, (JSGlobalObject*, JSObject*, JSString*, JSObject*, JSObject*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDefineAccessorPropertyStringIdent, void, (JSGlobalObject*, JSObject*, UniquedStringImpl*, JSObject*, JSObject*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDefineAccessorPropertySymbol, void, (JSGlobalObject*, JSObject*, Symbol*, JSObject*, JSObject*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationArrayPush, EncodedJSValue, (JSGlobalObject*, EncodedJSValue encodedValue, JSArray*));
JSC_DECLARE_JIT_OPERATION(operationArrayPushMultiple, EncodedJSValue, (JSGlobalObject*, JSArray*, void* buffer, int32_t elementCount));
JSC_DECLARE_JIT_OPERATION(operationArrayPushMultipleSlow, EncodedJSValue, (JSGlobalObject*, JSArray*, void* buffer, int32_t elementCount));
JSC_DECLARE_JIT_OPERATION(operationArrayPushDouble, EncodedJSValue, (JSGlobalObject*, double value, JSArray*));
JSC_DECLARE_JIT_OPERATION(operationArrayPushDoubleMultiple, EncodedJSValue, (JSGlobalObject*, JSArray*, void* buffer, int32_t elementCount));
JSC_DECLARE_JIT_OPERATION(operationArrayPop, EncodedJSValue, (JSGlobalObject*, JSArray*));
JSC_DECLARE_JIT_OPERATION(operationArrayPopAndRecoverLength, EncodedJSValue, (JSGlobalObject*, JSArray*));
JSC_DECLARE_JIT_OPERATION(operationArraySplice, EncodedJSValue, (JSGlobalObject*, JSArray*, int32_t start, int32_t deleteCount, EncodedJSValue*, unsigned));
JSC_DECLARE_JIT_OPERATION(operationArraySpliceIgnoreResult, EncodedJSValue, (JSGlobalObject*, JSArray*, int32_t start, int32_t deleteCount, EncodedJSValue*, unsigned));
JSC_DECLARE_JIT_OPERATION(operationRegExpExecString, EncodedJSValue, (JSGlobalObject*, RegExpObject*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationRegExpExec, EncodedJSValue, (JSGlobalObject*, RegExpObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationRegExpExecGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationRegExpExecNonGlobalOrSticky, EncodedJSValue, (JSGlobalObject*, RegExp*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationRegExpMatchFastGlobalString, EncodedJSValue, (JSGlobalObject*, RegExp*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationRegExpMatchFastString, EncodedJSValue, (JSGlobalObject*, RegExpObject*, JSString*));
// These comparisons return a boolean within a size_t such that the value is zero extended to fill the register.
JSC_DECLARE_JIT_OPERATION(operationRegExpTestString, size_t, (JSGlobalObject*, RegExpObject*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationRegExpTest, size_t, (JSGlobalObject*, RegExpObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationRegExpTestGeneric, size_t, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationCompareStrictEqCell, size_t, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationSubHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationMulHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationModHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationDivHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationPowHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationBitAndHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationBitNotHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1));
JSC_DECLARE_JIT_OPERATION(operationBitOrHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationBitLShiftHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationAddHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationBitRShiftHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationBitXorHeapBigInt, EncodedJSValue, (JSGlobalObject*, JSCell* op1, JSCell* op2));
JSC_DECLARE_JIT_OPERATION(operationSameValue, size_t, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCreateActivationDirect, JSCell*, (VM*, Structure*, JSScope*, SymbolTable*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCreateDirectArguments, JSCell*, (VM*, Structure*, uint32_t length, uint32_t minCapacity));
JSC_DECLARE_JIT_OPERATION(operationCreateDirectArgumentsDuringExit, JSCell*, (VM*, InlineCallFrame*, JSFunction*, uint32_t argumentCount));
JSC_DECLARE_JIT_OPERATION(operationCreateScopedArguments, JSCell*, (JSGlobalObject*, Structure*, Register* argumentStart, uint32_t length, JSFunction* callee, JSLexicalEnvironment*));
JSC_DECLARE_JIT_OPERATION(operationCreateClonedArgumentsDuringExit, JSCell*, (VM*, InlineCallFrame*, JSFunction*, uint32_t argumentCount));
JSC_DECLARE_JIT_OPERATION(operationCreateClonedArguments, JSCell*, (JSGlobalObject*, Structure*, Register* argumentStart, uint32_t length, JSFunction* callee, Butterfly*));
JSC_DECLARE_JIT_OPERATION(operationCreateRest, JSCell*, (JSGlobalObject*, Register* argumentStart, unsigned numberOfArgumentsToSkip, unsigned arraySize));
JSC_DECLARE_JIT_OPERATION(operationNewArrayBuffer, JSCell*, (VM*, Structure*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationSetAdd, void, (JSGlobalObject*, JSCell*, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationMapSet, void, (JSGlobalObject*, JSCell*, EncodedJSValue, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationMapDelete, size_t, (JSGlobalObject*, JSCell*, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationSetDelete, size_t, (JSGlobalObject*, JSCell*, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationWeakSetAdd, void, (JSGlobalObject*, JSCell*, JSCell*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationWeakMapSet, void, (JSGlobalObject*, JSCell*, JSCell*, EncodedJSValue, int32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationFModOnInts, double, (int32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationTypeOfIsObject, size_t, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationTypeOfIsFunction, size_t, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationObjectIsCallable, size_t, (JSGlobalObject*, JSCell*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationIsConstructor, size_t, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationTypeOfObject, JSCell*, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationAllocateSimplePropertyStorageWithInitialCapacity, Butterfly*, (VM*));
JSC_DECLARE_JIT_OPERATION(operationAllocateSimplePropertyStorage, Butterfly*, (VM*, size_t newSize));
JSC_DECLARE_JIT_OPERATION(operationAllocateComplexPropertyStorageWithInitialCapacity, Butterfly*, (VM*, JSObject*));
JSC_DECLARE_JIT_OPERATION(operationAllocateComplexPropertyStorage, Butterfly*, (VM*, JSObject*, size_t newSize));
JSC_DECLARE_JIT_OPERATION(operationEnsureInt32, Butterfly*, (VM*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationEnsureDouble, Butterfly*, (VM*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationEnsureContiguous, Butterfly*, (VM*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationEnsureArrayStorage, Butterfly*, (VM*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationResolveRope, StringImpl*, (JSGlobalObject*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationResolveRopeString, JSString*, (JSGlobalObject*, JSRopeString*));
JSC_DECLARE_JIT_OPERATION(operationSingleCharacterString, JSString*, (VM*, int32_t));

JSC_DECLARE_JIT_OPERATION(operationStringSubstr, JSCell*, (JSGlobalObject*, JSCell*, int32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationStringSlice, JSString*, (JSGlobalObject*, JSString*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationStringSliceWithEnd, JSString*, (JSGlobalObject*, JSString*, int32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationStringValueOf, JSString*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationStringReplaceStringString, JSString*, (JSGlobalObject*, JSString*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationStringReplaceStringStringWithoutSubstitution, JSString*, (JSGlobalObject*, JSString*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationStringReplaceStringEmptyString, JSString*, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationStringReplaceStringStringWithTable8, JSString*, (JSGlobalObject*, JSString*, JSString*, JSString*, const BoyerMooreHorspoolTable<uint8_t>*));
JSC_DECLARE_JIT_OPERATION(operationStringReplaceStringStringWithoutSubstitutionWithTable8, JSString*, (JSGlobalObject*, JSString*, JSString*, JSString*, const BoyerMooreHorspoolTable<uint8_t>*));
JSC_DECLARE_JIT_OPERATION(operationStringReplaceStringEmptyStringWithTable8, JSString*, (JSGlobalObject*, JSString*, JSString*, const BoyerMooreHorspoolTable<uint8_t>*));
JSC_DECLARE_JIT_OPERATION(operationStringReplaceStringGeneric, JSString*, (JSGlobalObject*, JSString*, JSString*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationStringSubstring, JSString*, (JSGlobalObject*, JSString*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationStringSubstringWithEnd, JSString*, (JSGlobalObject*, JSString*, int32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationToLowerCase, JSString*, (JSGlobalObject*, JSString*, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationStringLocaleCompare, UCPUStrictInt32, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationStringIndexOf, UCPUStrictInt32, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationStringIndexOfWithOneChar, UCPUStrictInt32, (JSGlobalObject*, JSString*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationStringIndexOfWithIndex, UCPUStrictInt32, (JSGlobalObject*, JSString*, JSString*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationStringIndexOfWithIndexWithOneChar, UCPUStrictInt32, (JSGlobalObject*, JSString*, int32_t, int32_t));

JSC_DECLARE_JIT_OPERATION(operationInt32ToString, char*, (JSGlobalObject*, int32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationInt52ToString, char*, (JSGlobalObject*, int64_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDoubleToString, char*, (JSGlobalObject*, double, int32_t));
JSC_DECLARE_JIT_OPERATION(operationInt32ToStringWithValidRadix, char*, (JSGlobalObject*, int32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationInt52ToStringWithValidRadix, char*, (JSGlobalObject*, int64_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationDoubleToStringWithValidRadix, char*, (JSGlobalObject*, double, int32_t));
JSC_DECLARE_JIT_OPERATION(operationFunctionToString, JSString*, (JSGlobalObject*, JSFunction*));
JSC_DECLARE_JIT_OPERATION(operationFunctionBind, JSBoundFunction*, (JSGlobalObject*, JSObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewBoundFunction, JSBoundFunction*, (JSGlobalObject*, JSFunction*, EncodedJSValue, EncodedJSValue, EncodedJSValue, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationNormalizeMapKeyHeapBigInt, EncodedJSValue, (VM*, JSBigInt*));
JSC_DECLARE_JIT_OPERATION(operationMapHash, UCPUStrictInt32, (JSGlobalObject*, EncodedJSValue input));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationMapHashHeapBigInt, UCPUStrictInt32, (VM*, JSBigInt*));

JSC_DECLARE_JIT_OPERATION(operationMapGet, JSValue*, (JSGlobalObject*, JSCell*, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationSetGet, JSValue*, (JSGlobalObject*, JSCell*, EncodedJSValue, int32_t));

JSC_DECLARE_JIT_OPERATION(operationMapStorage, EncodedJSValue, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationSetStorage, EncodedJSValue, (JSGlobalObject*, JSCell*));

JSC_DECLARE_JIT_OPERATION(operationMapIterationNext, EncodedJSValue, (JSGlobalObject*, JSCell*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationMapIterationEntry, EncodedJSValue, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationMapIterationEntryKey, EncodedJSValue, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationMapIterationEntryValue, EncodedJSValue, (JSGlobalObject*, JSCell*));

JSC_DECLARE_JIT_OPERATION(operationSetIterationNext, EncodedJSValue, (JSGlobalObject*, JSCell*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationSetIterationEntry, EncodedJSValue, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationSetIterationEntryKey, EncodedJSValue, (JSGlobalObject*, JSCell*));

JSC_DECLARE_JIT_OPERATION(operationMapIteratorNext, EncodedJSValue, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationMapIteratorKey, EncodedJSValue, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationMapIteratorValue, EncodedJSValue, (JSGlobalObject*, JSCell*));

JSC_DECLARE_JIT_OPERATION(operationSetIteratorNext, EncodedJSValue, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationSetIteratorKey, EncodedJSValue, (JSGlobalObject*, JSCell*));

JSC_DECLARE_JIT_OPERATION(operationNewMap, JSMap*, (VM*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationNewSet, JSSet*, (VM*, Structure*));

JSC_DECLARE_JIT_OPERATION(operationParseIntDoubleNoRadix, EncodedJSValue, (JSGlobalObject*, double));
JSC_DECLARE_JIT_OPERATION(operationParseIntStringNoRadix, EncodedJSValue, (JSGlobalObject*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationParseIntGenericNoRadix, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationParseIntInt32, EncodedJSValue, (JSGlobalObject*, int32_t, int32_t));
JSC_DECLARE_JIT_OPERATION(operationParseIntDouble, EncodedJSValue, (JSGlobalObject*, double, int32_t));
JSC_DECLARE_JIT_OPERATION(operationParseIntString, EncodedJSValue, (JSGlobalObject*, JSString*, int32_t));
JSC_DECLARE_JIT_OPERATION(operationParseIntGeneric, EncodedJSValue, (JSGlobalObject*, EncodedJSValue, int32_t));

JSC_DECLARE_JIT_OPERATION(operationNewSymbol, Symbol*, (VM*));
JSC_DECLARE_JIT_OPERATION(operationNewSymbolWithStringDescription, Symbol*, (JSGlobalObject*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationNewSymbolWithDescription, Symbol*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationNewStringObject, JSCell*, (VM*, JSString*, Structure*));
JSC_DECLARE_JIT_OPERATION(operationToStringOnCell, JSString*, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationToString, JSString*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationCallStringConstructorOnCell, JSString*, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationCallStringConstructor, JSString*, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationMakeRope2, JSString*, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationMakeRope3, JSString*, (JSGlobalObject*, JSString*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationStrCat2, JSString*, (JSGlobalObject*, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationStrCat3, JSString*, (JSGlobalObject*, EncodedJSValue, EncodedJSValue, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationMakeAtomString1, JSString*, (JSGlobalObject*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationMakeAtomString2, JSString*, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationMakeAtomString3, JSString*, (JSGlobalObject*, JSString*, JSString*, JSString*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationFindSwitchImmTargetForDouble, char*, (VM*, EncodedJSValue, size_t tableIndex, int32_t));
JSC_DECLARE_JIT_OPERATION(operationSwitchString, char*, (JSGlobalObject*, size_t tableIndex, const UnlinkedStringJumpTable*, JSString*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationCompareStringImplLess, uintptr_t, (StringImpl*, StringImpl*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationCompareStringImplLessEq, uintptr_t, (StringImpl*, StringImpl*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationCompareStringImplGreater, uintptr_t, (StringImpl*, StringImpl*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationCompareStringImplGreaterEq, uintptr_t, (StringImpl*, StringImpl*));
JSC_DECLARE_JIT_OPERATION(operationCompareStringLess, uintptr_t, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationCompareStringLessEq, uintptr_t, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationCompareStringGreater, uintptr_t, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationCompareStringGreaterEq, uintptr_t, (JSGlobalObject*, JSString*, JSString*));
JSC_DECLARE_JIT_OPERATION(operationNotifyWrite, void, (VM*, WatchpointSet*));
JSC_DECLARE_JIT_OPERATION(operationThrowStackOverflowForVarargs, void, (JSGlobalObject*));
JSC_DECLARE_JIT_OPERATION(operationSizeOfVarargs, UCPUStrictInt32, (JSGlobalObject*, EncodedJSValue arguments, uint32_t firstVarArgOffset));
JSC_DECLARE_JIT_OPERATION(operationLoadVarargs, void, (JSGlobalObject*, int32_t firstElementDest, EncodedJSValue arguments, uint32_t offset, uint32_t length, uint32_t mandatoryMinimum));
JSC_DECLARE_JIT_OPERATION(operationThrowDFG, void, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationThrowStaticError, void, (JSGlobalObject*, JSString*, uint32_t));

JSC_DECLARE_JIT_OPERATION(operationHasOwnProperty, size_t, (JSGlobalObject*, JSObject*, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationArrayIndexOfString, UCPUStrictInt32, (JSGlobalObject*, Butterfly*, JSString*, int32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationArrayIndexOfValueDouble, UCPUStrictInt32, (Butterfly*, EncodedJSValue, int32_t));
JSC_DECLARE_JIT_OPERATION(operationArrayIndexOfValueInt32OrContiguous, UCPUStrictInt32, (JSGlobalObject*, Butterfly*, EncodedJSValue, int32_t));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationArrayIndexOfNonStringIdentityValueContiguous, UCPUStrictInt32, (Butterfly*, EncodedJSValue, int32_t));

JSC_DECLARE_JIT_OPERATION(operationSpreadFastArray, JSCell*, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationSpreadGeneric, JSCell*, (JSGlobalObject*, JSCell*));
JSC_DECLARE_JIT_OPERATION(operationNewArrayWithSpreadSlow, JSCell*, (JSGlobalObject*, void*, uint32_t));
JSC_DECLARE_JIT_OPERATION(operationCreateImmutableButterfly, JSCell*, (JSGlobalObject*, unsigned length));
JSC_DECLARE_JIT_OPERATION(operationNewArrayWithSpeciesInt32, JSObject*, (JSGlobalObject*, int32_t, JSObject*, IndexingType));
JSC_DECLARE_JIT_OPERATION(operationNewArrayWithSpecies, JSObject*, (JSGlobalObject*, EncodedJSValue, JSObject*, IndexingType));

JSC_DECLARE_JIT_OPERATION(operationResolveScope, JSCell*, (JSGlobalObject*, JSScope*, UniquedStringImpl*));
JSC_DECLARE_JIT_OPERATION(operationResolveScopeForHoistingFuncDeclInEval, EncodedJSValue, (JSGlobalObject*, JSScope*, UniquedStringImpl*));
JSC_DECLARE_JIT_OPERATION(operationGetDynamicVar, EncodedJSValue, (JSGlobalObject*, JSObject* scope, UniquedStringImpl*, unsigned));
JSC_DECLARE_JIT_OPERATION(operationPutDynamicVarStrict, void, (JSGlobalObject*, JSObject* scope, EncodedJSValue, UniquedStringImpl*, unsigned));
JSC_DECLARE_JIT_OPERATION(operationPutDynamicVarSloppy, void, (JSGlobalObject*, JSObject* scope, EncodedJSValue, UniquedStringImpl*, unsigned));

JSC_DECLARE_JIT_OPERATION(operationNumberIsInteger, size_t, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationNumberIsNaN, UCPUStrictInt32, (EncodedJSValue));
JSC_DECLARE_JIT_OPERATION(operationIsNaN, UCPUStrictInt32, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationToIntegerOrInfinityDouble, EncodedJSValue, (double));
JSC_DECLARE_JIT_OPERATION(operationToIntegerOrInfinityUntyped, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationToLengthDouble, EncodedJSValue, (double));
JSC_DECLARE_JIT_OPERATION(operationToLengthUntyped, EncodedJSValue, (JSGlobalObject*, EncodedJSValue));

JSC_DECLARE_JIT_OPERATION(operationNewRawObject, char*, (VM*, Structure*, int32_t, Butterfly*));
JSC_DECLARE_JIT_OPERATION(operationNewObjectWithButterfly, JSCell*, (VM*, Structure*, Butterfly*));
JSC_DECLARE_JIT_OPERATION(operationNewObjectWithButterflyWithIndexingHeaderAndVectorLength, JSCell*, (VM*, Structure*, unsigned length, Butterfly*));

JSC_DECLARE_JIT_OPERATION(operationLinkDirectCall, void, (DirectCallLinkInfo*, JSFunction*));

JSC_DECLARE_JIT_OPERATION(operationDateGetFullYear, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetUTCFullYear, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetMonth, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetUTCMonth, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetDate, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetUTCDate, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetDay, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetUTCDay, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetHours, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetUTCHours, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetMinutes, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetUTCMinutes, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetSeconds, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetUTCSeconds, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetTimezoneOffset, EncodedJSValue, (VM*, DateInstance*));
JSC_DECLARE_JIT_OPERATION(operationDateGetYear, EncodedJSValue, (VM*, DateInstance*));

JSC_DECLARE_JIT_OPERATION(operationInt64ToBigInt, EncodedJSValue, (JSGlobalObject*, int64_t));

JSC_DECLARE_JIT_OPERATION(operationProcessTypeProfilerLogDFG, void, (VM*));

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationTriggerReoptimizationNow, void, (CodeBlock* baselineCodeBlock, CodeBlock* optimizedCodeBlock, OSRExitBase*));

#if USE(JSVALUE32_64)
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationRandom, double, (JSGlobalObject*));
#endif

#if ENABLE(FTL_JIT)
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationTriggerTierUpNow, void, (VM*));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationTriggerTierUpNowInLoop, void, (VM*, unsigned bytecodeIndexBits));
JSC_DECLARE_NOEXCEPT_JIT_OPERATION(operationTriggerOSREntryNow, char*, (VM*, unsigned bytecodeIndexBits));
#endif // ENABLE(FTL_JIT)

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
    case TypeFloat16:
        return operationNewFloat16ArrayWithSize;
    case TypeFloat32:
        return operationNewFloat32ArrayWithSize;
    case TypeFloat64:
        return operationNewFloat64ArrayWithSize;
    case TypeBigInt64:
        return operationNewBigInt64ArrayWithSize;
    case TypeBigUint64:
        return operationNewBigUint64ArrayWithSize;
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
    case TypeFloat16:
        return operationNewFloat16ArrayWithOneArgument;
    case TypeFloat32:
        return operationNewFloat32ArrayWithOneArgument;
    case TypeFloat64:
        return operationNewFloat64ArrayWithOneArgument;
    case TypeBigInt64:
        return operationNewBigInt64ArrayWithOneArgument;
    case TypeBigUint64:
        return operationNewBigUint64ArrayWithOneArgument;
    case NotTypedArray:
    case TypeDataView:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

inline auto operationOwnPropertyKeysVariant(NodeType type) -> decltype(&operationObjectKeys)
{
    switch (type) {
    case ObjectKeys:
        return operationObjectKeys;
    case ObjectGetOwnPropertyNames:
        return operationObjectGetOwnPropertyNames;
    case ObjectGetOwnPropertySymbols:
        return operationObjectGetOwnPropertySymbols;
    case ReflectOwnKeys:
        return operationReflectOwnKeys;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

inline auto operationOwnPropertyKeysVariantObject(NodeType type) -> decltype(&operationObjectKeysObject)
{
    switch (type) {
    case ObjectKeys:
        return operationObjectKeysObject;
    case ObjectGetOwnPropertyNames:
        return operationObjectGetOwnPropertyNamesObject;
    case ObjectGetOwnPropertySymbols:
        return operationObjectGetOwnPropertySymbolsObject;
    case ReflectOwnKeys:
        return operationReflectOwnKeysObject;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} } // namespace JSC::DFG

#endif
