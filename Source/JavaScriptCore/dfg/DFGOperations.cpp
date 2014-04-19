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

#include "config.h"
#include "DFGOperations.h"

#include "Arguments.h"
#include "ButterflyInlines.h"
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
#include "FTLForOSREntryJITCode.h"
#include "FTLOSREntry.h"
#include "HostCallReturnValue.h"
#include "GetterSetter.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITExceptions.h"
#include "JSActivation.h"
#include "VM.h"
#include "JSNameScope.h"
#include "NameInstance.h"
#include "ObjectConstructor.h"
#include "JSCInlines.h"
#include "Repatch.h"
#include "StringConstructor.h"
#include "TypedArrayInlines.h"
#include <wtf/InlineASM.h>

#if ENABLE(JIT)
#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

template<bool strict, bool direct>
static inline void putByVal(ExecState* exec, JSValue baseValue, uint32_t index, JSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
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
        putByVal<strict, direct>(exec, baseValue, property.asUInt32(), value);
        return;
    }

    if (property.isDouble()) {
        double propertyAsDouble = property.asDouble();
        uint32_t propertyAsUInt32 = static_cast<uint32_t>(propertyAsDouble);
        if (propertyAsDouble == propertyAsUInt32) {
            putByVal<strict, direct>(exec, baseValue, propertyAsUInt32, value);
            return;
        }
    }

    if (isName(property)) {
        PutPropertySlot slot(baseValue, strict);
        if (direct) {
            RELEASE_ASSERT(baseValue.isObject());
            asObject(baseValue)->putDirect(*vm, jsCast<NameInstance*>(property.asCell())->privateName(), value, slot);
        } else
            baseValue.put(exec, jsCast<NameInstance*>(property.asCell())->privateName(), value, slot);
        return;
    }

    // Don't put to an object if toString throws an exception.
    Identifier ident(exec, property.toString(exec)->value(exec));
    if (!vm->exception()) {
        PutPropertySlot slot(baseValue, strict);
        if (direct) {
            RELEASE_ASSERT(baseValue.isObject());
            asObject(baseValue)->putDirect(*vm, jsCast<NameInstance*>(property.asCell())->privateName(), value, slot);
        } else
            baseValue.put(exec, ident, value, slot);
    }
}

template<typename ViewClass>
char* newTypedArrayWithSize(ExecState* exec, Structure* structure, int32_t size)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    if (size < 0) {
        vm.throwException(exec, createRangeError(exec, "Requested length is negative"));
        return 0;
    }
    return bitwise_cast<char*>(ViewClass::create(exec, structure, size));
}

template<typename ViewClass>
char* newTypedArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    JSValue value = JSValue::decode(encodedValue);
    
    if (JSArrayBuffer* jsBuffer = jsDynamicCast<JSArrayBuffer*>(value)) {
        RefPtr<ArrayBuffer> buffer = jsBuffer->impl();
        
        if (buffer->byteLength() % ViewClass::elementSize) {
            vm.throwException(exec, createRangeError(exec, "ArrayBuffer length minus the byteOffset is not a multiple of the element size"));
            return 0;
        }
        return bitwise_cast<char*>(
            ViewClass::create(
                exec, structure, buffer, 0, buffer->byteLength() / ViewClass::elementSize));
    }
    
    if (JSObject* object = jsDynamicCast<JSObject*>(value)) {
        unsigned length = object->get(exec, vm.propertyNames->length).toUInt32(exec);
        if (exec->hadException())
            return 0;
        
        ViewClass* result = ViewClass::createUninitialized(exec, structure, length);
        if (!result)
            return 0;
        
        if (!result->set(exec, object, 0, length))
            return 0;
        
        return bitwise_cast<char*>(result);
    }
    
    int length;
    if (value.isInt32())
        length = value.asInt32();
    else if (!value.isNumber()) {
        vm.throwException(exec, createTypeError(exec, "Invalid array length argument"));
        return 0;
    } else {
        length = static_cast<int>(value.asNumber());
        if (length != value.asNumber()) {
            vm.throwException(exec, createTypeError(exec, "Invalid array length argument (fractional lengths not allowed)"));
            return 0;
        }
    }
    
    if (length < 0) {
        vm.throwException(exec, createRangeError(exec, "Requested length is negative"));
        return 0;
    }
    
    return bitwise_cast<char*>(ViewClass::create(exec, structure, length));
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

