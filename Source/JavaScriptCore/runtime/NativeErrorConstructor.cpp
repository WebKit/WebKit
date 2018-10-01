/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008, 2016 Apple Inc. All rights reserved.
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
#include "Interpreter.h"
#include "JSFunction.h"
#include "JSString.h"
#include "NativeErrorPrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(NativeErrorConstructor);

const ClassInfo NativeErrorConstructor::s_info = { "Function", &InternalFunction::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(NativeErrorConstructor) };

NativeErrorConstructor::NativeErrorConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, Interpreter::callNativeErrorConstructor, Interpreter::constructWithNativeErrorConstructor)
{
}

void NativeErrorConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, Structure* prototypeStructure, const String& name)
{
    Base::finishCreation(vm, name);
    ASSERT(inherits(vm, info()));
    
    NativeErrorPrototype* prototype = NativeErrorPrototype::create(vm, prototypeStructure, name, this);
    
    putDirect(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirect(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    m_errorStructure.set(vm, this, ErrorInstance::createStructure(vm, globalObject, prototype));
    ASSERT(m_errorStructure);
    ASSERT(m_errorStructure->isObject());
}

void NativeErrorConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    NativeErrorConstructor* thisObject = jsCast<NativeErrorConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_errorStructure);
}

EncodedJSValue JSC_HOST_CALL Interpreter::constructWithNativeErrorConstructor(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue message = exec->argument(0);
    Structure* errorStructure = InternalFunction::createSubclassStructure(exec, exec->newTarget(), jsCast<NativeErrorConstructor*>(exec->jsCallee())->errorStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    ASSERT(errorStructure);
    RELEASE_AND_RETURN(scope, JSValue::encode(ErrorInstance::create(exec, errorStructure, message, nullptr, TypeNothing, false)));
}

EncodedJSValue JSC_HOST_CALL Interpreter::callNativeErrorConstructor(ExecState* exec)
{
    JSValue message = exec->argument(0);
    Structure* errorStructure = static_cast<NativeErrorConstructor*>(exec->jsCallee())->errorStructure();
    return JSValue::encode(ErrorInstance::create(exec, errorStructure, message, nullptr, TypeNothing, false));
}

} // namespace JSC
