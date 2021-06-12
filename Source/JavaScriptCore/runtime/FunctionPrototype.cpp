/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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
#include "FunctionPrototype.h"

#include "BuiltinNames.h"
#include "FunctionExecutable.h"
#include "IntegrityInlines.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(FunctionPrototype);

const ClassInfo FunctionPrototype::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionPrototype) };

static JSC_DECLARE_HOST_FUNCTION(functionProtoFuncToString);
static JSC_DECLARE_HOST_FUNCTION(callFunctionPrototype);

// https://tc39.es/ecma262/#sec-properties-of-the-function-prototype-object
JSC_DEFINE_HOST_FUNCTION(callFunctionPrototype, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsUndefined());
}

FunctionPrototype::FunctionPrototype(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callFunctionPrototype, nullptr)
{
}

void FunctionPrototype::finishCreation(VM& vm, const String& name)
{
    Base::finishCreation(vm, 0, name, PropertyAdditionMode::WithoutStructureTransition);
    ASSERT(inherits(vm, info()));
}

void FunctionPrototype::addFunctionProperties(VM& vm, JSGlobalObject* globalObject, JSFunction** callFunction, JSFunction** applyFunction, JSFunction** hasInstanceSymbolFunction)
{
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().toStringPublicName(), functionProtoFuncToString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, FunctionToStringIntrinsic);

    *applyFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().applyPublicName(), functionPrototypeApplyCodeGenerator(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));
    *callFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().callPublicName(), functionPrototypeCallCodeGenerator(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->bind, functionPrototypeBindCodeGenerator(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));

    *hasInstanceSymbolFunction = JSFunction::create(vm, functionPrototypeSymbolHasInstanceCodeGenerator(vm), globalObject);
    putDirectWithoutTransition(vm, vm.propertyNames->hasInstanceSymbol, *hasInstanceSymbolFunction, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}
    
void FunctionPrototype::initRestrictedProperties(VM& vm, JSGlobalObject* globalObject)
{
    GetterSetter* errorGetterSetter = globalObject->throwTypeErrorArgumentsCalleeAndCallerGetterSetter();
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->caller, errorGetterSetter, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->arguments, errorGetterSetter, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
}

JSC_DEFINE_HOST_FUNCTION(functionProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (thisValue.inherits<JSFunction>(vm)) {
        JSFunction* function = jsCast<JSFunction*>(thisValue);
        Integrity::auditStructureID(vm, function->structureID());
        RELEASE_AND_RETURN(scope, JSValue::encode(function->toString(globalObject)));
    }

    if (thisValue.inherits<InternalFunction>(vm)) {
        InternalFunction* function = jsCast<InternalFunction*>(thisValue);
        Integrity::auditStructureID(vm, function->structureID());
        RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(globalObject, "function ", function->name(), "() {\n    [native code]\n}")));
    }

    if (thisValue.isObject()) {
        JSObject* object = asObject(thisValue);
        Integrity::auditStructureID(vm, object->structureID());
        if (object->isCallable(vm))
            RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(globalObject, "function ", object->classInfo(vm)->className, "() {\n    [native code]\n}")));
    }

    return throwVMTypeError(globalObject, scope);
}

} // namespace JSC
