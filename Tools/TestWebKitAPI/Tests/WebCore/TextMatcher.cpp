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

#include "config.h"
#include <WebCore/FindOptions.h>
#include <WebCore/TextMatcher.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WebCore;

static Vector<CharacterRange> matchesIn(const String& target, FindOptions options, const String& text, std::optional<size_t> startOffset = std::nullopt)
{
    TextMatcher matcher { target, options };
    Vector<CharacterRange> matches;
    matcher.forEachMatch(text, startOffset.value_or(options.contains(FindOption::Backwards) ? text.length() : 0), [&](CharacterRange match) {
        matches.append(match);
        return IterationStatus::Continue;
    });
    return matches;
}

static Vector<uint64_t> matchOffsetsIn(const String& target, FindOptions options, const String& text, std::optional<size_t> startOffset = std::nullopt)
{
    return matchesIn(target, options, text, startOffset).map([](auto& match) {
        return match.location;
    });
}

TEST(TextMatcher, FindsEveryOccurrence)
{
    EXPECT_EQ(matchOffsetsIn("brown"_s, { }, "the brown fox and the brown dog"_s), (Vector<uint64_t> { 4, 22 }));
}

TEST(TextMatcher, ReportsMatchLength)
{
    auto matches = matchesIn("brown"_s, { }, "the brown fox"_s);
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0].location, 4u);
    EXPECT_EQ(matches[0].length, 5u);
}

TEST(TextMatcher, EmptyTargetMatchesNothing)
{
    EXPECT_TRUE(matchesIn(""_s, { }, "the brown fox"_s).isEmpty());
}

TEST(TextMatcher, EmptyTextMatchesNothing)
{
    EXPECT_TRUE(matchesIn("brown"_s, { }, ""_s).isEmpty());
}

TEST(TextMatcher, StartOffsetSkipsEarlierMatches)
{
    EXPECT_EQ(matchOffsetsIn("brown"_s, { }, "the brown fox and the brown dog"_s, 5), (Vector<uint64_t> { 22 }));
}

TEST(TextMatcher, CaseSensitiveByDefault)
{
    EXPECT_TRUE(matchesIn("Brown"_s, { }, "the brown fox"_s).isEmpty());
}

TEST(TextMatcher, CaseInsensitiveOptionIgnoresCase)
{
    EXPECT_EQ(matchOffsetsIn("Brown"_s, { FindOption::CaseInsensitive }, "the brown fox"_s), (Vector<uint64_t> { 4 }));
}

TEST(TextMatcher, BackwardsReportsMatchesInReverse)
{
    EXPECT_EQ(matchOffsetsIn("brown"_s, { FindOption::Backwards }, "the brown fox and the brown dog"_s), (Vector<uint64_t> { 22, 4 }));
}

TEST(TextMatcher, AtWordStartsRejectsMidWordMatch)
{
    EXPECT_TRUE(matchesIn("Kit"_s, { FindOption::AtWordStarts }, "WebKit"_s).isEmpty());
}

TEST(TextMatcher, AtWordStartsAcceptsWordStart)
{
    EXPECT_EQ(matchOffsetsIn("kit"_s, { FindOption::AtWordStarts }, "a kit bag"_s), (Vector<uint64_t> { 2 }));
}

TEST(TextMatcher, AtWordStartsAcceptsOffsetZero)
{
    EXPECT_EQ(matchOffsetsIn("kit"_s, { FindOption::AtWordStarts }, "kit bag"_s), (Vector<uint64_t> { 0 }));
}

TEST(TextMatcher, AtWordEndsRejectsMatchNotEndingAWord)
{
    EXPECT_TRUE(matchesIn("fo"_s, { FindOption::AtWordEnds }, "the fox"_s).isEmpty());
}

TEST(TextMatcher, AtWordEndsAcceptsMatchEndingAWord)
{
    EXPECT_EQ(matchOffsetsIn("fox"_s, { FindOption::AtWordEnds }, "the fox"_s), (Vector<uint64_t> { 4 }));
}

TEST(TextMatcher, SeparatorPrefixedTargetDropsAtWordStarts)
{
    TextMatcher matcher { ".org"_s, { FindOption::AtWordStarts } };
    EXPECT_FALSE(matcher.options().contains(FindOption::AtWordStarts));
}

TEST(TextMatcher, NonSeparatorPrefixedTargetKeepsAtWordStarts)
{
    TextMatcher matcher { "org"_s, { FindOption::AtWordStarts } };
    EXPECT_TRUE(matcher.options().contains(FindOption::AtWordStarts));
}

TEST(TextMatcher, MedialCapitalTreatsUppercaseRunStartAsWordStart)
{
    EXPECT_EQ(matchOffsetsIn("Kit"_s, { FindOption::AtWordStarts, FindOption::TreatMedialCapitalAsWordStart }, "WebKit"_s), (Vector<uint64_t> { 3 }));
}

TEST(TextMatcher, MedialCapitalTreatsLastOfUppercaseRunAsWordStart)
{
    EXPECT_EQ(matchOffsetsIn("Request"_s, { FindOption::AtWordStarts, FindOption::TreatMedialCapitalAsWordStart }, "XMLHTTPRequest"_s), (Vector<uint64_t> { 7 }));
}

