/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#include "DFGOperations.h"

#include "ArrayConstructor.h"
#include "ButterflyInlines.h"
#include "ClonedArguments.h"
#include "CodeBlock.h"
#include "CommonSlowPaths.h"
#include "DFGDriver.h"
#include "DFGJITCode.h"
#include "DFGOSRExit.h"
#include "DFGThunks.h"
#include "DFGToFTLDeferredCompilationCallback.h"
#include "DFGToFTLForOSREntryDeferredCompilationCallback.h"
#include "DFGWorklist.h"
#include "DefinePropertyAttributes.h"
#include "DirectArguments.h"
#include "FTLForOSREntryJITCode.h"
#include "FTLOSREntry.h"
#include "FrameTracers.h"
#include "HasOwnPropertyCache.h"
#include "HostCallReturnValue.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITExceptions.h"
#include "JSArrayInlines.h"
#include "JSBigInt.h"
#include "JSCInlines.h"
#include "JSFixedArray.h"
#include "JSGenericTypedArrayViewConstructorInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSImmutableButterfly.h"
#include "JSLexicalEnvironment.h"
#include "JSMap.h"
#include "JSPropertyNameEnumerator.h"
#include "JSSet.h"
#include "JSWeakMap.h"
#include "JSWeakSet.h"
#include "NumberConstructor.h"
#include "ObjectConstructor.h"
#include "Operations.h"
#include "ParseInt.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObjectInlines.h"
#include "Repatch.h"
#include "ScopedArguments.h"
#include "StringConstructor.h"
#include "SuperSampler.h"
#include "Symbol.h"
#include "TypeProfilerLog.h"
#include "TypedArrayInlines.h"
#include "VMInlines.h"
#include <wtf/InlineASM.h>
#include <wtf/Variant.h>

#if ENABLE(JIT)
#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

template<bool strict, bool direct>
static inline void putByVal(ExecState* exec, VM& vm, JSValue baseValue, uint32_t index, JSValue value)
{
    ASSERT(isIndex(index));
    if (direct) {
        RELEASE_ASSERT(baseValue.isObject());
        asObject(baseValue)->putDirectIndex(exec, index, value, 0, strict ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        return;
    }
    if (baseValue.isObject()) {
        JSObject* object = asObject(baseValue);
        if (object->canSetIndexQuickly(index)) {
            object->setIndexQuickly(vm, index, value);
            return;
        }

        object->methodTable(vm)->putByIndex(object, exec, index, value, strict);
        return;
    }

    baseValue.putByIndex(exec, index, value, strict);
}

template<bool strict, bool direct>
ALWAYS_INLINE static void putByValInternal(ExecState* exec, VM& vm, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);
    JSValue value = JSValue::decode(encodedValue);

    if (LIKELY(property.isUInt32())) {
        // Despite its name, JSValue::isUInt32 will return true only for positive boxed int32_t; all those values are valid array indices.
        ASSERT(isIndex(property.asUInt32()));
        scope.release();
        putByVal<strict, direct>(exec, vm, baseValue, property.asUInt32(), value);
        return;
    }

    if (property.isDouble()) {
        double propertyAsDouble = property.asDouble();
        uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
        if (propertyAsDouble == propertyAsUInt32 && isIndex(propertyAsUInt32)) {
            scope.release();
            putByVal<strict, direct>(exec, vm, baseValue, propertyAsUInt32, value);
            return;
        }
    }

    // Don't put to an object if toString throws an exception.
    auto propertyName = property.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, void());

    PutPropertySlot slot(baseValue, strict);
    if (direct) {
        RELEASE_ASSERT(baseValue.isObject());
        JSObject* baseObject = asObject(baseValue);
        if (std::optional<uint32_t> index = parseIndex(propertyName)) {
            scope.release();
            baseObject->putDirectIndex(exec, index.value(), value, 0, strict ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
            return;
        }
        scope.release();
        CommonSlowPaths::putDirectWithReify(vm, exec, baseObject, propertyName, value, slot);
        return;
    }
    scope.release();
    baseValue.put(exec, propertyName, value, slot);
}

template<bool strict, bool direct>
ALWAYS_INLINE static void putByValCellInternal(ExecState* exec, VM& vm, JSCell* base, PropertyName propertyName, JSValue value)
{
    PutPropertySlot slot(base, strict);
    if (direct) {
        RELEASE_ASSERT(base->isObject());
        JSObject* baseObject = asObject(base);
        if (std::optional<uint32_t> index = parseIndex(propertyName)) {
            baseObject->putDirectIndex(exec, index.value(), value, 0, strict ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
            return;
        }
        CommonSlowPaths::putDirectWithReify(vm, exec, baseObject, propertyName, value, slot);
        return;
    }
    base->putInline(exec, propertyName, value, slot);
}

template<bool strict, bool direct>
ALWAYS_INLINE static void putByValCellStringInternal(ExecState* exec, VM& vm, JSCell* base, JSString* property, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = property->toIdentifier(exec);
    RETURN_IF_EXCEPTION(scope, void());

    scope.release();
    putByValCellInternal<strict, direct>(exec, vm, base, propertyName, value);
}

template<typename ViewClass>
char* newTypedArrayWithSize(ExecState* exec, Structure* structure, int32_t size, char* vector)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (size < 0) {
        throwException(exec, scope, createRangeError(exec, "Requested length is negative"_s));
        return 0;
    }
    
    if (vector)
        return bitwise_cast<char*>(ViewClass::createWithFastVector(exec, structure, size, vector));

    RELEASE_AND_RETURN(scope, bitwise_cast<char*>(ViewClass::create(exec, structure, size)));
}

template <bool strict>
static ALWAYS_INLINE void putWithThis(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedValue, const Identifier& ident)
{
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue thisVal = JSValue::decode(encodedThis);
    JSValue putValue = JSValue::decode(encodedValue);
    PutPropertySlot slot(thisVal, strict);
    baseValue.putInline(exec, ident, putValue, slot);
}

static ALWAYS_INLINE EncodedJSValue parseIntResult(double input)
{
    int asInt = static_cast<int>(input);
    if (static_cast<double>(asInt) == input)
        return JSValue::encode(jsNumber(asInt));
    return JSValue::encode(jsNumber(input));
}

ALWAYS_INLINE static JSValue getByValObject(ExecState* exec, VM& vm, JSObject* base, PropertyName propertyName)
{
    Structure& structure = *base->structure(vm);
    if (JSCell::canUseFastGetOwnProperty(structure)) {
        if (JSValue result = base->fastGetOwnProperty(vm, structure, propertyName))
            return result;
    }
    return base->get(exec, propertyName);
}

extern "C" {

EncodedJSValue JIT_OPERATION operationToThis(ExecState* exec, EncodedJSValue encodedOp)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return JSValue::encode(JSValue::decode(encodedOp).toThis(exec, NotStrictMode));
}

EncodedJSValue JIT_OPERATION operationToThisStrict(ExecState* exec, EncodedJSValue encodedOp)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return JSValue::encode(JSValue::decode(encodedOp).toThis(exec, StrictMode));
}

JSCell* JIT_OPERATION operationObjectCreate(ExecState* exec, EncodedJSValue encodedPrototype)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue prototype = JSValue::decode(encodedPrototype);

    if (!prototype.isObject() && !prototype.isNull()) {
        throwVMTypeError(exec, scope, "Object prototype may only be an Object or null."_s);
        return nullptr;
    }

    if (prototype.isObject())
        RELEASE_AND_RETURN(scope, constructEmptyObject(exec, asObject(prototype)));
    RELEASE_AND_RETURN(scope, constructEmptyObject(exec, exec->lexicalGlobalObject()->nullPrototypeObjectStructure()));
}

JSCell* JIT_OPERATION operationObjectCreateObject(ExecState* exec, JSObject* prototype)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return constructEmptyObject(exec, prototype);
}

JSCell* JIT_OPERATION operationCreateThis(ExecState* exec, JSObject* constructor, uint32_t inlineCapacity)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (constructor->type() == JSFunctionType && jsCast<JSFunction*>(constructor)->canUseAllocationProfile()) {
        auto rareData = jsCast<JSFunction*>(constructor)->ensureRareDataAndAllocationProfile(exec, inlineCapacity);
        RETURN_IF_EXCEPTION(scope, nullptr);
        ObjectAllocationProfile* allocationProfile = rareData->objectAllocationProfile();
        Structure* structure = allocationProfile->structure();
        JSObject* result = constructEmptyObject(exec, structure);
        if (structure->hasPolyProto()) {
            JSObject* prototype = allocationProfile->prototype();
            ASSERT(prototype == jsCast<JSFunction*>(constructor)->prototypeForConstruction(vm, exec));
            result->putDirect(vm, knownPolyProtoOffset, prototype);
            prototype->didBecomePrototype();
            ASSERT_WITH_MESSAGE(!hasIndexedProperties(result->indexingType()), "We rely on JSFinalObject not starting out with an indexing type otherwise we would potentially need to convert to slow put storage");
        }
        return result;
    }

    JSValue proto = constructor->get(exec, vm.propertyNames->prototype);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (proto.isObject())
        return constructEmptyObject(exec, asObject(proto));
    return constructEmptyObject(exec);
}

JSCell* JIT_OPERATION operationCallObjectConstructor(ExecState* exec, JSGlobalObject* globalObject, EncodedJSValue encodedTarget)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue value = JSValue::decode(encodedTarget);
    ASSERT(!value.isObject());

    if (value.isUndefinedOrNull())
        return constructEmptyObject(exec, globalObject->objectPrototype());
    return value.toObject(exec, globalObject);
}

JSCell* JIT_OPERATION operationToObject(ExecState* exec, JSGlobalObject* globalObject, EncodedJSValue encodedTarget, UniquedStringImpl* errorMessage)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue value = JSValue::decode(encodedTarget);
    ASSERT(!value.isObject());

    if (UNLIKELY(value.isUndefinedOrNull())) {
        if (errorMessage->length()) {
            throwVMTypeError(exec, scope, errorMessage);
            return nullptr;
        }
    }

    RELEASE_AND_RETURN(scope, value.toObject(exec, globalObject));
}

EncodedJSValue JIT_OPERATION operationValueBitAnd(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    auto leftNumeric = op1.toBigIntOrInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto rightNumeric = op2.toBigIntOrInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::bitwiseAnd(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            return JSValue::encode(result);
        }

        return throwVMTypeError(exec, scope, "Invalid mix of BigInt and other type in bitwise 'and' operation.");
    }

    return JSValue::encode(jsNumber(WTF::get<int32_t>(leftNumeric) & WTF::get<int32_t>(rightNumeric)));
}

EncodedJSValue JIT_OPERATION operationValueBitOr(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    auto leftNumeric = op1.toBigIntOrInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto rightNumeric = op2.toBigIntOrInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::bitwiseOr(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            return JSValue::encode(result);
        }

        return throwVMTypeError(exec, scope, "Invalid mix of BigInt and other type in bitwise 'or' operation.");
    }

    return JSValue::encode(jsNumber(WTF::get<int32_t>(leftNumeric) | WTF::get<int32_t>(rightNumeric)));
}

EncodedJSValue JIT_OPERATION operationValueBitXor(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    int32_t a = op1.toInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    int32_t b = op2.toInt32(exec);
    return JSValue::encode(jsNumber(a ^ b));
}

EncodedJSValue JIT_OPERATION operationValueBitLShift(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    int32_t a = op1.toInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    uint32_t b = op2.toUInt32(exec);
    return JSValue::encode(jsNumber(a << (b & 0x1f)));
}

EncodedJSValue JIT_OPERATION operationValueBitRShift(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    int32_t a = op1.toInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    uint32_t b = op2.toUInt32(exec);
    return JSValue::encode(jsNumber(a >> (b & 0x1f)));
}

EncodedJSValue JIT_OPERATION operationValueBitURShift(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    uint32_t a = op1.toUInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    uint32_t b = op2.toUInt32(exec);
    return JSValue::encode(jsNumber(static_cast<int32_t>(a >> (b & 0x1f))));
}

EncodedJSValue JIT_OPERATION operationValueAddNotNumber(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    ASSERT(!op1.isNumber() || !op2.isNumber());
    
    if (op1.isString() && !op2.isObject())
        return JSValue::encode(jsString(exec, asString(op1), op2.toString(exec)));

    return JSValue::encode(jsAddSlowCase(exec, op1, op2));
}

