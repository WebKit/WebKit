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
    // Keep the substring info above since the following will resolve the substring to a non-rope.
    auto input = substring->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    const String& pattern = regExp->atom();
    ASSERT(!pattern.isEmpty());

    if (pattern.is8Bit()) {
        if (input->is8Bit()) {
            if (pattern.length() == 1)
                oneCharMatches(input->span8(), pattern.span8()[0], numberOfMatches, startIndex);
            else
                genericMatches(input, pattern, numberOfMatches, startIndex);
        } else {
            if (pattern.length() == 1)
                oneCharMatches(input->span16(), pattern.characterAt(0), numberOfMatches, startIndex);
            else
                genericMatches(input, pattern, numberOfMatches, startIndex);
        }
    } else {
        if (input->is8Bit())
            genericMatches(input, pattern, numberOfMatches, startIndex);
        else {
            if (pattern.length() == 1)
                oneCharMatches(input->span16(), pattern.characterAt(0), numberOfMatches, startIndex);
            else
                genericMatches(input, pattern, numberOfMatches, startIndex);
        }
    }

    if (!numberOfMatches)
        return jsNull();

    // Construct the array
    JSArray* array = createPatternFilledArray(globalObject, jsString(vm, pattern), numberOfMatches);
    RETURN_IF_EXCEPTION(scope, { });

    // Cache
    {
        m_lastSubstringBase.setWithoutWriteBarrier(substringBase);
        m_lastSubstringOffset = substringOffset;
        m_lastSubstringLength = substringLength;

        m_lastRegExp.setWithoutWriteBarrier(regExp);
        m_lastNumberOfMatches = numberOfMatches;
        m_lastMatchEnd = startIndex;

        vm.writeBarrier(globalObject);
    }
    return array;
}

} // namespace JSC
