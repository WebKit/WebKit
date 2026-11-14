/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKit/UIProcess/API/glib/InputMethodSurrounding.h"

#include "Helpers/Test.h"
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using WebKit::InputMethodDeleteSurroundingUnit;

static void testDeletion(const char* text, unsigned cursor, InputMethodDeleteSurroundingUnit unit, int64_t offset, uint64_t count, std::optional<std::pair<unsigned, unsigned>> expected)
{
    auto range = WebKit::inputMethodSurroundingDeletionRange(String::fromUTF8(text), cursor, unit, offset, count);
    ASSERT_EQ(range.has_value(), expected.has_value());
    if (!range)
        return;
    EXPECT_EQ(range->start, expected->first);
    EXPECT_EQ(range->end, expected->second);
}

TEST(WebKitGLib, InputMethodSurroundingDeletionUTF16)
{
    auto unit = InputMethodDeleteSurroundingUnit::UTF16;
    testDeletion("abc", 3, unit, -1, 1, { { 2, 3 } });
    testDeletion("abc", 0, unit, 1, 1, { { 1, 2 } });
    // Moving the cursor.
    testDeletion("abc", 3, unit, -2, 0, { { 1, 1 } });
    // Nothing is deleted when the range starts before the text.
    testDeletion("abc", 1, unit, -2, 2, std::nullopt);
    // The end is clamped to the text.
    testDeletion("abc", 2, unit, 0, 5, { { 2, 3 } });
    // Surrogate pairs are deleted as a whole.
    testDeletion("a\U0001F600", 3, unit, -1, 1, { { 1, 3 } });
    testDeletion("\U0001F600a", 0, unit, 0, 1, { { 0, 2 } });
}

TEST(WebKitGLib, InputMethodSurroundingDeletionCodePoints)
{
    auto unit = InputMethodDeleteSurroundingUnit::CodePoints;
    testDeletion("abc", 3, unit, -1, 1, { { 2, 3 } });
    testDeletion("a\U0001F600", 3, unit, -1, 1, { { 1, 3 } });
    testDeletion("\U0001F600a", 3, unit, -2, 2, { { 0, 3 } });
    testDeletion("x\U0001F600y", 0, unit, 1, 1, { { 1, 3 } });
    // The range is clamped to the text.
    testDeletion("ab", 2, unit, -4, 4, { { 0, 2 } });
    testDeletion("ab", 0, unit, 0, 4, { { 0, 2 } });
    testDeletion("ab", 2, unit, 0, 4, std::nullopt);
    testDeletion("", 0, unit, -1, 1, std::nullopt);
    testDeletion("abc", 3, unit, -1, 0, std::nullopt);
}

TEST(WebKitGLib, InputMethodSurroundingDeletionUTF8Bytes)
{
    auto unit = InputMethodDeleteSurroundingUnit::UTF8;
    testDeletion("abc", 3, unit, -1, 1, { { 2, 3 } });
    // Two bytes, one UTF-16 code unit.
    testDeletion("aé", 2, unit, -2, 2, { { 1, 2 } });
    // Four bytes, two UTF-16 code units.
    testDeletion("a\U0001F600", 3, unit, -4, 4, { { 1, 3 } });
    testDeletion("\U0001F600b", 0, unit, 0, 4, { { 0, 2 } });
    testDeletion("\U0001F600a\U0001F600", 5, unit, -9, 9, { { 0, 5 } });
    // The range is extended to whole characters.
    testDeletion("a\U0001F600", 3, unit, -1, 1, { { 1, 3 } });
    testDeletion("\U0001F600b", 0, unit, 0, 2, { { 0, 2 } });
    testDeletion("aéb", 3, unit, -2, 1, { { 1, 2 } });
    // The range is clamped to the text.
    testDeletion("ab", 2, unit, -4, 4, { { 0, 2 } });
    testDeletion("ab", 0, unit, 0, 4, { { 0, 2 } });
    testDeletion("ab", 2, unit, 0, 4, std::nullopt);
    testDeletion("", 0, unit, -1, 1, std::nullopt);
    testDeletion("abc", 3, unit, -1, 0, std::nullopt);
}

} // namespace TestWebKitAPI
