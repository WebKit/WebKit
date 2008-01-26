// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007 Apple Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "interpreter.h"

#include "ExecState.h"
#include "JSGlobalObject.h"
#include "Parser.h"
#include "SavedBuiltins.h"
#include "array_object.h"
#include "bool_object.h"
#include "collector.h"
#include "date_object.h"
#include "debugger.h"
#include "error_object.h"
#include "function_object.h"
#include "internal.h"
#include "math_object.h"
#include "nodes.h"
#include "number_object.h"
#include "object.h"
#include "object_object.h"
#include "operations.h"
#include "regexp_object.h"
#include "runtime.h"
#include "string_object.h"
#include "types.h"
#include "value.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <wtf/Assertions.h>

namespace KJS {

Completion Interpreter::checkSyntax(ExecState* exec, const UString& sourceURL, int startingLineNumber, const UString& code)
{
    return checkSyntax(exec, sourceURL, startingLineNumber, code.data(), code.size());
}

Completion Interpreter::checkSyntax(ExecState* exec, const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength)
{
    JSLock lock;

    int errLine;
    UString errMsg;
    RefPtr<ProgramNode> progNode = parser().parse<ProgramNode>(sourceURL, startingLineNumber, code, codeLength, 0, &errLine, &errMsg);
    if (!progNode)
        return Completion(Throw, Error::create(exec, SyntaxError, errMsg, errLine, 0, sourceURL));
    return Completion(Normal);
}

Completion Interpreter::evaluate(ExecState* exec, const UString& sourceURL, int startingLineNumber, const UString& code, JSValue* thisV)
{
    return evaluate(exec, sourceURL, startingLineNumber, code.data(), code.size(), thisV);
}

Completion Interpreter::evaluate(ExecState* exec, const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength, JSValue* thisV)
{
    JSLock lock;
    
    JSGlobalObject* globalObject = exec->dynamicGlobalObject();

    if (globalObject->recursion() >= 20)
        return Completion(Throw, Error::create(exec, GeneralError, "Recursion too deep"));
    
    // parse the source code
    int sourceId;
    int errLine;
    UString errMsg;
    RefPtr<ProgramNode> progNode = parser().parse<ProgramNode>(sourceURL, startingLineNumber, code, codeLength, &sourceId, &errLine, &errMsg);
    
    // notify debugger that source has been parsed
    if (globalObject->debugger()) {
        bool cont = globalObject->debugger()->sourceParsed(exec, sourceId, sourceURL, UString(code, codeLength), startingLineNumber, errLine, errMsg);
        if (!cont)
            return Completion(Break);
    }
    
    // no program node means a syntax error occurred
    if (!progNode)
        return Completion(Throw, Error::create(exec, SyntaxError, errMsg, errLine, sourceId, sourceURL));
    
    exec->clearException();
    
    globalObject->incRecursion();
    
    JSObject* thisObj = globalObject;
    
    // "this" must be an object... use same rules as Function.prototype.apply()
    if (thisV && !thisV->isUndefinedOrNull())
        thisObj = thisV->toObject(exec);
    
    Completion res;
    if (exec->hadException())
        // the thisV->toObject() conversion above might have thrown an exception - if so, propagate it
        res = Completion(Throw, exec->exception());
    else {
        // execute the code
        InterpreterExecState newExec(globalObject, thisObj, progNode.get());
        JSValue* value = progNode->execute(&newExec);
        res = Completion(newExec.completionType(), value);
    }
    
    globalObject->decRecursion();
    
    if (shouldPrintExceptions() && res.complType() == Throw) {
        JSLock lock;
        ExecState* exec = globalObject->globalExec();
        CString f = sourceURL.UTF8String();
        CString message = res.value()->toObject(exec)->toString(exec).UTF8String();
        int line = res.value()->toObject(exec)->get(exec, "line")->toUInt32(exec);
#if PLATFORM(WIN_OS)
        printf("%s line %d: %s\n", f.c_str(), line, message.c_str());
#else
        printf("[%d] %s line %d: %s\n", getpid(), f.c_str(), line, message.c_str());
#endif
    }

    return res;
}

static bool printExceptions = false;

bool Interpreter::shouldPrintExceptions()
{
  return printExceptions;
}

void Interpreter::setShouldPrintExceptions(bool print)
{
  printExceptions = print;
}

} // namespace KJS
