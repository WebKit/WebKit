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

#include "Interpreter.h"
#include "JSImmediate.h"
#include "JSNumberCell.h"
#include "JSString.h"

namespace JSC {

    NEVER_INLINE JSValue throwOutOfMemoryError(ExecState*);
    NEVER_INLINE JSValue jsAddSlowCase(CallFrame*, JSValue, JSValue);
    JSValue jsTypeStringForValue(CallFrame*, JSValue);
    bool jsIsObjectType(JSValue);
    bool jsIsFunctionType(JSValue);

    // ECMA 11.9.3
    inline bool JSValue::equal(ExecState* exec, JSValue v1, JSValue v2)
    {
        if (v1.isInt32() && v2.isInt32())
            return v1 == v2;

        return equalSlowCase(exec, v1, v2);
    }

    ALWAYS_INLINE bool JSValue::equalSlowCaseInline(ExecState* exec, JSValue v1, JSValue v2)
    {
        do {
            if (v1.isNumber() && v2.isNumber())
                return v1.uncheckedGetNumber() == v2.uncheckedGetNumber();

            bool s1 = v1.isString();
            bool s2 = v2.isString();
            if (s1 && s2)
                return asString(v1)->value() == asString(v2)->value();

            if (v1.isUndefinedOrNull()) {
                if (v2.isUndefinedOrNull())
                    return true;
                if (!v2.isCell())
                    return false;
                return v2.asCell()->structure()->typeInfo().masqueradesAsUndefined();
            }

            if (v2.isUndefinedOrNull()) {
                if (!v1.isCell())
                    return false;
                return v1.asCell()->structure()->typeInfo().masqueradesAsUndefined();
            }

            if (v1.isObject()) {
                if (v2.isObject())
                    return v1 == v2;
                JSValue p1 = v1.toPrimitive(exec);
                if (exec->hadException())
                    return false;
                v1 = p1;
                if (v1.isInt32() && v2.isInt32())
                    return v1 == v2;
                continue;
            }

            if (v2.isObject()) {
                JSValue p2 = v2.toPrimitive(exec);
                if (exec->hadException())
                    return false;
                v2 = p2;
                if (v1.isInt32() && v2.isInt32())
                    return v1 == v2;
                continue;
            }

            if (s1 || s2) {
                double d1 = v1.toNumber(exec);
                double d2 = v2.toNumber(exec);
                return d1 == d2;
            }

            if (v1.isBoolean()) {
                if (v2.isNumber())
                    return static_cast<double>(v1.getBoolean()) == v2.uncheckedGetNumber();
            } else if (v2.isBoolean()) {
                if (v1.isNumber())
                    return v1.uncheckedGetNumber() == static_cast<double>(v2.getBoolean());
            }

            return v1 == v2;
        } while (true);
    }

    // ECMA 11.9.3
    ALWAYS_INLINE bool JSValue::strictEqualSlowCaseInline(JSValue v1, JSValue v2)
    {
        ASSERT(v1.isCell() && v2.isCell());

        if (v1.asCell()->isString() && v2.asCell()->isString())
            return asString(v1)->value() == asString(v2)->value();

        return v1 == v2;
    }

    inline bool JSValue::strictEqual(JSValue v1, JSValue v2)
    {
        if (v1.isInt32() && v2.isInt32())
            return v1 == v2;

        if (v1.isNumber() && v2.isNumber())
            return v1.uncheckedGetNumber() == v2.uncheckedGetNumber();

        if (!v1.isCell() || !v2.isCell())
            return v1 == v2;

        return strictEqualSlowCaseInline(v1, v2);
    }

    inline bool jsLess(CallFrame* callFrame, JSValue v1, JSValue v2)
    {
        if (v1.isInt32() && v2.isInt32())
            return v1.asInt32() < v2.asInt32();

        double n1;
        double n2;
        if (v1.getNumber(n1) && v2.getNumber(n2))
            return n1 < n2;

        JSGlobalData* globalData = &callFrame->globalData();
        if (isJSString(globalData, v1) && isJSString(globalData, v2))
            return asString(v1)->value() < asString(v2)->value();

        JSValue p1;
        JSValue p2;
        bool wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
        bool wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);

        if (wasNotString1 | wasNotString2)
            return n1 < n2;

