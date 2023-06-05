/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include <wtf/text/TextBreakIterator.h>

#if USE(CF)
#include <wtf/text/cf/TextBreakIteratorCF.h>
#endif

namespace TestWebKitAPI {

static String makeUTF16(std::vector<UChar> input)
{
    return { input.data(), static_cast<unsigned>(input.size()) };
}

TEST(WTF_TextBreakIterator, NumGraphemeClusters)
{
    EXPECT_EQ(0U, numGraphemeClusters(StringView { }));
    EXPECT_EQ(0U, numGraphemeClusters(StringView { ""_s }));
    EXPECT_EQ(0U, numGraphemeClusters(makeUTF16({ })));

    EXPECT_EQ(1U, numGraphemeClusters(StringView { "a"_s }));
    EXPECT_EQ(1U, numGraphemeClusters(makeUTF16({ 'a' })));
    EXPECT_EQ(1U, numGraphemeClusters(StringView { "\r\n"_s }));
    EXPECT_EQ(1U, numGraphemeClusters(StringView { "\n"_s }));
    EXPECT_EQ(1U, numGraphemeClusters(StringView { "\r"_s }));
    EXPECT_EQ(1U, numGraphemeClusters(makeUTF16({ '\r', '\n' })));
    EXPECT_EQ(1U, numGraphemeClusters(makeUTF16({ '\n' })));
    EXPECT_EQ(1U, numGraphemeClusters(makeUTF16({ '\r' })));

    EXPECT_EQ(2U, numGraphemeClusters(StringView { "\n\r"_s }));
    EXPECT_EQ(2U, numGraphemeClusters(makeUTF16({ '\n', '\r' })));

    EXPECT_EQ(2U, numGraphemeClusters(StringView { "\r\n\r"_s }));
    EXPECT_EQ(2U, numGraphemeClusters(makeUTF16({ '\r', '\n', '\r' })));

    EXPECT_EQ(1U, numGraphemeClusters(makeUTF16({ 'g', 0x308 })));
    EXPECT_EQ(1U, numGraphemeClusters(makeUTF16({ 0x1100, 0x1161, 0x11A8 })));
    EXPECT_EQ(1U, numGraphemeClusters(makeUTF16({ 0x0BA8, 0x0BBF })));

    EXPECT_EQ(2U, numGraphemeClusters(makeUTF16({ 0x308, 'g' })));

    EXPECT_EQ(3U, numGraphemeClusters(StringView { "\r\nbc"_s }));
    EXPECT_EQ(3U, numGraphemeClusters(makeUTF16({ 'g', 0x308, 'b', 'c' })));
}

TEST(WTF_TextBreakIterator, NumCodeUnitsInGraphemeClusters)
{
    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { }, 0));
    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { }, 1));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { ""_s }, 0));
    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { ""_s }, 1));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(makeUTF16({ }), 0));
    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(makeUTF16({ }), 1));

    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(StringView { "a"_s }, 1));
    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'a' }), 1));
    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(StringView { "\n"_s }, 1));
    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(StringView { "\r"_s }, 1));
    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\n' }), 1));
    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r' }), 1));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { "abc"_s }, 0));
    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(StringView { "abc"_s }, 1));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(StringView { "abc"_s }, 2));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(StringView { "abc"_s }, 3));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(StringView { "abc"_s }, 4));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'a', 'b', 'c' }), 0));
    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'a', 'b', 'c' }), 1));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'a', 'b', 'c' }), 2));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'a', 'b', 'c' }), 3));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'a', 'b', 'c' }), 4));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { "\r\n"_s }, 0));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(StringView { "\r\n"_s }, 1));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(StringView { "\r\n"_s }, 2));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(StringView { "\r\n"_s }, 3));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r', '\n' }), 0));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r', '\n' }), 1));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r', '\n' }), 2));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r', '\n' }), 3));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { "\n\r"_s }, 0));
    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(StringView { "\n\r"_s }, 1));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(StringView { "\n\r"_s }, 2));

    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\n', '\r' }), 1));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\n', '\r' }), 2));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { "\r\n\r"_s }, 0));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(StringView { "\r\n\r"_s }, 1));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(StringView { "\r\n\r"_s }, 2));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(StringView { "\r\n\r"_s }, 3));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r', '\n', '\r' }), 0));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r', '\n', '\r' }), 1));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r', '\n', '\r' }), 2));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(makeUTF16({ '\r', '\n', '\r' }), 3));

    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'g', 0x308 }), 1));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(makeUTF16({ 0x1100, 0x1161, 0x11A8 }), 1));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ 0x0BA8, 0x0BBF }), 1));

    EXPECT_EQ(1U, numCodeUnitsInGraphemeClusters(makeUTF16({ 0x308, 'g' }), 1));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(StringView { "\r\nbc"_s }, 0));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(StringView { "\r\nbc"_s }, 1));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(StringView { "\r\nbc"_s }, 2));
    EXPECT_EQ(4U, numCodeUnitsInGraphemeClusters(StringView { "\r\nbc"_s }, 3));
    EXPECT_EQ(4U, numCodeUnitsInGraphemeClusters(StringView { "\r\nbc"_s }, 4));
    EXPECT_EQ(4U, numCodeUnitsInGraphemeClusters(StringView { "\r\nbc"_s }, 5));

    EXPECT_EQ(0U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'g', 0x308, 'b', 'c' }), 0));
    EXPECT_EQ(2U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'g', 0x308, 'b', 'c' }), 1));
    EXPECT_EQ(3U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'g', 0x308, 'b', 'c' }), 2));
    EXPECT_EQ(4U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'g', 0x308, 'b', 'c' }), 3));
    EXPECT_EQ(4U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'g', 0x308, 'b', 'c' }), 4));
    EXPECT_EQ(4U, numCodeUnitsInGraphemeClusters(makeUTF16({ 'g', 0x308, 'b', 'c' }), 5));
}

