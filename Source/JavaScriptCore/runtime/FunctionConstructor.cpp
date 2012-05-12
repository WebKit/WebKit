/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "Debugger.h"
#include "ExceptionHelpers.h"
#include "FunctionPrototype.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "Lexer.h"
#include "Nodes.h"
#include "Parser.h"
#include "UStringBuilder.h"
#include "UStringConcatenate.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(FunctionConstructor);
ASSERT_HAS_TRIVIAL_DESTRUCTOR(FunctionConstructor);

const ClassInfo FunctionConstructor::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(FunctionConstructor) };

FunctionConstructor::FunctionConstructor(JSGlobalObject* globalObject, Structure* structure)
    : InternalFunction(globalObject, structure)
{
}

void FunctionConstructor::finishCreation(ExecState* exec, FunctionPrototype* functionPrototype)
{
    Base::finishCreation(exec->globalData(), functionPrototype->classInfo()->className);
    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().prototype, functionPrototype, DontEnum | DontDelete | ReadOnly);

    // Number of arguments for constructor
    putDirectWithoutTransition(exec->globalData(), exec->propertyNames().length, jsNumber(1), ReadOnly | DontDelete | DontEnum);
}

static EncodedJSValue JSC_HOST_CALL constructWithFunctionConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructFunction(exec, asInternalFunction(exec->callee())->globalObject(), args));
}

ConstructType FunctionConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithFunctionConstructor;
    return ConstructTypeHost;
}

static EncodedJSValue JSC_HOST_CALL callFunctionConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructFunction(exec, asInternalFunction(exec->callee())->globalObject(), args));
}

// ECMA 15.3.1 The Function Constructor Called as a Function
CallType FunctionConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callFunctionConstructor;
    return CallTypeHost;
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(ExecState* exec, JSGlobalObject* globalObject, const ArgList& args, const Identifier& functionName, const UString& sourceURL, const TextPosition& position)
{
    if (!globalObject->evalEnabled())
        return throwError(exec, createEvalError(exec, "Function constructor is disabled"));
    return constructFunctionSkippingEvalEnabledCheck(exec, globalObject, args, functionName, sourceURL, position);
}

JSObject* constructFunctionSkippingEvalEnabledCheck(ExecState* exec, JSGlobalObject* globalObject, const ArgList& args, const Identifier& functionName, const UString& sourceURL, const TextPosition& position)
{
    // Functions need to have a space following the opening { due to for web compatibility
    // see https://bugs.webkit.org/show_bug.cgi?id=24350
    // We also need \n before the closing } to handle // comments at the end of the last line
    UString program;
    if (args.isEmpty())
        program = "(function() { \n})";
    else if (args.size() == 1)
        program = makeUString("(function() { ", args.at(0).toString(exec)->value(exec), "\n})");
    else {
        UStringBuilder builder;
        builder.append("(function(");
        builder.append(args.at(0).toString(exec)->value(exec));
        for (size_t i = 1; i < args.size() - 1; i++) {
            builder.append(",");
            builder.append(args.at(i).toString(exec)->value(exec));
        }
        builder.append(") { ");
        builder.append(args.at(args.size() - 1).toString(exec)->value(exec));
        builder.append("\n})");
        program = builder.toUString();
    }

    JSGlobalData& globalData = globalObject->globalData();
    SourceCode source = makeSource(program, sourceURL, position);
    JSObject* exception = 0;
    FunctionExecutable* function = FunctionExecutable::fromGlobalCode(functionName, exec, exec->dynamicGlobalObject()->debugger(), source, &exception);
    if (!function) {
        ASSERT(exception);
        return throwError(exec, exception);
    }

    ScopeChainNode* scopeChain = ScopeChainNode::create(exec, 0, globalObject, &globalData, globalObject, exec->globalThisValue());
    return JSFunction::create(exec, function, scopeChain);
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(ExecState* exec, JSGlobalObject* globalObject, const ArgList& args)
{
    return constructFunction(exec, globalObject, args, Identifier(exec, "anonymous"), UString(), TextPosition::minimumPosition());
}

} // namespace JSC
