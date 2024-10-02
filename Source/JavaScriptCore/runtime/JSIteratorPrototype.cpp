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
#include "CachedCall.h"
#include "InterpreterInlines.h"
#include "IteratorOperations.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSIteratorConstructor.h"
#include "JSObject.h"
#include "VMEntryScopeInlines.h"
#include <wtf/PlatformCallingConventions.h>

namespace JSC {

const ClassInfo JSIteratorPrototype::s_info = { "Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSIteratorPrototype) };

static JSC_DECLARE_HOST_FUNCTION(iteratorProtoFuncIterator);
static JSC_DECLARE_CUSTOM_GETTER(iteratorProtoConstructorGetter);
static JSC_DECLARE_CUSTOM_SETTER(iteratorProtoConstructorSetter);
static JSC_DECLARE_CUSTOM_GETTER(iteratorProtoToStringTagGetter);
static JSC_DECLARE_CUSTOM_SETTER(iteratorProtoToStringTagSetter);
static JSC_DECLARE_HOST_FUNCTION(iteratorProtoFuncToArray);
static JSC_DECLARE_HOST_FUNCTION(iteratorProtoFuncForEach);

void JSIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSFunction* iteratorFunction = JSFunction::create(vm, globalObject, 0, "[Symbol.iterator]"_s, iteratorProtoFuncIterator, ImplementationVisibility::Public, IteratorIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, iteratorFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::useIteratorHelpers()) {
        // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.constructor
        putDirectCustomGetterSetterWithoutTransition(vm, vm.propertyNames->constructor, CustomGetterSetter::create(vm, iteratorProtoConstructorGetter, iteratorProtoConstructorSetter), static_cast<unsigned>(PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor));
        // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype-@@tostringtag
        putDirectCustomGetterSetterWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, CustomGetterSetter::create(vm, iteratorProtoToStringTagGetter, iteratorProtoToStringTagSetter), static_cast<unsigned>(PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor));
        // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.toarray
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("toArray"_s, iteratorProtoFuncToArray, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Private);
        // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.foreach
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("forEach"_s, iteratorProtoFuncForEach, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Private);
        // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.some
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().somePublicName(), jsIteratorPrototypeSomeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
        // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.every
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().everyPublicName(), jsIteratorPrototypeEveryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
        // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.find
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findPublicName(), jsIteratorPrototypeFindCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
        // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.reduce
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().reducePublicName(), jsIteratorPrototypeReduceCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }
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
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = JSValue::decode(encodedThisValue);

    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Iterator.prototype.constructor setter expected |this| to be an object."_s);

    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (thisObject == globalObject->iteratorPrototype())
        return throwVMTypeError(globalObject, scope, "Iterator.prototype.constructor setter cannot be applied to Iterator.prototype."_s);

    thisObject->putDirect(vm, propertyName, JSValue::decode(value), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

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
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = JSValue::decode(encodedThisValue);

    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "Iterator.prototype[Symbol.toStringTag] setter expected |this| to be an object."_s);

    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (thisObject == globalObject->iteratorPrototype())
        return throwVMTypeError(globalObject, scope, "Iterator.prototype[Symbol.toStringTag] setter cannot be applied to Iterator.prototype."_s);

    thisObject->putDirect(vm, propertyName, JSValue::decode(value), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

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
        if (UNLIKELY(value.hasOverflowed()))
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
    if (!callbackArg.isCallable())
        return throwVMTypeError(globalObject, scope, "Iterator.prototype.forEach requires the callback argument to be callable."_s);

    CachedCall cachedCall(globalObject, jsCast<JSFunction*>(asObject(callbackArg)), 2);
    RETURN_IF_EXCEPTION(scope, { });
    uint32_t count = 0;
    forEachInIteratorProtocol(globalObject, thisValue, [&cachedCall, &count](VM& vm, JSGlobalObject* globalObject, JSValue nextItem) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        cachedCall.callWithArguments(globalObject, jsUndefined(), nextItem, jsNumber(count++));
        RETURN_IF_EXCEPTION(scope, void());
    });
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(jsUndefined());
}

} // namespace JSC