TEST(WTF_TextBreakIterator, Behaviors)
{
    {
        auto string = u"ぁぁ"_str;
        auto locale = AtomString("ja"_str);
        CachedTextBreakIterator strictIterator({ string }, nullptr, 0, TextBreakIterator::LineMode { TextBreakIterator::LineMode::Behavior::Strict }, locale);
        EXPECT_EQ(strictIterator.following(0), 2);
        CachedTextBreakIterator looseIterator({ string }, nullptr, 0, TextBreakIterator::LineMode { TextBreakIterator::LineMode::Behavior::Loose }, locale);
        EXPECT_EQ(looseIterator.following(0), 1);
    }
    {
        auto string = u"中〜"_str;
        auto locale = AtomString("zh-Hans"_str);
        CachedTextBreakIterator strictIterator({ string }, nullptr, 0, TextBreakIterator::LineMode { TextBreakIterator::LineMode::Behavior::Strict }, locale);
        EXPECT_EQ(strictIterator.following(0), 2);
        CachedTextBreakIterator looseIterator({ string }, nullptr, 0, TextBreakIterator::LineMode { TextBreakIterator::LineMode::Behavior::Loose }, locale);
        EXPECT_EQ(looseIterator.following(0), 1);
    }
}

TEST(WTF_TextBreakIterator, CachedLineBreakIteratorFactoryPriorContext)
{
    CachedLineBreakIteratorFactory::PriorContext priorContext;
    EXPECT_EQ(0U, priorContext.length());
    priorContext.set({ 'a', 'b' });
    EXPECT_EQ(2U, priorContext.length());
    CachedLineBreakIteratorFactory::PriorContext priorContext2;
    EXPECT_FALSE(priorContext == priorContext2);
    priorContext2.set({ 'a', 'b' });
    EXPECT_TRUE(priorContext == priorContext2);
    EXPECT_EQ('a', priorContext.characters()[0]);
    priorContext.set({ '\0', 'b' });
    EXPECT_EQ('b', priorContext.characters()[0]);
    priorContext.reset();
    EXPECT_EQ(0U, priorContext.length());
}

