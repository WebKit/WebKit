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

#include <WebCore/FindOptions.h>
#include <unicode/usearch.h>

namespace WebCore {

class ICUSearcher {
public:
    explicit ICUSearcher(const String& foldedTarget, FindOptions&);
    ~ICUSearcher();
    void setCollationStrength(UCollationStrength);
    void setAttribute(USearchAttribute, USearchAttributeValue);
    void setPattern(std::span<const char16_t>);
    void setText(std::span<const char16_t>);
    void setOffset(size_t);
    std::optional<size_t> next();
#if !PLATFORM(PLAYSTATION)
    std::optional<size_t> previous();
#endif
    size_t matchedLength();
private:
    UStringSearch* searcher();
    void reset();
    void NODELETE lock();
    void NODELETE unlock();
};

bool isBadMatch(std::span<const char16_t> match, std::span<const char16_t> normalizedTarget, Vector<char16_t>& scratchBuffer);
bool isWordStartMatch(std::span<const char16_t> buffer, size_t matchStart, size_t matchLength, FindOptions);
bool isWordEndMatch(std::span<const char16_t> buffer, size_t matchStart, size_t matchLength, FindOptions);
void normalizeCharacters(const char16_t* characters, unsigned length, Vector<char16_t>& buffer);
char16_t foldQuoteMarkAndReplaceNoBreakSpace(char16_t);
bool containsKanaLetters(const String& pattern);
bool isSeparator(char32_t character);
WEBCORE_EXPORT String foldQuoteMarks(const String&);

} // namespace WebCore
