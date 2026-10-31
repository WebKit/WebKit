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

#include "config.h"
#include "WebKit/UIProcess/API/gtk/InputMethodDeleteSurroundingGtk.h"

#include "Helpers/Test.h"
#include <string_view>

namespace TestWebKitAPI {

static void testDeleteSurrounding(std::u8string_view text, unsigned cursorIndex, unsigned selectionIndex, int offset, int characterCount, int expectedOffset, unsigned expectedLength)
{
    auto range = WebKit::deleteSurroundingInUTF16(std::span { text }, cursorIndex, selectionIndex, offset, characterCount);
    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->first, expectedOffset);
    EXPECT_EQ(range->second, expectedLength);
}

TEST(WebKitGTK, InputMethodDeleteSurroundingASCII)
{
    testDeleteSurrounding(u8"abc", 3, 3, -1, 1, -1, 1);
    testDeleteSurrounding(u8"abc", 0, 0, 0, 2, 0, 2);
    testDeleteSurrounding(u8"abc", 1, 1, -1, 2, -1, 2);
}

TEST(WebKitGTK, InputMethodDeleteSurroundingNonASCII)
{
    testDeleteSurrounding(u8"a\u00e9", 3, 3, -1, 1, -1, 1);
    testDeleteSurrounding(u8"a\U0001F600", 5, 5, -1, 1, -2, 2);
    testDeleteSurrounding(u8"\U0001F600a", 5, 5, -2, 2, -3, 3);
    testDeleteSurrounding(u8"a\U0001F600", 1, 1, -1, 2, -1, 3);
    testDeleteSurrounding(u8"x\U0001F600y", 0, 0, 1, 1, 1, 2);
}

TEST(WebKitGTK, InputMethodDeleteSurroundingMoveCursor)
{
    testDeleteSurrounding(u8"a\U0001F600", 5, 5, -1, 0, -2, 0);
}

TEST(WebKitGTK, InputMethodDeleteSurroundingSelection)
{
    testDeleteSurrounding(u8"ab\U0001F600", 6, 0, -1, 1, 2, 2);
    testDeleteSurrounding(u8"ab\U0001F600", 2, 6, -1, 1, -1, 1);
}

TEST(WebKitGTK, InputMethodDeleteSurroundingOutsideSurroundingText)
{
    testDeleteSurrounding(u8"ab", 2, 2, -4, 4, -4, 4);
}

TEST(WebKitGTK, InputMethodDeleteSurroundingNegativeCount)
{
    EXPECT_FALSE(WebKit::deleteSurroundingInUTF16(std::span { std::u8string_view { u8"abc" } }, 3, 3, -1, -1).has_value());
}

} // namespace TestWebKitAPI