TEST(WTF_TextBreakIterator, ICULine)
{
    auto context = u"th"_str;
    auto string = u"is is some text"_str;
    ASSERT(!context.is8Bit());
    WTF::TextBreakIteratorICU iterator(string, context.characters16(), context.length(), WTF::TextBreakIteratorICU::LineMode { WTF::TextBreakIteratorICU::LineMode::Behavior::Default }, AtomString("en_US"_str));

    {
        unsigned wordBoundaries[] = { 3, 6, 11, 15 };
        unsigned index = 0;
        for (unsigned i = 0; i < string.length(); ++i) {
            auto result = iterator.following(i);
            if (i == wordBoundaries[index]) {
                EXPECT_TRUE(iterator.isBoundary(i));
                ++index;
            } else
                EXPECT_FALSE(iterator.isBoundary(i));
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.following(string.length()).has_value());
        EXPECT_FALSE(iterator.following(string.length() + 1).has_value());
    }
    {
        unsigned wordBoundaries[] = { 11, 6, 3, 0 };
        unsigned index = 0;
        for (unsigned i = string.length(); i > 0; --i) {
            auto result = iterator.preceding(i);
            if (i == wordBoundaries[index])
                ++index;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.preceding(0).has_value());
        auto result = iterator.preceding(string.length() + 1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, string.length());
    }
    {
        auto context = u"di"_str;
        auto string = u"ctionary order"_str;
        ASSERT(!context.is8Bit());
        iterator.setText(string, context.characters16(), context.length());
        auto result = iterator.following(1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, 9U);
    }
    {
        auto context = u"a "_str;
        auto string = u"word"_str;
        ASSERT(!context.is8Bit());
        iterator.setText(string, context.characters16(), context.length());
        ASSERT_TRUE(iterator.isBoundary(0));
    }
    {
        auto string = u"word"_str;
        iterator.setText(string, nullptr, 0);
        ASSERT_TRUE(iterator.isBoundary(0));
    }
}

TEST(WTF_TextBreakIterator, ICUCharacter)
{
    auto context = u"==>"_str;
    auto string = u" कि <=="_str;
    WTF::TextBreakIteratorICU iterator(string, context.characters16(), context.length(), WTF::TextBreakIteratorICU::CharacterMode { }, AtomString("hi_IN"_str));

    {
        unsigned wordBoundaries[] = { 1, 3, 4, 5, 6, 7 };
        unsigned index = 0;
        for (unsigned i = 0; i < string.length(); ++i) {
            auto result = iterator.following(i);
            if (i == wordBoundaries[index])
                ++index;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.following(string.length()).has_value());
        EXPECT_FALSE(iterator.following(string.length() + 1).has_value());

        EXPECT_TRUE(iterator.isBoundary(0));
        EXPECT_TRUE(iterator.isBoundary(1));
        EXPECT_FALSE(iterator.isBoundary(2));
        EXPECT_TRUE(iterator.isBoundary(3));
        EXPECT_TRUE(iterator.isBoundary(4));
        EXPECT_TRUE(iterator.isBoundary(5));
        EXPECT_TRUE(iterator.isBoundary(6));
        EXPECT_TRUE(iterator.isBoundary(7));
    }
}

TEST(WTF_TextBreakIterator, Line)
{
    auto context = u"th"_str;
    auto string = u"is is some text"_str;
    ASSERT(!context.is8Bit());
    WTF::CachedTextBreakIterator iterator(string, context.characters16(), context.length(), WTF::TextBreakIterator::LineMode { WTF::TextBreakIterator::LineMode::Behavior::Default }, AtomString("en_US"_str));

    {
        unsigned wordBoundaries[] = { 3, 6, 11, 15 };
        unsigned index = 0;
        for (unsigned i = 0; i < string.length(); ++i) {
            auto result = iterator.following(i);
            if (i == wordBoundaries[index]) {
                EXPECT_TRUE(iterator.isBoundary(i));
                ++index;
            } else
                EXPECT_FALSE(iterator.isBoundary(i));
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.following(string.length()).has_value());
        EXPECT_FALSE(iterator.following(string.length() + 1).has_value());
    }
    {
        unsigned wordBoundaries[] = { 11, 6, 3, 0 };
        unsigned index = 0;
        for (unsigned i = string.length(); i > 0; --i) {
            auto result = iterator.preceding(i);
            if (i == wordBoundaries[index])
                ++index;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.preceding(0).has_value());
        auto result = iterator.preceding(string.length() + 1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, string.length());
    }
}

