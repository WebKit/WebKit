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
#include "value.h"

#include "error_object.h"
#include "nodes.h"
#include <stdio.h>
#include <string.h>
#include <wtf/MathExtras.h>

namespace KJS {

#if defined NAN && defined INFINITY

extern const double NaN = NAN;
extern const double Inf = INFINITY;

#else // !(defined NAN && defined INFINITY)

// The trick is to define the NaN and Inf globals with a different type than the declaration.
// This trick works because the mangled name of the globals does not include the type, although
// I'm not sure that's guaranteed. There could be alignment issues with this, since arrays of
// characters don't necessarily need the same alignment doubles do, but for now it seems to work.
// It would be good to figure out a 100% clean way that still avoids code that runs at init time.

// Note, we have to use union to ensure alignment. Otherwise, NaN_Bytes can start anywhere,
// while NaN_double has to be 4-byte aligned for 32-bits.
// With -fstrict-aliasing enabled, unions are the only safe way to do type masquerading.

static const union {
    struct {
        unsigned char NaN_Bytes[8];
        unsigned char Inf_Bytes[8];
    } bytes;
    
    struct {
        double NaN_Double;
        double Inf_Double;
    } doubles;
    
} NaNInf = { {
#if PLATFORM(BIG_ENDIAN)
    { 0x7f, 0xf8, 0, 0, 0, 0, 0, 0 },
    { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 }
#elif PLATFORM(MIDDLE_ENDIAN)
    { 0, 0, 0xf8, 0x7f, 0, 0, 0, 0 },
    { 0, 0, 0xf0, 0x7f, 0, 0, 0, 0 }
#else
    { 0, 0, 0, 0, 0, 0, 0xf8, 0x7f },
    { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f }
#endif
} } ;

extern const double NaN = NaNInf.doubles.NaN_Double;
extern const double Inf = NaNInf.doubles.Inf_Double;
 
#endif // !(defined NAN && defined INFINITY)

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
