/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef JSGenericTypedArrayViewPrototypeFunctions_h
#define JSGenericTypedArrayViewPrototypeFunctions_h

#include "ArrayPrototype.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArrayBufferViewInlines.h"
#include "JSArrayIterator.h"
#include "JSCBuiltins.h"
#include "JSCJSValueInlines.h"
#include "JSFunction.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSGenericTypedArrayViewPrototypeInlines.h"
#include "JSStringJoiner.h"
#include "StructureInlines.h"
#include "TypedArrayAdaptors.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

static const char* typedArrayBufferHasBeenDetachedErrorMessage = "Underlying ArrayBuffer has been detached from the view";

inline unsigned argumentClampedIndexFromStartOrEnd(ExecState* exec, int argument, unsigned length, unsigned undefinedValue = 0)
{
    JSValue value = exec->argument(argument);
    if (value.isUndefined())
        return undefinedValue;

    double indexDouble = value.toInteger(exec);
    if (indexDouble < 0) {
        indexDouble += length;
        return indexDouble < 0 ? 0 : static_cast<unsigned>(indexDouble);
    }
    return indexDouble > length ? length : static_cast<unsigned>(indexDouble);
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncSet(ExecState* exec)
{
    // 22.2.3.22
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());

    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    unsigned offset;
    if (exec->argumentCount() >= 2) {
        double offsetNumber = exec->uncheckedArgument(1).toInteger(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        if (offsetNumber < 0)
            return throwVMRangeError(exec, "Offset should not be negative");
        offset = static_cast<unsigned>(std::min(offsetNumber, static_cast<double>(std::numeric_limits<unsigned>::max())));
    } else
        offset = 0;

    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    JSObject* sourceArray = jsDynamicCast<JSObject*>(exec->uncheckedArgument(0));
    if (!sourceArray)
        return throwVMError(exec, createTypeError(exec, "First argument should be an object"));

    unsigned length;
    if (isTypedView(sourceArray->classInfo()->typedArrayStorageType)) {
        JSArrayBufferView* sourceView = jsCast<JSArrayBufferView*>(sourceArray);
        if (sourceView->isNeutered())
            return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

        length = jsCast<JSArrayBufferView*>(sourceArray)->length();
    } else
        length = sourceArray->get(exec, exec->vm().propertyNames->length).toUInt32(exec);

    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    thisObject->set(exec, sourceArray, offset, length);
    return JSValue::encode(jsUndefined());
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncEntries(ExecState* exec)
{
    // 22.2.3.6
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateKeyValue, thisObject));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncCopyWithin(ExecState* exec)
{
    // 22.2.3.5
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    if (exec->argumentCount() < 2)
        return throwVMError(exec, createTypeError(exec, "Expected at least two arguments"));

    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    long length = thisObject->length();
    long to = argumentClampedIndexFromStartOrEnd(exec, 0, length);
    long from = argumentClampedIndexFromStartOrEnd(exec, 1, length);
    long final = argumentClampedIndexFromStartOrEnd(exec, 2, length, length);

    if (final < from)
        return JSValue::encode(exec->thisValue());

    long count = std::min(length - std::max(to, from), final - from);

    typename ViewClass::ElementType* array = thisObject->typedVector();
    memmove(array + to, array + from, count * thisObject->elementSize);

    return JSValue::encode(exec->thisValue());
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncFill(ExecState* exec)
{
    // 22.2.3.8
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    JSValue valueToInsert = exec->argument(0);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    unsigned length = thisObject->length();
    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 1, length);
    unsigned end = argumentClampedIndexFromStartOrEnd(exec, 2, length, length);

    if (end < begin)
        return JSValue::encode(exec->thisValue());

    if (!thisObject->setRangeToValue(exec, begin, end, valueToInsert))
        return JSValue::encode(jsUndefined());

    return JSValue::encode(exec->thisValue());
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncIndexOf(ExecState* exec)
{
    // 22.2.3.13
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    unsigned length = thisObject->length();

    JSValue valueToFind = exec->argument(0);
    unsigned index = argumentClampedIndexFromStartOrEnd(exec, 1, length);

    typename ViewClass::ElementType* array = thisObject->typedVector();
    typename ViewClass::ElementType target = ViewClass::toAdaptorNativeFromValue(exec, valueToFind);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    for (; index < length; ++index) {
        if (array[index] == target)
            return JSValue::encode(jsNumber(index));
    }

    return JSValue::encode(jsNumber(-1));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncJoin(ExecState* exec)
{
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    // 22.2.3.14
    auto joinWithSeparator = [&] (StringView separator) -> EncodedJSValue {
        ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
        unsigned length = thisObject->length();

        JSStringJoiner joiner(*exec, separator, length);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        for (unsigned i = 0; i < length; i++) {
            joiner.append(*exec, thisObject->getIndexQuickly(i));
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        }
        return JSValue::encode(joiner.join(*exec));
    };

    JSValue separatorValue = exec->argument(0);
    if (separatorValue.isUndefined()) {
        const LChar* comma = reinterpret_cast<const LChar*>(",");
        return joinWithSeparator({ comma, 1 });
    }

    JSString* separatorString = separatorValue.toString(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    return joinWithSeparator(separatorString->view(exec).get());
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncKeys(ExecState* exec)
{
    // 22.2.3.15
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateKey, thisObject));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncLastIndexOf(ExecState* exec)
{
    // 22.2.3.16
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    unsigned length = thisObject->length();

    JSValue valueToFind = exec->argument(0);

    int index = length - 1;
    if (exec->argumentCount() >= 2) {
        JSValue fromValue = exec->uncheckedArgument(1);
        double fromDouble = fromValue.toInteger(exec);
        if (fromDouble < 0) {
            fromDouble += length;
            if (fromDouble < 0)
                return JSValue::encode(jsNumber(-1));
        }
        if (fromDouble < length)
            index = static_cast<unsigned>(fromDouble);
    }

    typename ViewClass::ElementType* array = thisObject->typedVector();
    typename ViewClass::ElementType target = ViewClass::toAdaptorNativeFromValue(exec, valueToFind);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    for (; index >= 0; --index) {
        if (array[index] == target)
            return JSValue::encode(jsNumber(index));
    }

    return JSValue::encode(jsNumber(-1));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoGetterFuncLength(ExecState* exec)
{
    // 22.2.3.17
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());

    return JSValue::encode(jsNumber(thisObject->length()));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoGetterFuncByteLength(ExecState* exec)
{
    // 22.2.3.2
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());

    return JSValue::encode(jsNumber(thisObject->byteLength()));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoGetterFuncByteOffset(ExecState* exec)
{
    // 22.2.3.3
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());

    return JSValue::encode(jsNumber(thisObject->byteOffset()));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncReverse(ExecState* exec)
{
    // 22.2.3.21
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    typename ViewClass::ElementType* array = thisObject->typedVector();
    std::reverse(array, array + thisObject->length());

    return JSValue::encode(thisObject);
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewPrivateFuncSort(ExecState* exec)
{
    // 22.2.3.25
    ViewClass* thisObject = jsCast<ViewClass*>(exec->argument(0));
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    thisObject->sort();

    return JSValue::encode(thisObject);
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncSlice(ExecState* exec)
{
    // 22.2.3.26
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());

    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    unsigned thisLength = thisObject->length();

    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 0, thisLength);
    unsigned end = argumentClampedIndexFromStartOrEnd(exec, 1, thisLength, thisLength);

    // Clamp end to begin.
    end = std::max(begin, end);

    ASSERT(end >= begin);
    unsigned length = end - begin;

    typename ViewClass::ElementType* array = thisObject->typedVector();

    Structure* structure =
    callee->globalObject()->typedArrayStructure(ViewClass::TypedArrayStorageType);

    ViewClass* result = ViewClass::createUninitialized(exec, structure, length);

    // We can use memcpy since we know this a new buffer
    memcpy(static_cast<void*>(result->typedVector()), static_cast<void*>(array + begin), length * thisObject->elementSize);

    return JSValue::encode(result);
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncSubarray(ExecState* exec)
{
    // 22.2.3.23
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());

    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    // Get the length here; later assert that the length didn't change.
    unsigned thisLength = thisObject->length();

    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 0, thisLength);
    unsigned end = argumentClampedIndexFromStartOrEnd(exec, 1, thisLength, thisLength);

    // Clamp end to begin.
    end = std::max(begin, end);

    ASSERT(end >= begin);
    unsigned offset = begin;
    unsigned length = end - begin;

    RefPtr<ArrayBuffer> arrayBuffer = thisObject->buffer();
    RELEASE_ASSERT(thisLength == thisObject->length());

    Structure* structure =
    callee->globalObject()->typedArrayStructure(ViewClass::TypedArrayStorageType);

    ViewClass* result = ViewClass::create(
        exec, structure, arrayBuffer,
        thisObject->byteOffset() + offset * ViewClass::elementSize,
        length);

    return JSValue::encode(result);
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL typedArrayViewProtoFuncValues(ExecState* exec)
{
    // 22.2.3.29
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateValue, thisObject));
}

} // namespace JSC

#endif /* JSGenericTypedArrayViewPrototypeFunctions_h */
