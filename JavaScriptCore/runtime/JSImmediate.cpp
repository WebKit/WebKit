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

#include "BooleanConstructor.h"
#include "BooleanPrototype.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSGlobalObject.h"
#include "JSNotAnObject.h"
#include "NumberConstructor.h"
#include "NumberPrototype.h"

namespace JSC {

JSObject* JSImmediate::toThisObject(JSValue v, ExecState* exec)
{
    ASSERT(isImmediate(v));
    if (isNumber(v))
        return constructNumber(exec, v);
    if (isBoolean(v))
        return constructBooleanFromImmediateBoolean(exec, v);
    ASSERT(v.isUndefinedOrNull());
    return exec->globalThisValue();
}

JSObject* JSImmediate::toObject(JSValue v, ExecState* exec)
{
    ASSERT(isImmediate(v));
    if (isNumber(v))
        return constructNumber(exec, v);
    if (isBoolean(v))
        return constructBooleanFromImmediateBoolean(exec, v);
    
    JSNotAnObjectErrorStub* exception = createNotAnObjectErrorStub(exec, v.isNull());
    exec->setException(exception);
    return new (exec) JSNotAnObject(exec, exception);
}

JSObject* JSImmediate::prototype(JSValue v, ExecState* exec)
{
    ASSERT(isImmediate(v));
    if (isNumber(v))
        return exec->lexicalGlobalObject()->numberPrototype();
    if (isBoolean(v))
        return exec->lexicalGlobalObject()->booleanPrototype();

    JSNotAnObjectErrorStub* exception = createNotAnObjectErrorStub(exec, v.isNull());
    exec->setException(exception);
    return new (exec) JSNotAnObject(exec, exception);
}

UString JSImmediate::toString(JSValue v)
{
    ASSERT(isImmediate(v));
    if (isIntegerNumber(v))
        return UString::from(getTruncatedInt32(v));
#if USE(ALTERNATE_JSIMMEDIATE)
    if (isNumber(v)) {
        ASSERT(isDoubleNumber(v));
        double value = doubleValue(v);
        if (value == 0.0) // +0.0 or -0.0
            return "0";
        return UString::from(value);
    }
#else
        ASSERT(!isNumber(v));
#endif
    if (jsBoolean(false) == v)
        return "false";
    if (jsBoolean(true) == v)
        return "true";
    if (v.isNull())
        return "null";
    ASSERT(v.isUndefined());
    return "undefined";
}

NEVER_INLINE double JSImmediate::nonInlineNaN()
{
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace JSC
