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
#include "function_object.h"

#include "JSGlobalObject.h"
#include "Parser.h"
#include "array_object.h"
#include "debugger.h"
#include "function.h"
#include "internal.h"
#include "lexer.h"
#include "nodes.h"
#include "object.h"
#include <stdio.h>
#include <string.h>
#include <wtf/Assertions.h>

namespace KJS {

// ------------------------------ FunctionPrototype -------------------------

static JSValue* functionProtoFuncToString(ExecState*, JSObject*, const List&);
static JSValue* functionProtoFuncApply(ExecState*, JSObject*, const List&);
static JSValue* functionProtoFuncCall(ExecState*, JSObject*, const List&);

FunctionPrototype::FunctionPrototype(ExecState* exec)
{
    static const Identifier* applyPropertyName = new Identifier("apply");
    static const Identifier* callPropertyName = new Identifier("call");

    putDirect(exec->propertyNames().length, jsNumber(0), DontDelete | ReadOnly | DontEnum);

    putDirectFunction(new PrototypeFunction(exec, this, 0, exec->propertyNames().toString, functionProtoFuncToString), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, this, 2, *applyPropertyName, functionProtoFuncApply), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, this, 1, *callPropertyName, functionProtoFuncCall), DontEnum);
}

// ECMA 15.3.4
JSValue* FunctionPrototype::callAsFunction(ExecState*, JSObject*, const List&)
{
    return jsUndefined();
}

// Functions

JSValue* functionProtoFuncToString(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj || !thisObj->inherits(&InternalFunctionImp::info)) {
#ifndef NDEBUG
        fprintf(stderr,"attempted toString() call on null or non-function object\n");
#endif
        return throwError(exec, TypeError);
    }

    if (thisObj->inherits(&FunctionImp::info)) {
        FunctionImp* fi = static_cast<FunctionImp*>(thisObj);
        return jsString("function " + fi->functionName().ustring() + "(" + fi->body->paramString() + ") " + fi->body->toString());
    }

    return jsString("function " + static_cast<InternalFunctionImp*>(thisObj)->functionName().ustring() + "() {\n    [native code]\n}");
}

JSValue* functionProtoFuncApply(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->implementsCall())
        return throwError(exec, TypeError);

    JSValue* thisArg = args[0];
    JSValue* argArray = args[1];

    JSObject* applyThis;
    if (thisArg->isUndefinedOrNull())
        applyThis = exec->dynamicGlobalObject();
    else
        applyThis = thisArg->toObject(exec);

    List applyArgs;
    if (!argArray->isUndefinedOrNull()) {
        if (argArray->isObject() &&
            (static_cast<JSObject*>(argArray)->inherits(&ArrayInstance::info) ||
             static_cast<JSObject*>(argArray)->inherits(&Arguments::info))) {

            JSObject* argArrayObj = static_cast<JSObject*>(argArray);
            unsigned int length = argArrayObj->get(exec, exec->propertyNames().length)->toUInt32(exec);
            for (unsigned int i = 0; i < length; i++)
                applyArgs.append(argArrayObj->get(exec, i));
        } else
            return throwError(exec, TypeError);
    }

    return thisObj->call(exec, applyThis, applyArgs);
}

JSValue* functionProtoFuncCall(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->implementsCall())
        return throwError(exec, TypeError);

    JSValue* thisArg = args[0];

    JSObject* callThis;
    if (thisArg->isUndefinedOrNull())
        callThis = exec->dynamicGlobalObject();
    else
        callThis = thisArg->toObject(exec);

    List argsTail;
    args.getSlice(1, argsTail);
    return thisObj->call(exec, callThis, argsTail);
}

// ------------------------------ FunctionObjectImp ----------------------------

FunctionObjectImp::FunctionObjectImp(ExecState* exec, FunctionPrototype* functionPrototype)
    : InternalFunctionImp(functionPrototype, functionPrototype->classInfo()->className)
{
    putDirect(exec->propertyNames().prototype, functionPrototype, DontEnum | DontDelete | ReadOnly);

    // Number of arguments for constructor
    putDirect(exec->propertyNames().length, jsNumber(1), ReadOnly | DontDelete | DontEnum);
}

bool FunctionObjectImp::implementsConstruct() const
{
    return true;
}

// ECMA 15.3.2 The Function Constructor
JSObject* FunctionObjectImp::construct(ExecState* exec, const List& args, const Identifier& functionName, const UString& sourceURL, int lineNumber)
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
    RefPtr<FunctionBodyNode> functionBody = parser().parse<FunctionBodyNode>(sourceURL, lineNumber, body.data(), body.size(), &sourceId, &errLine, &errMsg);

    // notify debugger that source has been parsed
    Debugger* dbg = exec->dynamicGlobalObject()->debugger();
    if (dbg) {
        // send empty sourceURL to indicate constructed code
        bool cont = dbg->sourceParsed(exec, sourceId, UString(), body, lineNumber, errLine, errMsg);
        if (!cont) {
            dbg->imp()->abort();
            return new JSObject();
        }
    }

    // No program node == syntax error - throw a syntax error
    if (!functionBody)
        // We can't return a Completion(Throw) here, so just set the exception
        // and return it
        return throwError(exec, SyntaxError, errMsg, errLine, sourceId, sourceURL);

    ScopeChain scopeChain;
    scopeChain.push(exec->lexicalGlobalObject());

    FunctionImp* fimp = new FunctionImp(exec, functionName, functionBody.get(), scopeChain);

    // parse parameter list. throw syntax error on illegal identifiers
    int len = p.size();
    const UChar* c = p.data();
    int i = 0, params = 0;
    UString param;
    while (i < len) {
        while (*c == ' ' && i < len)
            c++, i++;
        if (Lexer::isIdentStart(c->uc)) {  // else error
            param = UString(c, 1);
            c++, i++;
            while (i < len && (Lexer::isIdentPart(c->uc))) {
                param += UString(c, 1);
                c++, i++;
            }
            while (i < len && *c == ' ')
                c++, i++;
            if (i == len) {
                functionBody->parameters().append(Identifier(param));
                params++;
                break;
            } else if (*c == ',') {
                functionBody->parameters().append(Identifier(param));
                params++;
                c++, i++;
                continue;
            } // else error
        }
        return throwError(exec, SyntaxError, "Syntax error in parameter list");
    }
  
    List consArgs;

    JSObject* objCons = exec->lexicalGlobalObject()->objectConstructor();
    JSObject* prototype = objCons->construct(exec, exec->emptyList());
    prototype->putDirect(exec->propertyNames().constructor, fimp, DontEnum | DontDelete | ReadOnly);
    fimp->putDirect(exec->propertyNames().prototype, prototype, Internal | DontDelete);
    return fimp;
}

// ECMA 15.3.2 The Function Constructor
JSObject* FunctionObjectImp::construct(ExecState* exec, const List& args)
{
    return construct(exec, args, "anonymous", UString(), 0);
}

// ECMA 15.3.1 The Function Constructor Called as a Function
JSValue* FunctionObjectImp::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    return construct(exec, args);
}

} // namespace KJS