EncodedJSValue JIT_OPERATION operationValueDiv(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    auto leftNumeric = op1.toNumeric(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto rightNumeric = op2.toNumeric(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::divide(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            return JSValue::encode(result);
        }

        return throwVMTypeError(exec, scope, "Invalid mix of BigInt and other type in division operation.");
    }

    scope.release();

    double a = WTF::get<double>(leftNumeric);
    double b = WTF::get<double>(rightNumeric);
    return JSValue::encode(jsNumber(a / b));
}

double JIT_OPERATION operationArithAbs(ExecState* exec, EncodedJSValue encodedOp1)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(exec);
    RETURN_IF_EXCEPTION(scope, PNaN);
    return fabs(a);
}

uint32_t JIT_OPERATION operationArithClz32(ExecState* exec, EncodedJSValue encodedOp1)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    uint32_t value = op1.toUInt32(exec);
    RETURN_IF_EXCEPTION(scope, 0);
    return clz32(value);
}

double JIT_OPERATION operationArithFRound(ExecState* exec, EncodedJSValue encodedOp1)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(exec);
    RETURN_IF_EXCEPTION(scope, PNaN);
    return static_cast<float>(a);
}

#define DFG_ARITH_UNARY(capitalizedName, lowerName) \
double JIT_OPERATION operationArith##capitalizedName(ExecState* exec, EncodedJSValue encodedOp1) \
{ \
    VM* vm = &exec->vm(); \
    NativeCallFrameTracer tracer(vm, exec); \
    auto scope = DECLARE_THROW_SCOPE(*vm); \
    JSValue op1 = JSValue::decode(encodedOp1); \
    double result = op1.toNumber(exec); \
    RETURN_IF_EXCEPTION(scope, PNaN); \
    return JSC::Math::lowerName(result); \
}
    FOR_EACH_DFG_ARITH_UNARY_OP(DFG_ARITH_UNARY)
#undef DFG_ARITH_UNARY

double JIT_OPERATION operationArithSqrt(ExecState* exec, EncodedJSValue encodedOp1)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue op1 = JSValue::decode(encodedOp1);
    double a = op1.toNumber(exec);
    RETURN_IF_EXCEPTION(scope, PNaN);
    return sqrt(a);
}

EncodedJSValue JIT_OPERATION operationArithRound(ExecState* exec, EncodedJSValue encodedArgument)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(jsRound(valueOfArgument)));
}

EncodedJSValue JIT_OPERATION operationArithFloor(ExecState* exec, EncodedJSValue encodedArgument)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(floor(valueOfArgument)));
}

EncodedJSValue JIT_OPERATION operationArithCeil(ExecState* exec, EncodedJSValue encodedArgument)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double valueOfArgument = argument.toNumber(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(ceil(valueOfArgument)));
}

EncodedJSValue JIT_OPERATION operationArithTrunc(ExecState* exec, EncodedJSValue encodedArgument)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    auto scope = DECLARE_THROW_SCOPE(*vm);

    JSValue argument = JSValue::decode(encodedArgument);
    double truncatedValueOfArgument = argument.toIntegerPreserveNaN(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsNumber(truncatedValueOfArgument));
}

static ALWAYS_INLINE EncodedJSValue getByVal(ExecState* exec, JSCell* base, uint32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (base->isObject()) {
        JSObject* object = asObject(base);
        if (object->canGetIndexQuickly(index))
            return JSValue::encode(object->getIndexQuickly(index));
    }

    if (isJSString(base) && asString(base)->canGetIndex(index))
        return JSValue::encode(asString(base)->getIndex(exec, index));

    return JSValue::encode(JSValue(base).get(exec, index));
}

EncodedJSValue JIT_OPERATION operationGetByVal(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);

    if (LIKELY(baseValue.isCell())) {
        JSCell* base = baseValue.asCell();

        if (property.isUInt32())
            RELEASE_AND_RETURN(scope, getByVal(exec, base, property.asUInt32()));

        if (property.isDouble()) {
            double propertyAsDouble = property.asDouble();
            uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
            if (propertyAsUInt32 == propertyAsDouble && isIndex(propertyAsUInt32))
                RELEASE_AND_RETURN(scope, getByVal(exec, base, propertyAsUInt32));

        } else if (property.isString()) {
            Structure& structure = *base->structure(vm);
            if (JSCell::canUseFastGetOwnProperty(structure)) {
                if (RefPtr<AtomicStringImpl> existingAtomicString = asString(property)->toExistingAtomicString(exec)) {
                    if (JSValue result = base->fastGetOwnProperty(vm, structure, existingAtomicString.get()))
                        return JSValue::encode(result);
                }
            }
        }
    }

    baseValue.requireObjectCoercible(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto propertyName = property.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(baseValue.get(exec, propertyName)));
}

EncodedJSValue JIT_OPERATION operationGetByValCell(ExecState* exec, JSCell* base, EncodedJSValue encodedProperty)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue property = JSValue::decode(encodedProperty);

    if (property.isUInt32())
        RELEASE_AND_RETURN(scope, getByVal(exec, base, property.asUInt32()));

    if (property.isDouble()) {
        double propertyAsDouble = property.asDouble();
        uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
        if (propertyAsUInt32 == propertyAsDouble)
            RELEASE_AND_RETURN(scope, getByVal(exec, base, propertyAsUInt32));

    } else if (property.isString()) {
        Structure& structure = *base->structure(vm);
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            if (RefPtr<AtomicStringImpl> existingAtomicString = asString(property)->toExistingAtomicString(exec)) {
                if (JSValue result = base->fastGetOwnProperty(vm, structure, existingAtomicString.get()))
                    return JSValue::encode(result);
            }
        }
    }

    auto propertyName = property.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(JSValue(base).get(exec, propertyName)));
}

ALWAYS_INLINE EncodedJSValue getByValCellInt(ExecState* exec, JSCell* base, int32_t index)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    if (index < 0) {
        // Go the slowest way possible because negative indices don't use indexed storage.
        return JSValue::encode(JSValue(base).get(exec, Identifier::from(exec, index)));
    }

    // Use this since we know that the value is out of bounds.
    return JSValue::encode(JSValue(base).get(exec, static_cast<unsigned>(index)));
}

EncodedJSValue JIT_OPERATION operationGetByValObjectInt(ExecState* exec, JSObject* base, int32_t index)
{
    return getByValCellInt(exec, base, index);
}

EncodedJSValue JIT_OPERATION operationGetByValStringInt(ExecState* exec, JSString* base, int32_t index)
{
    return getByValCellInt(exec, base, index);
}

EncodedJSValue JIT_OPERATION operationGetByValObjectString(ExecState* exec, JSCell* base, JSCell* string)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto propertyName = asString(string)->toIdentifier(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(getByValObject(exec, vm, asObject(base), propertyName)));
}

EncodedJSValue JIT_OPERATION operationGetByValObjectSymbol(ExecState* exec, JSCell* base, JSCell* symbol)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return JSValue::encode(getByValObject(exec, vm, asObject(base), asSymbol(symbol)->privateName()));
}

