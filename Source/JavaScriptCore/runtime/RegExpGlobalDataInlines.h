/*
 * Copyright (C) 2019 Apple Inc. All Rights Reserved.
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

#pragma once

#include "RegExpGlobalData.h"

namespace JSC {

ALWAYS_INLINE void RegExpCachedResult::record(VM& vm, JSObject* owner, RegExp* regExp, JSString* input, MatchResult result)
{
    m_lastRegExp.setWithoutWriteBarrier(regExp);
    m_lastInput.setWithoutWriteBarrier(input);
    m_result = result;
    m_reified = false;
    vm.writeBarrier(owner);
}

inline void RegExpGlobalData::setInput(JSGlobalObject* globalObject, JSString* string)
{
    m_cachedResult.setInput(globalObject, globalObject, string);
}

/*
   To facilitate result caching, exec(), test(), match(), search(), and replace() dipatch regular
   expression matching through the performMatch function. We use cached results to calculate,
   e.g., RegExp.lastMatch and RegExp.leftParen.
*/
ALWAYS_INLINE MatchResult RegExpGlobalData::performMatch(JSGlobalObject* owner, RegExp* regExp, JSString* string, const String& input, int startOffset, int** ovector)
{
    ASSERT(owner);
    VM& vm = owner->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    int position = regExp->match(owner, input, startOffset, m_ovector);
    RETURN_IF_EXCEPTION(scope, MatchResult::failed());

    if (ovector)
        *ovector = m_ovector.data();

    if (position == -1)
        return MatchResult::failed();

    ASSERT(!m_ovector.isEmpty());
    ASSERT(m_ovector[0] == position);
    ASSERT(m_ovector[1] >= position);
    size_t end = m_ovector[1];

    m_cachedResult.record(vm, owner, regExp, string, MatchResult(position, end));

    return MatchResult(position, end);
}

ALWAYS_INLINE MatchResult RegExpGlobalData::performMatch(JSGlobalObject* owner, RegExp* regExp, JSString* string, const String& input, int startOffset)
{
    ASSERT(owner);
    VM& vm = owner->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    MatchResult result = regExp->match(owner, input, startOffset);
    RETURN_IF_EXCEPTION(scope, MatchResult::failed());
    if (result)
        m_cachedResult.record(vm, owner, regExp, string, result);
    return result;
}

ALWAYS_INLINE void RegExpGlobalData::recordMatch(VM& vm, JSGlobalObject* owner, RegExp* regExp, JSString* string, const MatchResult& result)
{
    ASSERT(result);
    m_cachedResult.record(vm, owner, regExp, string, result);
}

inline MatchResult RegExpGlobalData::matchResult() const
{
    return m_cachedResult.result();
}

inline void RegExpGlobalData::resetResultFromCache(JSGlobalObject* owner, RegExp* regExp, JSString* string, MatchResult matchResult, Vector<int>&& vector)
{
    m_ovector = WTFMove(vector);
    m_cachedResult.record(getVM(owner), owner, regExp, string, matchResult);
}

}
