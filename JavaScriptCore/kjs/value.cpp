/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
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
#include "value.h"

#include "error_object.h"
#include "nodes.h"
#include <stdio.h>
#include <string.h>
#include <wtf/MathExtras.h>

namespace KJS {

static const double D16 = 65536.0;
static const double D32 = 4294967296.0;

void *JSCell::operator new(size_t size)
{
    return Collector::allocate(size);
}

bool JSCell::getUInt32(uint32_t&) const
{
    return false;
}

bool JSCell::getTruncatedInt32(int32_t&) const
{
    return false;
}

bool JSCell::getTruncatedUInt32(uint32_t&) const
{
    return false;
}

// ECMA 9.4
double JSValue::toInteger(ExecState *exec) const
{
    int32_t i;
    if (getTruncatedInt32(i))
        return i;
    double d = toNumber(exec);
    return isnan(d) ? 0.0 : trunc(d);
}

double JSValue::toIntegerPreserveNaN(ExecState *exec) const
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

bool JSCell::getNumber(double &numericValue) const
{
    if (!isNumber())
        return false;
    numericValue = static_cast<const NumberImp *>(this)->value();
    return true;
}

double JSCell::getNumber() const
{
    return isNumber() ? static_cast<const NumberImp *>(this)->value() : NaN;
}

bool JSCell::getString(UString &stringValue) const
{
    if (!isString())
        return false;
    stringValue = static_cast<const StringImp *>(this)->value();
    return true;
}

UString JSCell::getString() const
{
    return isString() ? static_cast<const StringImp *>(this)->value() : UString();
}

JSObject *JSCell::getObject()
{
    return isObject() ? static_cast<JSObject *>(this) : 0;
}

const JSObject *JSCell::getObject() const
{
    return isObject() ? static_cast<const JSObject *>(this) : 0;
}

JSCell* jsString(const char* s)
{
    return new StringImp(s ? s : "");
}

JSCell* jsString(const UString& s)
{
    return s.isNull() ? new StringImp("") : new StringImp(s);
}

JSCell* jsOwnedString(const UString& s)
{
    return s.isNull() ? new StringImp("", StringImp::HasOtherOwner) : new StringImp(s, StringImp::HasOtherOwner);
}

// This method includes a PIC branch to set up the NumberImp's vtable, so we quarantine
// it in a separate function to keep the normal case speedy.
JSValue *jsNumberCell(double d)
{
    return new NumberImp(d);
}

} // namespace KJS
