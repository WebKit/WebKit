/*
 * Copyright (C) 2015-2017 Apple Inc. All Rights Reserved.
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
#include "ReflectObject.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL reflectObjectConstruct(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectDefineProperty(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectGet(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectGetOwnPropertyDescriptor(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectGetPrototypeOf(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectIsExtensible(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectOwnKeys(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectPreventExtensions(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectSet(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL reflectObjectSetPrototypeOf(JSGlobalObject*, CallFrame*);

}

#include "ReflectObject.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ReflectObject);

const ClassInfo ReflectObject::s_info = { "Reflect", &Base::s_info, &reflectObjectTable, nullptr, CREATE_METHOD_TABLE(ReflectObject) };

/* Source for ReflectObject.lut.h
@begin reflectObjectTable
    apply                    JSBuiltin                             DontEnum|Function 3
    construct                reflectObjectConstruct                DontEnum|Function 2
    defineProperty           reflectObjectDefineProperty           DontEnum|Function 3
    deleteProperty           JSBuiltin                             DontEnum|Function 2
    get                      reflectObjectGet                      DontEnum|Function 2
    getOwnPropertyDescriptor reflectObjectGetOwnPropertyDescriptor DontEnum|Function 2
    getPrototypeOf           reflectObjectGetPrototypeOf           DontEnum|Function 1 ReflectGetPrototypeOfIntrinsic
    has                      JSBuiltin                             DontEnum|Function 2
    isExtensible             reflectObjectIsExtensible             DontEnum|Function 1
    ownKeys                  reflectObjectOwnKeys                  DontEnum|Function 1
    preventExtensions        reflectObjectPreventExtensions        DontEnum|Function 1
    set                      reflectObjectSet                      DontEnum|Function 3
    setPrototypeOf           reflectObjectSetPrototypeOf           DontEnum|Function 2
@end
*/

ReflectObject::ReflectObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void ReflectObject::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// ------------------------------ Functions --------------------------------

// https://tc39.github.io/ecma262/#sec-reflect.construct
EncodedJSValue JSC_HOST_CALL reflectObjectConstruct(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.construct requires the first argument be a constructor"_s));

    auto constructData = getConstructData(vm, target);
    if (constructData.type == CallData::Type::None)
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.construct requires the first argument be a constructor"_s));

    JSValue newTarget = target;
    if (callFrame->argumentCount() >= 3) {
        newTarget = callFrame->argument(2);
        if (!newTarget.isConstructor(vm))
            return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.construct requires the third argument be a constructor if present"_s));
    }

    MarkedArgumentBuffer arguments;
    JSObject* argumentsObject = jsDynamicCast<JSObject*>(vm, callFrame->argument(1));
    if (!argumentsObject)
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.construct requires the second argument be an object"_s));

    createListFromArrayLike(globalObject, argumentsObject, RuntimeTypeMaskAllTypes, "This error must not be raised"_s, "This error must not be raised"_s, [&] (JSValue value, RuntimeType) -> bool {
        arguments.append(value);
        return false;
    });
    RETURN_IF_EXCEPTION(scope, (arguments.overflowCheckNotNeeded(), encodedJSValue()));
    if (UNLIKELY(arguments.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(construct(globalObject, target, constructData, arguments, newTarget)));
}

// https://tc39.github.io/ecma262/#sec-reflect.defineproperty
EncodedJSValue JSC_HOST_CALL reflectObjectDefineProperty(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.defineProperty requires the first argument be an object"_s));
    auto propertyName = callFrame->argument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    PropertyDescriptor descriptor;
    bool success = toPropertyDescriptor(globalObject, callFrame->argument(2), descriptor);
    EXCEPTION_ASSERT(!scope.exception() == success);
    if (UNLIKELY(!success))
        return encodedJSValue();
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    scope.assertNoException();

    // Reflect.defineProperty should not throw an error when the defineOwnProperty operation fails.
    bool shouldThrow = false;
    JSObject* targetObject = asObject(target);
    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(targetObject->methodTable(vm)->defineOwnProperty(targetObject, globalObject, propertyName, descriptor, shouldThrow))));
}

