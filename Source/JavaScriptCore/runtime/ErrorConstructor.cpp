/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

const ClassInfo ErrorConstructor::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ErrorConstructor) };

static EncodedJSValue JSC_HOST_CALL callErrorConstructor(ExecState*);
static EncodedJSValue JSC_HOST_CALL constructErrorConstructor(ExecState*);

ErrorConstructor::ErrorConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callErrorConstructor, constructErrorConstructor)
{
}

void ErrorConstructor::finishCreation(VM& vm, ErrorPrototype* errorPrototype)
{
    Base::finishCreation(vm, vm.propertyNames->Error.string(), NameVisibility::Visible, NameAdditionMode::WithoutStructureTransition);
    // ECMA 15.11.3.1 Error.prototype
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, errorPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->stackTraceLimit, jsNumber(globalObject(vm)->stackTraceLimit().valueOr(Options::defaultErrorStackTraceLimit())), static_cast<unsigned>(PropertyAttribute::None));
}

// ECMA 15.9.3

EncodedJSValue JSC_HOST_CALL constructErrorConstructor(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue message = exec->argument(0);
    Structure* errorStructure = InternalFunction::createSubclassStructure(exec, exec->newTarget(), jsCast<InternalFunction*>(exec->jsCallee())->globalObject(vm)->errorStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(ErrorInstance::create(exec, errorStructure, message, nullptr, TypeNothing, false)));
}

EncodedJSValue JSC_HOST_CALL callErrorConstructor(ExecState* exec)
{
    JSValue message = exec->argument(0);
    Structure* errorStructure = jsCast<InternalFunction*>(exec->jsCallee())->globalObject(exec->vm())->errorStructure();
    return JSValue::encode(ErrorInstance::create(exec, errorStructure, message, nullptr, TypeNothing, false));
}

bool ErrorConstructor::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    ErrorConstructor* thisObject = jsCast<ErrorConstructor*>(cell);

    if (propertyName == vm.propertyNames->stackTraceLimit) {
        if (value.isNumber()) {
            double effectiveLimit = value.asNumber();
            effectiveLimit = std::max(0., effectiveLimit);
            effectiveLimit = std::min(effectiveLimit, static_cast<double>(std::numeric_limits<unsigned>::max()));
            thisObject->globalObject(vm)->setStackTraceLimit(static_cast<unsigned>(effectiveLimit));
        } else
            thisObject->globalObject(vm)->setStackTraceLimit(WTF::nullopt);
    }

    return Base::put(thisObject, exec, propertyName, value, slot);
}

bool ErrorConstructor::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    VM& vm = exec->vm();
    ErrorConstructor* thisObject = jsCast<ErrorConstructor*>(cell);

    if (propertyName == vm.propertyNames->stackTraceLimit)
        thisObject->globalObject(vm)->setStackTraceLimit(WTF::nullopt);

    return Base::deleteProperty(thisObject, exec, propertyName);
}

} // namespace JSC
