/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
#include "JSGlobalObjectFunctions.h"
#include "JSPropertyNameIterator.h"
#include "Lookup.h"
#include "ObjectConstructor.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL reflectObjectDefineProperty(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectEnumerate(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectGet(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectGetOwnPropertyDescriptor(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectGetPrototypeOf(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectIsExtensible(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectOwnKeys(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectPreventExtensions(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectSetPrototypeOf(ExecState*);

}

#include "ReflectObject.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ReflectObject);

const ClassInfo ReflectObject::s_info = { "Reflect", &Base::s_info, &reflectObjectTable, CREATE_METHOD_TABLE(ReflectObject) };

/* Source for ReflectObject.lut.h
@begin reflectObjectTable
    apply                    JSBuiltin                             DontEnum|Function 3
    defineProperty           reflectObjectDefineProperty           DontEnum|Function 3
    deleteProperty           JSBuiltin                             DontEnum|Function 2
    enumerate                reflectObjectEnumerate                DontEnum|Function 1
    get                      reflectObjectGet                      DontEnum|Function 2
    getOwnPropertyDescriptor reflectObjectGetOwnPropertyDescriptor DontEnum|Function 2
    getPrototypeOf           reflectObjectGetPrototypeOf           DontEnum|Function 1
    has                      JSBuiltin                             DontEnum|Function 2
    isExtensible             reflectObjectIsExtensible             DontEnum|Function 1
    ownKeys                  reflectObjectOwnKeys                  DontEnum|Function 1
    preventExtensions        reflectObjectPreventExtensions        DontEnum|Function 1
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
    ASSERT(inherits(info()));
}

bool ReflectObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<Base>(exec, reflectObjectTable, jsCast<ReflectObject*>(object), propertyName, slot);
}

// ------------------------------ Functions --------------------------------

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.defineproperty
EncodedJSValue JSC_HOST_CALL reflectObjectDefineProperty(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.defineProperty requires the first argument be an object")));
    auto propertyName = exec->argument(1).toPropertyKey(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    PropertyDescriptor descriptor;
    if (!toPropertyDescriptor(exec, exec->argument(2), descriptor))
        return JSValue::encode(jsUndefined());
    ASSERT((descriptor.attributes() & Accessor) || (!descriptor.isAccessorDescriptor()));
    ASSERT(!exec->hadException());

    // Reflect.defineProperty should not throw an error when the defineOwnProperty operation fails.
    bool shouldThrow = false;
    JSObject* targetObject = asObject(target);
    return JSValue::encode(jsBoolean(targetObject->methodTable(exec->vm())->defineOwnProperty(targetObject, exec, propertyName, descriptor, shouldThrow)));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.enumerate
EncodedJSValue JSC_HOST_CALL reflectObjectEnumerate(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.enumerate requires the first argument be an object")));
    return JSValue::encode(JSPropertyNameIterator::create(exec, exec->lexicalGlobalObject()->propertyNameIteratorStructure(), asObject(target)));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.get
EncodedJSValue JSC_HOST_CALL reflectObjectGet(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.get requires the first argument be an object")));

    const Identifier propertyName = exec->argument(1).toPropertyKey(exec);
    if (exec->hadException())
        return JSValue::encode(jsNull());

    JSValue receiver = target;
    if (exec->argumentCount() >= 3)
        receiver = exec->argument(2);

    PropertySlot slot(receiver, PropertySlot::InternalMethodType::Get);
    return JSValue::encode(target.get(exec, propertyName, slot));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.getownpropertydescriptor
EncodedJSValue JSC_HOST_CALL reflectObjectGetOwnPropertyDescriptor(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.getOwnPropertyDescriptor requires the first argument be an object")));

    auto key = exec->argument(1).toPropertyKey(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    return JSValue::encode(objectConstructorGetOwnPropertyDescriptor(exec, asObject(target), key));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.getprototypeof
EncodedJSValue JSC_HOST_CALL reflectObjectGetPrototypeOf(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.getPrototypeOf requires the first argument be an object")));
    return JSValue::encode(objectConstructorGetPrototypeOf(exec, asObject(target)));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.isextensible
EncodedJSValue JSC_HOST_CALL reflectObjectIsExtensible(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.isExtensible requires the first argument be an object")));

    bool isExtensible = asObject(target)->isExtensibleInline(exec);
    if (exec->hadException())
        return JSValue::encode(JSValue());
    return JSValue::encode(jsBoolean(isExtensible));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.ownkeys
EncodedJSValue JSC_HOST_CALL reflectObjectOwnKeys(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.ownKeys requires the first argument be an object")));
    return JSValue::encode(ownPropertyKeys(exec, jsCast<JSObject*>(target), PropertyNameMode::StringsAndSymbols, DontEnumPropertiesMode::Include));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.preventextensions
EncodedJSValue JSC_HOST_CALL reflectObjectPreventExtensions(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.preventExtensions requires the first argument be an object")));
    JSObject* object = asObject(target);
    bool result = object->methodTable(exec->vm())->preventExtensions(object, exec);
    if (exec->hadException())
        return JSValue::encode(JSValue());
    return JSValue::encode(jsBoolean(result));
}

// http://www.ecma-international.org/ecma-262/6.0/#sec-reflect.setprototypeof
EncodedJSValue JSC_HOST_CALL reflectObjectSetPrototypeOf(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.setPrototypeOf requires the first argument be an object")));
    JSValue proto = exec->argument(1);
    if (!proto.isObject() && !proto.isNull())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.setPrototypeOf requires the second argument be either an object or null")));

    JSObject* object = asObject(target);

    if (!checkProtoSetterAccessAllowed(exec, object))
        return JSValue::encode(jsBoolean(false));

    if (object->prototype() == proto)
        return JSValue::encode(jsBoolean(true));

    bool isExtensible = object->isExtensibleInline(exec);
    if (exec->hadException())
        return JSValue::encode(JSValue());
    if (!isExtensible)
        return JSValue::encode(jsBoolean(false));

    VM& vm = exec->vm();
    bool didSetPrototype = object->setPrototypeOfInline(vm, exec, proto);
    if (vm.exception())
        return JSValue::encode(JSValue());
    return JSValue::encode(jsBoolean(didSetPrototype));
}

} // namespace JSC