// https://tc39.github.io/ecma262/#sec-reflect.get
EncodedJSValue JSC_HOST_CALL reflectObjectGet(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.get requires the first argument be an object"_s));

    const Identifier propertyName = callFrame->argument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue receiver = target;
    if (callFrame->argumentCount() >= 3)
        receiver = callFrame->argument(2);

    PropertySlot slot(receiver, PropertySlot::InternalMethodType::Get);
    RELEASE_AND_RETURN(scope, JSValue::encode(target.get(globalObject, propertyName, slot)));
}

// https://tc39.github.io/ecma262/#sec-reflect.getownpropertydescriptor
EncodedJSValue JSC_HOST_CALL reflectObjectGetOwnPropertyDescriptor(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.getOwnPropertyDescriptor requires the first argument be an object"_s));

    auto key = callFrame->argument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(objectConstructorGetOwnPropertyDescriptor(globalObject, asObject(target), key)));
}

// https://tc39.github.io/ecma262/#sec-reflect.getprototypeof
EncodedJSValue JSC_HOST_CALL reflectObjectGetPrototypeOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.getPrototypeOf requires the first argument be an object"_s));
    RELEASE_AND_RETURN(scope, JSValue::encode(asObject(target)->getPrototype(vm, globalObject)));
}

// https://tc39.github.io/ecma262/#sec-reflect.isextensible
EncodedJSValue JSC_HOST_CALL reflectObjectIsExtensible(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.isExtensible requires the first argument be an object"_s));

    bool isExtensible = asObject(target)->isExtensible(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsBoolean(isExtensible));
}

// https://tc39.github.io/ecma262/#sec-reflect.ownkeys
EncodedJSValue JSC_HOST_CALL reflectObjectOwnKeys(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.ownKeys requires the first argument be an object"_s));
    RELEASE_AND_RETURN(scope, JSValue::encode(ownPropertyKeys(globalObject, jsCast<JSObject*>(target), PropertyNameMode::StringsAndSymbols, DontEnumPropertiesMode::Include)));
}

// https://tc39.github.io/ecma262/#sec-reflect.preventextensions
EncodedJSValue JSC_HOST_CALL reflectObjectPreventExtensions(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.preventExtensions requires the first argument be an object"_s));
    JSObject* object = asObject(target);
    bool result = object->methodTable(vm)->preventExtensions(object, globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsBoolean(result));
}

// https://tc39.github.io/ecma262/#sec-reflect.set
EncodedJSValue JSC_HOST_CALL reflectObjectSet(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.set requires the first argument be an object"_s));
    JSObject* targetObject = asObject(target);

    auto propertyName = callFrame->argument(1).toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue receiver = target;
    if (callFrame->argumentCount() >= 4)
        receiver = callFrame->argument(3);

    // Do not raise any readonly errors that happen in strict mode.
    bool shouldThrowIfCantSet = false;
    PutPropertySlot slot(receiver, shouldThrowIfCantSet);
    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(targetObject->methodTable(vm)->put(targetObject, globalObject, propertyName, callFrame->argument(2), slot))));
}

// https://tc39.github.io/ecma262/#sec-reflect.setprototypeof
EncodedJSValue JSC_HOST_CALL reflectObjectSetPrototypeOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.setPrototypeOf requires the first argument be an object"_s));
    JSValue proto = callFrame->argument(1);
    if (!proto.isObject() && !proto.isNull())
        return JSValue::encode(throwTypeError(globalObject, scope, "Reflect.setPrototypeOf requires the second argument be either an object or null"_s));

    JSObject* object = asObject(target);

    bool shouldThrowIfCantSet = false;
    bool didSetPrototype = object->setPrototype(vm, globalObject, proto, shouldThrowIfCantSet);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(jsBoolean(didSetPrototype));
}

} // namespace JSC
