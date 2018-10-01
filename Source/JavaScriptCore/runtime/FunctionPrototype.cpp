/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2009, 2015-2016 Apple Inc. All rights reserved.
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

#include "BuiltinExecutables.h"
#include "BuiltinNames.h"
#include "Error.h"
#include "GetterSetter.h"
#include "JSAsyncFunction.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSStringInlines.h"
#include "Lexer.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(FunctionPrototype);

const ClassInfo FunctionPrototype::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionPrototype) };

static EncodedJSValue JSC_HOST_CALL functionProtoFuncToString(ExecState*);

// ECMA 15.3.4
static EncodedJSValue JSC_HOST_CALL callFunctionPrototype(ExecState*)
{
    return JSValue::encode(jsUndefined());
}

FunctionPrototype::FunctionPrototype(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callFunctionPrototype, nullptr)
{
}

void FunctionPrototype::finishCreation(VM& vm, const String& name)
{
    Base::finishCreation(vm, name);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

void FunctionPrototype::addFunctionProperties(ExecState* exec, JSGlobalObject* globalObject, JSFunction** callFunction, JSFunction** applyFunction, JSFunction** hasInstanceSymbolFunction)
{
    VM& vm = exec->vm();

    JSFunction* toStringFunction = JSFunction::create(vm, globalObject, 0, vm.propertyNames->toString.string(), functionProtoFuncToString);
    putDirectWithoutTransition(vm, vm.propertyNames->toString, toStringFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));

    *applyFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().applyPublicName(), functionPrototypeApplyCodeGenerator(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));
    *callFunction = putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->builtinNames().callPublicName(), functionPrototypeCallCodeGenerator(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectBuiltinFunctionWithoutTransition(vm, globalObject, vm.propertyNames->bind, functionPrototypeBindCodeGenerator(vm), static_cast<unsigned>(PropertyAttribute::DontEnum));

    *hasInstanceSymbolFunction = JSFunction::create(vm, functionPrototypeSymbolHasInstanceCodeGenerator(vm), globalObject);
    putDirectWithoutTransition(vm, vm.propertyNames->hasInstanceSymbol, *hasInstanceSymbolFunction, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}
    
void FunctionPrototype::initRestrictedProperties(ExecState* exec, JSGlobalObject* globalObject)
{
    VM& vm = exec->vm();
    GetterSetter* errorGetterSetter = globalObject->throwTypeErrorArgumentsCalleeAndCallerGetterSetter();
    putDirectAccessor(exec, vm.propertyNames->caller, errorGetterSetter, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    putDirectAccessor(exec, vm.propertyNames->arguments, errorGetterSetter, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
}

EncodedJSValue JSC_HOST_CALL functionProtoFuncToString(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    if (thisValue.inherits<JSFunction>(vm)) {
        JSFunction* function = jsCast<JSFunction*>(thisValue);
        if (function->isHostOrBuiltinFunction())
            RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(exec, "function ", function->name(vm), "() {\n    [native code]\n}")));

        FunctionExecutable* executable = function->jsExecutable();
        if (executable->isClass()) {
            StringView classSource = executable->classSource().view();
            return JSValue::encode(jsString(exec, classSource.toStringWithoutCopying()));
        }

        String functionHeader;
        switch (executable->parseMode()) {
        case SourceParseMode::GeneratorWrapperFunctionMode:
        case SourceParseMode::GeneratorWrapperMethodMode:
            functionHeader = "function* ";
            break;

        case SourceParseMode::NormalFunctionMode:
        case SourceParseMode::GetterMode:
        case SourceParseMode::SetterMode:
        case SourceParseMode::MethodMode:
        case SourceParseMode::ProgramMode:
        case SourceParseMode::ModuleAnalyzeMode:
        case SourceParseMode::ModuleEvaluateMode:
        case SourceParseMode::GeneratorBodyMode:
        case SourceParseMode::AsyncGeneratorBodyMode:
        case SourceParseMode::AsyncFunctionBodyMode:
        case SourceParseMode::AsyncArrowFunctionBodyMode:
            functionHeader = "function ";
            break;

        case SourceParseMode::ArrowFunctionMode:
            functionHeader = "";
            break;

        case SourceParseMode::AsyncFunctionMode:
        case SourceParseMode::AsyncMethodMode:
            functionHeader = "async function ";
            break;

        case SourceParseMode::AsyncArrowFunctionMode:
            functionHeader = "async ";
            break;

        case SourceParseMode::AsyncGeneratorWrapperFunctionMode:
        case SourceParseMode::AsyncGeneratorWrapperMethodMode:
            functionHeader = "async function* ";
            break;
        }

        StringView source = executable->source().provider()->getRange(
            executable->parametersStartOffset(),
            executable->parametersStartOffset() + executable->source().length());
        RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(exec, functionHeader, function->name(vm), source)));
    }

    if (thisValue.inherits<InternalFunction>(vm)) {
        InternalFunction* function = jsCast<InternalFunction*>(thisValue);
        RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(exec, "function ", function->name(), "() {\n    [native code]\n}")));
    }

    if (thisValue.isObject()) {
        JSObject* object = asObject(thisValue);
        if (object->isFunction(vm))
            RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(exec, "function ", object->classInfo(vm)->className, "() {\n    [native code]\n}")));
    }

    return throwVMTypeError(exec, scope);
}

} // namespace JSC
