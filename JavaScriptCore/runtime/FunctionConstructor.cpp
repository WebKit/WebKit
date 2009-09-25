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

#include "FunctionPrototype.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "Parser.h"
#include "Debugger.h"
#include "Lexer.h"
#include "Nodes.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(FunctionConstructor);

FunctionConstructor::FunctionConstructor(ExecState* exec, PassRefPtr<Structure> structure, FunctionPrototype* functionPrototype)
    : InternalFunction(&exec->globalData(), structure, Identifier(exec, functionPrototype->classInfo()->className))
{
    putDirectWithoutTransition(exec->propertyNames().prototype, functionPrototype, DontEnum | DontDelete | ReadOnly);

    // Number of arguments for constructor
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontDelete | DontEnum);
}

static JSObject* constructWithFunctionConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructFunction(exec, args);
}

ConstructType FunctionConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithFunctionConstructor;
    return ConstructTypeHost;
}

static JSValue JSC_HOST_CALL callFunctionConstructor(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    return constructFunction(exec, args);
}

// ECMA 15.3.1 The Function Constructor Called as a Function
CallType FunctionConstructor::getCallData(CallData& callData)
{
    callData.native.function = callFunctionConstructor;
    return CallTypeHost;
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(ExecState* exec, const ArgList& args, const Identifier& functionName, const UString& sourceURL, int lineNumber)
{
    // Functions need to have a space following the opening { due to for web compatibility
    // see https://bugs.webkit.org/show_bug.cgi?id=24350
    // We also need \n before the closing } to handle // comments at the end of the last line
    UString program;
    if (args.isEmpty())
        program = "(function() { \n})";
    else if (args.size() == 1)
        program = "(function() { " + args.at(0).toString(exec) + "\n})";
    else {
        program = "(function(" + args.at(0).toString(exec);
        for (size_t i = 1; i < args.size() - 1; i++)
            program += "," + args.at(i).toString(exec);
        program += ") { " + args.at(args.size() - 1).toString(exec) + "\n})";
    }

    int errLine;
    UString errMsg;
    SourceCode source = makeSource(program, sourceURL, lineNumber);
    RefPtr<FunctionExecutable> function = FunctionExecutable::fromGlobalCode(functionName, exec, exec->dynamicGlobalObject()->debugger(), source, &errLine, &errMsg);
    if (!function)
        return throwError(exec, SyntaxError, errMsg, errLine, source.provider()->asID(), source.provider()->url());

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    ScopeChain scopeChain(globalObject, globalObject->globalData(), globalObject, exec->globalThisValue());
    return new (exec) JSFunction(exec, function, scopeChain.node());
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(ExecState* exec, const ArgList& args)
{
    return constructFunction(exec, args, Identifier(exec, "anonymous"), UString(), 1);
}

} // namespace JSC
