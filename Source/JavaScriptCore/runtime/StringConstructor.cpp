/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "StringConstructor.h"

#include "Executable.h"
#include "JITCode.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "Operations.h"
#include "StringPrototype.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL stringFromCharCode(ExecState*);

}

#include "StringConstructor.lut.h"

namespace JSC {

const ClassInfo StringConstructor::s_info = { "Function", &InternalFunction::s_info, 0, ExecState::stringConstructorTable, CREATE_METHOD_TABLE(StringConstructor) };

/* Source for StringConstructor.lut.h
@begin stringConstructorTable
  fromCharCode          stringFromCharCode         DontEnum|Function 1
@end
*/

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(StringConstructor);

StringConstructor::StringConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void StringConstructor::finishCreation(VM& vm, StringPrototype* stringPrototype)
{
    Base::finishCreation(vm, stringPrototype->classInfo()->className);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, stringPrototype, ReadOnly | DontEnum | DontDelete);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

bool StringConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot &slot)
{
    return getStaticFunctionSlot<InternalFunction>(exec, ExecState::stringConstructorTable(exec), jsCast<StringConstructor*>(object), propertyName, slot);
}

// ------------------------------ Functions --------------------------------

static NEVER_INLINE JSValue stringFromCharCodeSlowCase(ExecState* exec)
{
    unsigned length = exec->argumentCount();
    UChar* buf;
    PassRefPtr<StringImpl> impl = StringImpl::createUninitialized(length, buf);
    for (unsigned i = 0; i < length; ++i)
        buf[i] = static_cast<UChar>(exec->uncheckedArgument(i).toUInt32(exec));
    return jsString(exec, impl);
}

static EncodedJSValue JSC_HOST_CALL stringFromCharCode(ExecState* exec)
{
    if (LIKELY(exec->argumentCount() == 1))
        return JSValue::encode(jsSingleCharacterString(exec, exec->uncheckedArgument(0).toUInt32(exec)));
    return JSValue::encode(stringFromCharCodeSlowCase(exec));
}

JSCell* JSC_HOST_CALL stringFromCharCode(ExecState* exec, int32_t arg)
{
    return jsSingleCharacterString(exec, arg);
}

static EncodedJSValue JSC_HOST_CALL constructWithStringConstructor(ExecState* exec)
{
    JSGlobalObject* globalObject = asInternalFunction(exec->callee())->globalObject();
    VM& vm = exec->vm();

    if (!exec->argumentCount())
        return JSValue::encode(StringObject::create(vm, globalObject->stringObjectStructure()));
    
    return JSValue::encode(StringObject::create(vm, globalObject->stringObjectStructure(), exec->uncheckedArgument(0).toString(exec)));
}

ConstructType StringConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithStringConstructor;
    return ConstructTypeHost;
}

static EncodedJSValue JSC_HOST_CALL callStringConstructor(ExecState* exec)
{
    if (!exec->argumentCount())
        return JSValue::encode(jsEmptyString(exec));
    return JSValue::encode(exec->uncheckedArgument(0).toString(exec));
}

CallType StringConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callStringConstructor;
    return CallTypeHost;
}

} // namespace JSC
