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
#include "JSPropertyNameIterator.h"
#include "Lookup.h"
#include "ObjectConstructor.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL reflectObjectEnumerate(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectIsExtensible(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectOwnKeys(ExecState*);
static EncodedJSValue JSC_HOST_CALL reflectObjectPreventExtensions(ExecState*);

}

#include "ReflectObject.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ReflectObject);

const ClassInfo ReflectObject::s_info = { "Reflect", &Base::s_info, &reflectObjectTable, CREATE_METHOD_TABLE(ReflectObject) };

/* Source for ReflectObject.lut.h
@begin reflectObjectTable
    apply             reflectObjectApply             DontEnum|Function 3
    deleteProperty    reflectObjectDeleteProperty    DontEnum|Function 2
    enumerate         reflectObjectEnumerate         DontEnum|Function 1
    isExtensible      reflectObjectIsExtensible      DontEnum|Function 1
    ownKeys           reflectObjectOwnKeys           DontEnum|Function 1
    preventExtensions reflectObjectPreventExtensions DontEnum|Function 1
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

EncodedJSValue JSC_HOST_CALL reflectObjectEnumerate(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.enumerate requires the first argument be an object")));
    return JSValue::encode(JSPropertyNameIterator::create(exec, exec->lexicalGlobalObject()->propertyNameIteratorStructure(), asObject(target)));
}

EncodedJSValue JSC_HOST_CALL reflectObjectIsExtensible(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.isExtensible requires the first argument be an object")));
    return JSValue::encode(jsBoolean(asObject(target)->isExtensible()));
}

EncodedJSValue JSC_HOST_CALL reflectObjectOwnKeys(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.ownKeys requires the first argument be an object")));
    return JSValue::encode(ownPropertyKeys(exec, jsCast<JSObject*>(target), PropertyNameMode::StringsAndSymbols, DontEnumPropertiesMode::Include));
}

EncodedJSValue JSC_HOST_CALL reflectObjectPreventExtensions(ExecState* exec)
{
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Reflect.preventExtensions requires the first argument be an object")));
    asObject(target)->preventExtensions(exec->vm());
    return JSValue::encode(jsBoolean(true));
}

} // namespace JSC
