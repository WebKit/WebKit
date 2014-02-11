/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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
#include "ErrorConstructor.h"

#include "ErrorPrototype.h"
#include "Interpreter.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ErrorConstructor);

const ClassInfo ErrorConstructor::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(ErrorConstructor) };

ErrorConstructor::ErrorConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void ErrorConstructor::finishCreation(VM& vm, ErrorPrototype* errorPrototype)
{
    Base::finishCreation(vm, errorPrototype->classInfo()->className);
    // ECMA 15.11.3.1 Error.prototype
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, errorPrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), DontDelete | ReadOnly | DontEnum);
}

// ECMA 15.9.3

EncodedJSValue JSC_HOST_CALL Interpreter::constructWithErrorConstructor(ExecState* exec)
{
    JSValue message = exec->argumentCount() ? exec->argument(0) : jsUndefined();
    Structure* errorStructure = asInternalFunction(exec->callee())->globalObject()->errorStructure();
    Vector<StackFrame> stackTrace;
    exec->vm().interpreter->getStackTrace(stackTrace, std::numeric_limits<size_t>::max());
    stackTrace.remove(0);
    return JSValue::encode(ErrorInstance::create(exec, errorStructure, message, stackTrace));
}

ConstructType ErrorConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = Interpreter::constructWithErrorConstructor;
    return ConstructTypeHost;
}

EncodedJSValue JSC_HOST_CALL Interpreter::callErrorConstructor(ExecState* exec)
{
    JSValue message = exec->argumentCount() ? exec->argument(0) : jsUndefined();
    Structure* errorStructure = asInternalFunction(exec->callee())->globalObject()->errorStructure();
    Vector<StackFrame> stackTrace;
    exec->vm().interpreter->getStackTrace(stackTrace, std::numeric_limits<size_t>::max());
    stackTrace.remove(0);
    return JSValue::encode(ErrorInstance::create(exec, errorStructure, message, stackTrace));
}

CallType ErrorConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = Interpreter::callErrorConstructor;
    return CallTypeHost;
}

} // namespace JSC