#if USE(CF)

TEST(WTF_TextBreakIterator, CFWord)
{
    auto context = u"th"_str;
    auto string = u"is is some text"_str;
    WTF::TextBreakIteratorCF iterator(string, context, WTF::TextBreakIteratorCF::Mode::Word, AtomString("en_US"_str));

    {
        unsigned wordBoundaries[] = { 2, 5, 10, 15 };
        unsigned index = 0;
        bool shouldBeBoundary = false;
        for (unsigned i = 0; i < string.length(); ++i) {
            auto result = iterator.following(i);
            if (i == wordBoundaries[index]) {
                EXPECT_FALSE(result.has_value());
                EXPECT_TRUE(iterator.isBoundary(i));
                ++index;
                shouldBeBoundary = true;
                continue;
            }
            EXPECT_EQ(iterator.isBoundary(i), shouldBeBoundary);
            shouldBeBoundary = false;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.following(string.length()).has_value());
        EXPECT_FALSE(iterator.following(string.length() + 1).has_value());
    }
    {
        unsigned wordBoundaries[] = { 11, 6, 3, 0 };
        unsigned index = 0;
        for (unsigned i = string.length(); i > 0; --i) {
            auto result = iterator.preceding(i);
            if (i == wordBoundaries[index]) {
                EXPECT_FALSE(result.has_value());
                ++index;
                continue;
            }
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.preceding(0).has_value());
        auto result = iterator.preceding(string.length() + 1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, string.length());
    }
    {
        auto context = u"di"_str;
        auto string = u"ctionary order"_str;
        iterator.setText(string, context.characters16(), context.length());
        auto result = iterator.following(1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, 8U);
    }
}

TEST(WTF_TextBreakIterator, CFSentence)
{
    auto context = u"Th"_str;
    auto string = u"is is some text. And some more text."_str;
    WTF::TextBreakIteratorCF iterator(string, context, WTF::TextBreakIteratorCF::Mode::Sentence, AtomString("en_US"_str));
    auto result = iterator.following(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 17U);
}

TEST(WTF_TextBreakIterator, CFLine)
{
    auto context = u"th"_str;
    auto string = u"is is some text"_str;
    WTF::TextBreakIteratorCF iterator(string, context, WTF::TextBreakIteratorCF::Mode::LineBreak, AtomString("en_US"_str));

    {
        unsigned wordBoundaries[] = { 3, 6, 11, 15 };
        unsigned index = 0;
        for (unsigned i = 0; i < string.length(); ++i) {
            auto result = iterator.following(i);
            if (i == wordBoundaries[index]) {
                EXPECT_TRUE(iterator.isBoundary(i));
                ++index;
            } else
                EXPECT_FALSE(iterator.isBoundary(i));
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.following(string.length()).has_value());
        EXPECT_FALSE(iterator.following(string.length() + 1).has_value());
    }
    {
        unsigned wordBoundaries[] = { 11, 6, 3, 0 };
        unsigned index = 0;
        for (unsigned i = string.length(); i > 0; --i) {
            auto result = iterator.preceding(i);
            if (i == wordBoundaries[index])
                ++index;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.preceding(0).has_value());
        auto result = iterator.preceding(string.length() + 1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, string.length());
    }
    {
        auto context = u"di"_str;
        auto string = u"ctionary order"_str;
        iterator.setText(string, context.characters16(), context.length());
        auto result = iterator.following(1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, 9U);
    }
    {
        auto context = u"a "_str;
        auto string = u"word"_str;
        iterator.setText(string, context.characters16(), context.length());
        ASSERT_TRUE(iterator.isBoundary(0));
    }
    {
        auto string = u"word"_str;
        iterator.setText(string, nullptr, 0);
        ASSERT_TRUE(iterator.isBoundary(0));
    }
}

