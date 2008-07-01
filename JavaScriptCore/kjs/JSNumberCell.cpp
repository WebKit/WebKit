/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSNumberCell.h"

#include "JSType.h"
#include "NumberObject.h"
#include "ustring.h"

namespace KJS {

JSType JSNumberCell::type() const
{
    return NumberType;
}

JSValue* JSNumberCell::toPrimitive(ExecState*, JSType) const
{
    return const_cast<JSNumberCell*>(this);
}

bool JSNumberCell::getPrimitiveNumber(ExecState*, double& number, JSValue*& value)
{
    number = val;
    value = this;
    return true;
}

bool JSNumberCell::toBoolean(ExecState *) const
{
  return val < 0.0 || val > 0.0; // false for NaN
}

double JSNumberCell::toNumber(ExecState *) const
{
  return val;
}

UString JSNumberCell::toString(ExecState*) const
{
    if (val == 0.0) // +0.0 or -0.0
        return "0";
    return UString::from(val);
}

UString JSNumberCell::toThisString(ExecState*) const
{
    if (val == 0.0) // +0.0 or -0.0
        return "0";
    return UString::from(val);
}

JSObject* JSNumberCell::toObject(ExecState* exec) const
{
    return constructNumber(exec, const_cast<JSNumberCell*>(this));
}

JSObject* JSNumberCell::toThisObject(ExecState* exec) const
{
    return constructNumber(exec, const_cast<JSNumberCell*>(this));
}

bool JSNumberCell::getUInt32(uint32_t& uint32) const
{
    uint32 = static_cast<uint32_t>(val);
    return uint32 == val;
}

bool JSNumberCell::getTruncatedInt32(int32_t& int32) const
{
    if (!(val >= -2147483648.0 && val < 2147483648.0))
        return false;
    int32 = static_cast<int32_t>(val);
    return true;
}

bool JSNumberCell::getTruncatedUInt32(uint32_t& uint32) const
{
    if (!(val >= 0.0 && val < 4294967296.0))
        return false;
    uint32 = static_cast<uint32_t>(val);
    return true;
}

JSValue* JSNumberCell::getJSNumber()
{
    return this;
}

} // namespace KJS
