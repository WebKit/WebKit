/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003-2006 Apple Computer, Inc
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
#include "JSImmediate.h"

#include "JSGlobalObject.h"
#include "bool_object.h"
#include "number_object.h"
#include "object.h"

namespace KJS {

JSObject *JSImmediate::toObject(const JSValue *v, ExecState *exec)
{
    ASSERT(isImmediate(v));
    if (v == jsNull())
        return throwError(exec, TypeError, "Null value");
    else if (v == jsUndefined())
        return throwError(exec, TypeError, "Undefined value");
    else if (isBoolean(v)) {
        List args;
        args.append(const_cast<JSValue *>(v));
        return exec->lexicalGlobalObject()->booleanConstructor()->construct(exec, args);
    } else {
        ASSERT(isNumber(v));
        List args;
        args.append(const_cast<JSValue *>(v));
        return exec->lexicalGlobalObject()->numberConstructor()->construct(exec, args);
    }
}

UString JSImmediate::toString(const JSValue *v)
{
    ASSERT(isImmediate(v));
    
    if (v == jsNull())
        return "null";
    else if (v == jsUndefined())
        return "undefined";
    else if (v == jsBoolean(true))
        return "true";
    else if (v == jsBoolean(false))
        return "false";
    else {
        ASSERT(isNumber(v));
        double d = toDouble(v);
        if (d == 0.0) // +0.0 or -0.0
            return "0";
        return UString::from(d);
    }
}

JSType JSImmediate::type(const JSValue *v)
{
    ASSERT(isImmediate(v));
    
    uintptr_t tag = getTag(v);
    if (tag == UndefinedType)
        return v == jsUndefined() ? UndefinedType : NullType;
    return static_cast<JSType>(tag);
}

} // namespace KJS
