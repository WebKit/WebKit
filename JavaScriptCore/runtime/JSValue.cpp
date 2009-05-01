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

namespace JSC {

static const double D32 = 4294967296.0;

// ECMA 9.4
double JSValue::toInteger(ExecState* exec) const
{
    if (isInt32Fast())
        return getInt32Fast();
    double d = toNumber(exec);
    return isnan(d) ? 0.0 : trunc(d);
}

double JSValue::toIntegerPreserveNaN(ExecState* exec) const
{
    if (isInt32Fast())
        return getInt32Fast();
    return trunc(toNumber(exec));
}

int32_t toInt32SlowCase(double d, bool& ok)
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

uint32_t toUInt32SlowCase(double d, bool& ok)
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

} // namespace JSC
