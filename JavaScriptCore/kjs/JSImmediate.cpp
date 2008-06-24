/*
 *  Copyright (C) 2003-2006, 2008 Apple Inc. All rights reserved.
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
#include "BooleanObject.h"
#include "JSNotAnObject.h"
#include "NumberObject.h"
#include "JSObject.h"

namespace KJS {

JSObject* JSImmediate::toObject(const JSValue *v, ExecState *exec)
{
    ASSERT(isImmediate(v));
    if (v == jsNull())
        return new (exec) JSNotAnObject(throwError(exec, TypeError, "Null value"));
    if (v == jsUndefined())
        return new (exec) JSNotAnObject(throwError(exec, TypeError, "Undefined value"));
    if (isBoolean(v))
        return constructBooleanFromImmediateBoolean(exec, const_cast<JSValue*>(v));
    ASSERT(isNumber(v));
    return constructNumberFromImmediateNumber(exec, const_cast<JSValue*>(v));
}

UString JSImmediate::toString(const JSValue* v)
{
    ASSERT(isImmediate(v));
    if (v == jsNull())
        return "null";
    if (v == jsUndefined())
        return "undefined";
    if (v == jsBoolean(true))
        return "true";
    if (v == jsBoolean(false))
        return "false";
    ASSERT(isNumber(v));
    return UString::from(getTruncatedInt32(v));
}

} // namespace KJS