#if !ASSERT_DISABLED
    ConstructData constructData;
    ASSERT(jsCast<JSFunction*>(constructor)->methodTable(vm)->getConstructData(jsCast<JSFunction*>(constructor), constructData) == ConstructTypeJS);
#endif
    
    return constructEmptyObject(exec, jsCast<JSFunction*>(constructor)->allocationProfile(exec, inlineCapacity)->structure());
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

static inline EncodedJSValue getByVal(ExecState* exec, JSCell* base, uint32_t index)
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
            if (propertyAsUInt32 == propertyAsDouble)
                return getByVal(exec, base, propertyAsUInt32);
        } else if (property.isString()) {
            if (JSValue result = base->fastGetOwnProperty(vm, asString(property)->value(exec)))
                return JSValue::encode(result);
        }
    }

    if (isName(property))
        return JSValue::encode(baseValue.get(exec, jsCast<NameInstance*>(property.asCell())->privateName()));

    Identifier ident(exec, property.toString(exec)->value(exec));
    return JSValue::encode(baseValue.get(exec, ident));
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
        if (JSValue result = base->fastGetOwnProperty(vm, asString(property)->value(exec)))
            return JSValue::encode(result);
    }

    if (isName(property))
        return JSValue::encode(JSValue(base).get(exec, jsCast<NameInstance*>(property.asCell())->privateName()));

    Identifier ident(exec, property.toString(exec)->value(exec));
    return JSValue::encode(JSValue(base).get(exec, ident));
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
    return JSValue::encode(JSValue(base).get(exec, index));
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
        
EncodedJSValue JIT_OPERATION operationRegExpExec(ExecState* exec, JSCell* base, JSCell* argument)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!base->inherits(RegExpObject::info()))
        return throwVMTypeError(exec);

    ASSERT(argument->isString() || argument->isObject());
    JSString* input = argument->isString() ? asString(argument) : asObject(argument)->toString(exec);
    return JSValue::encode(asRegExpObject(base)->exec(exec, input));
}
        
size_t JIT_OPERATION operationRegExpTest(ExecState* exec, JSCell* base, JSCell* argument)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (!base->inherits(RegExpObject::info())) {
        throwTypeError(exec);
        return false;
    }

    ASSERT(argument->isString() || argument->isObject());
    JSString* input = argument->isString() ? asString(argument) : asObject(argument)->toString(exec);
    return asRegExpObject(base)->test(exec, input);
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

    return bitwise_cast<char*>(JSArray::create(*vm, arrayStructure, size));
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
    return newTypedArrayWithOneArgument<JSInt8Array>(exec, structure, encodedValue);
}

char* JIT_OPERATION operationNewInt16ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSInt16Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewInt16ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return newTypedArrayWithOneArgument<JSInt16Array>(exec, structure, encodedValue);
}

char* JIT_OPERATION operationNewInt32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSInt32Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewInt32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return newTypedArrayWithOneArgument<JSInt32Array>(exec, structure, encodedValue);
}

char* JIT_OPERATION operationNewUint8ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSUint8Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewUint8ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return newTypedArrayWithOneArgument<JSUint8Array>(exec, structure, encodedValue);
}

char* JIT_OPERATION operationNewUint8ClampedArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSUint8ClampedArray>(exec, structure, length);
}

char* JIT_OPERATION operationNewUint8ClampedArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return newTypedArrayWithOneArgument<JSUint8ClampedArray>(exec, structure, encodedValue);
}

char* JIT_OPERATION operationNewUint16ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSUint16Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewUint16ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return newTypedArrayWithOneArgument<JSUint16Array>(exec, structure, encodedValue);
}

char* JIT_OPERATION operationNewUint32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSUint32Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewUint32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return newTypedArrayWithOneArgument<JSUint32Array>(exec, structure, encodedValue);
}

