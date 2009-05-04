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

#include "NumberObject.h"
#include "UString.h"

namespace JSC {

#if !USE(ALTERNATE_JSIMMEDIATE)

JSValue JSNumberCell::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    return const_cast<JSNumberCell*>(this);
}

bool JSNumberCell::getPrimitiveNumber(ExecState*, double& number, JSValue& value)
{
    number = m_value;
    value = this;
    return true;
}

bool JSNumberCell::toBoolean(ExecState*) const
{
    return m_value < 0.0 || m_value > 0.0; // false for NaN
}

double JSNumberCell::toNumber(ExecState*) const
{
  return m_value;
}

UString JSNumberCell::toString(ExecState*) const
{
    if (m_value == 0.0) // +0.0 or -0.0
        return "0";
    return UString::from(m_value);
}

UString JSNumberCell::toThisString(ExecState*) const
{
    if (m_value == 0.0) // +0.0 or -0.0
        return "0";
    return UString::from(m_value);
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
    uint32 = static_cast<uint32_t>(m_value);
    return uint32 == m_value;
}

bool JSNumberCell::getTruncatedInt32(int32_t& int32) const
{
    if (!(m_value >= -2147483648.0 && m_value < 2147483648.0))
        return false;
    int32 = static_cast<int32_t>(m_value);
    return true;
}

bool JSNumberCell::getTruncatedUInt32(uint32_t& uint32) const
{
    if (!(m_value >= 0.0 && m_value < 4294967296.0))
        return false;
    uint32 = static_cast<uint32_t>(m_value);
    return true;
}

JSValue JSNumberCell::getJSNumber()
{
    return this;
}

JSValue jsNumberCell(ExecState* exec, double d)
{
    return new (exec) JSNumberCell(exec, d);
}

JSValue jsNumberCell(JSGlobalData* globalData, double d)
{
    return new (globalData) JSNumberCell(globalData, d);
}

JSValue jsAPIMangledNumber(ExecState* exec, double d)
{
    return new (exec) JSNumberCell(JSNumberCell::APIMangled, d);
}

#else

JSValue jsNumberCell(ExecState*, double)
{
    ASSERT_NOT_REACHED();
    return JSValue();
}

JSValue jsAPIMangledNumber(ExecState*, double)
{
    ASSERT_NOT_REACHED();
    return JSValue();
}

#endif

} // namespace JSC
