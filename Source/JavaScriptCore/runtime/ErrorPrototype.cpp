/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "ErrorPrototype.h"

#include "IntegrityInlines.h"
#include "JSCInlines.h"
#include "StringRecursionChecker.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ErrorPrototypeBase);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ErrorPrototype);

static JSC_DECLARE_HOST_FUNCTION(errorProtoFuncToString);

}

#include "ErrorPrototype.lut.h"

namespace JSC {

const ClassInfo ErrorPrototype::s_info = { "Object", &Base::s_info, &errorPrototypeTable, nullptr, CREATE_METHOD_TABLE(ErrorPrototype) };

/* Source for ErrorPrototype.lut.h
@begin errorPrototypeTable
  toString          errorProtoFuncToString         DontEnum|Function 0
@end
*/

ErrorPrototypeBase::ErrorPrototypeBase(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void ErrorPrototypeBase::finishCreation(VM& vm, const String& name)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    putDirectWithoutTransition(vm, vm.propertyNames->name, jsString(vm, name), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->message, jsEmptyString(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));
}

ErrorPrototype::ErrorPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

// ------------------------------ Functions ---------------------------

// ECMA-262 5.1, 15.11.4.4
JSC_DEFINE_HOST_FUNCTION(errorProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let O be the this value.
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    // 2. If Type(O) is not Object, throw a TypeError exception.
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope);
    JSObject* thisObj = asObject(thisValue);
    Integrity::auditStructureID(vm, thisObj->structureID());

    // Guard against recursion!
    StringRecursionChecker checker(globalObject, thisObj);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    // 3. Let name be the result of calling the [[Get]] internal method of O with argument "name".
    JSValue name = thisObj->get(globalObject, vm.propertyNames->name);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 4. If name is undefined, then let name be "Error"; else let name be ToString(name).
    String nameString;
    if (name.isUndefined())
        nameString = "Error"_s;
    else {
        nameString = name.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    // 5. Let msg be the result of calling the [[Get]] internal method of O with argument "message".
    JSValue message = thisObj->get(globalObject, vm.propertyNames->message);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // (sic)
    // 6. If msg is undefined, then let msg be the empty String; else let msg be ToString(msg).
    // 7. If msg is undefined, then let msg be the empty String; else let msg be ToString(msg).
    String messageString;
    if (message.isUndefined())
        messageString = String();
    else {
        messageString = message.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    // 8. If name is the empty String, return msg.
    if (!nameString.length())
        return JSValue::encode(message.isString() ? message : jsString(vm, messageString));

    // 9. If msg is the empty String, return name.
    if (!messageString.length())
        return JSValue::encode(name.isString() ? name : jsString(vm, nameString));

    // 10. Return the result of concatenating name, ":", a single space character, and msg.
    RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(globalObject, nameString, ": ", messageString)));
}

} // namespace JSC