char* JIT_OPERATION operationNewFloat32ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSFloat32Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewFloat32ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return newTypedArrayWithOneArgument<JSFloat32Array>(exec, structure, encodedValue);
}

char* JIT_OPERATION operationNewFloat64ArrayWithSize(
    ExecState* exec, Structure* structure, int32_t length)
{
    return newTypedArrayWithSize<JSFloat64Array>(exec, structure, length);
}

char* JIT_OPERATION operationNewFloat64ArrayWithOneArgument(
    ExecState* exec, Structure* structure, EncodedJSValue encodedValue)
{
    return newTypedArrayWithOneArgument<JSFloat64Array>(exec, structure, encodedValue);
}

JSCell* JIT_OPERATION operationCreateInlinedArguments(
    ExecState* exec, InlineCallFrame* inlineCallFrame)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    // NB: This needs to be exceedingly careful with top call frame tracking, since it
    // may be called from OSR exit, while the state of the call stack is bizarre.
    Arguments* result = Arguments::create(vm, exec, inlineCallFrame);
    ASSERT(!vm.exception());
    return result;
}

void JIT_OPERATION operationTearOffInlinedArguments(
    ExecState* exec, JSCell* argumentsCell, JSCell* activationCell, InlineCallFrame* inlineCallFrame)
{
    ASSERT_UNUSED(activationCell, !activationCell); // Currently, we don't inline functions with activations.
    jsCast<Arguments*>(argumentsCell)->tearOff(exec, inlineCallFrame);
}

EncodedJSValue JIT_OPERATION operationGetArgumentByVal(ExecState* exec, int32_t argumentsRegister, int32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue argumentsValue = exec->uncheckedR(argumentsRegister).jsValue();
    
    // If there are no arguments, and we're accessing out of bounds, then we have to create the
    // arguments in case someone has installed a getter on a numeric property.
    if (!argumentsValue)
        exec->uncheckedR(argumentsRegister) = argumentsValue = Arguments::create(exec->vm(), exec);
    
    return JSValue::encode(argumentsValue.get(exec, index));
}

EncodedJSValue JIT_OPERATION operationGetInlinedArgumentByVal(
    ExecState* exec, int32_t argumentsRegister, InlineCallFrame* inlineCallFrame, int32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue argumentsValue = exec->uncheckedR(argumentsRegister).jsValue();
    
    // If there are no arguments, and we're accessing out of bounds, then we have to create the
    // arguments in case someone has installed a getter on a numeric property.
    if (!argumentsValue) {
        exec->uncheckedR(argumentsRegister) = argumentsValue =
            Arguments::create(exec->vm(), exec, inlineCallFrame);
    }
    
    return JSValue::encode(argumentsValue.get(exec, index));
}

JSCell* JIT_OPERATION operationNewFunctionNoCheck(ExecState* exec, JSCell* functionExecutable)
{
    ASSERT(functionExecutable->inherits(FunctionExecutable::info()));
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return JSFunction::create(vm, static_cast<FunctionExecutable*>(functionExecutable), exec->scope());
}

size_t JIT_OPERATION operationIsObject(ExecState* exec, EncodedJSValue value)
{
    return jsIsObjectType(exec, JSValue::decode(value));
}

size_t JIT_OPERATION operationIsFunction(EncodedJSValue value)
{
    return jsIsFunctionType(JSValue::decode(value));
}

JSCell* JIT_OPERATION operationTypeOf(ExecState* exec, JSCell* value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return jsTypeStringForValue(exec, JSValue(value)).asCell();
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

char* JIT_OPERATION operationRageEnsureContiguous(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (!cell->isObject())
        return 0;
    
    return reinterpret_cast<char*>(asObject(cell)->rageEnsureContiguous(vm).data());
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

JSCell* JIT_OPERATION operationMakeRope2(ExecState* exec, JSString* left, JSString* right)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    if (static_cast<int32_t>(left->length() + right->length()) < 0) {
        throwOutOfMemoryError(exec);
        return nullptr;
    }

    return JSRopeString::create(vm, left, right);
}

JSCell* JIT_OPERATION operationMakeRope3(ExecState* exec, JSString* a, JSString* b, JSString* c)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    Checked<int32_t, RecordOverflow> length = a->length();
    length += b->length();
    length += c->length();
    if (length.hasOverflowed()) {
        throwOutOfMemoryError(exec);
        return nullptr;
    }

    return JSRopeString::create(vm, a, b, c);
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

void JIT_OPERATION operationInvalidate(ExecState* exec, VariableWatchpointSet* set)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    set->invalidate();
}

