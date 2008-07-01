/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSValue.h"

#include "JSFunction.h"
#include <wtf/MathExtras.h>

namespace KJS {

static const double D16 = 65536.0; // FIXME: This seems to be unused.
static const double D32 = 4294967296.0;

// ECMA 9.4
double JSValue::toInteger(ExecState* exec) const
{
    int32_t i;
    if (getTruncatedInt32(i))
        return i;
    double d = toNumber(exec);
    return isnan(d) ? 0.0 : trunc(d);
}

double JSValue::toIntegerPreserveNaN(ExecState* exec) const
{
    int32_t i;
    if (getTruncatedInt32(i))
        return i;
    return trunc(toNumber(exec));
}

int32_t JSValue::toInt32SlowCase(double d, bool& ok)
{
    ok = true;

    if (d >= -D32 / 2 && d < D32 / 2)
        return static_cast<int32_t>(d);

    if (isnan(d) || isinf(d)) {
        ok = false;
        return 0;
    }

    double d32 = fmod(trunc(d), D32);
    if (d32 >= D32 / 2)
        d32 -= D32;
    else if (d32 < -D32 / 2)
        d32 += D32;
    return static_cast<int32_t>(d32);
}

int32_t JSValue::toInt32SlowCase(ExecState* exec, bool& ok) const
{
    return JSValue::toInt32SlowCase(toNumber(exec), ok);
}

uint32_t JSValue::toUInt32SlowCase(double d, bool& ok)
{
    ok = true;

    if (d >= 0.0 && d < D32)
        return static_cast<uint32_t>(d);

    if (isnan(d) || isinf(d)) {
        ok = false;
        return 0;
    }

    double d32 = fmod(trunc(d), D32);
    if (d32 < 0)
        d32 += D32;
    return static_cast<uint32_t>(d32);
}

uint32_t JSValue::toUInt32SlowCase(ExecState* exec, bool& ok) const
{
    return JSValue::toUInt32SlowCase(toNumber(exec), ok);
}

float JSValue::toFloat(ExecState* exec) const
{
    return static_cast<float>(toNumber(exec));
}

// Declared in CallData.h
JSValue* call(ExecState* exec, JSValue* functionObject, CallType callType, const CallData& callData, JSValue* thisValue, const ArgList& args)
{
    if (callType == CallTypeNative)
        return callData.native.function(exec, static_cast<JSObject*>(functionObject), thisValue, args);
    ASSERT(callType == CallTypeJS);
    // FIXME: Can this be done more efficiently using the callData?
    return static_cast<JSFunction*>(functionObject)->call(exec, thisValue, args);
}

// Declared in ConstructData.h
JSObject* construct(ExecState* exec, JSValue* object, ConstructType constructType, const ConstructData& constructData, const ArgList& args)
{
    if (constructType == ConstructTypeNative)
        return constructData.native.function(exec, static_cast<JSObject*>(object), args);
    ASSERT(constructType == ConstructTypeJS);
    // FIXME: Can this be done more efficiently using the constructData?
    return static_cast<JSFunction*>(object)->construct(exec, args);
}

} // namespace KJS
