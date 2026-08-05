/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CharacterRange.h>
#include <WebCore/FindOptions.h>
#include <span>
#include <unicode/usearch.h>
#include <wtf/Function.h>
#include <wtf/IterationStatus.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TextMatcher {
    WTF_MAKE_NONCOPYABLE(TextMatcher);
public:
    WEBCORE_EXPORT TextMatcher(const String& target, FindOptions);
    WEBCORE_EXPORT ~TextMatcher();

    FindOptions options() const { return m_options; }

    WEBCORE_EXPORT void forEachMatch(StringView, size_t startOffset, NOESCAPE const Function<IterationStatus(CharacterRange)>&) const;
    WEBCORE_EXPORT void forEachCandidate(std::span<const char16_t>, size_t startOffset, NOESCAPE const Function<IterationStatus(CharacterRange)>&) const;
    WEBCORE_EXPORT bool isAcceptableMatch(std::span<const char16_t>, CharacterRange) const;

private:
    void setText(std::span<const char16_t>) const;
    void clearText() const;
    void setOffset(size_t) const;
    std::optional<CharacterRange> next() const;
    std::optional<CharacterRange> nextInDirection(bool backwards) const;
#if !PLATFORM(PLAYSTATION)
    std::optional<CharacterRange> previous() const;
#endif

    const String m_target;
    const StringView::UpconvertedCharacters m_targetCharacters;
    const FindOptions m_options;
    bool m_requiresKanaWorkaround { false };
    Vector<char16_t> m_normalizedTarget;
    mutable Vector<char16_t> m_normalizedMatch;
    UStringSearch* m_searcher { nullptr };
};

char16_t foldQuoteMarkAndReplaceNoBreakSpace(char16_t);
WEBCORE_EXPORT String foldQuoteMarks(const String&);

} // namespace WebCore
