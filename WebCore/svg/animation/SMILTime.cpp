/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#if ENABLE(SVG)
#include "SMILTime.h"

#include <float.h>

using namespace WebCore;

const double SMILTime::unresolvedValue = DBL_MAX;
// Just a big value smaller than DBL_MAX. Our times are relative to 0, we don't really need the full range.
const double SMILTime::indefiniteValue = FLT_MAX;    

SMILTime WebCore::operator+(const SMILTime& a, const SMILTime& b)
{
    if (a.isUnresolved() || b.isUnresolved())
        return SMILTime::unresolved();
    if (a.isIndefinite() || b.isIndefinite())
        return SMILTime::indefinite();
    return a.value() + b.value();
}

SMILTime WebCore::operator-(const SMILTime& a, const SMILTime& b)
{
    if (a.isUnresolved() || b.isUnresolved())
        return SMILTime::unresolved();
    if (a.isIndefinite() || b.isIndefinite())
        return SMILTime::indefinite();
    return a.value() - b.value();
}

SMILTime WebCore::operator*(const SMILTime& a,  const SMILTime& b)
{
    if (a.isUnresolved() || b.isUnresolved())
        return SMILTime::unresolved();
    if (a.value() == 0 || b.value() == 0)
        return SMILTime(0);
    if (a.isIndefinite() || b.isIndefinite())
        return SMILTime::indefinite();
    return a.value() * b.value();
}
#endif