double JIT_OPERATION operationFModOnInts(int32_t a, int32_t b)
{
    return fmod(a, b);
}

JSCell* JIT_OPERATION operationStringFromCharCode(ExecState* exec, int32_t op1)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    return JSC::stringFromCharCode(exec, op1);
}

size_t JIT_OPERATION dfgConvertJSValueToInt32(ExecState* exec, EncodedJSValue value)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    // toInt32/toUInt32 return the same value; we want the value zero extended to fill the register.
    return JSValue::decode(value).toUInt32(exec);
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

extern "C" void JIT_OPERATION triggerReoptimizationNow(CodeBlock* codeBlock)
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

    // In order to trigger reoptimization, one of two things must have happened:
    // 1) We exited more than some number of times.
    // 2) We exited and got stuck in a loop, and now we're exiting again.
    bool didExitABunch = optimizedCodeBlock->shouldReoptimizeNow();
    bool didGetStuckInLoop =
        codeBlock->checkIfOptimizationThresholdReached()
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
static void triggerFTLReplacementCompile(VM* vm, CodeBlock* codeBlock, JITCode* jitCode)
{
    if (codeBlock->baselineVersion()->m_didFailFTLCompilation) {
        if (Options::verboseOSR())
            dataLog("Deferring FTL-optimization of ", *codeBlock, " indefinitely because there was an FTL failure.\n");
        jitCode->dontOptimizeAnytimeSoon(codeBlock);
        return;
    }
    
    if (!jitCode->checkIfOptimizationThresholdReached(codeBlock)) {
        if (Options::verboseOSR())
            dataLog("Choosing not to FTL-optimize ", *codeBlock, " yet.\n");
        return;
    }
    
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
        *vm, codeBlock->newReplacement().get(), codeBlock, FTLMode, UINT_MAX,
        Operands<JSValue>(), ToFTLDeferredCompilationCallback::create(codeBlock));
}

void JIT_OPERATION triggerTierUpNow(ExecState* exec)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    DeferGC deferGC(vm->heap);
    CodeBlock* codeBlock = exec->codeBlock();
    
    JITCode* jitCode = codeBlock->jitCode()->dfg();
    
    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered triggerTierUpNow with executeCounter = ",
            jitCode->tierUpCounter, "\n");
    }
    
    triggerFTLReplacementCompile(vm, codeBlock, jitCode);
}