void JIT_OPERATION operationPutByValStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    putByValInternal<true, false>(exec, vm, encodedBase, encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValNonStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    putByValInternal<false, false>(exec, vm, encodedBase, encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValCellStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    putByValInternal<true, false>(exec, vm, JSValue::encode(cell), encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValCellNonStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    putByValInternal<false, false>(exec, vm, JSValue::encode(cell), encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValCellStringStrict(ExecState* exec, JSCell* cell, JSCell* string, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putByValCellStringInternal<true, false>(exec, vm, cell, asString(string), JSValue::decode(encodedValue));
}

void JIT_OPERATION operationPutByValCellStringNonStrict(ExecState* exec, JSCell* cell, JSCell* string, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putByValCellStringInternal<false, false>(exec, vm, cell, asString(string), JSValue::decode(encodedValue));
}

void JIT_OPERATION operationPutByValCellSymbolStrict(ExecState* exec, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putByValCellInternal<true, false>(exec, vm, cell, asSymbol(symbol)->privateName(), JSValue::decode(encodedValue));
}

void JIT_OPERATION operationPutByValCellSymbolNonStrict(ExecState* exec, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putByValCellInternal<false, false>(exec, vm, cell, asSymbol(symbol)->privateName(), JSValue::decode(encodedValue));
}

void JIT_OPERATION operationPutByValBeyondArrayBoundsStrict(ExecState* exec, JSObject* object, int32_t index, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (index >= 0) {
        object->putByIndexInline(exec, index, JSValue::decode(encodedValue), true);
        return;
    }
    
    PutPropertySlot slot(object, true);
    object->methodTable(vm)->put(
        object, exec, Identifier::from(exec, index), JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByValBeyondArrayBoundsNonStrict(ExecState* exec, JSObject* object, int32_t index, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    if (index >= 0) {
        object->putByIndexInline(exec, index, JSValue::decode(encodedValue), false);
        return;
    }
    
    PutPropertySlot slot(object, false);
    object->methodTable(*vm)->put(
        object, exec, Identifier::from(exec, index), JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutDoubleByValBeyondArrayBoundsStrict(ExecState* exec, JSObject* object, int32_t index, double value)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);
    
    if (index >= 0) {
        object->putByIndexInline(exec, index, jsValue, true);
        return;
    }
    
    PutPropertySlot slot(object, true);
    object->methodTable(*vm)->put(
        object, exec, Identifier::from(exec, index), jsValue, slot);
}

void JIT_OPERATION operationPutDoubleByValBeyondArrayBoundsNonStrict(ExecState* exec, JSObject* object, int32_t index, double value)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);
    
    if (index >= 0) {
        object->putByIndexInline(exec, index, jsValue, false);
        return;
    }
    
    PutPropertySlot slot(object, false);
    object->methodTable(*vm)->put(
        object, exec, Identifier::from(exec, index), jsValue, slot);
}

void JIT_OPERATION operationPutDoubleByValDirectBeyondArrayBoundsStrict(ExecState* exec, JSObject* object, int32_t index, double value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);

    if (index >= 0) {
        object->putDirectIndex(exec, index, jsValue, 0, PutDirectIndexShouldThrow);
        return;
    }

    PutPropertySlot slot(object, true);
    CommonSlowPaths::putDirectWithReify(vm, exec, object, Identifier::from(exec, index), jsValue, slot);
}

void JIT_OPERATION operationPutDoubleByValDirectBeyondArrayBoundsNonStrict(ExecState* exec, JSObject* object, int32_t index, double value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);

    if (index >= 0) {
        object->putDirectIndex(exec, index, jsValue);
        return;
    }

    PutPropertySlot slot(object, false);
    CommonSlowPaths::putDirectWithReify(vm, exec, object, Identifier::from(exec, index), jsValue, slot);
}

void JIT_OPERATION operationPutByValDirectStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    putByValInternal<true, true>(exec, vm, encodedBase, encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValDirectNonStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    putByValInternal<false, true>(exec, vm, encodedBase, encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValDirectCellStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    putByValInternal<true, true>(exec, vm, JSValue::encode(cell), encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValDirectCellNonStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    putByValInternal<false, true>(exec, vm, JSValue::encode(cell), encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValDirectCellStringStrict(ExecState* exec, JSCell* cell, JSCell* string, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putByValCellStringInternal<true, true>(exec, vm, cell, asString(string), JSValue::decode(encodedValue));
}

void JIT_OPERATION operationPutByValDirectCellStringNonStrict(ExecState* exec, JSCell* cell, JSCell* string, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putByValCellStringInternal<false, true>(exec, vm, cell, asString(string), JSValue::decode(encodedValue));
}

void JIT_OPERATION operationPutByValDirectCellSymbolStrict(ExecState* exec, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putByValCellInternal<true, true>(exec, vm, cell, asSymbol(symbol)->privateName(), JSValue::decode(encodedValue));
}

void JIT_OPERATION operationPutByValDirectCellSymbolNonStrict(ExecState* exec, JSCell* cell, JSCell* symbol, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putByValCellInternal<false, true>(exec, vm, cell, asSymbol(symbol)->privateName(), JSValue::decode(encodedValue));
}

void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsStrict(ExecState* exec, JSObject* object, int32_t index, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    if (index >= 0) {
        object->putDirectIndex(exec, index, JSValue::decode(encodedValue), 0, PutDirectIndexShouldThrow);
        return;
    }
    
    PutPropertySlot slot(object, true);
    CommonSlowPaths::putDirectWithReify(vm, exec, object, Identifier::from(exec, index), JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsNonStrict(ExecState* exec, JSObject* object, int32_t index, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (index >= 0) {
        object->putDirectIndex(exec, index, JSValue::decode(encodedValue));
        return;
    }
    
    PutPropertySlot slot(object, false);
    CommonSlowPaths::putDirectWithReify(vm, exec, object, Identifier::from(exec, index), JSValue::decode(encodedValue), slot);
}

EncodedJSValue JIT_OPERATION operationArrayPush(ExecState* exec, EncodedJSValue encodedValue, JSArray* array)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    array->pushInline(exec, JSValue::decode(encodedValue));
    return JSValue::encode(jsNumber(array->length()));
}

EncodedJSValue JIT_OPERATION operationArrayPushDouble(ExecState* exec, double value, JSArray* array)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    array->pushInline(exec, JSValue(JSValue::EncodeAsDouble, value));
    return JSValue::encode(jsNumber(array->length()));
}

EncodedJSValue JIT_OPERATION operationArrayPushMultiple(ExecState* exec, JSArray* array, void* buffer, int32_t elementCount)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // We assume that multiple JSArray::push calls with ArrayWithInt32/ArrayWithContiguous do not cause JS traps.
    // If it can cause any JS interactions, we can call the caller JS function of this function and overwrite the
    // content of ScratchBuffer. If the IndexingType is now ArrayWithInt32/ArrayWithContiguous, we can ensure
    // that there is no indexed accessors in this object and its prototype chain.
    //
    // ArrayWithArrayStorage is also OK. It can have indexed accessors. But if you define an indexed accessor, the array's length
    // becomes larger than that index. So Array#push never overlaps with this accessor. So accessors are never called unless
    // the IndexingType is ArrayWithSlowPutArrayStorage which could have an indexed accessor in a prototype chain.
    RELEASE_ASSERT(!shouldUseSlowPut(array->indexingType()));

    EncodedJSValue* values = static_cast<EncodedJSValue*>(buffer);
    for (int32_t i = 0; i < elementCount; ++i) {
        array->pushInline(exec, JSValue::decode(values[i]));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(jsNumber(array->length()));
}

EncodedJSValue JIT_OPERATION operationArrayPushDoubleMultiple(ExecState* exec, JSArray* array, void* buffer, int32_t elementCount)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // We assume that multiple JSArray::push calls with ArrayWithDouble do not cause JS traps.
    // If it can cause any JS interactions, we can call the caller JS function of this function and overwrite the
    // content of ScratchBuffer. If the IndexingType is now ArrayWithDouble, we can ensure
    // that there is no indexed accessors in this object and its prototype chain.
    ASSERT(array->indexingMode() == ArrayWithDouble);

    double* values = static_cast<double*>(buffer);
    for (int32_t i = 0; i < elementCount; ++i) {
        array->pushInline(exec, JSValue(JSValue::EncodeAsDouble, values[i]));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(jsNumber(array->length()));
}

EncodedJSValue JIT_OPERATION operationArrayPop(ExecState* exec, JSArray* array)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return JSValue::encode(array->pop(exec));
}
        
EncodedJSValue JIT_OPERATION operationArrayPopAndRecoverLength(ExecState* exec, JSArray* array)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    array->butterfly()->setPublicLength(array->butterfly()->publicLength() + 1);
    
    return JSValue::encode(array->pop(exec));
}
        
EncodedJSValue JIT_OPERATION operationRegExpExecString(ExecState* exec, JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* argument)
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    return JSValue::encode(regExpObject->execInline(exec, globalObject, argument));
}
        
EncodedJSValue JIT_OPERATION operationRegExpExec(ExecState* exec, JSGlobalObject* globalObject, RegExpObject* regExpObject, EncodedJSValue encodedArgument)
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue argument = JSValue::decode(encodedArgument);

    JSString* input = argument.toStringOrNull(exec);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        return encodedJSValue();
    RELEASE_AND_RETURN(scope, JSValue::encode(regExpObject->execInline(exec, globalObject, input)));
}
        
EncodedJSValue JIT_OPERATION operationRegExpExecGeneric(ExecState* exec, JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedArgument)
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(encodedBase);
    JSValue argument = JSValue::decode(encodedArgument);
    
    auto* regexp = jsDynamicCast<RegExpObject*>(vm, base);
    if (UNLIKELY(!regexp))
        return throwVMTypeError(exec, scope);

    JSString* input = argument.toStringOrNull(exec);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        return JSValue::encode(jsUndefined());
    RELEASE_AND_RETURN(scope, JSValue::encode(regexp->exec(exec, globalObject, input)));
}

EncodedJSValue JIT_OPERATION operationRegExpExecNonGlobalOrSticky(ExecState* exec, JSGlobalObject* globalObject, RegExp* regExp, JSString* string)
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExpConstructor* regExpConstructor = globalObject->regExpConstructor();
    String input = string->value(exec);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned lastIndex = 0;
    MatchResult result;
    JSArray* array = createRegExpMatchesArray(vm, globalObject, string, input, regExp, lastIndex, result);
    if (!array) {
        ASSERT(!scope.exception());
        return JSValue::encode(jsNull());
    }

    RETURN_IF_EXCEPTION(scope, { });
    regExpConstructor->recordMatch(vm, regExp, string, result);
    return JSValue::encode(array);
}

EncodedJSValue JIT_OPERATION operationRegExpMatchFastString(ExecState* exec, JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* argument)
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (!regExpObject->regExp()->global())
        return JSValue::encode(regExpObject->execInline(exec, globalObject, argument));
    return JSValue::encode(regExpObject->matchGlobal(exec, globalObject, argument));
}

EncodedJSValue JIT_OPERATION operationRegExpMatchFastGlobalString(ExecState* exec, JSGlobalObject* globalObject, RegExp* regExp, JSString* string)
{
    SuperSamplerScope superSamplerScope(false);

    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(regExp->global());

    String s = string->value(exec);
    RETURN_IF_EXCEPTION(scope, { });

    RegExpConstructor* regExpConstructor = globalObject->regExpConstructor();

    if (regExp->unicode()) {
        unsigned stringLength = s.length();
        RELEASE_AND_RETURN(scope, JSValue::encode(collectMatches(
            vm, exec, string, s, regExpConstructor, regExp,
            [&] (size_t end) -> size_t {
                return advanceStringUnicode(s, stringLength, end);
            })));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(collectMatches(
        vm, exec, string, s, regExpConstructor, regExp,
        [&] (size_t end) -> size_t {
            return end + 1;
        })));
}

EncodedJSValue JIT_OPERATION operationParseIntNoRadixGeneric(ExecState* exec, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return toStringView(exec, JSValue::decode(value), [&] (StringView view) {
        // This version is as if radix was undefined. Hence, undefined.toNumber() === 0.
        return parseIntResult(parseInt(view, 0));
    });
}

EncodedJSValue JIT_OPERATION operationParseIntStringNoRadix(ExecState* exec, JSString* string)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto viewWithString = string->viewWithUnderlyingString(exec);
    RETURN_IF_EXCEPTION(scope, { });

    // This version is as if radix was undefined. Hence, undefined.toNumber() === 0.
    return parseIntResult(parseInt(viewWithString.view, 0));
}

EncodedJSValue JIT_OPERATION operationParseIntString(ExecState* exec, JSString* string, int32_t radix)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto viewWithString = string->viewWithUnderlyingString(exec);
    RETURN_IF_EXCEPTION(scope, { });

    return parseIntResult(parseInt(viewWithString.view, radix));
}

EncodedJSValue JIT_OPERATION operationParseIntGeneric(ExecState* exec, EncodedJSValue value, int32_t radix)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return toStringView(exec, JSValue::decode(value), [&] (StringView view) {
        return parseIntResult(parseInt(view, radix));
    });
}
        
size_t JIT_OPERATION operationRegExpTestString(ExecState* exec, JSGlobalObject* globalObject, RegExpObject* regExpObject, JSString* input)
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return regExpObject->testInline(exec, globalObject, input);
}

size_t JIT_OPERATION operationRegExpTest(ExecState* exec, JSGlobalObject* globalObject, RegExpObject* regExpObject, EncodedJSValue encodedArgument)
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue argument = JSValue::decode(encodedArgument);

    JSString* input = argument.toStringOrNull(exec);
    if (!input)
        return false;
    return regExpObject->testInline(exec, globalObject, input);
}

size_t JIT_OPERATION operationRegExpTestGeneric(ExecState* exec, JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedArgument)
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(encodedBase);
    JSValue argument = JSValue::decode(encodedArgument);

    auto* regexp = jsDynamicCast<RegExpObject*>(vm, base);
    if (UNLIKELY(!regexp)) {
        throwTypeError(exec, scope);
        return false;
    }

    JSString* input = argument.toStringOrNull(exec);
    EXCEPTION_ASSERT(!!scope.exception() == !input);
    if (!input)
        return false;
    RELEASE_AND_RETURN(scope, regexp->test(exec, globalObject, input));
}

JSCell* JIT_OPERATION operationSubBigInt(ExecState* exec, JSCell* op1, JSCell* op2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSBigInt::sub(exec, leftOperand, rightOperand);
}

JSCell* JIT_OPERATION operationBitAndBigInt(ExecState* exec, JSCell* op1, JSCell* op2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);

    return JSBigInt::bitwiseAnd(exec, leftOperand, rightOperand);
}

JSCell* JIT_OPERATION operationAddBigInt(ExecState* exec, JSCell* op1, JSCell* op2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSBigInt::add(exec, leftOperand, rightOperand);
}

JSCell* JIT_OPERATION operationBitOrBigInt(ExecState* exec, JSCell* op1, JSCell* op2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSBigInt* leftOperand = jsCast<JSBigInt*>(op1);
    JSBigInt* rightOperand = jsCast<JSBigInt*>(op2);
    
    return JSBigInt::bitwiseOr(exec, leftOperand, rightOperand);
}

size_t JIT_OPERATION operationCompareStrictEqCell(ExecState* exec, JSCell* op1, JSCell* op2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return JSValue::strictEqualSlowCaseInline(exec, op1, op2);
}

size_t JIT_OPERATION operationSameValue(ExecState* exec, EncodedJSValue arg1, EncodedJSValue arg2)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return sameValue(exec, JSValue::decode(arg1), JSValue::decode(arg2));
}

EncodedJSValue JIT_OPERATION operationToPrimitive(ExecState* exec, EncodedJSValue value)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return JSValue::encode(JSValue::decode(value).toPrimitive(exec));
}

EncodedJSValue JIT_OPERATION operationToNumber(ExecState* exec, EncodedJSValue value)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return JSValue::encode(jsNumber(JSValue::decode(value).toNumber(exec)));
}

EncodedJSValue JIT_OPERATION operationGetByValWithThis(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedSubscript)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue thisVal = JSValue::decode(encodedThis);
    JSValue subscript = JSValue::decode(encodedSubscript);

    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        Structure& structure = *baseValue.asCell()->structure(vm);
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            if (RefPtr<AtomicStringImpl> existingAtomicString = asString(subscript)->toExistingAtomicString(exec)) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomicString.get()))
                    return JSValue::encode(result);
            }
        }
    }
    
    PropertySlot slot(thisVal, PropertySlot::PropertySlot::InternalMethodType::Get);
    if (subscript.isUInt32()) {
        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            return JSValue::encode(asString(baseValue)->getIndex(exec, i));
        
        RELEASE_AND_RETURN(scope, JSValue::encode(baseValue.get(exec, i, slot)));
    }

    baseValue.requireObjectCoercible(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    auto property = subscript.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(baseValue.get(exec, property, slot)));
}

void JIT_OPERATION operationPutByIdWithThisStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedValue, UniquedStringImpl* impl)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putWithThis<true>(exec, encodedBase, encodedThis, encodedValue, Identifier::fromUid(exec, impl));
}

void JIT_OPERATION operationPutByIdWithThis(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedValue, UniquedStringImpl* impl)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putWithThis<false>(exec, encodedBase, encodedThis, encodedValue, Identifier::fromUid(exec, impl));
}

