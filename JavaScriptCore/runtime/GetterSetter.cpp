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
#include "GetterSetter.h"

#include "JSObject.h"
#include <wtf/Assertions.h>

namespace JSC {

void GetterSetter::mark()
{
    JSCell::mark();

    if (m_getter && !m_getter->marked())
        m_getter->mark();
    if (m_setter && !m_setter->marked())
        m_setter->mark();
}

JSValue GetterSetter::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    ASSERT_NOT_REACHED();
    return jsNull();
}

bool GetterSetter::getPrimitiveNumber(ExecState*, double& number, JSValue& value)
{
    ASSERT_NOT_REACHED();
    number = 0;
    value = JSValue();
    return true;
}

bool GetterSetter::toBoolean(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

double GetterSetter::toNumber(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return 0.0;
}

UString GetterSetter::toString(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return UString::null();
}

JSObject* GetterSetter::toObject(ExecState* exec) const
{
    ASSERT_NOT_REACHED();
    return jsNull().toObject(exec);
}

bool GetterSetter::isGetterSetter() const
{
    return true;
}

} // namespace JSC