char* JIT_OPERATION triggerOSREntryNow(
    ExecState* exec, int32_t bytecodeIndex, int32_t streamIndex)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    DeferGC deferGC(vm->heap);
    CodeBlock* codeBlock = exec->codeBlock();
    
    JITCode* jitCode = codeBlock->jitCode()->dfg();
    
    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered triggerTierUpNow with executeCounter = ",
            jitCode->tierUpCounter, "\n");
    }
    
    // - If we don't have an FTL code block, then try to compile one.
    // - If we do have an FTL code block, then try to enter for a while.
    // - If we couldn't enter for a while, then trigger OSR entry.
    
    triggerFTLReplacementCompile(vm, codeBlock, jitCode);

    if (!codeBlock->hasOptimizedReplacement())
        return 0;
    
    if (jitCode->osrEntryRetry < Options::ftlOSREntryRetryThreshold()) {
        jitCode->osrEntryRetry++;
        return 0;
    }
    
    // It's time to try to compile code for OSR entry.
    Worklist::State worklistState;
    if (Worklist* worklist = existingGlobalFTLWorklistOrNull()) {
        worklistState = worklist->completeAllReadyPlansForVM(
            *vm, CompilationKey(codeBlock->baselineVersion(), FTLForOSREntryMode));
    } else
        worklistState = Worklist::NotKnown;
    
    if (worklistState == Worklist::Compiling)
        return 0;
    
    if (CodeBlock* entryBlock = jitCode->osrEntryBlock.get()) {
        void* address = FTL::prepareOSREntry(
            exec, codeBlock, entryBlock, bytecodeIndex, streamIndex);
        if (address)
            return static_cast<char*>(address);
        
        FTL::ForOSREntryJITCode* entryCode = entryBlock->jitCode()->ftlForOSREntry();
        entryCode->countEntryFailure();
        if (entryCode->entryFailureCount() <
            Options::ftlOSREntryFailureCountForReoptimization())
            return 0;
        
        // OSR entry failed. Oh no! This implies that we need to retry. We retry
        // without exponential backoff and we only do this for the entry code block.
        jitCode->osrEntryBlock.clear();
        jitCode->osrEntryRetry = 0;
        return 0;
    }
    
    if (worklistState == Worklist::Compiled) {
        // This means that compilation failed and we already set the thresholds.
        if (Options::verboseOSR())
            dataLog("Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.\n");
        return 0;
    }

    // We aren't compiling and haven't compiled anything for OSR entry. So, try to compile
    // something.
    Operands<JSValue> mustHandleValues;
    jitCode->reconstruct(
        exec, codeBlock, CodeOrigin(bytecodeIndex), streamIndex, mustHandleValues);
    RefPtr<CodeBlock> replacementCodeBlock = codeBlock->newReplacement();
    CompilationResult forEntryResult = compile(
        *vm, replacementCodeBlock.get(), codeBlock, FTLForOSREntryMode, bytecodeIndex,
        mustHandleValues, ToFTLForOSREntryDeferredCompilationCallback::create(codeBlock));
    
    if (forEntryResult != CompilationSuccessful) {
        ASSERT(forEntryResult == CompilationDeferred || replacementCodeBlock->hasOneRef());
        return 0;
    }

    // It's possible that the for-entry compile already succeeded. In that case OSR
    // entry will succeed unless we ran out of stack. It's not clear what we should do.
    // We signal to try again after a while if that happens.
    void* address = FTL::prepareOSREntry(
        exec, codeBlock, jitCode->osrEntryBlock.get(), bytecodeIndex, streamIndex);
    return static_cast<char*>(address);
}

// FIXME: Make calls work well. Currently they're a pure regression.
// https://bugs.webkit.org/show_bug.cgi?id=113621
EncodedJSValue JIT_OPERATION operationFTLCall(ExecState* exec)
{
    ExecState* callerExec = exec->callerFrame();
    
    VM* vm = &callerExec->vm();
    NativeCallFrameTracer tracer(vm, callerExec);
    
    JSValue callee = exec->calleeAsValue();
    CallData callData;
    CallType callType = getCallData(callee, callData);
    if (callType == CallTypeNone) {
        vm->throwException(callerExec, createNotAFunctionError(callerExec, callee));
        return JSValue::encode(jsUndefined());
    }
    
    return JSValue::encode(call(callerExec, callee, callType, callData, exec->thisValue(), exec));
}

// FIXME: Make calls work well. Currently they're a pure regression.
// https://bugs.webkit.org/show_bug.cgi?id=113621
EncodedJSValue JIT_OPERATION operationFTLConstruct(ExecState* exec)
{
    ExecState* callerExec = exec->callerFrame();
    
    VM* vm = &callerExec->vm();
    NativeCallFrameTracer tracer(vm, callerExec);
    
    JSValue callee = exec->calleeAsValue();
    ConstructData constructData;
    ConstructType constructType = getConstructData(callee, constructData);
    if (constructType == ConstructTypeNone) {
        vm->throwException(callerExec, createNotAFunctionError(callerExec, callee));
        return JSValue::encode(jsUndefined());
    }
    
    return JSValue::encode(construct(callerExec, callee, constructType, constructData, exec));
}
#endif // ENABLE(FTL_JIT)

} // extern "C"
} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // ENABLE(JIT)
