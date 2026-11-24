/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/MatchResult.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/WriteBarrier.h>

namespace JSC {

class JSArray;
class JSString;
class RegExp;

// RegExpCachedResult is used to track the cached results of the last
// match, stores on the RegExp constructor (e.g. $&, $_, $1, $2 ...).
// Recording a match stores only m_lastRegExp, m_lastInput and m_result
// (the JIT writes these fields directly). The matches array is built on
// demand, which sets m_reified and fills m_reifiedResult and
// m_reifiedInput. m_result describes the last match in both states.
class RegExpCachedResult {
public:
    inline void record(VM&, JSObject* owner, RegExp*, JSString* input, MatchResult, bool oneCharacterMatch);

    JSArray* lastResult(JSGlobalObject*, JSObject* owner);
    void setInput(JSGlobalObject*, JSObject* owner, JSString*);

    JSString* lastMatch(JSGlobalObject*, JSObject* owner);
    JSString* leftContext(JSGlobalObject*, JSObject* owner);
    JSString* rightContext(JSGlobalObject*, JSObject* owner);

    JSString* input()
    {
        return m_reified ? m_reifiedInput.get() : m_lastInput.get();
    }

    DECLARE_VISIT_AGGREGATE;

    // m_lastRegExp would be nullptr when RegExpCachedResult is not reified.
    // If we find m_lastRegExp is nullptr, it means this should hold the empty RegExp.
    static constexpr ptrdiff_t offsetOfLastRegExp() { return OBJECT_OFFSETOF(RegExpCachedResult, m_lastRegExp); }
    static constexpr ptrdiff_t offsetOfLastInput() { return OBJECT_OFFSETOF(RegExpCachedResult, m_lastInput); }
    static constexpr ptrdiff_t offsetOfResult() { return OBJECT_OFFSETOF(RegExpCachedResult, m_result); }
    static constexpr ptrdiff_t offsetOfReified() { return OBJECT_OFFSETOF(RegExpCachedResult, m_reified); }
    static constexpr ptrdiff_t offsetOfOneCharacterMatch() { return OBJECT_OFFSETOF(RegExpCachedResult, m_oneCharacterMatch); }

    MatchResult result() const { return m_result; }

private:
    bool hasLazyMatch() const { return !m_reified && m_lastRegExp; }
    JSString* lastInputWithPreciseResult(JSGlobalObject*);
    void makeResultPrecise(StringView input);

    MatchResult m_result { 0, 0 };
    bool m_reified { false };
    bool m_oneCharacterMatch { false };
    WriteBarrier<JSString> m_lastInput;
    WriteBarrier<RegExp> m_lastRegExp;
    WriteBarrier<JSArray> m_reifiedResult;
    WriteBarrier<JSString> m_reifiedInput;
    WriteBarrier<JSString> m_reifiedLeftContext;
    WriteBarrier<JSString> m_reifiedRightContext;
};

} // namespace JSC
