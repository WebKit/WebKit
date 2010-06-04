/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel (eric@webkit.org)
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
#include "Error.h"

#include "ConstructData.h"
#include "ErrorConstructor.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include "JSString.h"
#include "NativeErrorConstructor.h"

namespace JSC {

const char* expressionBeginOffsetPropertyName = "expressionBeginOffset";
const char* expressionCaretOffsetPropertyName = "expressionCaretOffset";
const char* expressionEndOffsetPropertyName = "expressionEndOffset";

static JSObject* constructNativeError(ExecState* exec, const UString& message, NativeErrorConstructor* constructor, const char* name)
{
    ErrorInstance* object = new (exec) ErrorInstance(constructor->errorStructure());
    JSString* messageString = message.isEmpty() ? jsString(exec, name) : jsString(exec, message);
    object->putDirect(exec->propertyNames().message, messageString);
    return object;
}

JSObject* Error::create(ExecState* exec, ErrorType type, const UString& message, int lineNumber, intptr_t sourceID, const UString& sourceURL)
{
    JSObject* error;

    switch (type) {
        case EvalError:
            error = constructNativeError(exec, message, exec->lexicalGlobalObject()->evalErrorConstructor(), "Evaluation error");
            break;
        case RangeError:
            error = constructNativeError(exec, message, exec->lexicalGlobalObject()->rangeErrorConstructor(), "Range error");
            break;
        case ReferenceError:
            error = constructNativeError(exec, message, exec->lexicalGlobalObject()->referenceErrorConstructor(), "Reference error");
            break;
        case SyntaxError:
            error = constructNativeError(exec, message, exec->lexicalGlobalObject()->syntaxErrorConstructor(), "Syntax error");
            break;
        case TypeError:
            error = constructNativeError(exec, message, exec->lexicalGlobalObject()->typeErrorConstructor(), "Type error");
            break;
        case URIError:
            error = constructNativeError(exec, message, exec->lexicalGlobalObject()->URIErrorConstructor(), "URI error");
            break;
        default:
            JSObject* constructor = exec->lexicalGlobalObject()->errorConstructor();
            const char* name = "Error";
            MarkedArgumentBuffer args;
            if (message.isEmpty())
                args.append(jsString(exec, name));
            else
                args.append(jsString(exec, message));
            ConstructData constructData;
            ConstructType constructType = constructor->getConstructData(constructData);
            error = construct(exec, constructor, constructType, constructData, args);
    }

    if (lineNumber != -1)
        error->putWithAttributes(exec, Identifier(exec, "line"), jsNumber(exec, lineNumber), ReadOnly | DontDelete);
    if (sourceID != -1)
        error->putWithAttributes(exec, Identifier(exec, "sourceId"), jsNumber(exec, sourceID), ReadOnly | DontDelete);
    if (!sourceURL.isNull())
        error->putWithAttributes(exec, Identifier(exec, "sourceURL"), jsString(exec, sourceURL), ReadOnly | DontDelete);

    return error;
}

JSObject* Error::create(ExecState* exec, ErrorType type, const char* message)
{
    return create(exec, type, message, -1, -1, UString());
}

JSObject* throwError(ExecState* exec, JSObject* error)
{
    exec->setException(error);
    return error;
}

JSObject* throwError(ExecState* exec, ErrorType type)
{
    JSObject* error = Error::create(exec, type, UString(), -1, -1, UString());
    exec->setException(error);
    return error;
}

JSObject* throwError(ExecState* exec, ErrorType type, const UString& message)
{
    JSObject* error = Error::create(exec, type, message, -1, -1, UString());
    exec->setException(error);
    return error;
}

JSObject* throwError(ExecState* exec, ErrorType type, const char* message)
{
    JSObject* error = Error::create(exec, type, message, -1, -1, UString());
    exec->setException(error);
    return error;
}

JSObject* throwError(ExecState* exec, ErrorType type, const UString& message, int line, intptr_t sourceID, const UString& sourceURL)
{
    JSObject* error = Error::create(exec, type, message, line, sourceID, sourceURL);
    exec->setException(error);
    return error;
}

} // namespace JSC
