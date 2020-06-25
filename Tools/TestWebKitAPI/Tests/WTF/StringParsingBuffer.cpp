/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "WTFStringUtilities.h"

#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringView.h>

namespace TestWebKitAPI {

TEST(WTF, StringParsingBufferEmpty)
{
    StringParsingBuffer<LChar> parsingBuffer;

    EXPECT_TRUE(parsingBuffer.atEnd());
    EXPECT_FALSE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(parsingBuffer.position(), nullptr);
    EXPECT_EQ(parsingBuffer.end(), nullptr);
    EXPECT_EQ(parsingBuffer.lengthRemaining(), 0u);

}

TEST(WTF, StringParsingBufferInitial)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };

    EXPECT_FALSE(parsingBuffer.atEnd());
    EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(parsingBuffer.position(), string.characters8());
    EXPECT_EQ(parsingBuffer.end(), string.characters8() + string.length());
    EXPECT_EQ(parsingBuffer.lengthRemaining(), 3u);
    EXPECT_EQ(*parsingBuffer, 'a');
}

TEST(WTF, StringParsingBufferAdvance)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };

    parsingBuffer.advance();
    EXPECT_FALSE(parsingBuffer.atEnd());
    EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(parsingBuffer.lengthRemaining(), 2u);
    EXPECT_EQ(*parsingBuffer, 'b');
}

TEST(WTF, StringParsingBufferAdvanceBy)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };

    parsingBuffer.advanceBy(2);
    EXPECT_FALSE(parsingBuffer.atEnd());
    EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(parsingBuffer.lengthRemaining(), 1u);
    EXPECT_EQ(*parsingBuffer, 'c');
}

TEST(WTF, StringParsingBufferPreIncrement)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };

    auto preIncrementedParsingBuffer = ++parsingBuffer;
    EXPECT_FALSE(parsingBuffer.atEnd());
    EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(parsingBuffer.lengthRemaining(), 2u);
    EXPECT_EQ(*parsingBuffer, 'b');
    EXPECT_FALSE(preIncrementedParsingBuffer.atEnd());
    EXPECT_TRUE(preIncrementedParsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(preIncrementedParsingBuffer.lengthRemaining(), 2u);
    EXPECT_EQ(*preIncrementedParsingBuffer, 'b');
}

TEST(WTF, StringParsingBufferPostIncrement)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };

    auto postIncrementedParsingBuffer = parsingBuffer++;
    EXPECT_FALSE(parsingBuffer.atEnd());
    EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(parsingBuffer.lengthRemaining(), 2u);
    EXPECT_EQ(*parsingBuffer, 'b');
    EXPECT_FALSE(postIncrementedParsingBuffer.atEnd());
    EXPECT_TRUE(postIncrementedParsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(postIncrementedParsingBuffer.lengthRemaining(), 3u);
    EXPECT_EQ(*postIncrementedParsingBuffer, 'a');
}

TEST(WTF, StringParsingBufferPlusEquals)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };

    parsingBuffer += 2;
    EXPECT_FALSE(parsingBuffer.atEnd());
    EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(parsingBuffer.lengthRemaining(), 1u);
    EXPECT_EQ(*parsingBuffer, 'c');
}

TEST(WTF, StringParsingBufferEnd)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };

    ++parsingBuffer;
    EXPECT_FALSE(parsingBuffer.atEnd());
    EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(*parsingBuffer, 'b');

    ++parsingBuffer;
    EXPECT_FALSE(parsingBuffer.atEnd());
    EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
    EXPECT_EQ(*parsingBuffer, 'c');

    ++parsingBuffer;
    EXPECT_TRUE(parsingBuffer.atEnd());
    EXPECT_FALSE(parsingBuffer.hasCharactersRemaining());
}

TEST(WTF, StringParsingBufferSubscript)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };
    
    ++parsingBuffer;
    EXPECT_EQ(parsingBuffer[0], 'b');
    EXPECT_EQ(parsingBuffer[1], 'c');
}

TEST(WTF, StringParsingBufferStringView)
{
    StringView string { "abc" };
    StringParsingBuffer<LChar> parsingBuffer { string.characters8(), string.length() };

    ++parsingBuffer;
    auto viewRemaining = parsingBuffer.stringViewOfCharactersRemaining();
    EXPECT_EQ(viewRemaining.length(), 2u);
    EXPECT_EQ(viewRemaining[0], 'b');
    EXPECT_EQ(viewRemaining[1], 'c');
}

TEST(WTF, StringParsingBufferReadCharactersForParsing)
{
    auto latin1 = StringView { "abc" };
    auto result1 = WTF::readCharactersForParsing(latin1, [](auto parsingBuffer) {
        EXPECT_FALSE(parsingBuffer.atEnd());
        EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
        EXPECT_EQ(parsingBuffer.lengthRemaining(), 3u);
        EXPECT_EQ(*parsingBuffer, 'a');
    
        ++parsingBuffer;
        return parsingBuffer.lengthRemaining();
    });
    EXPECT_EQ(result1, 2u);

    auto utf16 = utf16String(u"😍😉🙃");
    auto result2 = WTF::readCharactersForParsing(utf16, [](auto parsingBuffer) {
        EXPECT_FALSE(parsingBuffer.atEnd());
        EXPECT_TRUE(parsingBuffer.hasCharactersRemaining());
        EXPECT_EQ(parsingBuffer.lengthRemaining(), 6u);
        EXPECT_EQ(*parsingBuffer, 0xD83D);

        ++parsingBuffer;
        return parsingBuffer.lengthRemaining();
    });
    EXPECT_EQ(result2, 5u);
}

}