void JIT_OPERATION operationPutByValWithThisStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier property = JSValue::decode(encodedSubscript).toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    putWithThis<true>(exec, encodedBase, encodedThis, encodedValue, property);
}

void JIT_OPERATION operationPutByValWithThis(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedThis, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier property = JSValue::decode(encodedSubscript).toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    putWithThis<false>(exec, encodedBase, encodedThis, encodedValue, property);
}

ALWAYS_INLINE static void defineDataProperty(ExecState* exec, VM& vm, JSObject* base, const Identifier& propertyName, JSValue value, int32_t attributes)
{
    PropertyDescriptor descriptor = toPropertyDescriptor(value, jsUndefined(), jsUndefined(), DefinePropertyAttributes(attributes));
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    if (base->methodTable(vm)->defineOwnProperty == JSObject::defineOwnProperty)
        JSObject::defineOwnProperty(base, exec, propertyName, descriptor, true);
    else
        base->methodTable(vm)->defineOwnProperty(base, exec, propertyName, descriptor, true);
}

void JIT_OPERATION operationDefineDataProperty(ExecState* exec, JSObject* base, EncodedJSValue encodedProperty, EncodedJSValue encodedValue, int32_t attributes)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = JSValue::decode(encodedProperty).toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    defineDataProperty(exec, vm, base, propertyName, JSValue::decode(encodedValue), attributes);
}

void JIT_OPERATION operationDefineDataPropertyString(ExecState* exec, JSObject* base, JSString* property, EncodedJSValue encodedValue, int32_t attributes)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = property->toIdentifier(exec);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    defineDataProperty(exec, vm, base, propertyName, JSValue::decode(encodedValue), attributes);
}

void JIT_OPERATION operationDefineDataPropertyStringIdent(ExecState* exec, JSObject* base, UniquedStringImpl* property, EncodedJSValue encodedValue, int32_t attributes)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    defineDataProperty(exec, vm, base, Identifier::fromUid(&vm, property), JSValue::decode(encodedValue), attributes);
}

void JIT_OPERATION operationDefineDataPropertySymbol(ExecState* exec, JSObject* base, Symbol* property, EncodedJSValue encodedValue, int32_t attributes)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    defineDataProperty(exec, vm, base, Identifier::fromUid(property->privateName()), JSValue::decode(encodedValue), attributes);
}

ALWAYS_INLINE static void defineAccessorProperty(ExecState* exec, VM& vm, JSObject* base, const Identifier& propertyName, JSObject* getter, JSObject* setter, int32_t attributes)
{
    PropertyDescriptor descriptor = toPropertyDescriptor(jsUndefined(), getter, setter, DefinePropertyAttributes(attributes));
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    if (base->methodTable(vm)->defineOwnProperty == JSObject::defineOwnProperty)
        JSObject::defineOwnProperty(base, exec, propertyName, descriptor, true);
    else
        base->methodTable(vm)->defineOwnProperty(base, exec, propertyName, descriptor, true);
}

void JIT_OPERATION operationDefineAccessorProperty(ExecState* exec, JSObject* base, EncodedJSValue encodedProperty, JSObject* getter, JSObject* setter, int32_t attributes)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = JSValue::decode(encodedProperty).toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, void());
    defineAccessorProperty(exec, vm, base, propertyName, getter, setter, attributes);
}

void JIT_OPERATION operationDefineAccessorPropertyString(ExecState* exec, JSObject* base, JSString* property, JSObject* getter, JSObject* setter, int32_t attributes)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier propertyName = property->toIdentifier(exec);
    RETURN_IF_EXCEPTION(scope, void());
    defineAccessorProperty(exec, vm, base, propertyName, getter, setter, attributes);
}

void JIT_OPERATION operationDefineAccessorPropertyStringIdent(ExecState* exec, JSObject* base, UniquedStringImpl* property, JSObject* getter, JSObject* setter, int32_t attributes)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    defineAccessorProperty(exec, vm, base, Identifier::fromUid(&vm, property), getter, setter, attributes);
}

void JIT_OPERATION operationDefineAccessorPropertySymbol(ExecState* exec, JSObject* base, Symbol* property, JSObject* getter, JSObject* setter, int32_t attributes)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    defineAccessorProperty(exec, vm, base, Identifier::fromUid(property->privateName()), getter, setter, attributes);
}

char* JIT_OPERATION operationNewArray(ExecState* exec, Structure* arrayStructure, void* buffer, size_t size)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return bitwise_cast<char*>(constructArray(exec, arrayStructure, static_cast<JSValue*>(buffer), size));
}

char* JIT_OPERATION operationNewEmptyArray(ExecState* exec, Structure* arrayStructure)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return bitwise_cast<char*>(JSArray::create(*vm, arrayStructure));
}

char* JIT_OPERATION operationNewArrayWithSize(ExecState* exec, Structure* arrayStructure, int32_t size, Butterfly* butterfly)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(size < 0))
        return bitwise_cast<char*>(throwException(exec, scope, createRangeError(exec, "Array size is not a small enough positive integer."_s)));

    JSArray* result;
    if (butterfly)
        result = JSArray::createWithButterfly(vm, nullptr, arrayStructure, butterfly);
    else
        result = JSArray::create(vm, arrayStructure, size);
    return bitwise_cast<char*>(result);
}

char* JIT_OPERATION operationNewArrayWithSizeAndHint(ExecState* exec, Structure* arrayStructure, int32_t size, int32_t vectorLengthHint, Butterfly* butterfly)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(size < 0))
        return bitwise_cast<char*>(throwException(exec, scope, createRangeError(exec, "Array size is not a small enough positive integer."_s)));

    JSArray* result;
    if (butterfly)
        result = JSArray::createWithButterfly(vm, nullptr, arrayStructure, butterfly);
    else {
        result = JSArray::tryCreate(vm, arrayStructure, size, vectorLengthHint);
        RELEASE_ASSERT(result);
    }
    return bitwise_cast<char*>(result);
}

JSCell* JIT_OPERATION operationNewArrayBuffer(ExecState* exec, Structure* arrayStructure, JSCell* immutableButterflyCell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    ASSERT(!arrayStructure->outOfLineCapacity());
    auto* immutableButterfly = jsCast<JSImmutableButterfly*>(immutableButterflyCell);
    ASSERT(arrayStructure->indexingMode() == immutableButterfly->indexingMode() || hasAnyArrayStorage(arrayStructure->indexingMode()));
    auto* result = CommonSlowPaths::allocateNewArrayBuffer(vm, arrayStructure, immutableButterfly);
    ASSERT(result->indexingMode() == result->structure(vm)->indexingMode());
    ASSERT(result->structure(vm) == arrayStructure);
    return result;
}

char* JIT_OPERATION operationNewInt8ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSInt8Array>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewInt8ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSInt8Array>(exec, structure, encodedValue, 0, std::nullopt));
}

char* JIT_OPERATION operationNewInt16ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSInt16Array>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewInt16ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSInt16Array>(exec, structure, encodedValue, 0, std::nullopt));
}

char* JIT_OPERATION operationNewInt32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSInt32Array>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewInt32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSInt32Array>(exec, structure, encodedValue, 0, std::nullopt));
}

char* JIT_OPERATION operationNewUint8ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSUint8Array>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewUint8ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSUint8Array>(exec, structure, encodedValue, 0, std::nullopt));
}

char* JIT_OPERATION operationNewUint8ClampedArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSUint8ClampedArray>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewUint8ClampedArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSUint8ClampedArray>(exec, structure, encodedValue, 0, std::nullopt));
}

char* JIT_OPERATION operationNewUint16ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSUint16Array>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewUint16ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSUint16Array>(exec, structure, encodedValue, 0, std::nullopt));
}

char* JIT_OPERATION operationNewUint32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSUint32Array>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewUint32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSUint32Array>(exec, structure, encodedValue, 0, std::nullopt));
}

char* JIT_OPERATION operationNewFloat32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSFloat32Array>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewFloat32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSFloat32Array>(exec, structure, encodedValue, 0, std::nullopt));
}

char* JIT_OPERATION operationNewFloat64ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length, char* vector)
{
    return newTypedArrayWithSize<JSFloat64Array>(exec, structure, length, vector);
}

char* JIT_OPERATION operationNewFloat64ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSFloat64Array>(exec, structure, encodedValue, 0, std::nullopt));
}

JSCell* JIT_OPERATION operationCreateActivationDirect(ExecState* exec, Structure* structure, JSScope* scope, SymbolTable* table, EncodedJSValue initialValueEncoded)
{
    JSValue initialValue = JSValue::decode(initialValueEncoded);
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return JSLexicalEnvironment::create(vm, structure, scope, table, initialValue);
}

JSCell* JIT_OPERATION operationCreateDirectArguments(ExecState* exec, Structure* structure, uint32_t length, uint32_t minCapacity)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer target(&vm, exec);
    DirectArguments* result = DirectArguments::create(
        vm, structure, length, std::max(length, minCapacity));
    // The caller will store to this object without barriers. Most likely, at this point, this is
    // still a young object and so no barriers are needed. But it's good to be careful anyway,
    // since the GC should be allowed to do crazy (like pretenuring, for example).
    vm.heap.writeBarrier(result);
    return result;
}

JSCell* JIT_OPERATION operationCreateScopedArguments(ExecState* exec, Structure* structure, Register* argumentStart, uint32_t length, JSFunction* callee, JSLexicalEnvironment* scope)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer target(&vm, exec);
    
    // We could pass the ScopedArgumentsTable* as an argument. We currently don't because I
    // didn't feel like changing the max number of arguments for a slow path call from 6 to 7.
    ScopedArgumentsTable* table = scope->symbolTable()->arguments();
    
    return ScopedArguments::createByCopyingFrom(
        vm, structure, argumentStart, length, callee, table, scope);
}

JSCell* JIT_OPERATION operationCreateClonedArguments(ExecState* exec, Structure* structure, Register* argumentStart, uint32_t length, JSFunction* callee)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer target(&vm, exec);
    return ClonedArguments::createByCopyingFrom(
        exec, structure, argumentStart, length, callee);
}

JSCell* JIT_OPERATION operationCreateDirectArgumentsDuringExit(ExecState* exec, InlineCallFrame* inlineCallFrame, JSFunction* callee, uint32_t argumentCount)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer target(&vm, exec);
    
    DeferGCForAWhile deferGC(vm.heap);
    
    CodeBlock* codeBlock;
    if (inlineCallFrame)
        codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
    else
        codeBlock = exec->codeBlock();
    
    unsigned length = argumentCount - 1;
    unsigned capacity = std::max(length, static_cast<unsigned>(codeBlock->numParameters() - 1));
    DirectArguments* result = DirectArguments::create(
        vm, codeBlock->globalObject()->directArgumentsStructure(), length, capacity);
    
    result->setCallee(vm, callee);
    
    Register* arguments =
        exec->registers() + (inlineCallFrame ? inlineCallFrame->stackOffset : 0) +
        CallFrame::argumentOffset(0);
    for (unsigned i = length; i--;)
        result->setIndexQuickly(vm, i, arguments[i].jsValue());
    
    return result;
}

JSCell* JIT_OPERATION operationCreateClonedArgumentsDuringExit(ExecState* exec, InlineCallFrame* inlineCallFrame, JSFunction* callee, uint32_t argumentCount)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer target(&vm, exec);
    
    DeferGCForAWhile deferGC(vm.heap);
    
    CodeBlock* codeBlock;
    if (inlineCallFrame)
        codeBlock = baselineCodeBlockForInlineCallFrame(inlineCallFrame);
    else
        codeBlock = exec->codeBlock();
    
    unsigned length = argumentCount - 1;
    ClonedArguments* result = ClonedArguments::createEmpty(
        vm, codeBlock->globalObject()->clonedArgumentsStructure(), callee, length);
    
    Register* arguments =
        exec->registers() + (inlineCallFrame ? inlineCallFrame->stackOffset : 0) +
        CallFrame::argumentOffset(0);
    for (unsigned i = length; i--;)
        result->putDirectIndex(exec, i, arguments[i].jsValue());

    
    return result;
}

JSCell* JIT_OPERATION operationCreateRest(ExecState* exec, Register* argumentStart, unsigned numberOfParamsToSkip, unsigned arraySize)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    Structure* structure = globalObject->restParameterStructure();
    static_assert(sizeof(Register) == sizeof(JSValue), "This is a strong assumption here.");
    JSValue* argumentsToCopyRegion = bitwise_cast<JSValue*>(argumentStart) + numberOfParamsToSkip;
    return constructArray(exec, structure, argumentsToCopyRegion, arraySize);
}

