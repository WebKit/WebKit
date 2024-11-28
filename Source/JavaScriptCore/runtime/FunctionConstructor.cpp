/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2024 Apple Inc. All rights reserved.
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

#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "GlobalObjectMethodTable.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSFunction.h"
#include "JSGeneratorFunction.h"
#include "JSGlobalObject.h"
#include "JSCInlines.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(FunctionConstructor);

const ClassInfo FunctionConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(FunctionConstructor) };

static JSC_DECLARE_HOST_FUNCTION(constructWithFunctionConstructor);
static JSC_DECLARE_HOST_FUNCTION(callFunctionConstructor);

JSC_DEFINE_HOST_FUNCTION(constructWithFunctionConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructFunction(globalObject, callFrame, args, FunctionConstructionMode::Function, callFrame->newTarget()));
}

// ECMA 15.3.1 The Function Constructor Called as a Function
JSC_DEFINE_HOST_FUNCTION(callFunctionConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ArgList args(callFrame);
    return JSValue::encode(constructFunction(globalObject, callFrame, args));
}

FunctionConstructor::FunctionConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callFunctionConstructor, constructWithFunctionConstructor)
{
}

void FunctionConstructor::finishCreation(VM& vm, FunctionPrototype* functionPrototype)
{
    Base::finishCreation(vm, 1, vm.propertyNames->Function.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, functionPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

static String stringifyFunction(JSGlobalObject* globalObject, const ArgList& args, const Identifier& functionName, FunctionConstructionMode functionConstructionMode, ThrowScope& scope, std::optional<int>& functionConstructorParametersEndPosition)
{
    ASCIILiteral prefix;
    switch (functionConstructionMode) {
    case FunctionConstructionMode::Function:
        prefix = "function "_s;
        break;
    case FunctionConstructionMode::Generator:
        prefix = "function* "_s;
        break;
    case FunctionConstructionMode::Async:
        prefix = "async function "_s;
        break;
    case FunctionConstructionMode::AsyncGenerator:
        prefix = "async function* "_s;
        break;
    }

    // How we stringify functions is sometimes important for web compatibility.
    // See https://bugs.webkit.org/show_bug.cgi?id=24350.
    String program;
    functionConstructorParametersEndPosition = std::nullopt;
    if (args.isEmpty())
        program = makeString(prefix, functionName.string(), "(\n) {\n\n}"_s);
    else if (args.size() == 1) {
        auto body = args.at(0).toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        program = tryMakeString(prefix, functionName.string(), "(\n) {\n"_s, body, "\n}"_s);
        if (UNLIKELY(!program)) {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
    } else {
        StringBuilder builder(OverflowPolicy::RecordOverflow);
        builder.append(prefix, functionName.string(), '(');

        auto* jsString = args.at(0).toString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto view = jsString->view(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        builder.append(view.data);
        for (size_t i = 1; !builder.hasOverflowed() && i < args.size() - 1; i++) {
            auto* jsString = args.at(i).toString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            auto view = jsString->view(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            builder.append(',', view.data);
        }
        if (UNLIKELY(builder.hasOverflowed())) {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }

        functionConstructorParametersEndPosition = builder.length() + sizeof("\n)") - 1;

        auto* bodyString = args.at(args.size() - 1).toString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto body = bodyString->view(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        builder.append("\n) {\n"_s, body.data, "\n}"_s);
        if (UNLIKELY(builder.hasOverflowed())) {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
        program = builder.toString();
    }

    return program;
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(JSGlobalObject* globalObject, const ArgList& args, const Identifier& functionName, const SourceOrigin& sourceOrigin, const String& sourceURL, SourceTaintedOrigin taintedOrigin, const TextPosition& position, FunctionConstructionMode functionConstructionMode, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::optional<int> functionConstructorParametersEndPosition;
    auto code = stringifyFunction(globalObject, args, functionName, functionConstructionMode, scope, functionConstructorParametersEndPosition);
    EXCEPTION_ASSERT(!!scope.exception() == code.isNull());

    if (UNLIKELY(!globalObject->evalEnabled())) {
        scope.clearException();
        globalObject->globalObjectMethodTable()->reportViolationForUnsafeEval(globalObject, !code.isNull() ? jsNontrivialString(vm, WTFMove(code)) : nullptr);
        throwException(globalObject, scope, createEvalError(globalObject, globalObject->evalDisabledErrorMessage()));
        return nullptr;
    }
    if (UNLIKELY(code.isNull()))
        return nullptr;

    LexicallyScopedFeatures lexicallyScopedFeatures = globalObject->globalScopeExtension() ? TaintedByWithScopeLexicallyScopedFeature : NoLexicallyScopedFeatures;
    RELEASE_AND_RETURN(scope, constructFunctionSkippingEvalEnabledCheck(globalObject, WTFMove(code), lexicallyScopedFeatures, functionName, sourceOrigin, sourceURL, taintedOrigin, position, -1, functionConstructorParametersEndPosition, functionConstructionMode, newTarget));
}

JSObject* constructFunctionSkippingEvalEnabledCheck(JSGlobalObject* globalObject, String&& program, LexicallyScopedFeatures lexicallyScopedFeatures, const Identifier& functionName, const SourceOrigin& sourceOrigin, const String& sourceURL, SourceTaintedOrigin taintedOrigin, const TextPosition& position, int overrideLineNumber, std::optional<int> functionConstructorParametersEndPosition, FunctionConstructionMode functionConstructionMode, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    SourceCode source = makeSource(program, sourceOrigin, taintedOrigin, sourceURL, position);
    JSObject* exception = nullptr;
    FunctionExecutable* function = FunctionExecutable::fromGlobalCode(functionName, globalObject, source, lexicallyScopedFeatures, exception, overrideLineNumber, functionConstructorParametersEndPosition);
    if (UNLIKELY(!function)) {
        ASSERT(exception);
        throwException(globalObject, scope, exception);
        return nullptr;
    }

    JSGlobalObject* structureGlobalObject = globalObject;
    bool needsSubclassStructure = newTarget && newTarget != globalObject->functionConstructor();
    if (needsSubclassStructure) {
        structureGlobalObject = getFunctionRealm(globalObject, asObject(newTarget));
        RETURN_IF_EXCEPTION(scope, nullptr);
    }
    Structure* structure = nullptr;
    switch (functionConstructionMode) {
    case FunctionConstructionMode::Function:
        structure = JSFunction::selectStructureForNewFuncExp(structureGlobalObject, function);
        break;
    case FunctionConstructionMode::Generator:
        structure = structureGlobalObject->generatorFunctionStructure();
        break;
    case FunctionConstructionMode::Async:
        structure = structureGlobalObject->asyncFunctionStructure();
        break;
    case FunctionConstructionMode::AsyncGenerator:
        structure = structureGlobalObject->asyncGeneratorFunctionStructure();
        break;
    }

    if (needsSubclassStructure) {
        structure = InternalFunction::createSubclassStructure(globalObject, asObject(newTarget), structure);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    switch (functionConstructionMode) {
    case FunctionConstructionMode::Function:
        return JSFunction::create(vm, globalObject, function, globalObject->globalScope(), structure);
    case FunctionConstructionMode::Generator:
        return JSGeneratorFunction::create(vm, globalObject, function, globalObject->globalScope(), structure);
    case FunctionConstructionMode::Async:
        return JSAsyncFunction::create(vm, globalObject, function, globalObject->globalScope(), structure);
    case FunctionConstructionMode::AsyncGenerator:
        return JSAsyncGeneratorFunction::create(vm, globalObject, function, globalObject->globalScope(), structure);
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(JSGlobalObject* globalObject, CallFrame* callFrame, const ArgList& args, FunctionConstructionMode functionConstructionMode, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    return constructFunction(globalObject, args, vm.propertyNames->anonymous, callFrame->callerSourceOrigin(vm), String(), computeNewSourceTaintedOriginFromStack(vm, callFrame), TextPosition(), functionConstructionMode, newTarget);
}

} // namespace JSC
