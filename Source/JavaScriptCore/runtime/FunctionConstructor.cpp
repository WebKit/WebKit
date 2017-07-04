/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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
#include "FunctionConstructor.h"

#include "Completion.h"
#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "JSAsyncFunction.h"
#include "JSFunction.h"
#include "JSGeneratorFunction.h"
#include "JSGlobalObject.h"
#include "JSCInlines.h"
#include <wtf/text/StringBuilder.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(FunctionConstructor);

const ClassInfo FunctionConstructor::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionConstructor) };

FunctionConstructor::FunctionConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void FunctionConstructor::finishCreation(VM& vm, FunctionPrototype* functionPrototype)
{
    Base::finishCreation(vm, functionPrototype->classInfo()->className);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, functionPrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum);
}

static EncodedJSValue JSC_HOST_CALL constructWithFunctionConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructFunction(exec, asInternalFunction(exec->jsCallee())->globalObject(), args, FunctionConstructionMode::Function, exec->newTarget()));
}

ConstructType FunctionConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithFunctionConstructor;
    return ConstructType::Host;
}

static EncodedJSValue JSC_HOST_CALL callFunctionConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructFunction(exec, asInternalFunction(exec->jsCallee())->globalObject(), args));
}

// ECMA 15.3.1 The Function Constructor Called as a Function
CallType FunctionConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callFunctionConstructor;
    return CallType::Host;
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(ExecState* exec, JSGlobalObject* globalObject, const ArgList& args, const Identifier& functionName, const SourceOrigin& sourceOrigin, const String& sourceURL, const TextPosition& position, FunctionConstructionMode functionConstructionMode, JSValue newTarget)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!globalObject->evalEnabled())
        return throwException(exec, scope, createEvalError(exec, globalObject->evalDisabledErrorMessage()));
    scope.release();
    return constructFunctionSkippingEvalEnabledCheck(exec, globalObject, args, functionName, sourceOrigin, sourceURL, position, -1, functionConstructionMode, newTarget);
}

JSObject* constructFunctionSkippingEvalEnabledCheck(
    ExecState* exec, JSGlobalObject* globalObject, const ArgList& args, 
    const Identifier& functionName, const SourceOrigin& sourceOrigin, const String& sourceURL, 
    const TextPosition& position, int overrideLineNumber, FunctionConstructionMode functionConstructionMode, JSValue newTarget)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const char* prefix = nullptr;
    Structure* structure = nullptr;
    switch (functionConstructionMode) {
    case FunctionConstructionMode::Function:
        structure = globalObject->functionStructure();
        prefix = "function ";
        break;
    case FunctionConstructionMode::Generator:
        structure = globalObject->generatorFunctionStructure();
        prefix = "function *";
        break;
    case FunctionConstructionMode::Async:
        structure = globalObject->asyncFunctionStructure();
        prefix = "async function ";
        break;
    }

    auto checkBody = [&] (const String& body) {
        // The spec mandates that the body parses a valid function body independent
        // of the parameters.
        String program = makeString("(", prefix, "(){\n", body, "\n})");
        SourceCode source = makeSource(program, sourceOrigin, sourceURL, position);
        JSValue exception;
        checkSyntax(exec, source, &exception);
        if (exception) {
            scope.throwException(exec, exception);
            return;
        }
    };

    // How we stringify functions is sometimes important for web compatibility.
    // See https://bugs.webkit.org/show_bug.cgi?id=24350.
    String program;
    if (args.isEmpty())
        program = makeString("{", prefix, functionName.string(), "() {\n\n}}");
    else if (args.size() == 1) {
        auto body = args.at(0).toWTFString(exec);
        RETURN_IF_EXCEPTION(scope, nullptr);
        checkBody(body);
        RETURN_IF_EXCEPTION(scope, nullptr);
        program = makeString("{", prefix, functionName.string(), "() {\n", body, "\n}}");
    } else {
        StringBuilder builder;
        builder.append('{');
        builder.append(prefix);
        builder.append(functionName.string());
        builder.append('(');
        StringBuilder parameterBuilder;
        auto viewWithString = args.at(0).toString(exec)->viewWithUnderlyingString(exec);
        RETURN_IF_EXCEPTION(scope, nullptr);
        parameterBuilder.append(viewWithString.view);
        for (size_t i = 1; i < args.size() - 1; i++) {
            parameterBuilder.appendLiteral(", ");
            auto viewWithString = args.at(i).toString(exec)->viewWithUnderlyingString(exec);
            RETURN_IF_EXCEPTION(scope, nullptr);
            parameterBuilder.append(viewWithString.view);
        }

        {
            // The spec mandates that the parameters parse as a valid parameter list
            // independent of the function body.
            String program = makeString("(", prefix, "(", parameterBuilder.toString(), "){\n\n})");
            SourceCode source = makeSource(program, sourceOrigin, sourceURL, position);
            JSValue exception;
            checkSyntax(exec, source, &exception);
            if (exception) {
                scope.throwException(exec, exception);
                return nullptr;
            }
        }

        builder.append(parameterBuilder);
        builder.appendLiteral(") {\n");
        auto body = args.at(args.size() - 1).toWTFString(exec);
        RETURN_IF_EXCEPTION(scope, nullptr);
        checkBody(body);
        RETURN_IF_EXCEPTION(scope, nullptr);
        builder.append(body);
        builder.appendLiteral("\n}}");
        program = builder.toString();
    }

    SourceCode source = makeSource(program, sourceOrigin, sourceURL, position);
    JSObject* exception = nullptr;
    FunctionExecutable* function = FunctionExecutable::fromGlobalCode(functionName, *exec, source, exception, overrideLineNumber);
    if (!function) {
        ASSERT(exception);
        return throwException(exec, scope, exception);
    }

    Structure* subclassStructure = InternalFunction::createSubclassStructure(exec, newTarget, structure);
    RETURN_IF_EXCEPTION(scope, nullptr);

    switch (functionConstructionMode) {
    case FunctionConstructionMode::Function:
        return JSFunction::create(vm, function, globalObject->globalScope(), subclassStructure);
    case FunctionConstructionMode::Generator:
        return JSGeneratorFunction::create(vm, function, globalObject->globalScope(), subclassStructure);
    case FunctionConstructionMode::Async:
        return JSAsyncFunction::create(vm, function, globalObject->globalScope(), subclassStructure);
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(ExecState* exec, JSGlobalObject* globalObject, const ArgList& args, FunctionConstructionMode functionConstructionMode, JSValue newTarget)
{
    return constructFunction(exec, globalObject, args, exec->propertyNames().anonymous, exec->callerSourceOrigin(), String(), TextPosition(), functionConstructionMode, newTarget);
}

} // namespace JSC