size_t JIT_OPERATION operationObjectIsObject(ExecState* exec, JSGlobalObject* globalObject, JSCell* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(jsDynamicCast<JSObject*>(vm, object));
    
    if (object->structure(vm)->masqueradesAsUndefined(globalObject))
        return false;
    if (object->isFunction(vm))
        return false;
    return true;
}

size_t JIT_OPERATION operationObjectIsFunction(ExecState* exec, JSGlobalObject* globalObject, JSCell* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(jsDynamicCast<JSObject*>(vm, object));
    
    if (object->structure(vm)->masqueradesAsUndefined(globalObject))
        return false;
    if (object->isFunction(vm))
        return true;
    return false;
}

JSCell* JIT_OPERATION operationTypeOfObject(ExecState* exec, JSGlobalObject* globalObject, JSCell* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(jsDynamicCast<JSObject*>(vm, object));
    
    if (object->structure(vm)->masqueradesAsUndefined(globalObject))
        return vm.smallStrings.undefinedString();
    if (object->isFunction(vm))
        return vm.smallStrings.functionString();
    return vm.smallStrings.objectString();
}

int32_t JIT_OPERATION operationTypeOfObjectAsTypeofType(ExecState* exec, JSGlobalObject* globalObject, JSCell* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(jsDynamicCast<JSObject*>(vm, object));
    
    if (object->structure(vm)->masqueradesAsUndefined(globalObject))
        return static_cast<int32_t>(TypeofType::Undefined);
    if (object->isFunction(vm))
        return static_cast<int32_t>(TypeofType::Function);
    return static_cast<int32_t>(TypeofType::Object);
}

char* JIT_OPERATION operationAllocateSimplePropertyStorageWithInitialCapacity(ExecState* exec)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return reinterpret_cast<char*>(
        Butterfly::createUninitialized(vm, 0, 0, initialOutOfLineCapacity, false, 0));
}

char* JIT_OPERATION operationAllocateSimplePropertyStorage(ExecState* exec, size_t newSize)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return reinterpret_cast<char*>(
        Butterfly::createUninitialized(vm, 0, 0, newSize, false, 0));
}

char* JIT_OPERATION operationAllocateComplexPropertyStorageWithInitialCapacity(ExecState* exec, JSObject* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(!object->structure(vm)->outOfLineCapacity());
    return reinterpret_cast<char*>(
        object->allocateMoreOutOfLineStorage(vm, 0, initialOutOfLineCapacity));
}

char* JIT_OPERATION operationAllocateComplexPropertyStorage(ExecState* exec, JSObject* object, size_t newSize)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return reinterpret_cast<char*>(
        object->allocateMoreOutOfLineStorage(vm, object->structure(vm)->outOfLineCapacity(), newSize));
}

char* JIT_OPERATION operationEnsureInt32(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;

    auto* result = reinterpret_cast<char*>(asObject(cell)->tryMakeWritableInt32(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasInt32(cell->indexingMode())) || !result);
    return result;
}

char* JIT_OPERATION operationEnsureDouble(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;

    auto* result = reinterpret_cast<char*>(asObject(cell)->tryMakeWritableDouble(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasDouble(cell->indexingMode())) || !result);
    return result;
}

char* JIT_OPERATION operationEnsureContiguous(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;
    
    auto* result = reinterpret_cast<char*>(asObject(cell)->tryMakeWritableContiguous(vm).data());
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasContiguous(cell->indexingMode())) || !result);
    return result;
}

char* JIT_OPERATION operationEnsureArrayStorage(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;

    auto* result = reinterpret_cast<char*>(asObject(cell)->ensureArrayStorage(vm));
    ASSERT((!isCopyOnWrite(asObject(cell)->indexingMode()) && hasAnyArrayStorage(cell->indexingMode())) || !result);
    return result;
}

EncodedJSValue JIT_OPERATION operationHasGenericProperty(ExecState* exec, EncodedJSValue encodedBaseValue, JSCell* propertyName)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    if (baseValue.isUndefinedOrNull())
        return JSValue::encode(jsBoolean(false));

    JSObject* base = baseValue.toObject(exec);
    if (!base)
        return JSValue::encode(JSValue());
    return JSValue::encode(jsBoolean(base->hasPropertyGeneric(exec, asString(propertyName)->toIdentifier(exec), PropertySlot::InternalMethodType::GetOwnProperty)));
}

size_t JIT_OPERATION operationHasIndexedPropertyByInt(ExecState* exec, JSCell* baseCell, int32_t subscript, int32_t internalMethodType)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSObject* object = baseCell->toObject(exec, exec->lexicalGlobalObject());
    if (UNLIKELY(subscript < 0)) {
        // Go the slowest way possible because negative indices don't use indexed storage.
        return object->hasPropertyGeneric(exec, Identifier::from(exec, subscript), static_cast<PropertySlot::InternalMethodType>(internalMethodType));
    }
    return object->hasPropertyGeneric(exec, subscript, static_cast<PropertySlot::InternalMethodType>(internalMethodType));
}

JSCell* JIT_OPERATION operationGetPropertyEnumerator(ExecState* exec, EncodedJSValue encodedBase)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = JSValue::decode(encodedBase);
    if (base.isUndefinedOrNull())
        return JSPropertyNameEnumerator::create(vm);

    JSObject* baseObject = base.toObject(exec);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, propertyNameEnumerator(exec, baseObject));
}

JSCell* JIT_OPERATION operationGetPropertyEnumeratorCell(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* base = cell->toObject(exec, exec->lexicalGlobalObject());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, propertyNameEnumerator(exec, base));
}

JSCell* JIT_OPERATION operationToIndexString(ExecState* exec, int32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return jsString(exec, Identifier::from(exec, index).string());
}

JSCell* JIT_OPERATION operationNewRegexpWithLastIndex(ExecState* exec, JSCell* regexpPtr, EncodedJSValue encodedLastIndex)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    RegExp* regexp = static_cast<RegExp*>(regexpPtr);
    ASSERT(regexp->isValid());
    return RegExpObject::create(vm, exec->lexicalGlobalObject()->regExpStructure(), regexp, JSValue::decode(encodedLastIndex));
}

StringImpl* JIT_OPERATION operationResolveRope(ExecState* exec, JSString* string)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return string->value(exec).impl();
}

JSString* JIT_OPERATION operationStringValueOf(ExecState* exec, EncodedJSValue encodedArgument)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = JSValue::decode(encodedArgument);

    if (argument.isString())
        return asString(argument);

    if (auto* stringObject = jsDynamicCast<StringObject*>(vm, argument))
        return stringObject->internalValue();

    throwVMTypeError(exec, scope);
    return nullptr;
}

JSCell* JIT_OPERATION operationStringSubstr(ExecState* exec, JSCell* cell, int32_t from, int32_t span)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = jsCast<JSString*>(cell)->value(exec);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsSubstring(exec, string, from, span);
}

JSString* JIT_OPERATION operationToLowerCase(ExecState* exec, JSString* string, uint32_t failingIndex)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    auto scope = DECLARE_THROW_SCOPE(vm);

    const String& inputString = string->value(exec);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!inputString.length())
        return vm.smallStrings.emptyString();

    String lowercasedString = inputString.is8Bit() ? inputString.convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(failingIndex) : inputString.convertToLowercaseWithoutLocale();
    if (lowercasedString.impl() == inputString.impl())
        return string;
    RELEASE_AND_RETURN(scope, jsString(exec, lowercasedString));
}

char* JIT_OPERATION operationInt32ToString(ExecState* exec, int32_t value, int32_t radix)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(exec, scope, createRangeError(exec, "toString() radix argument must be between 2 and 36"_s));
        return nullptr;
    }

    return reinterpret_cast<char*>(int32ToString(vm, value, radix));
}

char* JIT_OPERATION operationInt52ToString(ExecState* exec, int64_t value, int32_t radix)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(exec, scope, createRangeError(exec, "toString() radix argument must be between 2 and 36"_s));
        return nullptr;
    }

    return reinterpret_cast<char*>(int52ToString(vm, value, radix));
}

char* JIT_OPERATION operationDoubleToString(ExecState* exec, double value, int32_t radix)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (radix < 2 || radix > 36) {
        throwVMError(exec, scope, createRangeError(exec, "toString() radix argument must be between 2 and 36"_s));
        return nullptr;
    }

    return reinterpret_cast<char*>(numberToString(vm, value, radix));
}

char* JIT_OPERATION operationInt32ToStringWithValidRadix(ExecState* exec, int32_t value, int32_t radix)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return reinterpret_cast<char*>(int32ToString(vm, value, radix));
}

char* JIT_OPERATION operationInt52ToStringWithValidRadix(ExecState* exec, int64_t value, int32_t radix)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return reinterpret_cast<char*>(int52ToString(vm, value, radix));
}

char* JIT_OPERATION operationDoubleToStringWithValidRadix(ExecState* exec, double value, int32_t radix)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return reinterpret_cast<char*>(numberToString(vm, value, radix));
}

JSString* JIT_OPERATION operationSingleCharacterString(ExecState* exec, int32_t character)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    return jsSingleCharacterString(exec, static_cast<UChar>(character));
}

JSCell* JIT_OPERATION operationNewStringObject(ExecState* exec, JSString* string, Structure* structure)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    return StringObject::create(vm, structure, string);
}

JSString* JIT_OPERATION operationToStringOnCell(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    return JSValue(cell).toString(exec);
}

JSString* JIT_OPERATION operationToString(ExecState* exec, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return JSValue::decode(value).toString(exec);
}

JSString* JIT_OPERATION operationCallStringConstructorOnCell(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return stringConstructor(exec, cell);
}

JSString* JIT_OPERATION operationCallStringConstructor(ExecState* exec, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return stringConstructor(exec, JSValue::decode(value));
}

JSString* JIT_OPERATION operationMakeRope2(ExecState* exec, JSString* left, JSString* right)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return jsString(exec, left, right);
}

JSString* JIT_OPERATION operationMakeRope3(ExecState* exec, JSString* a, JSString* b, JSString* c)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return jsString(exec, a, b, c);
}

JSString* JIT_OPERATION operationStrCat2(ExecState* exec, EncodedJSValue a, EncodedJSValue b)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!JSValue::decode(a).isSymbol());
    ASSERT(!JSValue::decode(b).isSymbol());
    JSString* str1 = JSValue::decode(a).toString(exec);
    scope.assertNoException(); // Impossible, since we must have been given non-Symbol primitives.
    JSString* str2 = JSValue::decode(b).toString(exec);
    scope.assertNoException();

    RELEASE_AND_RETURN(scope, jsString(exec, str1, str2));
}
    
JSString* JIT_OPERATION operationStrCat3(ExecState* exec, EncodedJSValue a, EncodedJSValue b, EncodedJSValue c)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!JSValue::decode(a).isSymbol());
    ASSERT(!JSValue::decode(b).isSymbol());
    ASSERT(!JSValue::decode(c).isSymbol());
    JSString* str1 = JSValue::decode(a).toString(exec);
    scope.assertNoException(); // Impossible, since we must have been given non-Symbol primitives.
    JSString* str2 = JSValue::decode(b).toString(exec);
    scope.assertNoException();
    JSString* str3 = JSValue::decode(c).toString(exec);
    scope.assertNoException();

    RELEASE_AND_RETURN(scope, jsString(exec, str1, str2, str3));
}

char* JIT_OPERATION operationFindSwitchImmTargetForDouble(
    ExecState* exec, EncodedJSValue encodedValue, size_t tableIndex)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    CodeBlock* codeBlock = exec->codeBlock();
    SimpleJumpTable& table = codeBlock->switchJumpTable(tableIndex);
    JSValue value = JSValue::decode(encodedValue);
    ASSERT(value.isDouble());
    double asDouble = value.asDouble();
    int32_t asInt32 = static_cast<int32_t>(asDouble);
    if (asDouble == asInt32)
        return table.ctiForValue(asInt32).executableAddress<char*>();
    return table.ctiDefault.executableAddress<char*>();
}

