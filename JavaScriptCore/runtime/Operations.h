/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2002, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef Operations_h
#define Operations_h

#include "JSImmediate.h"
#include "JSNumberCell.h"
#include "JSString.h"

namespace JSC {

    // ECMA 11.9.3
    inline bool JSValuePtr::equal(ExecState* exec, JSValuePtr v1, JSValuePtr v2)
    {
        if (JSImmediate::areBothImmediateNumbers(v1, v2))
            return v1 == v2;

        return equalSlowCase(exec, v1, v2);
    }

    ALWAYS_INLINE bool JSValuePtr::equalSlowCaseInline(ExecState* exec, JSValuePtr v1, JSValuePtr v2)
    {
        ASSERT(!JSImmediate::areBothImmediateNumbers(v1, v2));

        do {
            if (v1->isNumber() && v2->isNumber())
                return v1->uncheckedGetNumber() == v2->uncheckedGetNumber();

            bool s1 = v1->isString();
            bool s2 = v2->isString();
            if (s1 && s2)
                return asString(v1)->value() == asString(v2)->value();

            if (v1->isUndefinedOrNull()) {
                if (v2->isUndefinedOrNull())
                    return true;
                if (JSImmediate::isImmediate(v2))
                    return false;
                return v2->asCell()->structure()->typeInfo().masqueradesAsUndefined();
            }

            if (v2->isUndefinedOrNull()) {
                if (JSImmediate::isImmediate(v1))
                    return false;
                return v1->asCell()->structure()->typeInfo().masqueradesAsUndefined();
            }

            if (v1->isObject()) {
                if (v2->isObject())
                    return v1 == v2;
                JSValuePtr p1 = v1->toPrimitive(exec);
                if (exec->hadException())
                    return false;
                v1 = p1;
                if (JSImmediate::areBothImmediateNumbers(v1, v2))
                    return v1 == v2;
                continue;
            }

            if (v2->isObject()) {
                JSValuePtr p2 = v2->toPrimitive(exec);
                if (exec->hadException())
                    return false;
                v2 = p2;
                if (JSImmediate::areBothImmediateNumbers(v1, v2))
                    return v1 == v2;
                continue;
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
        } while (true);
    }

    // ECMA 11.9.3
    inline bool JSValuePtr::strictEqual(JSValuePtr v1, JSValuePtr v2)
    {
        if (JSImmediate::areBothImmediate(v1, v2))
            return v1 == v2;

        if (JSImmediate::isEitherImmediate(v1, v2) & (v1 != js0()) & (v2 != js0()))
            return false;

        return strictEqualSlowCase(v1, v2);
    }

    ALWAYS_INLINE bool JSValuePtr::strictEqualSlowCaseInline(JSValuePtr v1, JSValuePtr v2)
    {
        ASSERT(!JSImmediate::areBothImmediate(v1, v2));

        if (JSImmediate::isEitherImmediate(v1, v2)) {
            ASSERT(v1 == js0() || v2 == js0());
            ASSERT(v1 != v2);

            // The reason we can't just return false here is that 0 === -0,
            // and while the former is an immediate number, the latter is not.
            if (v1 == js0())
                return v2->asCell()->isNumber() && v2->asNumberCell()->value() == 0;
            return v1->asCell()->isNumber() && v1->asNumberCell()->value() == 0;
        }

        if (v1->asCell()->isNumber()) {
            return v2->asCell()->isNumber()
                && v1->asNumberCell()->value() == v2->asNumberCell()->value();
        }

        if (v1->asCell()->isString()) {
            return v2->asCell()->isString()
                && asString(v1)->value() == asString(v2)->value();
        }

        return v1 == v2;
    }

    JSValuePtr throwOutOfMemoryError(ExecState*);

}
#endif
