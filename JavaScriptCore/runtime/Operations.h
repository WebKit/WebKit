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

    NEVER_INLINE JSValuePtr throwOutOfMemoryError(ExecState*);
    NEVER_INLINE JSValuePtr jsAddSlowCase(CallFrame*, JSValuePtr, JSValuePtr);
    JSValuePtr jsTypeStringForValue(CallFrame*, JSValuePtr);
    bool jsIsObjectType(JSValuePtr);
    bool jsIsFunctionType(JSValuePtr);

    // ECMA 11.9.3
    inline bool JSValuePtr::equal(ExecState* exec, JSValuePtr v1, JSValuePtr v2)
    {
        if (JSImmediate::areBothImmediateIntegerNumbers(v1, v2))
            return v1 == v2;

        return equalSlowCase(exec, v1, v2);
    }

    ALWAYS_INLINE bool JSValuePtr::equalSlowCaseInline(ExecState* exec, JSValuePtr v1, JSValuePtr v2)
    {
        ASSERT(!JSImmediate::areBothImmediateIntegerNumbers(v1, v2));

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
                if (JSImmediate::isImmediate(v2))
                    return false;
                return v2.asCell()->structure()->typeInfo().masqueradesAsUndefined();
            }

            if (v2.isUndefinedOrNull()) {
                if (JSImmediate::isImmediate(v1))
                    return false;
                return v1.asCell()->structure()->typeInfo().masqueradesAsUndefined();
            }

            if (v1.isObject()) {
                if (v2.isObject())
                    return v1 == v2;
                JSValuePtr p1 = v1.toPrimitive(exec);
                if (exec->hadException())
                    return false;
                v1 = p1;
                if (JSImmediate::areBothImmediateIntegerNumbers(v1, v2))
                    return v1 == v2;
                continue;
            }

            if (v2.isObject()) {
                JSValuePtr p2 = v2.toPrimitive(exec);
                if (exec->hadException())
                    return false;
                v2 = p2;
                if (JSImmediate::areBothImmediateIntegerNumbers(v1, v2))
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
    inline bool JSValuePtr::strictEqual(JSValuePtr v1, JSValuePtr v2)
    {
        if (JSImmediate::areBothImmediateIntegerNumbers(v1, v2))
            return v1 == v2;

        if (v1.isNumber() && v2.isNumber())
            return v1.uncheckedGetNumber() == v2.uncheckedGetNumber();

        if (JSImmediate::isEitherImmediate(v1, v2))
            return v1 == v2;

        return strictEqualSlowCase(v1, v2);
    }

    ALWAYS_INLINE bool JSValuePtr::strictEqualSlowCaseInline(JSValuePtr v1, JSValuePtr v2)
    {
        ASSERT(!JSImmediate::isEitherImmediate(v1, v2));

        if (v1.asCell()->isString() && v2.asCell()->isString())
            return asString(v1)->value() == asString(v2)->value();

        return v1 == v2;
    }

    inline bool jsLess(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
    {
        if (JSValuePtr::areBothInt32Fast(v1, v2))
            return v1.getInt32Fast() < v2.getInt32Fast();

        double n1;
        double n2;
        if (v1.getNumber(n1) && v2.getNumber(n2))
            return n1 < n2;

        JSGlobalData* globalData = &callFrame->globalData();
        if (isJSString(globalData, v1) && isJSString(globalData, v2))
            return asString(v1)->value() < asString(v2)->value();

        JSValuePtr p1;
        JSValuePtr p2;
        bool wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
        bool wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);

        if (wasNotString1 | wasNotString2)
            return n1 < n2;

        return asString(p1)->value() < asString(p2)->value();
    }

    inline bool jsLessEq(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
    {
        if (JSValuePtr::areBothInt32Fast(v1, v2))
            return v1.getInt32Fast() <= v2.getInt32Fast();

        double n1;
        double n2;
        if (v1.getNumber(n1) && v2.getNumber(n2))
            return n1 <= n2;

        JSGlobalData* globalData = &callFrame->globalData();
        if (isJSString(globalData, v1) && isJSString(globalData, v2))
            return !(asString(v2)->value() < asString(v1)->value());

        JSValuePtr p1;
        JSValuePtr p2;
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

    ALWAYS_INLINE JSValuePtr jsAdd(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
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
            RefPtr<UString::Rep> value = v2.isInt32Fast() ?
                concatenate(asString(v1)->value().rep(), v2.getInt32Fast()) :
                concatenate(asString(v1)->value().rep(), right);

            if (!value)
                return throwOutOfMemoryError(callFrame);
            return jsString(callFrame, value.release());
        }

        // All other cases are pretty uncommon
        return jsAddSlowCase(callFrame, v1, v2);
    }

    inline StructureChain* cachePrototypeChain(CallFrame* callFrame, Structure* structure)
    {
        JSValuePtr prototype = structure->prototypeForLookup(callFrame);
        if (!prototype.isCell())
            return 0;
        RefPtr<StructureChain> chain = StructureChain::create(asObject(prototype)->structure());
        structure->setCachedPrototypeChain(chain.release());
        return structure->cachedPrototypeChain();
    }

    inline size_t countPrototypeChainEntriesAndCheckForProxies(CallFrame* callFrame, JSValuePtr baseValue, const PropertySlot& slot)
    {
        JSCell* cell = asCell(baseValue);
        size_t count = 0;

        while (slot.slotBase() != cell) {
            JSValuePtr v = cell->structure()->prototypeForLookup(callFrame);

            // If we didn't find slotBase in baseValue's prototype chain, then baseValue
            // must be a proxy for another object.

            if (v.isNull())
                return 0;

            cell = asCell(v);

            // Since we're accessing a prototype in a loop, it's a good bet that it
            // should not be treated as a dictionary.
            if (cell->structure()->isDictionary()) {
                RefPtr<Structure> transition = Structure::fromDictionaryTransition(cell->structure());
                asObject(cell)->setStructure(transition.release());
                cell->structure()->setCachedPrototypeChain(0);
            }

            ++count;
        }
        
        ASSERT(count);
        return count;
    }

    ALWAYS_INLINE JSValuePtr resolveBase(CallFrame* callFrame, Identifier& property, ScopeChainNode* scopeChain)
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
        return noValue();
    }

} // namespace JSC

#endif // Operations_h