TEST(WTF_TextBreakIterator, CFWordBoundary)
{
    auto context = u"th"_str;
    auto string = u"is is some text"_str;
    WTF::TextBreakIteratorCF iterator(string, context, WTF::TextBreakIteratorCF::Mode::WordBoundary, AtomString("en_US"_str));

    {
        unsigned wordBoundaries[] = { 2, 3, 5, 6, 10, 11, 15 };
        unsigned index = 0;
        for (unsigned i = 0; i < string.length(); ++i) {
            auto result = iterator.following(i);
            if (i == wordBoundaries[index]) {
                EXPECT_TRUE(iterator.isBoundary(i));
                ++index;
            } else
                EXPECT_FALSE(iterator.isBoundary(i));
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.following(string.length()).has_value());
        EXPECT_FALSE(iterator.following(string.length() + 1).has_value());
    }
    {
        unsigned wordBoundaries[] = { 11, 10, 6, 5, 3, 2, 0 };
        unsigned index = 0;
        for (unsigned i = string.length(); i > 0; --i) {
            auto result = iterator.preceding(i);
            if (i == wordBoundaries[index])
                ++index;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.preceding(0).has_value());
        auto result = iterator.preceding(string.length() + 1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, string.length());
    }
    {
        auto context = u"di"_str;
        auto string = u"ctionary order"_str;
        iterator.setText(string, context.characters16(), context.length());
        auto result = iterator.following(1);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, 8U);
    }
}

TEST(WTF_TextBreakIterator, CFComposedCharacter)
{
    auto context = u"==>"_str;
    auto string = u" कि <=="_str;
    WTF::TextBreakIteratorCF iterator(string, context, WTF::TextBreakIteratorCF::Mode::ComposedCharacter, AtomString("hi_IN"_str));

    {
        unsigned wordBoundaries[] = { 1, 3, 4, 5, 6, 7 };
        unsigned index = 0;
        for (unsigned i = 0; i < string.length(); ++i) {
            auto result = iterator.following(i);
            if (i == wordBoundaries[index])
                ++index;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.following(string.length()).has_value());
        EXPECT_FALSE(iterator.following(string.length() + 1).has_value());

        EXPECT_TRUE(iterator.isBoundary(0));
        EXPECT_TRUE(iterator.isBoundary(1));
        EXPECT_FALSE(iterator.isBoundary(2));
        EXPECT_TRUE(iterator.isBoundary(3));
        EXPECT_TRUE(iterator.isBoundary(4));
        EXPECT_TRUE(iterator.isBoundary(5));
        EXPECT_TRUE(iterator.isBoundary(6));
        EXPECT_TRUE(iterator.isBoundary(7));
    }
}

TEST(WTF_TextBreakIterator, CFBackwardDeletion)
{
    auto context = u"==>"_str;
    auto string = u" कि <=="_str;
    WTF::TextBreakIteratorCF iterator(string, context, WTF::TextBreakIteratorCF::Mode::BackwardDeletion, AtomString("hi_IN"_str));

    {
        unsigned wordBoundaries[] = { 1, 2, 3, 4, 5, 6, 7 };
        unsigned index = 0;
        for (unsigned i = 0; i < string.length(); ++i) {
            auto result = iterator.following(i);
            if (i == wordBoundaries[index])
                ++index;
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, wordBoundaries[index]);
        }
        EXPECT_FALSE(iterator.following(string.length()).has_value());
        EXPECT_FALSE(iterator.following(string.length() + 1).has_value());

        EXPECT_TRUE(iterator.isBoundary(0));
        EXPECT_TRUE(iterator.isBoundary(1));
        EXPECT_TRUE(iterator.isBoundary(2));
        EXPECT_TRUE(iterator.isBoundary(3));
        EXPECT_TRUE(iterator.isBoundary(4));
        EXPECT_TRUE(iterator.isBoundary(5));
        EXPECT_TRUE(iterator.isBoundary(6));
        EXPECT_TRUE(iterator.isBoundary(7));
    }
}

#endif // USE(CF)

} // namespace TestWebKitAPI
