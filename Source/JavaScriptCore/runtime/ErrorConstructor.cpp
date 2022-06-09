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
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ErrorConstructor);

const ClassInfo ErrorConstructor::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ErrorConstructor) };

static JSC_DECLARE_HOST_FUNCTION(callErrorConstructor);
static JSC_DECLARE_HOST_FUNCTION(constructErrorConstructor);

ErrorConstructor::ErrorConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callErrorConstructor, constructErrorConstructor)
{
}

void ErrorConstructor::finishCreation(VM& vm, ErrorPrototype* errorPrototype)
{
    Base::finishCreation(vm, 1, vm.propertyNames->Error.string(), PropertyAdditionMode::WithoutStructureTransition);
    // ECMA 15.11.3.1 Error.prototype
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, errorPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->stackTraceLimit, jsNumber(globalObject()->stackTraceLimit().value_or(Options::defaultErrorStackTraceLimit())), static_cast<unsigned>(PropertyAttribute::None));
}

// ECMA 15.9.3

JSC_DEFINE_HOST_FUNCTION(constructErrorConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue message = callFrame->argument(0);
    JSValue options = callFrame->argument(1);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* errorStructure = JSC_GET_DERIVED_STRUCTURE(vm, errorStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, options, nullptr, TypeNothing, ErrorType::Error, false)));
}

JSC_DEFINE_HOST_FUNCTION(callErrorConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue message = callFrame->argument(0);
    JSValue options = callFrame->argument(1);
    Structure* errorStructure = globalObject->errorStructure();
    return JSValue::encode(ErrorInstance::create(globalObject, errorStructure, message, options, nullptr, TypeNothing, ErrorType::Error, false));
}

bool ErrorConstructor::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ErrorConstructor* thisObject = jsCast<ErrorConstructor*>(cell);

    if (propertyName == vm.propertyNames->stackTraceLimit) {
        if (value.isNumber()) {
            double effectiveLimit = value.asNumber();
            effectiveLimit = std::max(0., effectiveLimit);
            effectiveLimit = std::min(effectiveLimit, static_cast<double>(std::numeric_limits<unsigned>::max()));
            thisObject->globalObject()->setStackTraceLimit(static_cast<unsigned>(effectiveLimit));
        } else
            thisObject->globalObject()->setStackTraceLimit(std::nullopt);
    }

    return Base::put(thisObject, globalObject, propertyName, value, slot);
}

bool ErrorConstructor::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ErrorConstructor* thisObject = jsCast<ErrorConstructor*>(cell);

    if (propertyName == vm.propertyNames->stackTraceLimit)
        thisObject->globalObject()->setStackTraceLimit(std::nullopt);

    return Base::deleteProperty(thisObject, globalObject, propertyName, slot);
}

} // namespace JSC
