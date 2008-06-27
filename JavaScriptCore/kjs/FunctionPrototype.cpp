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
#include "FunctionPrototype.h"

#include "JSGlobalObject.h"
#include "Parser.h"
#include "ArrayPrototype.h"
#include "debugger.h"
#include "JSFunction.h"
#include "JSString.h"
#include "lexer.h"
#include "nodes.h"
#include "JSObject.h"
#include <stdio.h>
#include <string.h>
#include <wtf/Assertions.h>

namespace KJS {

// ------------------------------ FunctionPrototype -------------------------

static JSValue* functionProtoFuncToString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionProtoFuncApply(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionProtoFuncCall(ExecState*, JSObject*, JSValue*, const ArgList&);

FunctionPrototype::FunctionPrototype(ExecState* exec)
{
    putDirect(exec->propertyNames().length, jsNumber(exec, 0), DontDelete | ReadOnly | DontEnum);

    putDirectFunction(new (exec) PrototypeFunction(exec, this, 0, exec->propertyNames().toString, functionProtoFuncToString), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, this, 2, exec->propertyNames().apply, functionProtoFuncApply), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, this, 1, exec->propertyNames().call, functionProtoFuncCall), DontEnum);
}

static JSValue* callFunctionPrototype(ExecState*, JSObject*, JSValue*, const ArgList&)
{
    return jsUndefined();
}

// ECMA 15.3.4
CallType FunctionPrototype::getCallData(CallData& callData)
{
    callData.native.function = callFunctionPrototype;
    return CallTypeNative;
}

// Functions

JSValue* functionProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!thisValue->isObject(&InternalFunction::info))
        return throwError(exec, TypeError);

    InternalFunction* function = static_cast<InternalFunction*>(thisValue);

    if (function->inherits(&JSFunction::info)) {
        JSFunction* fi = static_cast<JSFunction*>(thisValue);
        return jsString(exec, "function " + fi->functionName().ustring() + "(" + fi->body->paramString() + ") " + fi->body->toSourceString());
    }

    return jsString(exec, "function " + function->functionName().ustring() + "() {\n    [native code]\n}");
}

JSValue* functionProtoFuncApply(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    CallData callData;
    CallType callType = thisValue->getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSValue* thisArg = args[0];
    JSValue* argArray = args[1];

    JSValue* applyThis;
    if (thisArg->isUndefinedOrNull())
        applyThis = exec->globalThisValue();
    else
        applyThis = thisArg->toObject(exec);

    ArgList applyArgs;
    if (!argArray->isUndefinedOrNull()) {
        if (argArray->isObject() &&
            (static_cast<JSObject*>(argArray)->inherits(&JSArray::info) ||
             static_cast<JSObject*>(argArray)->inherits(&Arguments::info))) {

            JSObject* argArrayObj = static_cast<JSObject*>(argArray);
            unsigned int length = argArrayObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
            for (unsigned int i = 0; i < length; i++)
                applyArgs.append(argArrayObj->get(exec, i));
        } else
            return throwError(exec, TypeError);
    }

    return call(exec, thisValue, callType, callData, applyThis, applyArgs);
}

JSValue* functionProtoFuncCall(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    CallData callData;
    CallType callType = thisValue->getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSValue* thisArg = args[0];

    JSObject* callThis;
    if (thisArg->isUndefinedOrNull())
        callThis = exec->globalThisValue();
    else
        callThis = thisArg->toObject(exec);

    ArgList argsTail;
    args.getSlice(1, argsTail);
    return call(exec, thisValue, callType, callData, callThis, argsTail);
}

// ------------------------------ FunctionConstructor ----------------------------

FunctionConstructor::FunctionConstructor(ExecState* exec, FunctionPrototype* functionPrototype)
    : InternalFunction(functionPrototype, Identifier(exec, functionPrototype->classInfo()->className))
{
    putDirect(exec->propertyNames().prototype, functionPrototype, DontEnum | DontDelete | ReadOnly);

    // Number of arguments for constructor
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontDelete | DontEnum);
}

static JSObject* constructWithFunctionConstructor(ExecState* exec, JSObject*, const ArgList& args)
{
    return constructFunction(exec, args);
}

ConstructType FunctionConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructWithFunctionConstructor;
    return ConstructTypeNative;
}

static JSValue* callFunctionConstructor(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    return constructFunction(exec, args);
}

// ECMA 15.3.1 The Function Constructor Called as a Function
CallType FunctionConstructor::getCallData(CallData& callData)
{
    callData.native.function = callFunctionConstructor;
    return CallTypeNative;
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(ExecState* exec, const ArgList& args, const Identifier& functionName, const UString& sourceURL, int lineNumber)
{
    UString p("");
    UString body;
    int argsSize = args.size();
    if (argsSize == 0)
        body = "";
    else if (argsSize == 1)
        body = args[0]->toString(exec);
    else {
        p = args[0]->toString(exec);
        for (int k = 1; k < argsSize - 1; k++)
            p += "," + args[k]->toString(exec);
        body = args[argsSize - 1]->toString(exec);
    }

    // parse the source code
    int sourceId;
    int errLine;
    UString errMsg;
    RefPtr<SourceProvider> source = UStringSourceProvider::create(body);
    RefPtr<FunctionBodyNode> functionBody = exec->parser()->parse<FunctionBodyNode>(exec, sourceURL, lineNumber, source, &sourceId, &errLine, &errMsg);

    // No program node == syntax error - throw a syntax error
    if (!functionBody)
        // We can't return a Completion(Throw) here, so just set the exception
        // and return it
        return throwError(exec, SyntaxError, errMsg, errLine, sourceId, sourceURL);
    
    functionBody->setSource(SourceRange(source, 0, source->length()));
    ScopeChain scopeChain(exec->lexicalGlobalObject(), exec->globalThisValue());

    JSFunction* function = new (exec) JSFunction(exec, functionName, functionBody.get(), scopeChain.node());

    // parse parameter list. throw syntax error on illegal identifiers
    int len = p.size();
    const UChar* c = p.data();
    int i = 0, params = 0;
    UString param;
    while (i < len) {
        while (*c == ' ' && i < len)
            c++, i++;
        if (Lexer::isIdentStart(c[0])) {  // else error
            param = UString(c, 1);
            c++, i++;
            while (i < len && (Lexer::isIdentPart(c[0]))) {
                param.append(*c);
                c++, i++;
            }
            while (i < len && *c == ' ')
                c++, i++;
            if (i == len) {
                functionBody->parameters().append(Identifier(exec, param));
                params++;
                break;
            } else if (*c == ',') {
                functionBody->parameters().append(Identifier(exec, param));
                params++;
                c++, i++;
                continue;
            } // else error
        }
        return throwError(exec, SyntaxError, "Syntax error in parameter list");
    }
  
    JSObject* prototype = constructEmptyObject(exec);
    prototype->putDirect(exec->propertyNames().constructor, function, DontEnum);
    function->putDirect(exec->propertyNames().prototype, prototype, DontDelete);
    return function;
}

// ECMA 15.3.2 The Function Constructor
JSObject* constructFunction(ExecState* exec, const ArgList& args)
{
    return constructFunction(exec, args, Identifier(exec, "anonymous"), UString(), 1);
}

} // namespace KJS
