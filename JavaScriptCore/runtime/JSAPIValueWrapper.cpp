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
#include "JSAPIValueWrapper.h"

#include "NumberObject.h"
#include "UString.h"

namespace JSC {

JSValue JSAPIValueWrapper::toPrimitive(ExecState*, PreferredPrimitiveType) const
{
    ASSERT_NOT_REACHED();
    return JSValue();
}

bool JSAPIValueWrapper::getPrimitiveNumber(ExecState*, double&, JSValue&)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool JSAPIValueWrapper::toBoolean(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

double JSAPIValueWrapper::toNumber(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

UString JSAPIValueWrapper::toString(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return UString();
}

JSObject* JSAPIValueWrapper::toObject(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace JSC
