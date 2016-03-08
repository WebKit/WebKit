/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008, 2012, 2016 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef RegExpObjectInlines_h
#define RegExpObjectInlines_h

#include "ButterflyInlines.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "JSCInlines.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"

namespace JSC {

ALWAYS_INLINE unsigned getRegExpObjectLastIndexAsUnsigned(
    ExecState* exec, RegExpObject* regExpObject, const String& input)
{
    JSValue jsLastIndex = regExpObject->getLastIndex();
    unsigned lastIndex;
    if (LIKELY(jsLastIndex.isUInt32())) {
        lastIndex = jsLastIndex.asUInt32();
        if (lastIndex > input.length()) {
            regExpObject->setLastIndex(exec, 0);
            return UINT_MAX;
        }
    } else {
        double doubleLastIndex = jsLastIndex.toInteger(exec);
        if (doubleLastIndex < 0 || doubleLastIndex > input.length()) {
            regExpObject->setLastIndex(exec, 0);
            return UINT_MAX;
        }
        lastIndex = static_cast<unsigned>(doubleLastIndex);
    }
    return lastIndex;
}

JSValue RegExpObject::execInline(ExecState* exec, JSGlobalObject* globalObject, JSString* string)
{
    RegExp* regExp = this->regExp();
    RegExpConstructor* regExpConstructor = globalObject->regExpConstructor();
    String input = string->value(exec); // FIXME: Handle errors. https://bugs.webkit.org/show_bug.cgi?id=155145
    VM& vm = globalObject->vm();

    bool global = regExp->global();

    unsigned lastIndex;
    if (global) {
        lastIndex = getRegExpObjectLastIndexAsUnsigned(exec, this, input);
        if (lastIndex == UINT_MAX)
            return jsNull();
    } else
        lastIndex = 0;
    
    MatchResult result;
    JSArray* array =
        createRegExpMatchesArray(vm, globalObject, string, input, regExp, lastIndex, result);
    if (!array) {
        if (global)
            setLastIndex(exec, 0);
        return jsNull();
    }
    if (global)
        setLastIndex(exec, result.end);
    regExpConstructor->recordMatch(vm, regExp, string, result);
    return array;
}

// Shared implementation used by test and exec.
MatchResult RegExpObject::matchInline(
    ExecState* exec, JSGlobalObject* globalObject, JSString* string)
{
    RegExp* regExp = this->regExp();
    RegExpConstructor* regExpConstructor = globalObject->regExpConstructor();
    String input = string->value(exec); // FIXME: Handle errors. https://bugs.webkit.org/show_bug.cgi?id=155145
    VM& vm = globalObject->vm();
    if (!regExp->global())
        return regExpConstructor->performMatch(vm, regExp, string, input, 0);

    unsigned lastIndex = getRegExpObjectLastIndexAsUnsigned(exec, this, input);
    if (lastIndex == UINT_MAX)
        return MatchResult::failed();
    
    MatchResult result = regExpConstructor->performMatch(vm, regExp, string, input, lastIndex);
    setLastIndex(exec, result.end);
    return result;
}

} // namespace JSC

#endif // RegExpObjectInlines_h

