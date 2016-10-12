/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

// This implements 22.2.4.7 TypedArraySpeciesCreate
// Note, that this function throws.
template<typename Functor>
inline JSArrayBufferView* speciesConstruct(ExecState* exec, JSObject* exemplar, MarkedArgumentBuffer& args, const Functor& defaultConstructor)
{
    JSValue constructor = exemplar->get(exec, exec->propertyNames().constructor);
    if (exec->hadException())
        return nullptr;

    if (constructor.isUndefined())
        return defaultConstructor();
    if (!constructor.isObject()) {
        throwTypeError(exec, ASCIILiteral("constructor Property should not be null"));
        return nullptr;
    }

    JSValue species = constructor.get(exec, exec->propertyNames().speciesSymbol);
    if (exec->hadException())
        return nullptr;

    if (species.isUndefinedOrNull())
        return defaultConstructor();

    JSValue result = construct(exec, species, args, "species is not a constructor");
    if (exec->hadException())
        return nullptr;

    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(result)) {
        if (!view->isNeutered())
            return view;

        throwTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);
        return nullptr;
    }

    throwTypeError(exec, ASCIILiteral("species constructor did not return a TypedArray View"));
    return nullptr;
}

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
        return throwVMTypeError(exec, ASCIILiteral("Expected at least one argument"));

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
        return throwVMTypeError(exec, ASCIILiteral("First argument should be an object"));

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

    thisObject->set(exec, offset, sourceArray, 0, length, CopyType::Unobservable);
    return JSValue::encode(jsUndefined());
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncCopyWithin(ExecState* exec)
{
    // 22.2.3.5
    VM& vm = exec->vm();
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    long length = thisObject->length();
    long to = argumentClampedIndexFromStartOrEnd(exec, 0, length);
    if (vm.exception())
        return encodedJSValue();
    long from = argumentClampedIndexFromStartOrEnd(exec, 1, length);
    if (vm.exception())
        return encodedJSValue();
    long final = argumentClampedIndexFromStartOrEnd(exec, 2, length, length);
    if (vm.exception())
        return encodedJSValue();

    if (final < from)
        return JSValue::encode(exec->thisValue());

    long count = std::min(length - std::max(to, from), final - from);

    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    typename ViewClass::ElementType* array = thisObject->typedVector();
    memmove(array + to, array + from, count * thisObject->elementSize);

    return JSValue::encode(exec->thisValue());
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncIncludes(ExecState* exec)
{
    VM& vm = exec->vm();
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    unsigned length = thisObject->length();

    if (!length)
        return JSValue::encode(jsBoolean(false));

    JSValue valueToFind = exec->argument(0);

    unsigned index = argumentClampedIndexFromStartOrEnd(exec, 1, length);
    if (vm.exception())
        return JSValue::encode(jsUndefined());

    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    typename ViewClass::ElementType* array = thisObject->typedVector();
    auto targetOption = ViewClass::toAdaptorNativeFromValueWithoutCoercion(valueToFind);
    if (!targetOption)
        return JSValue::encode(jsBoolean(false));

    ASSERT(!vm.exception());
    RELEASE_ASSERT(!thisObject->isNeutered());

    if (std::isnan(static_cast<double>(*targetOption))) {
        for (; index < length; ++index) {
            if (std::isnan(static_cast<double>(array[index])))
                return JSValue::encode(jsBoolean(true));
        }
    } else {
        for (; index < length; ++index) {
            if (array[index] == targetOption)
                return JSValue::encode(jsBoolean(true));
        }
    }

    return JSValue::encode(jsBoolean(false));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncIndexOf(ExecState* exec)
{
    // 22.2.3.13
    VM& vm = exec->vm();
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    if (!exec->argumentCount())
        return throwVMTypeError(exec, ASCIILiteral("Expected at least one argument"));

    unsigned length = thisObject->length();

    JSValue valueToFind = exec->argument(0);
    unsigned index = argumentClampedIndexFromStartOrEnd(exec, 1, length);
    if (vm.exception())
        return encodedJSValue();

    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    typename ViewClass::ElementType* array = thisObject->typedVector();
    auto targetOption = ViewClass::toAdaptorNativeFromValueWithoutCoercion(valueToFind);
    if (!targetOption)
        return JSValue::encode(jsNumber(-1));
    ASSERT(!vm.exception());
    RELEASE_ASSERT(!thisObject->isNeutered());

    for (; index < length; ++index) {
        if (array[index] == targetOption)
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

    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);
    return joinWithSeparator(separatorString->view(exec).get());
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoFuncLastIndexOf(ExecState* exec)
{
    // 22.2.3.16
    VM& vm = exec->vm();
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    if (!exec->argumentCount())
        return throwVMTypeError(exec, ASCIILiteral("Expected at least one argument"));

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

    if (vm.exception())
        return JSValue::encode(JSValue());

    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    auto targetOption = ViewClass::toAdaptorNativeFromValueWithoutCoercion(valueToFind);
    if (!targetOption)
        return JSValue::encode(jsNumber(-1));

    typename ViewClass::ElementType* array = thisObject->typedVector();
    ASSERT(!vm.exception());
    RELEASE_ASSERT(!thisObject->isNeutered());

    for (; index >= 0; --index) {
        if (array[index] == targetOption)
            return JSValue::encode(jsNumber(index));
    }

    return JSValue::encode(jsNumber(-1));
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewProtoGetterFuncBuffer(ExecState* exec)
{
    // 22.2.3.3
    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());

    return JSValue::encode(thisObject->jsBuffer(exec));
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
    VM& vm = exec->vm();
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());

    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    unsigned thisLength = thisObject->length();

    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 0, thisLength);
    if (vm.exception())
        return encodedJSValue();
    unsigned end = argumentClampedIndexFromStartOrEnd(exec, 1, thisLength, thisLength);
    if (vm.exception())
        return encodedJSValue();

    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    // Clamp end to begin.
    end = std::max(begin, end);

    ASSERT(end >= begin);
    unsigned length = end - begin;

    MarkedArgumentBuffer args;
    args.append(jsNumber(length));

    JSArrayBufferView* result = speciesConstruct(exec, thisObject, args, [&]() {
        Structure* structure = callee->globalObject()->typedArrayStructure(ViewClass::TypedArrayStorageType);
        return ViewClass::createUninitialized(exec, structure, length);
    });
    if (exec->hadException())
        return JSValue::encode(JSValue());

    ASSERT(!result->isNeutered());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    // We return early here since we don't allocate a backing store if length is 0 and memmove does not like nullptrs
    if (!length)
        return JSValue::encode(result);

    // The species constructor may return an array with any arbitrary length.
    length = std::min(length, result->length());
    switch (result->classInfo()->typedArrayStorageType) {
    case TypeInt8:
        jsCast<JSInt8Array*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    case TypeInt16:
        jsCast<JSInt16Array*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    case TypeInt32:
        jsCast<JSInt32Array*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    case TypeUint8:
        jsCast<JSUint8Array*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    case TypeUint8Clamped:
        jsCast<JSUint8ClampedArray*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    case TypeUint16:
        jsCast<JSUint16Array*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    case TypeUint32:
        jsCast<JSUint32Array*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    case TypeFloat32:
        jsCast<JSFloat32Array*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    case TypeFloat64:
        jsCast<JSFloat64Array*>(result)->set(exec, 0, thisObject, begin, length, CopyType::LeftToRight);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return JSValue::encode(result);
}

template<typename ViewClass>
EncodedJSValue JSC_HOST_CALL genericTypedArrayViewPrivateFuncSubarrayCreate(ExecState* exec)
{
    // 22.2.3.23
    VM& vm = exec->vm();
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());

    ViewClass* thisObject = jsCast<ViewClass*>(exec->thisValue());
    if (thisObject->isNeutered())
        return throwVMTypeError(exec, typedArrayBufferHasBeenDetachedErrorMessage);

    // Get the length here; later assert that the length didn't change.
    unsigned thisLength = thisObject->length();

    // I would assert that the arguments are integers here but that's not true since
    // https://tc39.github.io/ecma262/#sec-tointeger allows the result of the operation
    // to be +/- Infinity and -0.
    ASSERT(exec->argument(0).isNumber());
    ASSERT(exec->argument(1).isUndefined() || exec->argument(1).isNumber());
    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 0, thisLength);
    ASSERT(!vm.exception());
    unsigned end = argumentClampedIndexFromStartOrEnd(exec, 1, thisLength, thisLength);
    ASSERT(!vm.exception());

    RELEASE_ASSERT(!thisObject->isNeutered());

    // Clamp end to begin.
    end = std::max(begin, end);

    ASSERT(end >= begin);
    unsigned offset = begin;
    unsigned length = end - begin;

    RefPtr<ArrayBuffer> arrayBuffer = thisObject->buffer();
    RELEASE_ASSERT(thisLength == thisObject->length());

    unsigned newByteOffset = thisObject->byteOffset() + offset * ViewClass::elementSize;

    JSObject* defaultConstructor = callee->globalObject()->typedArrayConstructor(ViewClass::TypedArrayStorageType);
    JSValue species = exec->uncheckedArgument(2);
    if (species == defaultConstructor) {
        Structure* structure = callee->globalObject()->typedArrayStructure(ViewClass::TypedArrayStorageType);

        return JSValue::encode(ViewClass::create(
            exec, structure, arrayBuffer,
            thisObject->byteOffset() + offset * ViewClass::elementSize,
            length));
    }

    MarkedArgumentBuffer args;
    args.append(vm.m_typedArrayController->toJS(exec, thisObject->globalObject(), thisObject->buffer()));
    args.append(jsNumber(newByteOffset));
    args.append(jsNumber(length));

    JSObject* result = construct(exec, species, args, "species is not a constructor");
    if (exec->hadException())
        return JSValue::encode(JSValue());

    if (jsDynamicCast<JSArrayBufferView*>(result))
        return JSValue::encode(result);

    throwTypeError(exec, "species constructor did not return a TypedArray View");
    return JSValue::encode(JSValue());
}

} // namespace JSC

#endif /* JSGenericTypedArrayViewPrototypeFunctions_h */
