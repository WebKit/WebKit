/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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

inline Structure* RegExpObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(RegExpObjectType, StructureFlags), info());
}

ALWAYS_INLINE unsigned getRegExpObjectLastIndexAsUnsigned(JSGlobalObject* globalObject, RegExpObject* regExpObject, StringView input)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue jsLastIndex = regExpObject->getLastIndex();
    unsigned lastIndex;
    if (jsLastIndex.isUInt32()) [[likely]] {
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

ALWAYS_INLINE JSValue RegExpObject::execInline(JSGlobalObject* globalObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExp* regExp = this->regExp();
    auto input = string->view(globalObject);
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
    JSArray* array = createRegExpMatchesArray(vm, globalObject, string, input, regExp, lastIndex, result);
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
    globalObject->regExpGlobalData().recordMatch(vm, globalObject, regExp, string, result, /* oneCharacterMatch */ false);
    return array;
}

// Shared implementation used by test and exec.
ALWAYS_INLINE MatchResult RegExpObject::matchInline(JSGlobalObject* globalObject, JSString* string)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RegExp* regExp = this->regExp();
    auto input = string->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned lastIndex = getRegExpObjectLastIndexAsUnsigned(globalObject, this, input);
    RETURN_IF_EXCEPTION(scope, { });
    if (!regExp->global() && !regExp->sticky()) {
        scope.release();
        return globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, input, 0);
    }

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

inline unsigned advanceStringUnicode(StringView s, unsigned length, unsigned currentIndex)
{
    if (currentIndex + 1 >= length)
        return currentIndex + 1;

    char16_t first = s[currentIndex];
    if (!U16_IS_LEAD(first))
        return currentIndex + 1;

    char16_t second = s[currentIndex + 1];
    if (!U16_IS_TRAIL(second))
        return currentIndex + 1;

    return currentIndex + 2;
}

template<typename FixEndFunc>
JSValue collectMatches(VM& vm, JSGlobalObject* globalObject, JSString* string, StringView s, RegExp* regExp, const FixEndFunc& fixEnd)
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
    auto iterate = [&]() ALWAYS_INLINE_LAMBDA {
        size_t end = result.end;
        size_t length = end - result.start;
        array->putDirectIndex(globalObject, arrayIndex++, jsSubstringOfResolved(vm, string, result.start, length));
        if (scope.exception()) [[unlikely]] {
            hasException = true;
            return;
        }
        if (!length)
            end = fixEnd(end);
        result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, string, s, end);
        if (scope.exception()) [[unlikely]] {
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
                if (hasException) [[unlikely]]
                    return { };
            } while (result);
            
            return array;
        }
        
        iterate();
        EXCEPTION_ASSERT(!!scope.exception() == hasException);
        if (hasException) [[unlikely]]
            return { };
    } while (result);
    
    return array;
}

template<typename SubjectChar, typename PatternChar>
ALWAYS_INLINE size_t genericMatches(VM& vm, std::span<const SubjectChar> input, std::span<const PatternChar> pattern, size_t& numberOfMatches, size_t& startIndex)
{
    ASSERT(!pattern.empty());
    if (startIndex > input.size())
        return notFound;
    AdaptiveStringSearcher<PatternChar, SubjectChar> search(vm.adaptiveStringSearcherTables(), pattern);
    size_t lastResult = notFound;
    size_t found = search.search(input, startIndex);
    while (found != notFound) {
        lastResult = found;
        startIndex = found + pattern.size();
        numberOfMatches++;
        found = search.search(input, startIndex);
    }
    return lastResult;
}

ALWAYS_INLINE JSValue collectGlobalAtomMatches(JSGlobalObject* globalObject, JSString* string, RegExp* regExp)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t numberOfMatches = 0;
    auto input = string->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    const String& pattern = regExp->atom();
    ASSERT(!pattern.isEmpty());

    size_t lastResult = 0;
    bool oneCharacterMatch = false;
    if (pattern.is8Bit()) {
        if (input->is8Bit()) {
            if (pattern.length() == 1) {
                oneCharacterMatch = true;
                numberOfMatches = WTF::countMatchedCharacters(input->span8(), pattern.span8()[0]);
            } else {
                size_t startIndex = 0;
                lastResult = genericMatches(vm, input->span8(), pattern.span8(), numberOfMatches, startIndex);
            }
        } else {
            if (pattern.length() == 1) {
                oneCharacterMatch = true;
                numberOfMatches = WTF::countMatchedCharacters(input->span16(), pattern.characterAt(0));
            } else {
                size_t startIndex = 0;
                lastResult = genericMatches(vm, input->span16(), pattern.span8(), numberOfMatches, startIndex);
            }
        }
    } else {
        if (input->is8Bit()) {
            size_t startIndex = 0;
            lastResult = genericMatches(vm, input->span8(), pattern.span16(), numberOfMatches, startIndex);
        } else {
            if (pattern.length() == 1) {
                oneCharacterMatch = true;
                numberOfMatches = WTF::countMatchedCharacters(input->span16(), pattern.characterAt(0));
            } else {
                size_t startIndex = 0;
                lastResult = genericMatches(vm, input->span16(), pattern.span16(), numberOfMatches, startIndex);
            }
        }
    }

    if (numberOfMatches > MAX_STORAGE_VECTOR_LENGTH) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return jsUndefined();
    }

    if (!numberOfMatches)
        return jsNull();

    auto* array = createPatternFilledArray(globalObject, jsString(vm, pattern), numberOfMatches);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    MatchResult matchResult { lastResult, lastResult + pattern.length() };
    globalObject->regExpGlobalData().recordMatch(vm, globalObject, regExp, string, matchResult, oneCharacterMatch);

    return array;
}

} // namespace JSC
