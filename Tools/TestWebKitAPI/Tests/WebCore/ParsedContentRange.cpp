/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <WebCore/ParsedContentRange.h>
#include <WebCore/ParsedRequestRange.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(WebCore, ParsedContentRangeFromString)
{
    // Basic parsing
    ASSERT_TRUE(ParsedContentRange("bytes 0-1/2"_s).isValid());
    ASSERT_TRUE(ParsedContentRange("bytes 0-1/*"_s).isValid());
    ASSERT_EQ(0, ParsedContentRange("bytes 0-1/2"_s).firstBytePosition());
    ASSERT_EQ(1, ParsedContentRange("bytes 0-1/2"_s).lastBytePosition());
    ASSERT_EQ(2, ParsedContentRange("bytes 0-1/2"_s).instanceLength());
    ASSERT_EQ(ParsedContentRange::unknownLength, ParsedContentRange("bytes 0-1/*"_s).instanceLength());

    // Whitespace errors
    ASSERT_FALSE(ParsedContentRange("bytes  0-1/*"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0 -1/*"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0- 1/*"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-1 /*"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-1/ *"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-1/* "_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-1/ 2"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-1/2 "_s).isValid());

    // Non-digit errors
    ASSERT_FALSE(ParsedContentRange("bytes abcd-1/2"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-abcd/2"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-1/abcd"_s).isValid());

    // Range requirement errors
    ASSERT_FALSE(ParsedContentRange("bytes 1-0/2"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-2/1"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 2/0-1"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("abcd 0/1-2"_s).isValid());

    // Negative value errors
    ASSERT_FALSE(ParsedContentRange("bytes -0-1/*"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes -1/*"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0--0/2"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 0-1/-2"_s).isValid());

    // Edge cases
    ASSERT_TRUE(ParsedContentRange("bytes 9223372036854775805-9223372036854775806/9223372036854775807"_s).isValid());
    ASSERT_FALSE(ParsedContentRange("bytes 9223372036854775808-9223372036854775809/9223372036854775810"_s).isValid());
}

TEST(WebCore, ParsedContentRangeFromValues)
{
    ASSERT_TRUE(ParsedContentRange(0, 1, 2).isValid());
    ASSERT_TRUE(ParsedContentRange(0, 1, ParsedContentRange::unknownLength).isValid());
    ASSERT_FALSE(ParsedContentRange().isValid());
    ASSERT_FALSE(ParsedContentRange(1, 0, 2).isValid());
    ASSERT_FALSE(ParsedContentRange(0, 2, 1).isValid());
    ASSERT_FALSE(ParsedContentRange(0, 0, 0).isValid());
    ASSERT_FALSE(ParsedContentRange(-1, 1, 2).isValid());
    ASSERT_FALSE(ParsedContentRange(0, -1, 2).isValid());
    ASSERT_FALSE(ParsedContentRange(0, 1, -2).isValid());
    ASSERT_FALSE(ParsedContentRange(-2, -1, 2).isValid());
}

TEST(WebCore, ParsedContentRangeToString)
{
    ASSERT_STREQ("bytes 0-1/2", ParsedContentRange(0, 1, 2).headerValue().utf8().data());
    ASSERT_STREQ("bytes 0-1/*", ParsedContentRange(0, 1, ParsedContentRange::unknownLength).headerValue().utf8().data());
    ASSERT_STREQ("", ParsedContentRange().headerValue().utf8().data());
}

TEST(WebCore, ParsedRequestRange)
{
    Vector<String> failureCases {
        { },
        emptyString(),
        "abc"_s,
        "bytes="_s,
        "bytes=-"_s,
        "bytes=abc-"_s,
        "bytes=1-abc"_s,
        "bytes=2-1"_s,
        "bytes=1-"_s,
        "bytes=-1"_s,
        "bytes=1-999999999999999999999999"_s
    };
    for (const auto& input : failureCases)
        EXPECT_EQ(std::nullopt, ParsedRequestRange::parse(input));

    auto compare = [] (const String& input, std::optional<size_t> begin, std::optional<size_t> end) {
        auto range = ParsedRequestRange::parse(input);
        EXPECT_NE(std::nullopt, range);
        
    };
    compare("bytes=1-1"_s, 1, 1);
    compare("bytes=1-2"_s, 1, 2);
}

}
