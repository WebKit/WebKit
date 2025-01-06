/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RegExpSubstringGlobalAtomCache.h"

#include "JSGlobalObjectInlines.h"
#include "RegExpObjectInlines.h"

namespace JSC {

template<typename Visitor>
void RegExpSubstringGlobalAtomCache::visitAggregateImpl(Visitor& visitor)
{
    visitor.append(m_lastSubstringBase);
    visitor.append(m_lastRegExp);
}

DEFINE_VISIT_AGGREGATE(RegExpSubstringGlobalAtomCache);

JSValue RegExpSubstringGlobalAtomCache::collectMatches(JSGlobalObject* globalObject, JSRopeString* substring, RegExp* regExp)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Try to get the last cache if possible
    size_t numberOfMatches = 0;
    size_t startIndex = 0;
    ([&]() ALWAYS_INLINE_LAMBDA {
        if (regExp != m_lastRegExp.get())
            return;
        if (substring->substringBase() != m_lastSubstringBase.get())
            return;
        if (substring->substringOffset() != m_lastSubstringOffset)
            return;
        if (substring->length() < m_lastSubstringLength)
            return;
        numberOfMatches = m_lastNumberOfMatches;
        startIndex = m_lastMatchEnd;
    })();

    JSString* substringBase = substring->substringBase();
    unsigned substringOffset = substring->substringOffset();
    unsigned substringLength = substring->length();
    // Keep the substring info above since the following may resolve the substring to a non-rope.
    auto input = substring->view(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    const String& pattern = regExp->atom();
    ASSERT(!pattern.isEmpty());

    auto regExpMatch = [&]() ALWAYS_INLINE_LAMBDA {
        MatchResult result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, substring, input, startIndex);
        if (UNLIKELY(scope.exception()))
            return;

        if (result) {
            do {
                if (UNLIKELY(numberOfMatches > MAX_STORAGE_VECTOR_LENGTH)) {
                    throwOutOfMemoryError(globalObject, scope);
                    return;
                }

                numberOfMatches++;
                startIndex = result.end;
                if (result.empty())
                    startIndex++;

                result = globalObject->regExpGlobalData().performMatch(globalObject, regExp, substring, input, startIndex);
                if (UNLIKELY(scope.exception()))
                    return;
            } while (result);
        }
    };

    if (pattern.is8Bit()) {
        if (input->is8Bit()) {
            if (pattern.length() == 1) {
                if (input->length() >= startIndex) {
                    numberOfMatches += WTF::countMatchedCharacters(input->span8().subspan(startIndex), pattern.span8()[0]);
                    startIndex = input->length(); // Because the pattern atom is one character, it is ensured that we no longer find anything until this input string's end.
                }
            } else {
                regExpMatch();
                if (UNLIKELY(scope.exception()))
                    return { };
            }
        } else {
            if (pattern.length() == 1) {
                if (input->length() >= startIndex) {
                    numberOfMatches += WTF::countMatchedCharacters(input->span16().subspan(startIndex), pattern.characterAt(0));
                    startIndex = input->length(); // Because the pattern atom is one character, it is ensured that we no longer find anything until this input string's end.
                }
            } else {
                regExpMatch();
                if (UNLIKELY(scope.exception()))
                    return { };
            }
        }
    } else {
        if (input->is8Bit()) {
            regExpMatch();
            if (UNLIKELY(scope.exception()))
                return { };
        } else {
            if (pattern.length() == 1) {
                if (input->length() >= startIndex) {
                    numberOfMatches += WTF::countMatchedCharacters(input->span16().subspan(startIndex), pattern.characterAt(0));
                    startIndex = input->length(); // Because the pattern atom is one character, it is ensured that we no longer find anything until this input string's end.
                }
            } else {
                regExpMatch();
                if (UNLIKELY(scope.exception()))
                    return { };
            }
        }
    }

    if (UNLIKELY(numberOfMatches > MAX_STORAGE_VECTOR_LENGTH)) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    if (!numberOfMatches)
        return jsNull();

    // Construct the array
    JSArray* array = createPatternFilledArray(globalObject, jsString(vm, pattern), numberOfMatches);
    RETURN_IF_EXCEPTION(scope, { });

    // Cache
    {
        m_lastSubstringBase.set(vm, globalObject, substringBase);
        m_lastSubstringOffset = substringOffset;
        m_lastSubstringLength = substringLength;

        m_lastRegExp.set(vm, globalObject, regExp);
        m_lastNumberOfMatches = numberOfMatches;
        m_lastMatchEnd = startIndex;
    }
    return array;
}

} // namespace JSC
