/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unicode/utf16.h>
#include <unicode/utf8.h>
#include <wtf/text/StringView.h>

namespace WebKit {

// The unit an input method counts the surrounding text in when it asks for a deletion.
enum class InputMethodDeleteSurroundingUnit : uint8_t {
    UTF16,
    CodePoints,
    UTF8,
};

// A range of the surrounding text, in UTF-16 code units.
struct InputMethodSurroundingRange {
    unsigned start { 0 };
    unsigned end { 0 };
};

// Returns the range of text deleted when an input method asks to delete count units starting offset units from the
// cursor, or std::nullopt when nothing is deleted and the cursor doesn't move.
//
// UTF-16 code units are what WebPage::deleteSurrounding() expects, so the range is the one it deletes: nothing when the
// range starts before the text, and characters outside the Basic Multilingual Plane are deleted as a whole.
// Code points and UTF-8 bytes are converted like GTK text widgets do: the range is clamped to the text and extended to
// whole characters.
inline std::optional<InputMethodSurroundingRange> inputMethodSurroundingDeletionRange(StringView text, unsigned cursor, InputMethodDeleteSurroundingUnit unit, int64_t offset, uint64_t count)
{
    unsigned length = text.length();
    cursor = std::min(cursor, length);

    // Returns the number of UTF-8 bytes of the character at index, and moves index to the next one.
    auto nextCharacterUTF8Length = [&](unsigned& index) -> int64_t {
        char32_t character;
        U16_NEXT(text, index, length, character);
        return U8_LENGTH(character);
    };

    switch (unit) {
    case InputMethodDeleteSurroundingUnit::UTF16: {
        int64_t start = static_cast<int64_t>(cursor) + offset;
        if (start < 0)
            return std::nullopt;
        unsigned startIndex = std::min<int64_t>(start, length);
        unsigned endIndex = std::min<uint64_t>(static_cast<uint64_t>(startIndex) + count, length);
        if (startIndex > 0 && startIndex < length && U16_IS_LEAD(text[startIndex - 1]) && U16_IS_TRAIL(text[startIndex]))
            --startIndex;
        if (endIndex > 0 && endIndex < length && U16_IS_LEAD(text[endIndex - 1]) && U16_IS_TRAIL(text[endIndex]))
            ++endIndex;
        return InputMethodSurroundingRange { startIndex, std::max(startIndex, endIndex) };
    }
    case InputMethodDeleteSurroundingUnit::CodePoints: {
        if (!count)
            return std::nullopt;
        auto move = [&](int64_t codePoints) {
            unsigned index = cursor;
            for (; codePoints < 0 && index > 0; ++codePoints)
                U16_BACK_1(text, 0, index);
            for (; codePoints > 0 && index < length; --codePoints)
                U16_FWD_1(text, index, length);
            return index;
        };
        InputMethodSurroundingRange range { move(offset), move(offset + static_cast<int64_t>(count)) };
        if (range.start == range.end)
            return std::nullopt;
        return range;
    }
    case InputMethodDeleteSurroundingUnit::UTF8: {
        if (!count)
            return std::nullopt;
        uint64_t cursorBytes = 0;
        for (unsigned index = 0; index < cursor;)
            cursorBytes += nextCharacterUTF8Length(index);
        // Returns the index of the character that contains the byte, or of the next one if roundUp is true and the
        // byte is not its first one.
        auto indexForBytes = [&](int64_t bytes, bool roundUp) {
            if (bytes <= 0)
                return 0u;
            int64_t bytesBeforeNextIndex = 0;
            for (unsigned index = 0; index < length;) {
                unsigned nextIndex = index;
                bytesBeforeNextIndex += nextCharacterUTF8Length(nextIndex);
                if (bytesBeforeNextIndex == bytes)
                    return nextIndex;
                if (bytesBeforeNextIndex > bytes)
                    return roundUp ? nextIndex : index;
                index = nextIndex;
            }
            return length;
        };
        int64_t start = static_cast<int64_t>(cursorBytes) + offset;
        InputMethodSurroundingRange range { indexForBytes(start, false), indexForBytes(start + static_cast<int64_t>(count), true) };
        if (range.start >= range.end)
            return std::nullopt;
        return range;
    }
    }

    return std::nullopt;
}

} // namespace WebKit
