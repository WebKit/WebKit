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
#include "operations.h"

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
bool equal(ExecState* exec, JSValue* v1, JSValue* v2)
{
startOver:
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return v1 == v2;

    if (v1->isNumber() && v2->isNumber())
        return v1->uncheckedGetNumber() == v2->uncheckedGetNumber();

    bool s1 = v1->isString();
    bool s2 = v2->isString();
    if (s1 && s2)
        return static_cast<JSString*>(v1)->value() == static_cast<JSString*>(v2)->value();

    if (v1->isUndefinedOrNull()) {
        if (v2->isUndefinedOrNull())
            return true;
        if (JSImmediate::isImmediate(v2))
            return false;
        return v2->asCell()->structureID()->typeInfo().masqueradesAsUndefined();
    }

    if (v2->isUndefinedOrNull()) {
        if (JSImmediate::isImmediate(v1))
            return false;
        return v1->asCell()->structureID()->typeInfo().masqueradesAsUndefined();
    }

    if (v1->isObject()) {
        if (v2->isObject())
            return v1 == v2;
        JSValue* p1 = v1->toPrimitive(exec);
        if (exec->hadException())
            return false;
        v1 = p1;
        goto startOver;
    }

    if (v2->isObject()) {
        JSValue* p2 = v2->toPrimitive(exec);
        if (exec->hadException())
            return false;
        v2 = p2;
        goto startOver;
    }

    if (s1 || s2) {
        double d1 = v1->toNumber(exec);
        double d2 = v2->toNumber(exec);
        return d1 == d2;
    }

    if (v1->isBoolean()) {
        if (v2->isNumber())
            return static_cast<double>(v1->getBoolean()) == v2->uncheckedGetNumber();
    } else if (v2->isBoolean()) {
        if (v1->isNumber())
            return v1->uncheckedGetNumber() == static_cast<double>(v2->getBoolean());
    }

    return v1 == v2;
}

bool strictEqual(JSValue* v1, JSValue* v2)
{
    if (JSImmediate::areBothImmediate(v1, v2))
        return v1 == v2;

    if (JSImmediate::isEitherImmediate(v1, v2) & (v1 != JSImmediate::from(0)) & (v2 != JSImmediate::from(0)))
        return false;

    return strictEqualSlowCaseInline(v1, v2);
}

bool strictEqualSlowCase(JSValue* v1, JSValue* v2)
{
    return strictEqualSlowCaseInline(v1, v2);
}

JSValue* throwOutOfMemoryError(ExecState* exec)
{
    JSObject* error = Error::create(exec, GeneralError, "Out of memory");
    exec->setException(error);
    return error;
}

} // namespace JSC
