#include "config.h"
#include "CompressionDictionaryMatcher.h"
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <gtest/gtest.h>
#include <wtf/text/StringConcatenate.h>

using namespace WebCore;

TEST(CompressionDictionaryMatcherTest, ValidPatternNoRegexSameOrigin)
{
    URL dictURL("https://example.com/dict");
    String validPattern = "/path/to/resource";
    EXPECT_TRUE(isValidDictionaryPattern(validPattern, dictURL));
}

TEST(CompressionDictionaryMatcherTest, InvalidPatternWithRegex)
{
    URL dictURL("https://example.com/dict");
    String invalidPattern = "/path/(.*)"; // Contains regex
    EXPECT_FALSE(isValidDictionaryPattern(invalidPattern, dictURL));
}

TEST(CompressionDictionaryMatcherTest, MatchSucceeds)
{
    URL dictURL("https://example.com/dict");
    CompressionDictionary dict {
        .match = "/assets/file.js",
        .matchDest = { },
        .dictionaryURL = dictURL,
        .fetchTime = 123.456
    };

    ResourceRequest request;
    request.setURL(URL("https://example.com/assets/file.js"));
    EXPECT_TRUE(doesRequestMatchDictionary(request, dict));
}

TEST(CompressionDictionaryMatcherTest, MatchFailsDueToOrigin)
{
    URL dictURL("https://example.com/dict");
    CompressionDictionary dict {
        .match = "/assets/file.js",
        .matchDest = { },
        .dictionaryURL = dictURL,
        .fetchTime = 123.456
    };

    ResourceRequest request;
    request.setURL(URL("https://evil.com/assets/file.js"));
    EXPECT_FALSE(doesRequestMatchDictionary(request, dict));
}

TEST(CompressionDictionaryMatcherTest, SelectBestMatchingDictionary)
{
    URL dictURL1("https://example.com/dict1");
    URL dictURL2("https://example.com/dict2");

    ResourceRequest request;
    request.setURL(URL("https://example.com/assets/file.js"));
    request.setDestination("script");

    Vector<CompressionDictionary> candidates = {
        {
            .match = "/assets",
            .matchDest = { }, // No dest
            .dictionaryURL = dictURL1,
            .fetchTime = 100.0
        },
        {
            .match = "/assets/file.js",
            .matchDest = { "script" },
            .dictionaryURL = dictURL2,
            .fetchTime = 50.0 // older, but more specific
        }
    };

    auto maybeDict = selectBestMatchingDictionary(request, candidates);
    ASSERT_TRUE(maybeDict.has_value());
    EXPECT_EQ(maybeDict->dictionaryURL, dictURL2);
}
// Copyright © 2025  All rights reserved.

