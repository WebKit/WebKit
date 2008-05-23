/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ExceptionHelpers.h"

#include "ExecState.h"
#include "nodes.h"
#include "object.h"

namespace KJS {

static void substitute(UString& string, const UString& substring)
{
    int position = string.find("%s");
    ASSERT(position != -1);
    UString newString = string.substr(0, position);
    newString.append(substring);
    newString.append(string.substr(position + 2));
    string = newString;
}
    
JSValue* createError(ExecState* exec, ErrorType e, const char* msg)
{
    return Error::create(exec, e, msg, -1, -1, 0);
}

JSValue* createError(ExecState* exec, ErrorType e, const char* msg, const Identifier& label)
{
    UString message = msg;
    substitute(message, label.ustring());
    return Error::create(exec, e, message, -1, -1, 0);
}

JSValue* createError(ExecState* exec, ErrorType e, const char* msg, JSValue* v, Node* expr)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    if (expr)
        substitute(message, expr->toString());
    return Error::create(exec, e, message, -1, -1, 0);
}

JSValue* createError(ExecState* exec, ErrorType e, const char* msg, JSValue* v)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    return Error::create(exec, e, message, -1, -1, 0);
}

JSValue* createStackOverflowError(ExecState* exec)
{
    return createError(exec, RangeError, "Maximum call stack size exceeded.");
}

JSValue* createUndefinedVariableError(ExecState* exec, const Identifier& ident)
{
    return createError(exec, ReferenceError, "Can't find variable: %s", ident);
}
    
JSValue* createInvalidParamError(ExecState* exec, const char* op, JSValue* v)
{
    UString message = "'%s' is not a valid argument for '%s'";
    substitute(message,  v->toString(exec));
    substitute(message, op);
    return Error::create(exec, TypeError, message, -1, -1, 0);
}

JSValue* createNotAConstructorError(ExecState* exec, JSValue* value, Node* expr)
{
    if (expr)
        return createError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", value, expr);
    return createError(exec, TypeError, "Value %s is not a constructor. Cannot be used with new.", value);
}

JSValue* createNotAFunctionError(ExecState* exec, JSValue* value, Node* expr)
{
    if (expr)
        return createError(exec, TypeError, "Value %s (result of expression %s) does not allow function calls.", value, expr);
    return createError(exec, TypeError, "Value %s does not allow function calls.", value);
}

}