        return asString(p1)->value() < asString(p2)->value();
    }

    inline bool jsLessEq(CallFrame* callFrame, JSValue v1, JSValue v2)
    {
        if (v1.isInt32() && v2.isInt32())
            return v1.asInt32() <= v2.asInt32();

        double n1;
        double n2;
        if (v1.getNumber(n1) && v2.getNumber(n2))
            return n1 <= n2;

        JSGlobalData* globalData = &callFrame->globalData();
        if (isJSString(globalData, v1) && isJSString(globalData, v2))
            return !(asString(v2)->value() < asString(v1)->value());

        JSValue p1;
        JSValue p2;
        bool wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
        bool wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);

        if (wasNotString1 | wasNotString2)
            return n1 <= n2;

        return !(asString(p2)->value() < asString(p1)->value());
    }

    // Fast-path choices here are based on frequency data from SunSpider:
    //    <times> Add case: <t1> <t2>
    //    ---------------------------
    //    5626160 Add case: 3 3 (of these, 3637690 are for immediate values)
    //    247412  Add case: 5 5
    //    20900   Add case: 5 6
    //    13962   Add case: 5 3
    //    4000    Add case: 3 5

    ALWAYS_INLINE JSValue jsAdd(CallFrame* callFrame, JSValue v1, JSValue v2)
    {
        double left;
        double right = 0.0;

        bool rightIsNumber = v2.getNumber(right);
        if (rightIsNumber && v1.getNumber(left))
            return jsNumber(callFrame, left + right);
        
        bool leftIsString = v1.isString();
        if (leftIsString && v2.isString()) {
            RefPtr<UString::Rep> value = concatenate(asString(v1)->value().rep(), asString(v2)->value().rep());
            if (!value)
                return throwOutOfMemoryError(callFrame);
            return jsString(callFrame, value.release());
        }

        if (rightIsNumber & leftIsString) {
            RefPtr<UString::Rep> value = v2.isInt32() ?
                concatenate(asString(v1)->value().rep(), v2.asInt32()) :
                concatenate(asString(v1)->value().rep(), right);

            if (!value)
                return throwOutOfMemoryError(callFrame);
            return jsString(callFrame, value.release());
        }

        // All other cases are pretty uncommon
        return jsAddSlowCase(callFrame, v1, v2);
    }

    inline size_t normalizePrototypeChain(CallFrame* callFrame, JSValue base, JSValue slotBase)
    {
        JSCell* cell = asCell(base);
        size_t count = 0;

        while (slotBase != cell) {
            JSValue v = cell->structure()->prototypeForLookup(callFrame);

            // If we didn't find slotBase in base's prototype chain, then base
            // must be a proxy for another object.

            if (v.isNull())
                return 0;

            cell = asCell(v);

            // Since we're accessing a prototype in a loop, it's a good bet that it
            // should not be treated as a dictionary.
            if (cell->structure()->isDictionary())
                asObject(cell)->flattenDictionaryObject();

            ++count;
        }
        
        ASSERT(count);
        return count;
    }

    inline size_t normalizePrototypeChain(CallFrame* callFrame, JSCell* base)
    {
        size_t count = 0;
        while (1) {
            JSValue v = base->structure()->prototypeForLookup(callFrame);
            if (v.isNull())
                return count;

            base = asCell(v);

            // Since we're accessing a prototype in a loop, it's a good bet that it
            // should not be treated as a dictionary.
            if (base->structure()->isDictionary())
                asObject(base)->flattenDictionaryObject();

            ++count;
        }
    }

    ALWAYS_INLINE JSValue resolveBase(CallFrame* callFrame, Identifier& property, ScopeChainNode* scopeChain)
    {
        ScopeChainIterator iter = scopeChain->begin();
        ScopeChainIterator next = iter;
        ++next;
        ScopeChainIterator end = scopeChain->end();
        ASSERT(iter != end);

        PropertySlot slot;
        JSObject* base;
        while (true) {
            base = *iter;
            if (next == end || base->getPropertySlot(callFrame, property, slot))
                return base;

            iter = next;
            ++next;
        }

        ASSERT_NOT_REACHED();
        return JSValue();
    }

    ALWAYS_INLINE JSValue concatenateStrings(CallFrame* callFrame, Register* strings, unsigned count)
    {
        ASSERT(count >= 3);

        // Estimate the amount of space required to hold the entire string.  If all
        // arguments are strings, we can easily calculate the exact amount of space
        // required.  For any other arguments, for now let's assume they may require
        // 11 UChars of storage.  This is enouch to hold any int, and likely is also
        // reasonable for the other immediates.  We may want to come back and tune
        // this value at some point.
        unsigned bufferSize = 0;
        for (unsigned i = 0; i < count; ++i) {
            JSValue v = strings[i].jsValue();
            if (LIKELY(v.isString()))
                bufferSize += asString(v)->value().size();
            else
                bufferSize += 11;
        }

        // Allocate an output string to store the result.
        // If the first argument is a String, and if it has the capacity (or can grow
        // its capacity) to hold the entire result then use this as a base to concatenate
        // onto.  Otherwise, allocate a new empty output buffer.
        JSValue firstValue = strings[0].jsValue();
        RefPtr<UString::Rep> resultRep;
        if (firstValue.isString() && (resultRep = asString(firstValue)->value().rep())->reserveCapacity(bufferSize)) {
            // We're going to concatenate onto the first string - remove it from the list of items to be appended.
            ++strings;
            --count;
        } else
            resultRep = UString::Rep::createEmptyBuffer(bufferSize);
        UString result(resultRep);

        // Loop over the operands, writing them into the output buffer.
        for (unsigned i = 0; i < count; ++i) {
            JSValue v = strings[i].jsValue();
            if (LIKELY(v.isString()))
                result.append(asString(v)->value());
            else
                result.append(v.toString(callFrame));
        }

        return jsString(callFrame, result);
    }

} // namespace JSC

#endif // Operations_h
