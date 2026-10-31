/*
 * Copyright (C) 2026 Novatech LLC
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

#include <algorithm>
#include <optional>
#include <span>
#include <unicode/utf16.h>
#include <utility>
#include <wtf/MathExtras.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

inline unsigned utf16LengthOfCharactersBefore(StringView text, unsigned characterCount)
{
    if (text.is8Bit())
        return characterCount;

    auto characters = text.span16();
    size_t index = characters.size();
    unsigned length = 0;
    for (; characterCount && index; --characterCount) {
        size_t previous = index;
        U16_BACK_1(characters, 0, index);
        length += previous - index;
    }
    return length + characterCount;
}

inline unsigned utf16LengthOfCharactersAfter(StringView text, unsigned characterCount)
{
    if (text.is8Bit())
        return characterCount;

    auto characters = text.span16();
    size_t index = 0;
    unsigned length = 0;
    for (; characterCount && index < characters.size(); --characterCount) {
        size_t previous = index;
        U16_FWD_1(characters, index, characters.size());
        length += index - previous;
    }
    return length + characterCount;
}

inline std::optional<std::pair<int, unsigned>> deleteSurroundingInUTF16(std::span<const char8_t> surroundingText, unsigned cursorIndex, unsigned selectionIndex, int offset, int characterCount)
{
    if (characterCount < 0)
        return std::nullopt;

    size_t cursor = std::min<size_t>(cursorIndex, surroundingText.size());
    size_t selectionStart = std::min<size_t>(selectionIndex, cursor);
    auto beforeCursor = String::fromUTF8(surroundingText.first(cursor));
    auto afterCursor = String::fromUTF8(surroundingText.subspan(cursor));
    unsigned cursorFromSelectionStart = String::fromUTF8(surroundingText.subspan(selectionStart, cursor - selectionStart)).length();

    unsigned count = characterCount;
    int64_t start;
    unsigned length;
    if (offset < 0) {
        unsigned charactersBefore = static_cast<unsigned>(-static_cast<int64_t>(offset));
        unsigned deletedBefore = std::min(count, charactersBefore);
        unsigned startToCursor = utf16LengthOfCharactersBefore(beforeCursor, charactersBefore);
        unsigned endToCursor = utf16LengthOfCharactersBefore(beforeCursor, charactersBefore - deletedBefore);
        start = -static_cast<int64_t>(startToCursor);
        length = startToCursor - endToCursor + utf16LengthOfCharactersAfter(afterCursor, count - deletedBefore);
    } else {
        unsigned cursorToStart = utf16LengthOfCharactersAfter(afterCursor, offset);
        start = cursorToStart;
        length = utf16LengthOfCharactersAfter(afterCursor, static_cast<unsigned>(offset) + count) - cursorToStart;
    }

    return std::make_pair(clampTo<int>(start + cursorFromSelectionStart), length);
}

} // namespace WebKit