TEST(TextMatcher, MedialCapitalTreatsDigitRunStartAsWordStart)
{
    EXPECT_EQ(matchOffsetsIn("2"_s, { FindOption::AtWordStarts, FindOption::TreatMedialCapitalAsWordStart }, "WebKit2"_s), (Vector<uint64_t> { 6 }));
}

TEST(TextMatcher, MedialCapitalTreatsRunAfterSeparatorAsWordStart)
{
    EXPECT_EQ(matchOffsetsIn("org"_s, { FindOption::AtWordStarts, FindOption::TreatMedialCapitalAsWordStart }, "webkit.org"_s), (Vector<uint64_t> { 7 }));
}

TEST(TextMatcher, MedialCapitalDoesNotTreatLowercaseRunAfterUppercaseAsWordStart)
{
    EXPECT_TRUE(matchesIn("ore"_s, { FindOption::AtWordStarts, FindOption::TreatMedialCapitalAsWordStart }, "WebCore"_s).isEmpty());
}

TEST(TextMatcher, CurlyApostropheInTargetMatchesStraightApostrophe)
{
    EXPECT_EQ(matchOffsetsIn(String::fromUTF8("don\xE2\x80\x99t"), { }, "I don't know"_s), (Vector<uint64_t> { 2 }));
}

TEST(TextMatcher, CurlyDoubleQuoteInTargetMatchesStraightDoubleQuote)
{
    EXPECT_EQ(matchOffsetsIn(String::fromUTF8("\xE2\x80\x9Chi\xE2\x80\x9D"), { }, "she said \"hi\" once"_s), (Vector<uint64_t> { 9 }));
}

TEST(TextMatcher, FoldQuoteMarksIsLengthPreserving)
{
    auto original = String::fromUTF8("\xE2\x80\x9Cquoted\xE2\x80\x9D and \xE2\x80\x98single\xE2\x80\x99");
    EXPECT_EQ(foldQuoteMarks(original).length(), original.length());
}

TEST(TextMatcher, KanaVoicedSoundMarkDifferenceIsNotAMatch)
{
    EXPECT_TRUE(matchesIn(String::fromUTF8("\xE3\x83\x8F"), { FindOption::CaseInsensitive }, String::fromUTF8("\xE3\x83\x90")).isEmpty());
}

TEST(TextMatcher, KanaSmallLetterDifferenceIsNotAMatch)
{
    EXPECT_TRUE(matchesIn(String::fromUTF8("\xE3\x83\x84"), { FindOption::CaseInsensitive }, String::fromUTF8("\xE3\x83\x83")).isEmpty());
}

TEST(TextMatcher, IdenticalKanaIsAMatch)
{
    EXPECT_EQ(matchOffsetsIn(String::fromUTF8("\xE3\x83\x8F"), { FindOption::CaseInsensitive }, String::fromUTF8("\xE3\x83\x8F")), (Vector<uint64_t> { 0 }));
}

TEST(TextMatcher, FindsSubstringInLongThaiRun)
{
    StringBuilder builder;
    for (unsigned i = 0; i < 500; ++i)
        builder.append(String::fromUTF8("\xE0\xB8\x81\xE0\xB8\xB2\xE0\xB8\xA3"));
    builder.append(String::fromUTF8("\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2"));

    auto matches = matchesIn(String::fromUTF8("\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2"), { }, builder.toString());
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0].location, 1500u);
}

TEST(TextMatcher, AtWordStartsInLongThaiRunDoesNotCrash)
{
    StringBuilder builder;
    for (unsigned i = 0; i < 500; ++i)
        builder.append(String::fromUTF8("\xE0\xB8\x81\xE0\xB8\xB2\xE0\xB8\xA3"));
    builder.append(String::fromUTF8("\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2"));

    matchesIn(String::fromUTF8("\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2"), { FindOption::AtWordStarts }, builder.toString());
}

TEST(TextMatcher, ForEachCandidateReportsMatchesRejectedByRules)
{
    TextMatcher matcher { "Kit"_s, { FindOption::AtWordStarts } };
    auto text = String("WebKit"_s);
    StringView::UpconvertedCharacters upconverted { StringView(text) };

    Vector<CharacterRange> candidates;
    matcher.forEachCandidate(upconverted.span(), 0, [&](CharacterRange candidate) {
        candidates.append(candidate);
        return IterationStatus::Continue;
    });

    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates[0].location, 3u);
    EXPECT_FALSE(matcher.isAcceptableMatch(upconverted.span(), candidates[0]));
}

TEST(TextMatcher, IsAcceptableMatchAcceptsWordStart)
{
    TextMatcher matcher { "kit"_s, { FindOption::AtWordStarts } };
    auto text = String("a kit bag"_s);
    StringView::UpconvertedCharacters upconverted { StringView(text) };
    EXPECT_TRUE(matcher.isAcceptableMatch(upconverted.span(), CharacterRange { 2, 3 }));
}

} // namespace TestWebKitAPI
