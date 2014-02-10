/*
 * Copyright (C) 2011, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGOperations_h
#define DFGOperations_h

#if ENABLE(DFG_JIT)

#include "JITOperations.h"
#include "PutKind.h"

namespace JSC {

namespace DFG {

extern "C" {

JSCell* JIT_OPERATION operationStringFromCharCode(ExecState*, int32_t)  WTF_INTERNAL; 

// These routines are provide callbacks out to C++ implementations of operations too complex to JIT.
JSCell* JIT_OPERATION operationCreateThis(ExecState*, JSObject* constructor, int32_t inlineCapacity) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToThis(ExecState*, EncodedJSValue encodedOp1) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToThisStrict(ExecState*, EncodedJSValue encodedOp1) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueAdd(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationValueAddNotNumber(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByVal(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValCell(ExecState*, JSCell*, EncodedJSValue encodedProperty) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValArrayInt(ExecState*, JSArray*, int32_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetByValStringInt(ExecState*, JSString*, int32_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationToPrimitive(ExecState*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewArray(ExecState*, Structure*, void*, size_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewArrayBuffer(ExecState*, Structure*, size_t, size_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewEmptyArray(ExecState*, Structure*) WTF_INTERNAL;
char* JIT_OPERATION operationNewArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt8ArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt8ArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt16ArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt16ArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt32ArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewInt32ArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint8ArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint8ArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint8ClampedArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint8ClampedArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint16ArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint16ArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint32ArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewUint32ArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewFloat32ArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewFloat32ArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
char* JIT_OPERATION operationNewFloat64ArrayWithSize(ExecState*, Structure*, int32_t) WTF_INTERNAL;
char* JIT_OPERATION operationNewFloat64ArrayWithOneArgument(ExecState*, Structure*, EncodedJSValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValStrict(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValNonStrict(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValCellStrict(ExecState*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValCellNonStrict(ExecState*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValBeyondArrayBoundsStrict(ExecState*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValBeyondArrayBoundsNonStrict(ExecState*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsNonStrict(ExecState*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectStrict(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectNonStrict(ExecState*, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectCellStrict(ExecState*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectCellNonStrict(ExecState*, JSCell*, EncodedJSValue encodedProperty, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsStrict(ExecState*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsNonStrict(ExecState*, JSObject*, int32_t index, EncodedJSValue encodedValue) WTF_INTERNAL;
void JIT_OPERATION operationPutDoubleByValBeyondArrayBoundsStrict(ExecState*, JSObject*, int32_t index, double value) WTF_INTERNAL;
void JIT_OPERATION operationPutDoubleByValBeyondArrayBoundsNonStrict(ExecState*, JSObject*, int32_t index, double value) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPush(ExecState*, EncodedJSValue encodedValue, JSArray*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPushDouble(ExecState*, double value, JSArray*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPop(ExecState*, JSArray*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationArrayPopAndRecoverLength(ExecState*, JSArray*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationRegExpExec(ExecState*, JSCell*, JSCell*) WTF_INTERNAL;
// These comparisons return a boolean within a size_t such that the value is zero extended to fill the register.
size_t JIT_OPERATION operationRegExpTest(ExecState*, JSCell*, JSCell*) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareStrictEqCell(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
size_t JIT_OPERATION operationCompareStrictEq(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2) WTF_INTERNAL;
JSCell* JIT_OPERATION operationCreateInlinedArguments(ExecState*, InlineCallFrame*) WTF_INTERNAL;
void JIT_OPERATION operationTearOffInlinedArguments(ExecState*, JSCell*, JSCell*, InlineCallFrame*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetInlinedArgumentByVal(ExecState*, int32_t, InlineCallFrame*, int32_t) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationGetArgumentByVal(ExecState*, int32_t, int32_t) WTF_INTERNAL;
JSCell* JIT_OPERATION operationNewFunctionNoCheck(ExecState*, JSCell*) WTF_INTERNAL;
double JIT_OPERATION operationFModOnInts(int32_t, int32_t) WTF_INTERNAL;
size_t JIT_OPERATION operationIsObject(ExecState*, EncodedJSValue) WTF_INTERNAL;
size_t JIT_OPERATION operationIsFunction(EncodedJSValue) WTF_INTERNAL;
JSCell* JIT_OPERATION operationTypeOf(ExecState*, JSCell*) WTF_INTERNAL;
char* JIT_OPERATION operationAllocatePropertyStorageWithInitialCapacity(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION operationAllocatePropertyStorage(ExecState*, size_t newSize) WTF_INTERNAL;
char* JIT_OPERATION operationReallocateButterflyToHavePropertyStorageWithInitialCapacity(ExecState*, JSObject*) WTF_INTERNAL;
char* JIT_OPERATION operationReallocateButterflyToGrowPropertyStorage(ExecState*, JSObject*, size_t newSize) WTF_INTERNAL;
char* JIT_OPERATION operationEnsureInt32(ExecState*, JSCell*);
char* JIT_OPERATION operationEnsureDouble(ExecState*, JSCell*);
char* JIT_OPERATION operationEnsureContiguous(ExecState*, JSCell*);
char* JIT_OPERATION operationRageEnsureContiguous(ExecState*, JSCell*);
char* JIT_OPERATION operationEnsureArrayStorage(ExecState*, JSCell*);
StringImpl* JIT_OPERATION operationResolveRope(ExecState*, JSString*);
JSString* JIT_OPERATION operationSingleCharacterString(ExecState*, int32_t);

JSCell* JIT_OPERATION operationNewStringObject(ExecState*, JSString*, Structure*);
JSCell* JIT_OPERATION operationToStringOnCell(ExecState*, JSCell*);
JSCell* JIT_OPERATION operationToString(ExecState*, EncodedJSValue);
JSCell* JIT_OPERATION operationMakeRope2(ExecState*, JSString*, JSString*);
JSCell* JIT_OPERATION operationMakeRope3(ExecState*, JSString*, JSString*, JSString*);
char* JIT_OPERATION operationFindSwitchImmTargetForDouble(ExecState*, EncodedJSValue, size_t tableIndex);
char* JIT_OPERATION operationSwitchString(ExecState*, size_t tableIndex, JSString*);
void JIT_OPERATION operationInvalidate(ExecState*, VariableWatchpointSet*);

#if ENABLE(FTL_JIT)
// FIXME: Make calls work well. Currently they're a pure regression.
// https://bugs.webkit.org/show_bug.cgi?id=113621
EncodedJSValue JIT_OPERATION operationFTLCall(ExecState*) WTF_INTERNAL;
EncodedJSValue JIT_OPERATION operationFTLConstruct(ExecState*) WTF_INTERNAL;
#endif // ENABLE(FTL_JIT)

// These operations implement the implicitly called ToInt32 and ToBoolean conversions from ES5.
// This conversion returns an int32_t within a size_t such that the value is zero extended to fill the register.
size_t JIT_OPERATION dfgConvertJSValueToInt32(ExecState*, EncodedJSValue) WTF_INTERNAL;

void JIT_OPERATION debugOperationPrintSpeculationFailure(ExecState*, void*, void*) WTF_INTERNAL;

void JIT_OPERATION triggerReoptimizationNow(CodeBlock*) WTF_INTERNAL;

#if ENABLE(FTL_JIT)
void JIT_OPERATION triggerTierUpNow(ExecState*) WTF_INTERNAL;
char* JIT_OPERATION triggerOSREntryNow(ExecState*, int32_t bytecodeIndex, int32_t streamIndex) WTF_INTERNAL;
#endif // ENABLE(FTL_JIT)

} // extern "C"

inline P_JITOperation_EStZ operationNewTypedArrayWithSizeForType(TypedArrayType type)
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
    return 0;
}

inline P_JITOperation_EStJ operationNewTypedArrayWithOneArgumentForType(TypedArrayType type)
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
    return 0;
}

} } // namespace JSC::DFG

#endif
#endif
