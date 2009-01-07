/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "Operations.h"

#include "Error.h"
#include "JSObject.h"
#include "JSString.h"
#include <math.h>
#include <stdio.h>
#include <wtf/MathExtras.h>

#if HAVE(FLOAT_H)
#include <float.h>
#endif

namespace JSC {

// ECMA 11.9.3
bool equal(ExecState* exec, JSValuePtr v1, JSValuePtr v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return v1 == v2;

    return equalSlowCaseInline(exec, v1, v2);
}

bool equalSlowCase(ExecState* exec, JSValuePtr v1, JSValuePtr v2)
{
    return equalSlowCaseInline(exec, v1, v2);
}

bool strictEqual(JSValuePtr v1, JSValuePtr v2)
{
    if (JSImmediate::areBothImmediate(v1, v2))
        return v1 == v2;

    if (JSImmediate::isEitherImmediate(v1, v2) & (v1 != JSImmediate::from(0)) & (v2 != JSImmediate::from(0)))
        return false;

    return strictEqualSlowCaseInline(v1, v2);
}

bool strictEqualSlowCase(JSValuePtr v1, JSValuePtr v2)
{
    return strictEqualSlowCaseInline(v1, v2);
}

NEVER_INLINE JSValuePtr throwOutOfMemoryError(ExecState* exec)
{
    JSObject* error = Error::create(exec, GeneralError, "Out of memory");
    exec->setException(error);
    return error;
}

} // namespace JSC