char* JIT_OPERATION operationSwitchString(ExecState* exec, size_t tableIndex, JSString* string)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return exec->codeBlock()->stringSwitchJumpTable(tableIndex).ctiForValue(string->value(exec).impl()).executableAddress<char*>();
}

int32_t JIT_OPERATION operationSwitchStringAndGetBranchOffset(ExecState* exec, size_t tableIndex, JSString* string)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return exec->codeBlock()->stringSwitchJumpTable(tableIndex).offsetForValue(string->value(exec).impl(), std::numeric_limits<int32_t>::min());
}

uintptr_t JIT_OPERATION operationCompareStringImplLess(StringImpl* a, StringImpl* b)
{
    return codePointCompare(a, b) < 0;
}

uintptr_t JIT_OPERATION operationCompareStringImplLessEq(StringImpl* a, StringImpl* b)
{
    return codePointCompare(a, b) <= 0;
}

uintptr_t JIT_OPERATION operationCompareStringImplGreater(StringImpl* a, StringImpl* b)
{
    return codePointCompare(a, b) > 0;
}

uintptr_t JIT_OPERATION operationCompareStringImplGreaterEq(StringImpl* a, StringImpl* b)
{
    return codePointCompare(a, b) >= 0;
}

uintptr_t JIT_OPERATION operationCompareStringLess(ExecState* exec, JSString* a, JSString* b)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return codePointCompareLessThan(asString(a)->value(exec), asString(b)->value(exec));
}

uintptr_t JIT_OPERATION operationCompareStringLessEq(ExecState* exec, JSString* a, JSString* b)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return !codePointCompareLessThan(asString(b)->value(exec), asString(a)->value(exec));
}

uintptr_t JIT_OPERATION operationCompareStringGreater(ExecState* exec, JSString* a, JSString* b)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return codePointCompareLessThan(asString(b)->value(exec), asString(a)->value(exec));
}

uintptr_t JIT_OPERATION operationCompareStringGreaterEq(ExecState* exec, JSString* a, JSString* b)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return !codePointCompareLessThan(asString(a)->value(exec), asString(b)->value(exec));
}

void JIT_OPERATION operationNotifyWrite(ExecState* exec, WatchpointSet* set)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    set->touch(vm, "Executed NotifyWrite");
}

void JIT_OPERATION operationThrowStackOverflowForVarargs(ExecState* exec)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwStackOverflowError(exec, scope);
}

int32_t JIT_OPERATION operationSizeOfVarargs(ExecState* exec, EncodedJSValue encodedArguments, uint32_t firstVarArgOffset)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue arguments = JSValue::decode(encodedArguments);
    
    return sizeOfVarargs(exec, arguments, firstVarArgOffset);
}

int32_t JIT_OPERATION operationHasOwnProperty(ExecState* exec, JSObject* thisObject, EncodedJSValue encodedKey)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue key = JSValue::decode(encodedKey);
    Identifier propertyName = key.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, false);

    PropertySlot slot(thisObject, PropertySlot::InternalMethodType::GetOwnProperty);
    bool result = thisObject->hasOwnProperty(exec, propertyName.impl(), slot);
    RETURN_IF_EXCEPTION(scope, false);

    HasOwnPropertyCache* hasOwnPropertyCache = vm.hasOwnPropertyCache();
    ASSERT(hasOwnPropertyCache);
    hasOwnPropertyCache->tryAdd(vm, slot, thisObject, propertyName.impl(), result);
    return result;
}

int32_t JIT_OPERATION operationNumberIsInteger(ExecState* exec, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return NumberConstructor::isIntegerImpl(JSValue::decode(value));
}

int32_t JIT_OPERATION operationArrayIndexOfString(ExecState* exec, Butterfly* butterfly, JSString* searchElement, int32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    int32_t length = butterfly->publicLength();
    auto data = butterfly->contiguous().data();
    for (; index < length; ++index) {
        JSValue value = data[index].get();
        if (!value || !value.isString())
            continue;
        auto* string = asString(value);
        if (string == searchElement)
            return index;
        if (string->equal(exec, searchElement))
            return index;
        RETURN_IF_EXCEPTION(scope, { });
    }
    return -1;
}

int32_t JIT_OPERATION operationArrayIndexOfValueInt32OrContiguous(ExecState* exec, Butterfly* butterfly, EncodedJSValue encodedValue, int32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue searchElement = JSValue::decode(encodedValue);

    int32_t length = butterfly->publicLength();
    auto data = butterfly->contiguous().data();
    for (; index < length; ++index) {
        JSValue value = data[index].get();
        if (!value)
            continue;
        if (JSValue::strictEqual(exec, searchElement, value))
            return index;
        RETURN_IF_EXCEPTION(scope, { });
    }
    return -1;
}

int32_t JIT_OPERATION operationArrayIndexOfValueDouble(ExecState* exec, Butterfly* butterfly, EncodedJSValue encodedValue, int32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue searchElement = JSValue::decode(encodedValue);

    if (!searchElement.isNumber())
        return -1;
    double number = searchElement.asNumber();

    int32_t length = butterfly->publicLength();
    const double* data = butterfly->contiguousDouble().data();
    for (; index < length; ++index) {
        // This comparison ignores NaN.
        if (data[index] == number)
            return index;
    }
    return -1;
}

void JIT_OPERATION operationLoadVarargs(ExecState* exec, int32_t firstElementDest, EncodedJSValue encodedArguments, uint32_t offset, uint32_t length, uint32_t mandatoryMinimum)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue arguments = JSValue::decode(encodedArguments);
    
    loadVarargs(exec, VirtualRegister(firstElementDest), arguments, offset, length);
    
    for (uint32_t i = length; i < mandatoryMinimum; ++i)
        exec->r(firstElementDest + i) = jsUndefined();
}

double JIT_OPERATION operationFModOnInts(int32_t a, int32_t b)
{
    return fmod(a, b);
}

#if USE(JSVALUE32_64)
double JIT_OPERATION operationRandom(JSGlobalObject* globalObject)
{
    return globalObject->weakRandomNumber();
}
#endif

JSCell* JIT_OPERATION operationStringFromCharCode(ExecState* exec, int32_t op1)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    return JSC::stringFromCharCode(exec, op1);
}

EncodedJSValue JIT_OPERATION operationStringFromCharCodeUntyped(ExecState* exec, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    JSValue charValue = JSValue::decode(encodedValue);
    int32_t chInt = charValue.toUInt32(exec);
    return JSValue::encode(JSC::stringFromCharCode(exec, chInt));
}

int64_t JIT_OPERATION operationConvertBoxedDoubleToInt52(EncodedJSValue encodedValue)
{
    JSValue value = JSValue::decode(encodedValue);
    if (!value.isDouble())
        return JSValue::notInt52;
    return tryConvertToInt52(value.asDouble());
}

int64_t JIT_OPERATION operationConvertDoubleToInt52(double value)
{
    return tryConvertToInt52(value);
}

char* JIT_OPERATION operationNewRawObject(ExecState* exec, Structure* structure, int32_t length, Butterfly* butterfly)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (!butterfly
        && (structure->outOfLineCapacity() || hasIndexedProperties(structure->indexingType()))) {
        IndexingHeader header;
        header.setVectorLength(length);
        header.setPublicLength(0);
        
        butterfly = Butterfly::create(
            vm, nullptr, 0, structure->outOfLineCapacity(),
            hasIndexedProperties(structure->indexingType()), header,
            length * sizeof(EncodedJSValue));
    }

    JSObject* result = JSObject::createRawObject(exec, structure, butterfly);
    result->butterfly(); // Ensure that the butterfly is in to-space.
    return bitwise_cast<char*>(result);
}

JSCell* JIT_OPERATION operationNewObjectWithButterfly(ExecState* exec, Structure* structure, Butterfly* butterfly)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!butterfly) {
        butterfly = Butterfly::create(
            vm, nullptr, 0, structure->outOfLineCapacity(), false, IndexingHeader(), 0);
    }
    
    JSObject* result = JSObject::createRawObject(exec, structure, butterfly);
    result->butterfly(); // Ensure that the butterfly is in to-space.
    return result;
}

JSCell* JIT_OPERATION operationNewObjectWithButterflyWithIndexingHeaderAndVectorLength(ExecState* exec, Structure* structure, unsigned length, Butterfly* butterfly)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    IndexingHeader header;
    header.setVectorLength(length);
    header.setPublicLength(0);
    if (butterfly)
        *butterfly->indexingHeader() = header;
    else {
        butterfly = Butterfly::create(
            vm, nullptr, 0, structure->outOfLineCapacity(), true, header,
            sizeof(EncodedJSValue) * length);
    }
    
    // Paradoxically this may allocate a JSArray. That's totally cool.
    JSObject* result = JSObject::createRawObject(exec, structure, butterfly);
    result->butterfly(); // Ensure that the butterfly is in to-space.
    return result;
}

JSCell* JIT_OPERATION operationNewArrayWithSpreadSlow(ExecState* exec, void* buffer, uint32_t numItems)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    EncodedJSValue* values = static_cast<EncodedJSValue*>(buffer);
    Checked<unsigned, RecordOverflow> checkedLength = 0;
    for (unsigned i = 0; i < numItems; i++) {
        JSValue value = JSValue::decode(values[i]);
        if (JSFixedArray* array = jsDynamicCast<JSFixedArray*>(vm, value))
            checkedLength += array->size();
        else
            ++checkedLength;
    }

    if (UNLIKELY(checkedLength.hasOverflowed())) {
        throwOutOfMemoryError(exec, scope);
        return nullptr;
    }

    unsigned length = checkedLength.unsafeGet();
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);

    JSArray* result = JSArray::tryCreate(vm, structure, length);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(exec, scope);
        return nullptr;
    }
    RETURN_IF_EXCEPTION(scope, nullptr);

    unsigned index = 0;
    for (unsigned i = 0; i < numItems; i++) {
        JSValue value = JSValue::decode(values[i]);
        if (JSFixedArray* array = jsDynamicCast<JSFixedArray*>(vm, value)) {
            // We are spreading.
            for (unsigned i = 0; i < array->size(); i++) {
                result->putDirectIndex(exec, index, array->get(i));
                RETURN_IF_EXCEPTION(scope, nullptr);
                ++index;
            }
        } else {
            // We are not spreading.
            result->putDirectIndex(exec, index, value);
            RETURN_IF_EXCEPTION(scope, nullptr);
            ++index;
        }
    }

    return result;
}

JSCell* operationCreateFixedArray(ExecState* exec, unsigned length)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (JSFixedArray* result = JSFixedArray::tryCreate(vm, vm.fixedArrayStructure.get(), length))
        return result;

    throwOutOfMemoryError(exec, scope);
    return nullptr;
}

JSCell* JIT_OPERATION operationSpreadGeneric(ExecState* exec, JSCell* iterable)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (isJSArray(iterable)) {
        JSArray* array = jsCast<JSArray*>(iterable);
        if (array->isIteratorProtocolFastAndNonObservable())
            RELEASE_AND_RETURN(throwScope, JSFixedArray::createFromArray(exec, vm, array));
    }

    // FIXME: we can probably make this path faster by having our caller JS code call directly into
    // the iteration protocol builtin: https://bugs.webkit.org/show_bug.cgi?id=164520

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    JSArray* array;
    {
        JSFunction* iterationFunction = globalObject->iteratorProtocolFunction();
        CallData callData;
        CallType callType = JSC::getCallData(vm, iterationFunction, callData);
        ASSERT(callType != CallType::None);

        MarkedArgumentBuffer arguments;
        arguments.append(iterable);
        ASSERT(!arguments.hasOverflowed());
        JSValue arrayResult = call(exec, iterationFunction, callType, callData, jsNull(), arguments);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
        array = jsCast<JSArray*>(arrayResult);
    }

    RELEASE_AND_RETURN(throwScope, JSFixedArray::createFromArray(exec, vm, array));
}

JSCell* JIT_OPERATION operationSpreadFastArray(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(isJSArray(cell));
    JSArray* array = jsCast<JSArray*>(cell);
    ASSERT(array->isIteratorProtocolFastAndNonObservable());

    return JSFixedArray::createFromArray(exec, vm, array);
}

void JIT_OPERATION operationProcessTypeProfilerLogDFG(ExecState* exec) 
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    vm.typeProfilerLog()->processLogEntries("Log Full, called from inside DFG."_s);
}

