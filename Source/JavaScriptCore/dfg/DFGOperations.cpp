/*
 * Copyright (C) 2011, 2013-2016 Apple Inc. All rights reserved.
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

#include "ButterflyInlines.h"
#include "ClonedArguments.h"
#include "CodeBlock.h"
#include "CommonSlowPaths.h"
#include "CopiedSpaceInlines.h"
#include "DFGDriver.h"
#include "DFGJITCode.h"
#include "DFGOSRExit.h"
#include "DFGThunks.h"
#include "DFGToFTLDeferredCompilationCallback.h"
#include "DFGToFTLForOSREntryDeferredCompilationCallback.h"
#include "DFGWorklist.h"
#include "DirectArguments.h"
#include "FTLForOSREntryJITCode.h"
#include "FTLOSREntry.h"
#include "HostCallReturnValue.h"
#include "GetterSetter.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITExceptions.h"
#include "JSCInlines.h"
#include "JSLexicalEnvironment.h"
#include "ObjectConstructor.h"
#include "Repatch.h"
#include "ScopedArguments.h"
#include "StringConstructor.h"
#include "SuperSampler.h"
#include "Symbol.h"
#include "TypeProfilerLog.h"
#include "TypedArrayInlines.h"
#include "VM.h"
#include <wtf/InlineASM.h>

#if ENABLE(JIT)
#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

template<bool strict, bool direct>
static inline void putByVal(ExecState* exec, JSValue baseValue, uint32_t index, JSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
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
ALWAYS_INLINE static void JIT_OPERATION operationPutByValInternal(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);
    JSValue value = JSValue::decode(encodedValue);

    if (LIKELY(property.isUInt32())) {
        // Despite its name, JSValue::isUInt32 will return true only for positive boxed int32_t; all those values are valid array indices.
        ASSERT(isIndex(property.asUInt32()));
        putByVal<strict, direct>(exec, baseValue, property.asUInt32(), value);
        return;
    }

    if (property.isDouble()) {
        double propertyAsDouble = property.asDouble();
        uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
        if (propertyAsDouble == propertyAsUInt32 && isIndex(propertyAsUInt32)) {
            putByVal<strict, direct>(exec, baseValue, propertyAsUInt32, value);
            return;
        }
    }

    // Don't put to an object if toString throws an exception.
    auto propertyName = property.toPropertyKey(exec);
    if (vm->exception())
        return;

    PutPropertySlot slot(baseValue, strict);
    if (direct) {
        RELEASE_ASSERT(baseValue.isObject());
        if (Optional<uint32_t> index = parseIndex(propertyName))
            asObject(baseValue)->putDirectIndex(exec, index.value(), value, 0, strict ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        else
            asObject(baseValue)->putDirect(*vm, propertyName, value, slot);
    } else
        baseValue.put(exec, propertyName, value, slot);
}

template<typename ViewClass>
char* newTypedArrayWithSize(ExecState* exec, Structure* structure, int32_t size)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    if (size < 0) {
        vm.throwException(exec, createRangeError(exec, ASCIILiteral("Requested length is negative")));
        return 0;
    }
    return bitwise_cast<char*>(ViewClass::create(exec, structure, size));
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

JSCell* JIT_OPERATION operationCreateThis(ExecState* exec, JSObject* constructor, int32_t inlineCapacity)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    if (constructor->type() == JSFunctionType)
        return constructEmptyObject(exec, jsCast<JSFunction*>(constructor)->rareData(exec, inlineCapacity)->objectAllocationProfile()->structure());

    JSValue proto = constructor->get(exec, exec->propertyNames().prototype);
    if (vm.exception())
        return nullptr;
    if (proto.isObject())
        return constructEmptyObject(exec, asObject(proto));
    return constructEmptyObject(exec);
}

EncodedJSValue JIT_OPERATION operationValueBitAnd(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    int32_t a = op1.toInt32(exec);
    int32_t b = op2.toInt32(exec);
    return JSValue::encode(jsNumber(a & b));
}

EncodedJSValue JIT_OPERATION operationValueBitOr(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    int32_t a = op1.toInt32(exec);
    int32_t b = op2.toInt32(exec);
    return JSValue::encode(jsNumber(a | b));
}

EncodedJSValue JIT_OPERATION operationValueBitXor(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    int32_t a = op1.toInt32(exec);
    int32_t b = op2.toInt32(exec);
    return JSValue::encode(jsNumber(a ^ b));
}

EncodedJSValue JIT_OPERATION operationValueBitLShift(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    int32_t a = op1.toInt32(exec);
    uint32_t b = op2.toUInt32(exec);
    return JSValue::encode(jsNumber(a << (b & 0x1f)));
}

EncodedJSValue JIT_OPERATION operationValueBitRShift(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    int32_t a = op1.toInt32(exec);
    uint32_t b = op2.toUInt32(exec);
    return JSValue::encode(jsNumber(a >> (b & 0x1f)));
}

EncodedJSValue JIT_OPERATION operationValueBitURShift(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    uint32_t a = op1.toUInt32(exec);
    uint32_t b = op2.toUInt32(exec);
    return JSValue::encode(jsNumber(static_cast<int32_t>(a >> (b & 0x1f))));
}

EncodedJSValue JIT_OPERATION operationValueAdd(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    return JSValue::encode(jsAdd(exec, op1, op2));
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

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    double a = op1.toNumber(exec);
    double b = op2.toNumber(exec);
    return JSValue::encode(jsNumber(a / b));
}

EncodedJSValue JIT_OPERATION operationValueMul(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    double a = op1.toNumber(exec);
    double b = op2.toNumber(exec);
    return JSValue::encode(jsNumber(a * b));
}

EncodedJSValue JIT_OPERATION operationValueSub(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);

    double a = op1.toNumber(exec);
    double b = op2.toNumber(exec);
    return JSValue::encode(jsNumber(a - b));
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
    
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue property = JSValue::decode(encodedProperty);

    if (LIKELY(baseValue.isCell())) {
        JSCell* base = baseValue.asCell();

        if (property.isUInt32()) {
            return getByVal(exec, base, property.asUInt32());
        } else if (property.isDouble()) {
            double propertyAsDouble = property.asDouble();
            uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
            if (propertyAsUInt32 == propertyAsDouble && isIndex(propertyAsUInt32))
                return getByVal(exec, base, propertyAsUInt32);
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
    if (vm.exception())
        return JSValue::encode(jsUndefined());
    auto propertyName = property.toPropertyKey(exec);
    if (vm.exception())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(baseValue.get(exec, propertyName));
}

EncodedJSValue JIT_OPERATION operationGetByValCell(ExecState* exec, JSCell* base, EncodedJSValue encodedProperty)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    JSValue property = JSValue::decode(encodedProperty);

    if (property.isUInt32())
        return getByVal(exec, base, property.asUInt32());
    if (property.isDouble()) {
        double propertyAsDouble = property.asDouble();
        uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
        if (propertyAsUInt32 == propertyAsDouble)
            return getByVal(exec, base, propertyAsUInt32);
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
    if (vm.exception())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(JSValue(base).get(exec, propertyName));
}

ALWAYS_INLINE EncodedJSValue getByValCellInt(ExecState* exec, JSCell* base, int32_t index)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    if (index < 0) {
        // Go the slowest way possible becase negative indices don't use indexed storage.
        return JSValue::encode(JSValue(base).get(exec, Identifier::from(exec, index)));
    }

    // Use this since we know that the value is out of bounds.
    return JSValue::encode(JSValue(base).get(exec, static_cast<unsigned>(index)));
}

EncodedJSValue JIT_OPERATION operationGetByValArrayInt(ExecState* exec, JSArray* base, int32_t index)
{
    return getByValCellInt(exec, base, index);
}

EncodedJSValue JIT_OPERATION operationGetByValStringInt(ExecState* exec, JSString* base, int32_t index)
{
    return getByValCellInt(exec, base, index);
}

void JIT_OPERATION operationPutByValStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    operationPutByValInternal<true, false>(exec, encodedBase, encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValNonStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    operationPutByValInternal<false, false>(exec, encodedBase, encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValCellStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    operationPutByValInternal<true, false>(exec, JSValue::encode(cell), encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValCellNonStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    operationPutByValInternal<false, false>(exec, JSValue::encode(cell), encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValBeyondArrayBoundsStrict(ExecState* exec, JSObject* array, int32_t index, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (index >= 0) {
        array->putByIndexInline(exec, index, JSValue::decode(encodedValue), true);
        return;
    }
    
    PutPropertySlot slot(array, true);
    array->methodTable()->put(
        array, exec, Identifier::from(exec, index), JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByValBeyondArrayBoundsNonStrict(ExecState* exec, JSObject* array, int32_t index, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    if (index >= 0) {
        array->putByIndexInline(exec, index, JSValue::decode(encodedValue), false);
        return;
    }
    
    PutPropertySlot slot(array, false);
    array->methodTable()->put(
        array, exec, Identifier::from(exec, index), JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutDoubleByValBeyondArrayBoundsStrict(ExecState* exec, JSObject* array, int32_t index, double value)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);
    
    if (index >= 0) {
        array->putByIndexInline(exec, index, jsValue, true);
        return;
    }
    
    PutPropertySlot slot(array, true);
    array->methodTable()->put(
        array, exec, Identifier::from(exec, index), jsValue, slot);
}

void JIT_OPERATION operationPutDoubleByValBeyondArrayBoundsNonStrict(ExecState* exec, JSObject* array, int32_t index, double value)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue jsValue = JSValue(JSValue::EncodeAsDouble, value);
    
    if (index >= 0) {
        array->putByIndexInline(exec, index, jsValue, false);
        return;
    }
    
    PutPropertySlot slot(array, false);
    array->methodTable()->put(
        array, exec, Identifier::from(exec, index), jsValue, slot);
}

void JIT_OPERATION operationPutByValDirectStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    operationPutByValInternal<true, true>(exec, encodedBase, encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValDirectNonStrict(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    operationPutByValInternal<false, true>(exec, encodedBase, encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValDirectCellStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    operationPutByValInternal<true, true>(exec, JSValue::encode(cell), encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValDirectCellNonStrict(ExecState* exec, JSCell* cell, EncodedJSValue encodedProperty, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    operationPutByValInternal<false, true>(exec, JSValue::encode(cell), encodedProperty, encodedValue);
}

void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsStrict(ExecState* exec, JSObject* array, int32_t index, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    if (index >= 0) {
        array->putDirectIndex(exec, index, JSValue::decode(encodedValue), 0, PutDirectIndexShouldThrow);
        return;
    }
    
    PutPropertySlot slot(array, true);
    array->putDirect(exec->vm(), Identifier::from(exec, index), JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByValDirectBeyondArrayBoundsNonStrict(ExecState* exec, JSObject* array, int32_t index, EncodedJSValue encodedValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    if (index >= 0) {
        array->putDirectIndex(exec, index, JSValue::decode(encodedValue));
        return;
    }
    
    PutPropertySlot slot(array, false);
    array->putDirect(exec->vm(), Identifier::from(exec, index), JSValue::decode(encodedValue), slot);
}

EncodedJSValue JIT_OPERATION operationArrayPush(ExecState* exec, EncodedJSValue encodedValue, JSArray* array)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    array->push(exec, JSValue::decode(encodedValue));
    return JSValue::encode(jsNumber(array->length()));
}

EncodedJSValue JIT_OPERATION operationArrayPushDouble(ExecState* exec, double value, JSArray* array)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    array->push(exec, JSValue(JSValue::EncodeAsDouble, value));
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
    
    JSValue argument = JSValue::decode(encodedArgument);

    JSString* input = argument.toStringOrNull(exec);
    if (!input)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(regExpObject->execInline(exec, globalObject, input));
}
        
EncodedJSValue JIT_OPERATION operationRegExpExecGeneric(ExecState* exec, JSGlobalObject* globalObject, EncodedJSValue encodedBase, EncodedJSValue encodedArgument)
{
    SuperSamplerScope superSamplerScope(false);
    
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue base = JSValue::decode(encodedBase);
    JSValue argument = JSValue::decode(encodedArgument);
    
    if (!base.inherits(RegExpObject::info()))
        return throwVMTypeError(exec);

    JSString* input = argument.toStringOrNull(exec);
    if (!input)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(asRegExpObject(base)->exec(exec, globalObject, input));
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
    SuperSamplerScope superSamplerScope;
    
    VM& vm = globalObject->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue base = JSValue::decode(encodedBase);
    JSValue argument = JSValue::decode(encodedArgument);

    if (!base.inherits(RegExpObject::info())) {
        throwTypeError(exec);
        return false;
    }

    JSString* input = argument.toStringOrNull(exec);
    if (!input)
        return false;
    return asRegExpObject(base)->test(exec, globalObject, input);
}

size_t JIT_OPERATION operationCompareStrictEqCell(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue op1 = JSValue::decode(encodedOp1);
    JSValue op2 = JSValue::decode(encodedOp2);
    
    ASSERT(op1.isCell());
    ASSERT(op2.isCell());
    
    return JSValue::strictEqualSlowCaseInline(exec, op1, op2);
}

size_t JIT_OPERATION operationCompareStrictEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue src1 = JSValue::decode(encodedOp1);
    JSValue src2 = JSValue::decode(encodedOp2);
    
    return JSValue::strictEqual(exec, src1, src2);
}

EncodedJSValue JIT_OPERATION operationToPrimitive(ExecState* exec, EncodedJSValue value)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return JSValue::encode(JSValue::decode(value).toPrimitive(exec));
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

char* JIT_OPERATION operationNewArrayWithSize(ExecState* exec, Structure* arrayStructure, int32_t size)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    if (UNLIKELY(size < 0))
        return bitwise_cast<char*>(exec->vm().throwException(exec, createRangeError(exec, ASCIILiteral("Array size is not a small enough positive integer."))));

    JSArray* result = JSArray::create(*vm, arrayStructure, size);
    result->butterfly(); // Ensure that the backing store is in to-space.
    return bitwise_cast<char*>(result);
}

char* JIT_OPERATION operationNewArrayBuffer(ExecState* exec, Structure* arrayStructure, size_t start, size_t size)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return bitwise_cast<char*>(constructArray(exec, arrayStructure, exec->codeBlock()->constantBuffer(start), size));
}

char* JIT_OPERATION operationNewInt8ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSInt8Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewInt8ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSInt8Array>(exec, structure, encodedValue, 0, Nullopt));
}

char* JIT_OPERATION operationNewInt16ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSInt16Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewInt16ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSInt16Array>(exec, structure, encodedValue, 0, Nullopt));
}

char* JIT_OPERATION operationNewInt32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSInt32Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewInt32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSInt32Array>(exec, structure, encodedValue, 0, Nullopt));
}

char* JIT_OPERATION operationNewUint8ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSUint8Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewUint8ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSUint8Array>(exec, structure, encodedValue, 0, Nullopt));
}

char* JIT_OPERATION operationNewUint8ClampedArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSUint8ClampedArray>(exec, structure, length);
}

char* JIT_OPERATION operationNewUint8ClampedArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSUint8ClampedArray>(exec, structure, encodedValue, 0, Nullopt));
}

char* JIT_OPERATION operationNewUint16ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSUint16Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewUint16ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSUint16Array>(exec, structure, encodedValue, 0, Nullopt));
}

char* JIT_OPERATION operationNewUint32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSUint32Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewUint32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSUint32Array>(exec, structure, encodedValue, 0, Nullopt));
}

char* JIT_OPERATION operationNewFloat32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSFloat32Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewFloat32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSFloat32Array>(exec, structure, encodedValue, 0, Nullopt));
}

char* JIT_OPERATION operationNewFloat64ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSFloat64Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewFloat64ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return reinterpret_cast<char*>(constructGenericTypedArrayViewWithArguments<JSFloat64Array>(exec, structure, encodedValue, 0, Nullopt));
}

JSCell* JIT_OPERATION operationCreateActivationDirect(ExecState* exec, Structure* structure, JSScope* scope, SymbolTable* table, EncodedJSValue initialValueEncoded)
{
    JSValue initialValue = JSValue::decode(initialValueEncoded);
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return JSLexicalEnvironment::create(vm, structure, scope, table, initialValue);
}

JSCell* JIT_OPERATION operationCreateDirectArguments(ExecState* exec, Structure* structure, int32_t length, int32_t minCapacity)
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

JSCell* JIT_OPERATION operationCreateScopedArguments(ExecState* exec, Structure* structure, Register* argumentStart, int32_t length, JSFunction* callee, JSLexicalEnvironment* scope)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer target(&vm, exec);
    
    // We could pass the ScopedArgumentsTable* as an argument. We currently don't because I
    // didn't feel like changing the max number of arguments for a slow path call from 6 to 7.
    ScopedArgumentsTable* table = scope->symbolTable()->arguments();
    
    return ScopedArguments::createByCopyingFrom(
        vm, structure, argumentStart, length, callee, table, scope);
}

JSCell* JIT_OPERATION operationCreateClonedArguments(ExecState* exec, Structure* structure, Register* argumentStart, int32_t length, JSFunction* callee)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer target(&vm, exec);
    return ClonedArguments::createByCopyingFrom(
        exec, structure, argumentStart, length, callee);
}

JSCell* JIT_OPERATION operationCreateDirectArgumentsDuringExit(ExecState* exec, InlineCallFrame* inlineCallFrame, JSFunction* callee, int32_t argumentCount)
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
    
    result->callee().set(vm, result, callee);
    
    Register* arguments =
        exec->registers() + (inlineCallFrame ? inlineCallFrame->stackOffset : 0) +
        CallFrame::argumentOffset(0);
    for (unsigned i = length; i--;)
        result->setIndexQuickly(vm, i, arguments[i].jsValue());
    
    return result;
}

JSCell* JIT_OPERATION operationCreateClonedArgumentsDuringExit(ExecState* exec, InlineCallFrame* inlineCallFrame, JSFunction* callee, int32_t argumentCount)
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
        result->initializeIndex(vm, i, arguments[i].jsValue());

    
    return result;
}

void JIT_OPERATION operationCopyRest(ExecState* exec, JSCell* arrayAsCell, Register* argumentStart, unsigned numberOfParamsToSkip, unsigned arraySize)
{
    ASSERT(arraySize);
    JSArray* array = jsCast<JSArray*>(arrayAsCell);
    ASSERT(arraySize == array->length());
    array->setLength(exec, arraySize);
    for (unsigned i = 0; i < arraySize; i++)
        array->putDirectIndex(exec, i, argumentStart[i + numberOfParamsToSkip].jsValue());
}

size_t JIT_OPERATION operationObjectIsObject(ExecState* exec, JSGlobalObject* globalObject, JSCell* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    if (object->structure(vm)->masqueradesAsUndefined(globalObject))
        return false;
    if (object->type() == JSFunctionType)
        return false;
    if (object->inlineTypeFlags() & TypeOfShouldCallGetCallData) {
        CallData callData;
        if (object->methodTable(vm)->getCallData(object, callData) != CallType::None)
            return false;
    }
    
    return true;
}

size_t JIT_OPERATION operationObjectIsFunction(ExecState* exec, JSGlobalObject* globalObject, JSCell* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    if (object->structure(vm)->masqueradesAsUndefined(globalObject))
        return false;
    if (object->type() == JSFunctionType)
        return true;
    if (object->inlineTypeFlags() & TypeOfShouldCallGetCallData) {
        CallData callData;
        if (object->methodTable(vm)->getCallData(object, callData) != CallType::None)
            return true;
    }
    
    return false;
}

JSCell* JIT_OPERATION operationTypeOfObject(ExecState* exec, JSGlobalObject* globalObject, JSCell* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    if (object->structure(vm)->masqueradesAsUndefined(globalObject))
        return vm.smallStrings.undefinedString();
    if (object->type() == JSFunctionType)
        return vm.smallStrings.functionString();
    if (object->inlineTypeFlags() & TypeOfShouldCallGetCallData) {
        CallData callData;
        if (object->methodTable(vm)->getCallData(object, callData) != CallType::None)
            return vm.smallStrings.functionString();
    }
    
    return vm.smallStrings.objectString();
}

int32_t JIT_OPERATION operationTypeOfObjectAsTypeofType(ExecState* exec, JSGlobalObject* globalObject, JSCell* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(jsDynamicCast<JSObject*>(object));
    
    if (object->structure(vm)->masqueradesAsUndefined(globalObject))
        return static_cast<int32_t>(TypeofType::Undefined);
    if (object->type() == JSFunctionType)
        return static_cast<int32_t>(TypeofType::Function);
    if (object->inlineTypeFlags() & TypeOfShouldCallGetCallData) {
        CallData callData;
        if (object->methodTable(vm)->getCallData(object, callData) != CallType::None)
            return static_cast<int32_t>(TypeofType::Function);
    }
    
    return static_cast<int32_t>(TypeofType::Object);
}

char* JIT_OPERATION operationAllocatePropertyStorageWithInitialCapacity(ExecState* exec)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return reinterpret_cast<char*>(
        Butterfly::createUninitialized(vm, 0, 0, initialOutOfLineCapacity, false, 0));
}

char* JIT_OPERATION operationAllocatePropertyStorage(ExecState* exec, size_t newSize)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return reinterpret_cast<char*>(
        Butterfly::createUninitialized(vm, 0, 0, newSize, false, 0));
}

char* JIT_OPERATION operationReallocateButterflyToHavePropertyStorageWithInitialCapacity(ExecState* exec, JSObject* object)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(!object->structure()->outOfLineCapacity());
    DeferGC deferGC(vm.heap);
    Butterfly* result = object->growOutOfLineStorage(vm, 0, initialOutOfLineCapacity);
    object->setButterflyWithoutChangingStructure(vm, result);
    return reinterpret_cast<char*>(result);
}

char* JIT_OPERATION operationReallocateButterflyToGrowPropertyStorage(ExecState* exec, JSObject* object, size_t newSize)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    DeferGC deferGC(vm.heap);
    Butterfly* result = object->growOutOfLineStorage(vm, object->structure()->outOfLineCapacity(), newSize);
    object->setButterflyWithoutChangingStructure(vm, result);
    return reinterpret_cast<char*>(result);
}

char* JIT_OPERATION operationEnsureInt32(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;
    
    return reinterpret_cast<char*>(asObject(cell)->ensureInt32(vm).data());
}

char* JIT_OPERATION operationEnsureDouble(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;
    
    return reinterpret_cast<char*>(asObject(cell)->ensureDouble(vm).data());
}

char* JIT_OPERATION operationEnsureContiguous(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;
    
    return reinterpret_cast<char*>(asObject(cell)->ensureContiguous(vm).data());
}

char* JIT_OPERATION operationEnsureArrayStorage(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;

    return reinterpret_cast<char*>(asObject(cell)->ensureArrayStorage(vm));
}

StringImpl* JIT_OPERATION operationResolveRope(ExecState* exec, JSString* string)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return string->value(exec).impl();
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

JSCell* JIT_OPERATION operationToStringOnCell(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    return JSValue(cell).toString(exec);
}

JSCell* JIT_OPERATION operationToString(ExecState* exec, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return JSValue::decode(value).toString(exec);
}

JSCell* JIT_OPERATION operationCallStringConstructorOnCell(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return stringConstructor(exec, cell);
}

JSCell* JIT_OPERATION operationCallStringConstructor(ExecState* exec, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return stringConstructor(exec, JSValue::decode(value));
}

JSCell* JIT_OPERATION operationMakeRope2(ExecState* exec, JSString* left, JSString* right)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (sumOverflows<int32_t>(left->length(), right->length())) {
        throwOutOfMemoryError(exec);
        return nullptr;
    }

    return JSRopeString::create(vm, left, right);
}

JSCell* JIT_OPERATION operationMakeRope3(ExecState* exec, JSString* a, JSString* b, JSString* c)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (sumOverflows<int32_t>(a->length(), b->length(), c->length())) {
        throwOutOfMemoryError(exec);
        return nullptr;
    }

    return JSRopeString::create(vm, a, b, c);
}

JSCell* JIT_OPERATION operationStrCat2(ExecState* exec, EncodedJSValue a, EncodedJSValue b)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSString* str1 = JSValue::decode(a).toString(exec);
    ASSERT(!vm.exception()); // Impossible, since we must have been given primitives.
    JSString* str2 = JSValue::decode(b).toString(exec);
    ASSERT(!vm.exception());

    if (sumOverflows<int32_t>(str1->length(), str2->length())) {
        throwOutOfMemoryError(exec);
        return nullptr;
    }

    return JSRopeString::create(vm, str1, str2);
}
    
JSCell* JIT_OPERATION operationStrCat3(ExecState* exec, EncodedJSValue a, EncodedJSValue b, EncodedJSValue c)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSString* str1 = JSValue::decode(a).toString(exec);
    ASSERT(!vm.exception()); // Impossible, since we must have been given primitives.
    JSString* str2 = JSValue::decode(b).toString(exec);
    ASSERT(!vm.exception());
    JSString* str3 = JSValue::decode(c).toString(exec);
    ASSERT(!vm.exception());

    if (sumOverflows<int32_t>(str1->length(), str2->length(), str3->length())) {
        throwOutOfMemoryError(exec);
        return nullptr;
    }

    return JSRopeString::create(vm, str1, str2, str3);
}

char* JIT_OPERATION operationFindSwitchImmTargetForDouble(
    ExecState* exec, EncodedJSValue encodedValue, size_t tableIndex)
{
    CodeBlock* codeBlock = exec->codeBlock();
    SimpleJumpTable& table = codeBlock->switchJumpTable(tableIndex);
    JSValue value = JSValue::decode(encodedValue);
    ASSERT(value.isDouble());
    double asDouble = value.asDouble();
    int32_t asInt32 = static_cast<int32_t>(asDouble);
    if (asDouble == asInt32)
        return static_cast<char*>(table.ctiForValue(asInt32).executableAddress());
    return static_cast<char*>(table.ctiDefault.executableAddress());
}

char* JIT_OPERATION operationSwitchString(ExecState* exec, size_t tableIndex, JSString* string)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return static_cast<char*>(exec->codeBlock()->stringSwitchJumpTable(tableIndex).ctiForValue(string->value(exec).impl()).executableAddress());
}

int32_t JIT_OPERATION operationSwitchStringAndGetBranchOffset(ExecState* exec, size_t tableIndex, JSString* string)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    return exec->codeBlock()->stringSwitchJumpTable(tableIndex).offsetForValue(string->value(exec).impl(), std::numeric_limits<int32_t>::min());
}

void JIT_OPERATION operationNotifyWrite(ExecState* exec, WatchpointSet* set)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    set->touch("Executed NotifyWrite");
}

void JIT_OPERATION operationThrowStackOverflowForVarargs(ExecState* exec)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    throwStackOverflowError(exec);
}

int32_t JIT_OPERATION operationSizeOfVarargs(ExecState* exec, EncodedJSValue encodedArguments, int32_t firstVarArgOffset)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue arguments = JSValue::decode(encodedArguments);
    
    return sizeOfVarargs(exec, arguments, firstVarArgOffset);
}

void JIT_OPERATION operationLoadVarargs(ExecState* exec, int32_t firstElementDest, EncodedJSValue encodedArguments, int32_t offset, int32_t length, int32_t mandatoryMinimum)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue arguments = JSValue::decode(encodedArguments);
    
    loadVarargs(exec, VirtualRegister(firstElementDest), arguments, offset, length);
    
    for (int32_t i = length; i < mandatoryMinimum; ++i)
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

size_t JIT_OPERATION operationDefaultHasInstance(ExecState* exec, JSCell* value, JSCell* proto) // Returns jsBoolean(True|False) on 64-bit.
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    if (JSObject::defaultHasInstance(exec, value, proto))
        return 1;
    return 0;
}

void JIT_OPERATION operationProcessTypeProfilerLogDFG(ExecState* exec) 
{
    exec->vm().typeProfilerLog()->processLogEntries(ASCIILiteral("Log Full, called from inside DFG."));
}

void JIT_OPERATION debugOperationPrintSpeculationFailure(ExecState* exec, void* debugInfoRaw, void* scratch)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    SpeculationFailureDebugInfo* debugInfo = static_cast<SpeculationFailureDebugInfo*>(debugInfoRaw);
    CodeBlock* codeBlock = debugInfo->codeBlock;
    CodeBlock* alternative = codeBlock->alternative();
    dataLog("Speculation failure in ", *codeBlock);
    dataLog(" @ exit #", vm->osrExitIndex, " (bc#", debugInfo->bytecodeOffset, ", ", exitKindToString(debugInfo->kind), ") with ");
    if (alternative) {
        dataLog(
            "executeCounter = ", alternative->jitExecuteCounter(),
            ", reoptimizationRetryCounter = ", alternative->reoptimizationRetryCounter(),
            ", optimizationDelayCounter = ", alternative->optimizationDelayCounter());
    } else
        dataLog("no alternative code block (i.e. we've been jettisoned)");
    dataLog(", osrExitCounter = ", codeBlock->osrExitCounter(), "\n");
    dataLog("    GPRs at time of exit:");
    char* scratchPointer = static_cast<char*>(scratch);
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
        GPRReg gpr = GPRInfo::toRegister(i);
        dataLog(" ", GPRInfo::debugName(gpr), ":", RawPointer(*reinterpret_cast_ptr<void**>(scratchPointer)));
        scratchPointer += sizeof(EncodedJSValue);
    }
    dataLog("\n");
    dataLog("    FPRs at time of exit:");
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        FPRReg fpr = FPRInfo::toRegister(i);
        dataLog(" ", FPRInfo::debugName(fpr), ":");
        uint64_t bits = *reinterpret_cast_ptr<uint64_t*>(scratchPointer);
        double value = *reinterpret_cast_ptr<double*>(scratchPointer);
        dataLogF("%llx:%lf", static_cast<long long>(bits), value);
        scratchPointer += sizeof(EncodedJSValue);
    }
    dataLog("\n");
}

extern "C" void JIT_OPERATION triggerReoptimizationNow(CodeBlock* codeBlock, OSRExitBase* exit)
{
    // It's sort of preferable that we don't GC while in here. Anyways, doing so wouldn't
    // really be profitable.
    DeferGCForAWhile deferGC(codeBlock->vm()->heap);
    
    if (Options::verboseOSR())
        dataLog(*codeBlock, ": Entered reoptimize\n");
    // We must be called with the baseline code block.
    ASSERT(JITCode::isBaselineCode(codeBlock->jitType()));

    // If I am my own replacement, then reoptimization has already been triggered.
    // This can happen in recursive functions.
    if (codeBlock->replacement() == codeBlock) {
        if (Options::verboseOSR())
            dataLog(*codeBlock, ": Not reoptimizing because we've already been jettisoned.\n");
        return;
    }
    
    // Otherwise, the replacement must be optimized code. Use this as an opportunity
    // to check our logic.
    ASSERT(codeBlock->hasOptimizedReplacement());
    CodeBlock* optimizedCodeBlock = codeBlock->replacement();
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
        if (Options::verboseOSR())
            dataLog("Deferring FTL-optimization of ", *codeBlock, " indefinitely because there was an FTL failure.\n");
        jitCode->dontOptimizeAnytimeSoon(codeBlock);
        return false;
    }

    if (!codeBlock->hasOptimizedReplacement()
        && !jitCode->checkIfOptimizationThresholdReached(codeBlock)) {
        if (Options::verboseOSR())
            dataLog("Choosing not to FTL-optimize ", *codeBlock, " yet.\n");
        return false;
    }
    return true;
}

static void triggerFTLReplacementCompile(VM* vm, CodeBlock* codeBlock, JITCode* jitCode)
{
    Worklist::State worklistState;
    if (Worklist* worklist = existingGlobalFTLWorklistOrNull()) {
        worklistState = worklist->completeAllReadyPlansForVM(
            *vm, CompilationKey(codeBlock->baselineVersion(), FTLMode));
    } else
        worklistState = Worklist::NotKnown;
    
    if (worklistState == Worklist::Compiling) {
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return;
    }
    
    if (codeBlock->hasOptimizedReplacement()) {
        // That's great, we've compiled the code - next time we call this function,
        // we'll enter that replacement.
        jitCode->optimizeSoon(codeBlock);
        return;
    }
    
    if (worklistState == Worklist::Compiled) {
        // This means that we finished compiling, but failed somehow; in that case the
        // thresholds will be set appropriately.
        if (Options::verboseOSR())
            dataLog("Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.\n");
        return;
    }

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
    DeferGC deferGC(vm->heap);
    CodeBlock* codeBlock = exec->codeBlock();
    
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
            // There is nothing more we can do, the only way this will be entered
            // is through the function entry point.
            jitCode->dontOptimizeAnytimeSoon(codeBlock);
            return;
        }
        if (jitCode->osrEntryBlock() && jitCode->tierUpEntryTriggers.size() == 1) {
            // There is only one outer loop and its trigger must have been set
            // when the plan completed.
            // Exiting the inner loop is useless, we can ignore the counter and leave
            // the trigger do its job.
            jitCode->dontOptimizeAnytimeSoon(codeBlock);
            return;
        }
    }
}

static char* tierUpCommon(ExecState* exec, unsigned originBytecodeIndex, unsigned osrEntryBytecodeIndex)
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
    if (worklistState == Worklist::Compiling) {
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return nullptr;
    }

    if (worklistState == Worklist::Compiled) {
        // This means that compilation failed and we already set the thresholds.
        if (Options::verboseOSR())
            dataLog("Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.\n");
        return nullptr;
    }

    // If we can OSR Enter, do it right away.
    if (originBytecodeIndex == osrEntryBytecodeIndex) {
        unsigned streamIndex = jitCode->bytecodeIndexToStreamIndex.get(originBytecodeIndex);
        if (CodeBlock* entryBlock = jitCode->osrEntryBlock()) {
            if (void* address = FTL::prepareOSREntry(exec, codeBlock, entryBlock, originBytecodeIndex, streamIndex))
                return static_cast<char*>(address);
        }
    }

    // - If we don't have an FTL code block, then try to compile one.
    // - If we do have an FTL code block, then try to enter for a while.
    // - If we couldn't enter for a while, then trigger OSR entry.

    if (!shouldTriggerFTLCompile(codeBlock, jitCode))
        return nullptr;

    if (!jitCode->neverExecutedEntry) {
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

        if (!codeBlock->hasOptimizedReplacement())
            return nullptr;

        if (jitCode->osrEntryRetry < Options::ftlOSREntryRetryThreshold()) {
            jitCode->osrEntryRetry++;
            return nullptr;
        }
    }

    // It's time to try to compile code for OSR entry.
    if (CodeBlock* entryBlock = jitCode->osrEntryBlock()) {
        if (jitCode->osrEntryRetry < Options::ftlOSREntryRetryThreshold()) {
            jitCode->osrEntryRetry++;
            jitCode->setOptimizationThresholdBasedOnCompilationResult(
                codeBlock, CompilationDeferred);
            return nullptr;
        }

        FTL::ForOSREntryJITCode* entryCode = entryBlock->jitCode()->ftlForOSREntry();
        entryCode->countEntryFailure();
        if (entryCode->entryFailureCount() <
            Options::ftlOSREntryFailureCountForReoptimization()) {
            jitCode->setOptimizationThresholdBasedOnCompilationResult(
                codeBlock, CompilationDeferred);
            return nullptr;
        }

        // OSR entry failed. Oh no! This implies that we need to retry. We retry
        // without exponential backoff and we only do this for the entry code block.
        unsigned osrEntryBytecode = entryBlock->jitCode()->ftlForOSREntry()->bytecodeIndex();
        jitCode->clearOSREntryBlock();
        jitCode->osrEntryRetry = 0;
        jitCode->tierUpEntryTriggers.set(osrEntryBytecode, 0);
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return nullptr;
    }

    unsigned streamIndex = jitCode->bytecodeIndexToStreamIndex.get(osrEntryBytecodeIndex);
    auto tierUpHierarchyEntry = jitCode->tierUpInLoopHierarchy.find(osrEntryBytecodeIndex);
    if (tierUpHierarchyEntry != jitCode->tierUpInLoopHierarchy.end()) {
        for (unsigned osrEntryCandidate : tierUpHierarchyEntry->value) {
            if (jitCode->tierUpEntrySeen.contains(osrEntryCandidate)) {
                osrEntryBytecodeIndex = osrEntryCandidate;
                streamIndex = jitCode->bytecodeIndexToStreamIndex.get(osrEntryBytecodeIndex);
            }
        }
    }

    // We aren't compiling and haven't compiled anything for OSR entry. So, try to compile
    // something.
    auto triggerIterator = jitCode->tierUpEntryTriggers.find(osrEntryBytecodeIndex);
    RELEASE_ASSERT(triggerIterator != jitCode->tierUpEntryTriggers.end());
    uint8_t* triggerAddress = &(triggerIterator->value);

    Operands<JSValue> mustHandleValues;
    jitCode->reconstruct(
        exec, codeBlock, CodeOrigin(osrEntryBytecodeIndex), streamIndex, mustHandleValues);
    CodeBlock* replacementCodeBlock = codeBlock->newReplacement();

    CompilationResult forEntryResult = compile(
        *vm, replacementCodeBlock, codeBlock, FTLForOSREntryMode, osrEntryBytecodeIndex,
        mustHandleValues, ToFTLForOSREntryDeferredCompilationCallback::create(triggerAddress));

    if (jitCode->neverExecutedEntry)
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    if (forEntryResult != CompilationSuccessful) {
        jitCode->setOptimizationThresholdBasedOnCompilationResult(
            codeBlock, CompilationDeferred);
        return nullptr;
    }

    // It's possible that the for-entry compile already succeeded. In that case OSR
    // entry will succeed unless we ran out of stack. It's not clear what we should do.
    // We signal to try again after a while if that happens.
    void* address = FTL::prepareOSREntry(
        exec, codeBlock, jitCode->osrEntryBlock(), originBytecodeIndex, streamIndex);
    return static_cast<char*>(address);
}

void JIT_OPERATION triggerTierUpNowInLoop(ExecState* exec, unsigned bytecodeIndex)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    DeferGC deferGC(vm->heap);
    CodeBlock* codeBlock = exec->codeBlock();

    if (codeBlock->jitType() != JITCode::DFGJIT) {
        dataLog("Unexpected code block in DFG->FTL tier-up: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

    JITCode* jitCode = codeBlock->jitCode()->dfg();

    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered triggerTierUpNowInLoop with executeCounter = ",
            jitCode->tierUpCounter, "\n");
    }

    auto tierUpHierarchyEntry = jitCode->tierUpInLoopHierarchy.find(bytecodeIndex);
    if (tierUpHierarchyEntry != jitCode->tierUpInLoopHierarchy.end()
        && !tierUpHierarchyEntry->value.isEmpty()) {
        tierUpCommon(exec, bytecodeIndex, tierUpHierarchyEntry->value.first());
    } else if (shouldTriggerFTLCompile(codeBlock, jitCode))
        triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    // Since we cannot OSR Enter here, the default "optimizeSoon()" is not useful.
    if (codeBlock->hasOptimizedReplacement())
        jitCode->setOptimizationThresholdBasedOnCompilationResult(codeBlock, CompilationDeferred);
}

char* JIT_OPERATION triggerOSREntryNow(ExecState* exec, unsigned bytecodeIndex)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    DeferGC deferGC(vm->heap);
    CodeBlock* codeBlock = exec->codeBlock();

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

    return tierUpCommon(exec, bytecodeIndex, bytecodeIndex);
}

#endif // ENABLE(FTL_JIT)

} // extern "C"
} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // ENABLE(JIT)
