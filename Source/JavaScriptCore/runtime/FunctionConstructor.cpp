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
#include "JSStringInlines.h"
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

ASCIILiteral functionConstructorPrefix(FunctionConstructionMode functionConstructionMode)
{
    switch (functionConstructionMode) {
    case FunctionConstructionMode::Function:
        return "function "_s;
    case FunctionConstructionMode::Generator:
        return "function* "_s;
    case FunctionConstructionMode::Async:
        return "async function "_s;
    case FunctionConstructionMode::AsyncGenerator:
        return "async function* "_s;
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral { };
}

static String stringifyFunction(JSGlobalObject* globalObject, const ArgList& args, const Identifier& functionName, FunctionConstructionMode functionConstructionMode, ThrowScope& scope, std::optional<int>& functionConstructorParametersEndPosition)
{
    ASCIILiteral prefix = functionConstructorPrefix(functionConstructionMode);

    // How we stringify functions is sometimes important for web compatibility.
    // See https://bugs.webkit.org/show_bug.cgi?id=24350.
    String program;
    functionConstructorParametersEndPosition = std::nullopt;
    if (args.isEmpty())
        program = makeString(prefix, functionName.string(), "(\n) {\n\n}"_s);
    else if (args.size() == 1) {
        JSValue arg0 = args.at(0);
        if (arg0.isString()) [[likely]]
            program = tryMakeString(prefix, functionName.string(), "(\n) {\n"_s, asString(arg0), "\n}"_s);
        else {
            auto body = arg0.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            program = tryMakeString(prefix, functionName.string(), "(\n) {\n"_s, body, "\n}"_s);
        }

        if (!program) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
    } else if (args.size() == 2) {
        // This is really common since it means (1) arguments + (2) body.
        JSValue arg0 = args.at(0);
        JSValue arg1 = args.at(1);
        size_t arg0Length = 0;
        if (arg0.isString() && arg1.isString()) [[likely]] {
            arg0Length = asString(arg0)->length();
            program = tryMakeString(prefix, functionName.string(), "("_s, asString(arg0), "\n) {\n"_s, asString(arg1), "\n}"_s);
        } else {
            auto arg = arg0.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            auto body = arg1.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            arg0Length = arg.length();
            program = tryMakeString(prefix, functionName.string(), "("_s, arg, "\n) {\n"_s, body, "\n}"_s);
        }

        if (!program) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
        functionConstructorParametersEndPosition = prefix.length() + functionName.string().length() + "("_s.length() + arg0Length + "\n)"_s.length();
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
        if (builder.hasOverflowed()) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }

        functionConstructorParametersEndPosition = builder.length() + "\n)"_s.length();

        auto* bodyString = args.at(args.size() - 1).toString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto body = bodyString->view(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        builder.append("\n) {\n"_s, body.data, "\n}"_s);
        if (builder.hasOverflowed()) [[unlikely]] {
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

    if (globalObject->trustedTypesEnforcement() != TrustedTypesEnforcement::None) [[unlikely]] {
        bool isTrusted = true;
        auto* structure = globalObject->trustedScriptStructure();
        for (size_t i = 0; i < args.size(); i++) {
            auto arg = args.at(i);

            if (!arg.isObject() || structure != asObject(arg)->structure()) {
                isTrusted = false;
                break;
            }
        }

        if (!isTrusted) {
            bool canCompileStrings = globalObject->globalObjectMethodTable()->canCompileStrings(globalObject, CompilationType::Function, code, args);
            RETURN_IF_EXCEPTION(scope, { });
            if (!canCompileStrings) {
                throwException(globalObject, scope, createEvalError(globalObject, "Refused to evaluate a string as JavaScript because this document requires a 'Trusted Type' assignment."_s));
                return nullptr;
            }
        }
    }

    if (!globalObject->evalEnabled()) [[unlikely]] {
        if (globalObject->trustedTypesEnforcement() != TrustedTypesEnforcement::EnforcedWithEvalEnabled) {
            scope.clearException();
            globalObject->globalObjectMethodTable()->reportViolationForUnsafeEval(globalObject, !code.isNull() ? WTFMove(code) : nullString());
            throwException(globalObject, scope, createEvalError(globalObject, globalObject->evalDisabledErrorMessage()));
            return nullptr;
        }
    }
    if (code.isNull()) [[unlikely]]
        return nullptr;

    LexicallyScopedFeatures lexicallyScopedFeatures = globalObject->globalScopeExtension() ? TaintedByWithScopeLexicallyScopedFeature : NoLexicallyScopedFeatures;
    RELEASE_AND_RETURN(scope, constructFunctionSkippingEvalEnabledCheck(globalObject, WTFMove(code), lexicallyScopedFeatures, functionName, sourceOrigin, sourceURL, taintedOrigin, position, -1, functionConstructorParametersEndPosition, functionConstructionMode, newTarget));
}

JSObject* constructFunctionSkippingEvalEnabledCheck(JSGlobalObject* globalObject, String&& program, LexicallyScopedFeatures lexicallyScopedFeatures, const Identifier& functionName, const SourceOrigin& sourceOrigin, const String& sourceURL, SourceTaintedOrigin taintedOrigin, const TextPosition& position, int overrideLineNumber, std::optional<int> functionConstructorParametersEndPosition, FunctionConstructionMode functionConstructionMode, JSValue newTarget)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* exception = nullptr;
    FunctionExecutable* function = FunctionExecutable::fromGlobalCode(functionName, globalObject, WTFMove(program), sourceOrigin, taintedOrigin, sourceURL, position, lexicallyScopedFeatures, exception, overrideLineNumber, functionConstructorParametersEndPosition, functionConstructionMode);
    if (!function) [[unlikely]] {
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
