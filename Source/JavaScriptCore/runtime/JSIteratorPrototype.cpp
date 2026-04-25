/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
 * Copyright (C) 2024 Tetsuharu Ohzeki <tetsuharu.ohzeki@gmail.com>.
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
#include "JSIteratorPrototype.h"

#include "BuiltinNames.h"
#include "CachedCallInlines.h"
#include "CallData.h"
#include "ExceptionScope.h"
#include "ImplementationVisibility.h"
#include "InterpreterInlines.h"
#include "IteratorOperations.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSIteratorConstructor.h"
#include "MathCommon.h"
#include "ThrowScope.h"
#include "VMEntryScopeInlines.h"
#include <cmath>
#include <cstdint>
#include <optional>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

namespace JSC {

const ClassInfo JSIteratorPrototype::s_info = { "Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSIteratorPrototype) };

static JSC_DECLARE_HOST_FUNCTION(iteratorProtoFuncIterator);
static JSC_DECLARE_CUSTOM_GETTER(iteratorProtoConstructorGetter);
static JSC_DECLARE_CUSTOM_SETTER(iteratorProtoConstructorSetter);
static JSC_DECLARE_CUSTOM_GETTER(iteratorProtoToStringTagGetter);
static JSC_DECLARE_CUSTOM_SETTER(iteratorProtoToStringTagSetter);
static JSC_DECLARE_HOST_FUNCTION(iteratorProtoFuncToArray);
static JSC_DECLARE_HOST_FUNCTION(iteratorProtoFuncForEach);
static JSC_DECLARE_HOST_FUNCTION(iteratorProtoFuncIncludes);
static JSC_DECLARE_HOST_FUNCTION(iteratorProtoFuncJoin);

void JSIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSFunction* iteratorFunction = JSFunction::create(vm, globalObject, 0, "[Symbol.iterator]"_s, iteratorProtoFuncIterator, ImplementationVisibility::Public, IteratorIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, iteratorFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.constructor
    putDirectCustomGetterSetterWithoutTransition(vm, vm.propertyNames->constructor, CustomGetterSetter::create(vm, iteratorProtoConstructorGetter, iteratorProtoConstructorSetter), static_cast<unsigned>(PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype-@@tostringtag
    putDirectCustomGetterSetterWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, CustomGetterSetter::create(vm, iteratorProtoToStringTagGetter, iteratorProtoToStringTagSetter), static_cast<unsigned>(PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.toarray
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toArray, iteratorProtoFuncToArray, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Private);
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.foreach
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->forEach, iteratorProtoFuncForEach, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Private);
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.some
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().somePublicName(), jsIteratorPrototypeSomeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.every
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().everyPublicName(), jsIteratorPrototypeEveryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.find
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findPublicName(), jsIteratorPrototypeFindCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.reduce
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().reducePublicName(), jsIteratorPrototypeReduceCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.map
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().mapPublicName(), jsIteratorPrototypeMapCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.filter
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().filterPublicName(), jsIteratorPrototypeFilterCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.take
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("take"_s, jsIteratorPrototypeTakeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.drop
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("drop"_s, jsIteratorPrototypeDropCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.flatmap
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().flatMapPublicName(), jsIteratorPrototypeFlatMapCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::useIteratorChunking()) {
        // https://tc39.es/proposal-iterator-chunking/#sec-iterator.prototype.chunks
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("chunks"_s, jsIteratorPrototypeChunksCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
        // https://tc39.es/proposal-iterator-chunking/#sec-iterator.prototype.windows
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("windows"_s, jsIteratorPrototypeWindowsCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }

    if (Options::useIteratorIncludes()) {
        // https://tc39.es/proposal-iterator-includes/#sec-iterator.prototype.includes
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().includesPublicName(), iteratorProtoFuncIncludes, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    }

    if (Options::useIteratorJoin()) {
        // https://tc39.es/proposal-iterator-join/
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, iteratorProtoFuncJoin, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    }

    if (Options::useExplicitResourceManagement())
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->disposeSymbol, jsIteratorPrototypeDisposeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(iteratorProtoFuncIterator, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(callFrame->thisValue().toThis(globalObject, ECMAMode::strict()));
}

// https://tc39.es/proposal-iterator-helpers/#sec-get-iteratorprototype-constructor
JSC_DEFINE_CUSTOM_GETTER(iteratorProtoConstructorGetter, (JSGlobalObject* globalObject, EncodedJSValue, PropertyName))
{
    return JSValue::encode(globalObject->iteratorConstructor());
}

// https://tc39.es/proposal-iterator-helpers/#sec-set-iteratorprototype-constructor
JSC_DEFINE_CUSTOM_SETTER(iteratorProtoConstructorSetter, (JSGlobalObject* globalObject, EncodedJSValue encodedThisValue, EncodedJSValue value, PropertyName propertyName))
{
    bool shouldThrow = true;
    setterThatIgnoresPrototypeProperties(globalObject, JSValue::decode(encodedThisValue), globalObject->iteratorPrototype(), propertyName, JSValue::decode(value), shouldThrow);
    return JSValue::encode(jsUndefined());
}

// https://tc39.es/proposal-iterator-helpers/#sec-get-iteratorprototype-@@tostringtag
JSC_DEFINE_CUSTOM_GETTER(iteratorProtoToStringTagGetter, (JSGlobalObject* globalObject, EncodedJSValue, PropertyName))
{
    VM& vm = globalObject->vm();
    return JSValue::encode(jsNontrivialString(vm, "Iterator"_s));
}

// https://tc39.es/proposal-iterator-helpers/#sec-set-iteratorprototype-@@tostringtag
JSC_DEFINE_CUSTOM_SETTER(iteratorProtoToStringTagSetter, (JSGlobalObject* globalObject, EncodedJSValue encodedThisValue, EncodedJSValue value, PropertyName propertyName))
{
    bool shouldThrow = true;
    setterThatIgnoresPrototypeProperties(globalObject, JSValue::decode(encodedThisValue), globalObject->iteratorPrototype(), propertyName, JSValue::decode(value), shouldThrow);
    return JSValue::encode(jsUndefined());
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.toarray
JSC_DEFINE_HOST_FUNCTION(iteratorProtoFuncToArray, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Iterator.prototype.toArray requires that |this| be an Object."_s);

    MarkedArgumentBuffer value;
    forEachInIteratorProtocol(globalObject, thisValue, [&value, &scope](VM&, JSGlobalObject* globalObject, JSValue nextItem) {
        value.append(nextItem);
        if (value.hasOverflowed()) [[unlikely]]
            throwOutOfMemoryError(globalObject, scope);
    });
    RETURN_IF_EXCEPTION(scope, { });

    auto* array = constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), value);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(array);
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.foreach
JSC_DEFINE_HOST_FUNCTION(iteratorProtoFuncForEach, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Iterator.prototype.forEach requires that |this| be an Object."_s);

    JSValue callbackArg = callFrame->argument(0);
    if (!callbackArg.isCallable()) {
        iteratorClose(globalObject, thisValue);
        TRY_CLEAR_EXCEPTION(scope, { });
        return throwVMTypeError(globalObject, scope, "Iterator.prototype.forEach requires the callback argument to be callable."_s);
    }

    auto callData = JSC::getCallDataInline(callbackArg);
    ASSERT(callData.type != CallData::Type::None);

    uint64_t counter = 0;

    if (callData.type == CallData::Type::JS) [[likely]] {
        CachedCall cachedCall(globalObject, uncheckedDowncast<JSFunction>(callbackArg), 2);
        RETURN_IF_EXCEPTION(scope, { });

        forEachInIteratorProtocol(globalObject, thisValue, [&](VM&, JSGlobalObject*, JSValue nextItem) ALWAYS_INLINE_LAMBDA {
            cachedCall.callWithArguments(globalObject, jsUndefined(), nextItem, jsNumber(counter++));
        });
    } else {
        forEachInIteratorProtocol(globalObject, thisValue, [&](VM&, JSGlobalObject*, JSValue nextItem) ALWAYS_INLINE_LAMBDA {
            MarkedArgumentBuffer args;
            args.append(nextItem);
            args.append(jsNumber(counter++));
            ASSERT(!args.hasOverflowed());

            call(globalObject, callbackArg, callData, jsUndefined(), args);
        });
    }

    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(jsUndefined());
}

// https://tc39.es/proposal-iterator-includes/#sec-iterator.prototype.includes
JSC_DEFINE_HOST_FUNCTION(iteratorProtoFuncIncludes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Iterator.prototype.includes requires that |this| be an Object."_s);

    JSValue skippedElementsArg = callFrame->argument(1);
    uint64_t toSkip = 0;
    if (!skippedElementsArg.isUndefined()) {
        constexpr ASCIILiteral errorMessage = "Iterator.prototype.includes requires that the second argument is a non-negative integral Number or Infinity."_s;

        if (skippedElementsArg.isInt32()) [[likely]] {
            int32_t skippedAsInt32 = skippedElementsArg.asInt32();
            if (skippedAsInt32 < 0) {
                iteratorClose(globalObject, thisValue);
                TRY_CLEAR_EXCEPTION(scope, { });
                return throwVMRangeError(globalObject, scope, errorMessage);
            }
            toSkip = skippedAsInt32;
        } else if (skippedElementsArg.isDouble()) {
            double skippedAsDouble = skippedElementsArg.asDouble();
            uint64_t skippedAsUInt = truncateDoubleToUint64(skippedAsDouble);
            if (skippedAsUInt == skippedAsDouble && skippedAsUInt < maxSafeIntegerAsUInt64())
                toSkip = skippedAsUInt;
            else if (!isInteger(skippedAsDouble) && !std::isinf(skippedAsDouble)) {
                iteratorClose(globalObject, thisValue);
                TRY_CLEAR_EXCEPTION(scope, { });
                return throwVMTypeError(globalObject, scope, errorMessage);                
            } else if (skippedAsDouble > 0) {
                // if the 2nd argument is +Infinity or too big, we should consume the iterator to the end.
                toSkip = std::numeric_limits<uint64_t>::max();
            } else {
                iteratorClose(globalObject, thisValue);
                TRY_CLEAR_EXCEPTION(scope, { });
                return throwVMRangeError(globalObject, scope, errorMessage);
            }
        } else {
            iteratorClose(globalObject, thisValue);
            TRY_CLEAR_EXCEPTION(scope, { });
            return throwVMTypeError(globalObject, scope, errorMessage);
        }
    }

    IterationRecord iterationRecord = iteratorDirect(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue nextMethod = iterationRecord.nextMethod;
    CallData callData = getCallDataInline(nextMethod);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (callData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, uncheckedDowncast<JSFunction>(nextMethod), 0);
        RETURN_IF_EXCEPTION(scope, { });

        cachedCall = &cachedCallHolder.value();
    }

    JSValue searchElement = callFrame->argument(0);
    uint64_t skipped = 0;

    while (true) {
        JSValue next;
        if (cachedCall) [[likely]] {
            cachedCall->clearArguments();
            next = iteratorStepWithCachedCall(globalObject, iterationRecord, cachedCall);
        } else
            next = iteratorStep(globalObject, iterationRecord);

        RETURN_IF_EXCEPTION(scope, { });

        if (next.isFalse())
            return JSValue::encode(jsBoolean(false));

        if (skipped < toSkip) {
            skipped += 1;
            continue;
        }

        JSValue nextValue = iteratorValue(globalObject, next);
        RETURN_IF_EXCEPTION(scope, { });

        bool isEqual = sameValueZero(globalObject, nextValue, searchElement);
        RETURN_IF_EXCEPTION(scope, { });

        if (isEqual) {
            iteratorClose(globalObject, iterationRecord.iterator);
            TRY_CLEAR_EXCEPTION(scope, { });
            return JSValue::encode(jsBoolean(true));
        }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

// https://tc39.es/proposal-iterator-join/
JSC_DEFINE_HOST_FUNCTION(iteratorProtoFuncJoin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });
    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Iterator.prototype.join requires that |this| be an Object."_s);

    JSValue separatorValue = callFrame->argument(0);
    JSString* separatorString = nullptr;
    if (separatorValue.isUndefined()) {
        constexpr Latin1Character comma = ',';
        separatorString = jsSingleCharacterString(vm, comma);
        RETURN_IF_EXCEPTION(scope, { });
    } else {
        separatorString = separatorValue.toStringOrNull(globalObject);
        EXCEPTION_ASSERT(!!scope.exception() == !separatorString);
        if (!separatorString) {
            scope.release();
            iteratorClose(globalObject, thisValue);
            return { };
        }
        RETURN_IF_EXCEPTION(scope, { });
    }

    IterationRecord iterationRecord = iteratorDirect(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue nextMethod = iterationRecord.nextMethod;
    CallData callData = getCallDataInline(nextMethod);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (callData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, uncheckedDowncast<JSFunction>(nextMethod), 0);
        RETURN_IF_EXCEPTION(scope, { });

        cachedCall = &cachedCallHolder.value();
    }

    JSString* result = nullptr;
    while (true) {
        JSValue next;
        if (cachedCall) [[likely]] {
            cachedCall->clearArguments();
            next = iteratorStepWithCachedCall(globalObject, iterationRecord, cachedCall);
        } else
            next = iteratorStep(globalObject, iterationRecord);
        RETURN_IF_EXCEPTION(scope, { });

        if (next.isFalse())
            break;

        JSValue nextValue = iteratorValue(globalObject, next);
        RETURN_IF_EXCEPTION(scope, { });

        if (nextValue.isUndefinedOrNull())
            continue;

        JSString* nextString = nextValue.toStringOrNull(globalObject);
        EXCEPTION_ASSERT(!!scope.exception() == !nextString);
        if (!nextString) {
            scope.release();
            iteratorClose(globalObject, thisValue);
            return { };
        }
        RETURN_IF_EXCEPTION(scope, { });

        if (!result)
            result = nextString;
        else {
            result = jsString(globalObject, result, separatorString, nextString);
            EXCEPTION_ASSERT(!!scope.exception() == !result);
            if (!result) {
                scope.release();
                iteratorClose(globalObject, thisValue);
                return { };
            }
            RETURN_IF_EXCEPTION(scope, { });
        }
    }
    if (!result)
        result = jsEmptyString(vm);
    return JSValue::encode(result);
}

} // namespace JSC