EncodedJSValue JIT_OPERATION operationResolveScopeForHoistingFuncDeclInEval(ExecState* exec, JSScope* scope, UniquedStringImpl* impl)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
        
    JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(exec, scope, Identifier::fromUid(exec, impl));
    return JSValue::encode(resolvedScope);
}
    
JSCell* JIT_OPERATION operationResolveScope(ExecState* exec, JSScope* scope, UniquedStringImpl* impl)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSObject* resolvedScope = JSScope::resolve(exec, scope, Identifier::fromUid(exec, impl));
    return resolvedScope;
}

EncodedJSValue JIT_OPERATION operationGetDynamicVar(ExecState* exec, JSObject* scope, UniquedStringImpl* impl, unsigned getPutInfoBits)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    Identifier ident = Identifier::fromUid(exec, impl);
    RELEASE_AND_RETURN(throwScope, JSValue::encode(scope->getPropertySlot(exec, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        if (!found) {
            GetPutInfo getPutInfo(getPutInfoBits);
            if (getPutInfo.resolveMode() == ThrowIfNotFound)
                throwException(exec, throwScope, createUndefinedVariableError(exec, ident));
            return jsUndefined();
        }

        if (scope->isGlobalLexicalEnvironment()) {
            // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
            JSValue result = slot.getValue(exec, ident);
            if (result == jsTDZValue()) {
                throwException(exec, throwScope, createTDZError(exec));
                return jsUndefined();
            }
            return result;
        }

        return slot.getValue(exec, ident);
    })));
}

void JIT_OPERATION operationPutDynamicVar(ExecState* exec, JSObject* scope, EncodedJSValue value, UniquedStringImpl* impl, unsigned getPutInfoBits)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    const Identifier& ident = Identifier::fromUid(exec, impl);
    GetPutInfo getPutInfo(getPutInfoBits);
    bool hasProperty = scope->hasProperty(exec, ident);
    RETURN_IF_EXCEPTION(throwScope, void());
    if (hasProperty
        && scope->isGlobalLexicalEnvironment()
        && !isInitialization(getPutInfo.initializationMode())) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
        JSGlobalLexicalEnvironment::getOwnPropertySlot(scope, exec, ident, slot);
        if (slot.getValue(exec, ident) == jsTDZValue()) {
            throwException(exec, throwScope, createTDZError(exec));
            return;
        }
    }

    if (getPutInfo.resolveMode() == ThrowIfNotFound && !hasProperty) {
        throwException(exec, throwScope, createUndefinedVariableError(exec, ident));
        return;
    }

    CodeOrigin origin = exec->codeOrigin();
    bool strictMode;
    if (origin.inlineCallFrame)
        strictMode = origin.inlineCallFrame->baselineCodeBlock->isStrictMode();
    else
        strictMode = exec->codeBlock()->isStrictMode();
    PutPropertySlot slot(scope, strictMode, PutPropertySlot::UnknownContext, isInitialization(getPutInfo.initializationMode()));
    throwScope.release();
    scope->methodTable(vm)->put(scope, exec, ident, JSValue::decode(value), slot);
}

int32_t JIT_OPERATION operationMapHash(ExecState* exec, EncodedJSValue input)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return jsMapHash(exec, vm, JSValue::decode(input));
}

JSCell* JIT_OPERATION operationJSMapFindBucket(ExecState* exec, JSCell* map, EncodedJSValue key, int32_t hash)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSMap::BucketType** bucket = jsCast<JSMap*>(map)->findBucket(exec, JSValue::decode(key), hash);
    if (!bucket)
        return vm.sentinelMapBucket.get();
    return *bucket;
}

JSCell* JIT_OPERATION operationJSSetFindBucket(ExecState* exec, JSCell* map, EncodedJSValue key, int32_t hash)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSSet::BucketType** bucket = jsCast<JSSet*>(map)->findBucket(exec, JSValue::decode(key), hash);
    if (!bucket)
        return vm.sentinelSetBucket.get();
    return *bucket;
}

JSCell* JIT_OPERATION operationSetAdd(ExecState* exec, JSCell* set, EncodedJSValue key, int32_t hash)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto* bucket = jsCast<JSSet*>(set)->addNormalized(exec, JSValue::decode(key), JSValue(), hash);
    if (!bucket)
        return vm.sentinelSetBucket.get();
    return bucket;
}

JSCell* JIT_OPERATION operationMapSet(ExecState* exec, JSCell* map, EncodedJSValue key, EncodedJSValue value, int32_t hash)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto* bucket = jsCast<JSMap*>(map)->addNormalized(exec, JSValue::decode(key), JSValue::decode(value), hash);
    if (!bucket)
        return vm.sentinelMapBucket.get();
    return bucket;
}

void JIT_OPERATION operationWeakSetAdd(ExecState* exec, JSCell* set, JSCell* key, int32_t hash)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    jsCast<JSWeakSet*>(set)->add(vm, asObject(key), JSValue(), hash);
}

void JIT_OPERATION operationWeakMapSet(ExecState* exec, JSCell* map, JSCell* key, EncodedJSValue value, int32_t hash)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    jsCast<JSWeakMap*>(map)->add(vm, asObject(key), JSValue::decode(value), hash);
}

EncodedJSValue JIT_OPERATION operationGetPrototypeOfObject(ExecState* exec, JSObject* thisObject)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return JSValue::encode(thisObject->getPrototype(vm, exec));
}

EncodedJSValue JIT_OPERATION operationGetPrototypeOf(ExecState* exec, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = JSValue::decode(encodedValue).toThis(exec, StrictMode);
    if (thisValue.isUndefinedOrNull())
        return throwVMError(exec, scope, createNotAnObjectError(exec, thisValue));

    JSObject* thisObject = jsDynamicCast<JSObject*>(vm, thisValue);
    if (!thisObject) {
        JSObject* prototype = thisValue.synthesizePrototype(exec);
        EXCEPTION_ASSERT(!!scope.exception() == !prototype);
        if (UNLIKELY(!prototype))
            return JSValue::encode(JSValue());
        return JSValue::encode(prototype);
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(thisObject->getPrototype(vm, exec)));
}

void JIT_OPERATION operationThrowDFG(ExecState* exec, EncodedJSValue valueToThrow)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);
    scope.throwException(exec, JSValue::decode(valueToThrow));
}

void JIT_OPERATION operationThrowStaticError(ExecState* exec, JSString* message, uint32_t errorType)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    auto scope = DECLARE_THROW_SCOPE(vm);
    String errorMessage = message->value(exec);
    scope.throwException(exec, createError(exec, static_cast<ErrorType>(errorType), errorMessage));
}

extern "C" void JIT_OPERATION triggerReoptimizationNow(CodeBlock* codeBlock, CodeBlock* optimizedCodeBlock, OSRExitBase* exit)
{
    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(codeBlock->vm()->heap);
    
    sanitizeStackForVM(codeBlock->vm());

    if (Options::verboseOSR())
        dataLog(*codeBlock, ": Entered reoptimize\n");
    // We must be called with the baseline code block.
    ASSERT(JITCode::isBaselineCode(codeBlock->jitType()));

    // If I am my own replacement, then reoptimization has already been triggered.
    // This can happen in recursive functions.
    //
    // Note that even if optimizedCodeBlock is an FTLForOSREntry style CodeBlock, this condition is a
    // sure bet that we don't have anything else left to do.
    CodeBlock* replacement = codeBlock->replacement();
    if (!replacement || replacement == codeBlock) {
        if (Options::verboseOSR())
            dataLog(*codeBlock, ": Not reoptimizing because we've already been jettisoned.\n");
        return;
    }
    
    // Otherwise, the replacement must be optimized code. Use this as an opportunity
    // to check our logic.
    ASSERT(codeBlock->hasOptimizedReplacement());
    ASSERT(JITCode::isOptimizingJIT(optimizedCodeBlock->jitType()));
    
    bool didTryToEnterIntoInlinedLoops = false;
    for (InlineCallFrame* inlineCallFrame = exit->m_codeOrigin.inlineCallFrame; inlineCallFrame; inlineCallFrame = inlineCallFrame->directCaller.inlineCallFrame) {
        if (inlineCallFrame->baselineCodeBlock->ownerScriptExecutable()->didTryToEnterInLoop()) {
            didTryToEnterIntoInlinedLoops = true;
            break;
        }
    }

    // In order to trigger reoptimization, one of two things must have happened:
    // 1) We exited more than some number of times.
    // 2) We exited and got stuck in a loop, and now we're exiting again.
    bool didExitABunch = optimizedCodeBlock->shouldReoptimizeNow();
    bool didGetStuckInLoop =
        (codeBlock->checkIfOptimizationThresholdReached() || didTryToEnterIntoInlinedLoops)
        && optimizedCodeBlock->shouldReoptimizeFromLoopNow();
    
    if (!didExitABunch && !didGetStuckInLoop) {
        if (Options::verboseOSR())
            dataLog(*codeBlock, ": Not reoptimizing ", *optimizedCodeBlock, " because it either didn't exit enough or didn't loop enough after exit.\n");
        codeBlock->optimizeAfterLongWarmUp();
        return;
    }
    
    optimizedCodeBlock->jettison(Profiler::JettisonDueToOSRExit, CountReoptimization);
}

#if ENABLE(FTL_JIT)
static bool shouldTriggerFTLCompile(CodeBlock* codeBlock, JITCode* jitCode)
{
    if (codeBlock->baselineVersion()->m_didFailFTLCompilation) {
        CODEBLOCK_LOG_EVENT(codeBlock, "abortFTLCompile", ());
        if (Options::verboseOSR())
            dataLog("Deferring FTL-optimization of ", *codeBlock, " indefinitely because there was an FTL failure.\n");
        jitCode->dontOptimizeAnytimeSoon(codeBlock);
        return false;
    }

    if (!codeBlock->hasOptimizedReplacement()
        && !jitCode->checkIfOptimizationThresholdReached(codeBlock)) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("counter = ", jitCode->tierUpCounter));
        if (Options::verboseOSR())
            dataLog("Choosing not to FTL-optimize ", *codeBlock, " yet.\n");
        return false;
    }
    return true;
}

static void triggerFTLReplacementCompile(VM* vm, CodeBlock* codeBlock, JITCode* jitCode)
{
    if (codeBlock->codeType() == GlobalCode) {
        // Global code runs once, so we don't want to do anything. We don't want to defer indefinitely,
        // since this may have been spuriously called from tier-up initiated in a loop, and that loop may
        // later want to run faster code. Deferring for warm-up seems safest.
        jitCode->optimizeAfterWarmUp(codeBlock);
        return;
    }
    
    Worklist::State worklistState;
    if (Worklist* worklist = existingGlobalFTLWorklistOrNull()) {
        worklistState = worklist->completeAllReadyPlansForVM(
            *vm, CompilationKey(codeBlock->baselineVersion(), FTLMode));
    } else
        worklistState = Worklist::NotKnown;
    
    if (worklistState == Worklist::Compiling) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("still compiling"));
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return;
    }
    
    if (codeBlock->hasOptimizedReplacement()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("has replacement"));
        // That's great, we've compiled the code - next time we call this function,
        // we'll enter that replacement.
        jitCode->optimizeSoon(codeBlock);
        return;
    }
    
    if (worklistState == Worklist::Compiled) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("compiled and failed"));
        // This means that we finished compiling, but failed somehow; in that case the
        // thresholds will be set appropriately.
        if (Options::verboseOSR())
            dataLog("Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.\n");
        return;
    }

    CODEBLOCK_LOG_EVENT(codeBlock, "triggerFTLReplacement", ());
    // We need to compile the code.
    compile(
        *vm, codeBlock->newReplacement(), codeBlock, FTLMode, UINT_MAX,
        Operands<JSValue>(), ToFTLDeferredCompilationCallback::create());

    // If we reached here, the counter has not be reset. Do that now.
    jitCode->setOptimizationThresholdBasedOnCompilationResult(
        codeBlock, CompilationDeferred);
}

