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
#include "NativeErrorConstructor.h"

#include "ErrorInstance.h"
#include "JSFunction.h"
#include "JSString.h"
#include "NativeErrorPrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(NativeErrorConstructor);

const ClassInfo NativeErrorConstructor::s_info = { "Function", &InternalFunction::s_info, 0, 0, CREATE_METHOD_TABLE(NativeErrorConstructor) };

NativeErrorConstructor::NativeErrorConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void NativeErrorConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, Structure* prototypeStructure, const String& name)
{
    Base::finishCreation(vm, name);
    ASSERT(inherits(info()));
    
    NativeErrorPrototype* prototype = NativeErrorPrototype::create(vm, globalObject, prototypeStructure, name, this);
    
    putDirect(vm, vm.propertyNames->length, jsNumber(1), DontDelete | ReadOnly | DontEnum); // ECMA 15.11.7.5
    putDirect(vm, vm.propertyNames->prototype, prototype, DontDelete | ReadOnly | DontEnum);
    m_errorStructure.set(vm, this, ErrorInstance::createStructure(vm, globalObject, prototype));
    ASSERT(m_errorStructure);
    ASSERT(m_errorStructure->isObject());
}

void NativeErrorConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    NativeErrorConstructor* thisObject = jsCast<NativeErrorConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    InternalFunction::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_errorStructure);
}

EncodedJSValue JSC_HOST_CALL Interpreter::constructWithNativeErrorConstructor(ExecState* exec)
{
    JSValue message = exec->argument(0);
    Structure* errorStructure = static_cast<NativeErrorConstructor*>(exec->callee())->errorStructure();
    ASSERT(errorStructure);
    Vector<StackFrame> stackTrace;
    exec->vm().interpreter->getStackTrace(stackTrace, std::numeric_limits<size_t>::max());
    stackTrace.remove(0);
    return JSValue::encode(ErrorInstance::create(exec, errorStructure, message, stackTrace));
}

ConstructType NativeErrorConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = Interpreter::constructWithNativeErrorConstructor;
    return ConstructTypeHost;
}
    
EncodedJSValue JSC_HOST_CALL Interpreter::callNativeErrorConstructor(ExecState* exec)
{
    JSValue message = exec->argument(0);
    Structure* errorStructure = static_cast<NativeErrorConstructor*>(exec->callee())->errorStructure();
    Vector<StackFrame> stackTrace;
    exec->vm().interpreter->getStackTrace(stackTrace, std::numeric_limits<size_t>::max());
    stackTrace.remove(0);
    return JSValue::encode(ErrorInstance::create(exec, errorStructure, message, stackTrace));
}

CallType NativeErrorConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = Interpreter::callNativeErrorConstructor;
    return CallTypeHost;
}

} // namespace JSC
