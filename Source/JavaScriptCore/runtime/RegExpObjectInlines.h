/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2018 Apple Inc. All Rights Reserved.
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

#pragma once

#include "ButterflyInlines.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSArray.h"
#include "JSGlobalObject.h"
#include "JSString.h"
#include "JSCInlines.h"
#include "RegExpGlobalDataInlines.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"

namespace JSC {

ALWAYS_INLINE unsigned getRegExpObjectLastIndexAsUnsigned(
    JSGlobalObject* globalObject, RegExpObject* regExpObject, const String& input)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue jsLastIndex = regExpObject->getLastIndex();
    unsigned lastIndex;
    if (LIKELY(jsLastIndex.isUInt32())) {
        lastIndex = jsLastIndex.asUInt32();
        if (lastIndex > input.length())
            return UINT_MAX;
    } else {
        double doubleLastIndex = jsLastIndex.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, UINT_MAX);
        if (doubleLastIndex > input.length())
            return UINT_MAX;
        lastIndex = (doubleLastIndex < 0) ? 0 : static_cast<unsigned>(doubleLastIndex);
    }
    return lastIndex;
}

inline JSValue RegExpObject::execInline(JSGlobalObject* globalObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExp* regExp = this->regExp();
    String input = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    bool globalOrSticky = regExp->globalOrSticky();
    unsigned lastIndex = getRegExpObjectLastIndexAsUnsigned(globalObject, this, input);
    RETURN_IF_EXCEPTION(scope, { });
    if (lastIndex == UINT_MAX && globalOrSticky) {
        scope.release();
        setLastIndex(globalObject, 0);
        return jsNull();
    }

    if (!globalOrSticky)
        lastIndex = 0;
    
    MatchResult result;
    JSArray* array =
        createRegExpMatchesArray(vm, globalObject, string, input, regExp, lastIndex, result);
    if (!array) {
        RETURN_IF_EXCEPTION(scope, { });
        scope.release();
        if (globalOrSticky)
            setLastIndex(globalObject, 0);
        return jsNull();
    }

    if (globalOrSticky)
        setLastIndex(globalObject, result.end);
    RETURN_IF_EXCEPTION(scope, { });
    globalObject->regExpGlobalData().recordMatch(vm, globalObject, regExp, string, result);
    return array;
}

// Shared implementation used by test and exec.
inline MatchResult RegExpObject::matchInline(
    JSGlobalObject* globalObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExp* regExp = this->regExp();
    String input = string->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (!regExp->global() && !regExp->sticky()) {
        scope.release();
        return globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, input, 0);
    }

    unsigned lastIndex = getRegExpObjectLastIndexAsUnsigned(globalObject, this, input);
    RETURN_IF_EXCEPTION(scope, { });
    if (lastIndex == UINT_MAX) {
        scope.release();
        setLastIndex(globalObject, 0);
        return MatchResult::failed();
    }
    
    MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, input, lastIndex);
    RETURN_IF_EXCEPTION(scope, { });
    scope.release();
    setLastIndex(globalObject, result.end);
    return result;
}

inline unsigned advanceStringUnicode(String s, unsigned length, unsigned currentIndex)
{
    if (currentIndex + 1 >= length)
        return currentIndex + 1;

    UChar first = s[currentIndex];
    if (first < 0xD800 || first > 0xDBFF)
        return currentIndex + 1;

    UChar second = s[currentIndex + 1];
    if (second < 0xDC00 || second > 0xDFFF)
        return currentIndex + 1;

    return currentIndex + 2;
}

template<typename FixEndFunc>
JSValue collectMatches(VM& vm, JSGlobalObject* globalObject, JSString* string, const String& s, RegExp* regExp, const FixEndFunc& fixEnd)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, s, 0);
    RETURN_IF_EXCEPTION(scope, { });
    if (!result)
        return jsNull();
    
    static unsigned maxSizeForDirectPath = 100000;
    
    JSArray* array = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });

    bool hasException = false;
    unsigned arrayIndex = 0;
    auto iterate = [&] () {
        size_t end = result.end;
        size_t length = end - result.start;
        array->putDirectIndex(globalObject, arrayIndex++, jsSubstringOfResolved(vm, string, result.start, length));
        if (UNLIKELY(scope.exception())) {
            hasException = true;
            return;
        }
        if (!length)
            end = fixEnd(end);
        result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, s, end);
        if (UNLIKELY(scope.exception())) {
            hasException = true;
            return;
        }
    };
    
    do {
        if (array->length() >= maxSizeForDirectPath) {
            // First do a throw-away match to see how many matches we'll get.
            unsigned matchCount = 0;
            MatchResult savedResult = result;
            do {
                if (array->length() + matchCount > MAX_STORAGE_VECTOR_LENGTH) {
                    throwOutOfMemoryError(globalObject, scope);
                    return jsUndefined();
                }
                
                size_t end = result.end;
                matchCount++;
                if (result.empty())
                    end = fixEnd(end);
                
                // Using RegExpGlobalData::performMatch() instead of calling RegExp::match()
                // directly is a surprising but profitable choice: it means that when we do OOM, we
                // will leave the cached result in the state it ought to have had just before the
                // OOM! On the other hand, if this loop concludes that the result is small enough,
                // then the iterate() loop below will overwrite the cached result anyway.
                result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, s, end);
                RETURN_IF_EXCEPTION(scope, { });
            } while (result);
            
            // OK, we have a sensible number of matches. Now we can create them for reals.
            result = savedResult;
            do {
                iterate();
                EXCEPTION_ASSERT(!!scope.exception() == hasException);
                if (UNLIKELY(hasException))
                    return { };
            } while (result);
            
            return array;
        }
        
        iterate();
        EXCEPTION_ASSERT(!!scope.exception() == hasException);
        if (UNLIKELY(hasException))
            return { };
    } while (result);
    
    return array;
}

} // namespace JSC