void JIT_OPERATION triggerTierUpNow(ExecState* exec)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    DeferGCForAWhile deferGC(vm->heap);
    CodeBlock* codeBlock = exec->codeBlock();
    
    sanitizeStackForVM(vm);

    if (codeBlock->jitType() != JITCode::DFGJIT) {
        dataLog("Unexpected code block in DFG->FTL tier-up: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    JITCode* jitCode = codeBlock->jitCode()->dfg();
    
    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered triggerTierUpNow with executeCounter = ",
            jitCode->tierUpCounter, "\n");
    }

    if (shouldTriggerFTLCompile(codeBlock, jitCode))
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    if (codeBlock->hasOptimizedReplacement()) {
        if (jitCode->tierUpEntryTriggers.isEmpty()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("replacement in place, delaying indefinitely"));
            // There is nothing more we can do, the only way this will be entered
            // is through the function entry point.
            jitCode->dontOptimizeAnytimeSoon(codeBlock);
            return;
        }
        if (jitCode->osrEntryBlock() && jitCode->tierUpEntryTriggers.size() == 1) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("trigger in place, delaying indefinitely"));
            // There is only one outer loop and its trigger must have been set
            // when the plan completed.
            // Exiting the inner loop is useless, we can ignore the counter and leave
            // the trigger do its job.
            jitCode->dontOptimizeAnytimeSoon(codeBlock);
            return;
        }
    }
}

static char* tierUpCommon(ExecState* exec, unsigned originBytecodeIndex, bool canOSREnterHere)
{
    VM* vm = &exec->vm();
    CodeBlock* codeBlock = exec->codeBlock();

    // Resolve any pending plan for OSR Enter on this function.
    Worklist::State worklistState;
    if (Worklist* worklist = existingGlobalFTLWorklistOrNull()) {
        worklistState = worklist->completeAllReadyPlansForVM(
            *vm, CompilationKey(codeBlock->baselineVersion(), FTLForOSREntryMode));
    } else
        worklistState = Worklist::NotKnown;

    JITCode* jitCode = codeBlock->jitCode()->dfg();
    
    bool triggeredSlowPathToStartCompilation = false;
    auto tierUpEntryTriggers = jitCode->tierUpEntryTriggers.find(originBytecodeIndex);
    if (tierUpEntryTriggers != jitCode->tierUpEntryTriggers.end()) {
        switch (tierUpEntryTriggers->value) {
        case JITCode::TriggerReason::DontTrigger:
            // The trigger isn't set, we entered because the counter reached its
            // threshold.
            break;

        case JITCode::TriggerReason::CompilationDone:
            // The trigger was set because compilation completed. Don't unset it
            // so that further DFG executions OSR enter as well.
            break;

        case JITCode::TriggerReason::StartCompilation:
            // We were asked to enter as soon as possible and start compiling an
            // entry for the current bytecode location. Unset this trigger so we
            // don't continually enter.
            tierUpEntryTriggers->value = JITCode::TriggerReason::DontTrigger;
            triggeredSlowPathToStartCompilation = true;
            break;
        }
    }

    if (worklistState == Worklist::Compiling) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("still compiling"));
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
        return nullptr;
    }

    // If we can OSR Enter, do it right away.
    if (canOSREnterHere) {
        auto iter = jitCode->bytecodeIndexToStreamIndex.find(originBytecodeIndex);
        if (iter != jitCode->bytecodeIndexToStreamIndex.end()) {
            unsigned streamIndex = iter->value;
            if (CodeBlock* entryBlock = jitCode->osrEntryBlock()) {
                if (Options::verboseOSR())
                    dataLog("OSR entry: From ", RawPointer(jitCode), " got entry block ", RawPointer(entryBlock), "\n");
                if (void* address = FTL::prepareOSREntry(exec, codeBlock, entryBlock, originBytecodeIndex, streamIndex)) {
                    CODEBLOCK_LOG_EVENT(entryBlock, "osrEntry", ("at bc#", originBytecodeIndex));
                    return retagCodePtr<char*>(address, JSEntryPtrTag, bitwise_cast<PtrTag>(exec));
                }
            }
        }
    }

    if (worklistState == Worklist::Compiled) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("compiled and failed"));
        // This means that compilation failed and we already set the thresholds.
        if (Options::verboseOSR())
            dataLog("Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.\n");
        return nullptr;
    }

    // - If we don't have an FTL code block, then try to compile one.
    // - If we do have an FTL code block, then try to enter for a while.
    // - If we couldn't enter for a while, then trigger OSR entry.

    if (!shouldTriggerFTLCompile(codeBlock, jitCode) && !triggeredSlowPathToStartCompilation)
        return nullptr;

    if (!jitCode->neverExecutedEntry && !triggeredSlowPathToStartCompilation) {
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

        if (!codeBlock->hasOptimizedReplacement())
            return nullptr;

        if (jitCode->osrEntryRetry < Options::ftlOSREntryRetryThreshold()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("avoiding OSR entry compile"));
            jitCode->osrEntryRetry++;
            return nullptr;
        }
    } else
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("avoiding replacement compile"));

    if (CodeBlock* entryBlock = jitCode->osrEntryBlock()) {
        if (jitCode->osrEntryRetry < Options::ftlOSREntryRetryThreshold()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR entry failed, OSR entry threshold not met"));
            jitCode->osrEntryRetry++;
            jitCode->setOptimizationThresholdBasedOnCompilationResult(
                codeBlock, CompilationDeferred);
            return nullptr;
        }

        FTL::ForOSREntryJITCode* entryCode = entryBlock->jitCode()->ftlForOSREntry();
        entryCode->countEntryFailure();
        if (entryCode->entryFailureCount() <
            Options::ftlOSREntryFailureCountForReoptimization()) {
            CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR entry failed"));
            jitCode->setOptimizationThresholdBasedOnCompilationResult(
                codeBlock, CompilationDeferred);
            return nullptr;
        }

        // OSR entry failed. Oh no! This implies that we need to retry. We retry
        // without exponential backoff and we only do this for the entry code block.
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR entry failed too many times"));
        unsigned osrEntryBytecode = entryBlock->jitCode()->ftlForOSREntry()->bytecodeIndex();
        jitCode->clearOSREntryBlock();
        jitCode->osrEntryRetry = 0;
        jitCode->tierUpEntryTriggers.set(osrEntryBytecode, JITCode::TriggerReason::DontTrigger);
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return nullptr;
    }

    // It's time to try to compile code for OSR entry.

    if (!triggeredSlowPathToStartCompilation) {

        // An inner loop didn't specifically ask for us to kick off a compilation. This means the counter
        // crossed its threshold. We either fall through and kick off a compile for originBytecodeIndex,
        // or we flag an outer loop to immediately try to compile itself. If there are outer loops,
        // we first try to make them compile themselves. But we will eventually fall back to compiling
        // a progressively inner loop if it takes too long for control to reach an outer loop.

        auto tryTriggerOuterLoopToCompile = [&] {
            auto tierUpHierarchyEntry = jitCode->tierUpInLoopHierarchy.find(originBytecodeIndex);
            if (tierUpHierarchyEntry == jitCode->tierUpInLoopHierarchy.end())
                return false;

            // This vector is ordered from innermost to outermost loop. Every bytecode entry in this vector is
            // allowed to do OSR entry. We start with the outermost loop and make our way inwards (hence why we
            // iterate the vector in reverse). Our policy is that we will trigger an outer loop to compile
            // immediately when program control reaches it. If program control is taking too long to reach that
            // outer loop, we progressively move inwards, meaning, we'll eventually trigger some loop that is
            // executing to compile. We start with trying to compile outer loops since we believe outer loop
            // compilations reveal the best opportunities for optimizing code.
            for (auto iter = tierUpHierarchyEntry->value.rbegin(), end = tierUpHierarchyEntry->value.rend(); iter != end; ++iter) {
                unsigned osrEntryCandidate = *iter;

                if (jitCode->tierUpEntryTriggers.get(osrEntryCandidate) == JITCode::TriggerReason::StartCompilation) {
                    // This means that we already asked this loop to compile. If we've reached here, it
                    // means program control has not yet reached that loop. So it's taking too long to compile.
                    // So we move on to asking the inner loop of this loop to compile itself.
                    continue;
                }

                // This is where we ask the outer to loop to immediately compile itself if program
                // control reaches it.
                if (Options::verboseOSR())
                    dataLog("Inner-loop bc#", originBytecodeIndex, " in ", *codeBlock, " setting parent loop bc#", osrEntryCandidate, "'s trigger and backing off.\n");
                jitCode->tierUpEntryTriggers.set(osrEntryCandidate, JITCode::TriggerReason::StartCompilation);
                return true;
            }

            return false;
        };

        if (tryTriggerOuterLoopToCompile()) {
            jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
            return nullptr;
        }
    }

    if (!canOSREnterHere) {
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
        return nullptr;
    }

    // We aren't compiling and haven't compiled anything for OSR entry. So, try to compile
    // something.

    auto triggerIterator = jitCode->tierUpEntryTriggers.find(originBytecodeIndex);
    if (triggerIterator == jitCode->tierUpEntryTriggers.end()) {
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
        return nullptr;
    }

    JITCode::TriggerReason* triggerAddress = &(triggerIterator->value);

    Operands<JSValue> mustHandleValues;
    unsigned streamIndex = jitCode->bytecodeIndexToStreamIndex.get(originBytecodeIndex);
    jitCode->reconstruct(
        exec, codeBlock, CodeOrigin(originBytecodeIndex), streamIndex, mustHandleValues);
    CodeBlock* replacementCodeBlock = codeBlock->newReplacement();

    CODEBLOCK_LOG_EVENT(codeBlock, "triggerFTLOSR", ());
    CompilationResult forEntryResult = compile(
        *vm, replacementCodeBlock, codeBlock, FTLForOSREntryMode, originBytecodeIndex,
        mustHandleValues, ToFTLForOSREntryDeferredCompilationCallback::create(triggerAddress));

    if (jitCode->neverExecutedEntry)
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    if (forEntryResult != CompilationSuccessful) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR ecompilation not successful"));
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return nullptr;
    }
    
    CODEBLOCK_LOG_EVENT(jitCode->osrEntryBlock(), "osrEntry", ("at bc#", originBytecodeIndex));
    // It's possible that the for-entry compile already succeeded. In that case OSR
    // entry will succeed unless we ran out of stack. It's not clear what we should do.
    // We signal to try again after a while if that happens.
    if (Options::verboseOSR())
        dataLog("Immediate OSR entry: From ", RawPointer(jitCode), " got entry block ", RawPointer(jitCode->osrEntryBlock()), "\n");

    void* address = FTL::prepareOSREntry(
        exec, codeBlock, jitCode->osrEntryBlock(), originBytecodeIndex, streamIndex);
    if (!address)
        return nullptr;
    return retagCodePtr<char*>(address, JSEntryPtrTag, bitwise_cast<PtrTag>(exec));
}

void JIT_OPERATION triggerTierUpNowInLoop(ExecState* exec, unsigned bytecodeIndex)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    DeferGCForAWhile deferGC(vm->heap);
    CodeBlock* codeBlock = exec->codeBlock();

    sanitizeStackForVM(vm);

    if (codeBlock->jitType() != JITCode::DFGJIT) {
        dataLog("Unexpected code block in DFG->FTL trigger tier up now in loop: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

    JITCode* jitCode = codeBlock->jitCode()->dfg();

    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered triggerTierUpNowInLoop with executeCounter = ",
            jitCode->tierUpCounter, "\n");
    }

    if (jitCode->tierUpInLoopHierarchy.contains(bytecodeIndex))
        tierUpCommon(exec, bytecodeIndex, false);
    else if (shouldTriggerFTLCompile(codeBlock, jitCode))
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    // Since we cannot OSR Enter here, the default "optimizeSoon()" is not useful.
    if (codeBlock->hasOptimizedReplacement()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayFTLCompile", ("OSR in loop failed, deferring"));
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
    }
}

char* JIT_OPERATION triggerOSREntryNow(ExecState* exec, unsigned bytecodeIndex)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    DeferGCForAWhile deferGC(vm->heap);
    CodeBlock* codeBlock = exec->codeBlock();

    sanitizeStackForVM(vm);

    if (codeBlock->jitType() != JITCode::DFGJIT) {
        dataLog("Unexpected code block in DFG->FTL tier-up: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

    JITCode* jitCode = codeBlock->jitCode()->dfg();
    jitCode->tierUpEntrySeen.add(bytecodeIndex);

    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered triggerOSREntryNow with executeCounter = ",
            jitCode->tierUpCounter, "\n");
    }

    return tierUpCommon(exec, bytecodeIndex, true);
}

#endif // ENABLE(FTL_JIT)

} // extern "C"
} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // ENABLE(JIT)
