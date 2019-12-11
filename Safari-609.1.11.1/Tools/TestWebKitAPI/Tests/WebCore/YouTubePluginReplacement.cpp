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
#include <WebCore/YouTubePluginReplacement.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>

using namespace WebCore;

namespace TestWebKitAPI {

class YouTubePluginReplacementTest : public testing::Test {
public:
    void SetUp() final {
        WTF::initializeMainThread();
    }
};

static bool test(const String& inputURLString, const String& expectedURLString)
{
    URL inputURL(URL(), inputURLString);
    String actualURLString = YouTubePluginReplacement::youTubeURLFromAbsoluteURL(inputURL, inputURLString);
    return actualURLString.utf8() == expectedURLString.utf8();
}

TEST_F(YouTubePluginReplacementTest, YouTubeURLFromAbsoluteURL)
{
    // YouTube non-video URL, not expected to be transformed.
    EXPECT_TRUE(test("https://www.youtube.com", "https://www.youtube.com"));

    // Basic YouTube video links, expected to be transformed.
    EXPECT_TRUE(test("https://www.youtube.com/v/dQw4w9WgXcQ", "https://www.youtube.com/embed/dQw4w9WgXcQ"));
    EXPECT_TRUE(test("http://www.youtube.com/v/dQw4w9WgXcQ", "http://www.youtube.com/embed/dQw4w9WgXcQ"));
    EXPECT_TRUE(test("https://youtube.com/v/dQw4w9WgXcQ", "https://youtube.com/embed/dQw4w9WgXcQ"));
    EXPECT_TRUE(test("http://youtube.com/v/dQw4w9WgXcQ", "http://youtube.com/embed/dQw4w9WgXcQ"));

    // With start time, preserved.
    EXPECT_TRUE(test("http://www.youtube.com/v/dQw4w9WgXcQ?start=4", "http://www.youtube.com/embed/dQw4w9WgXcQ?start=4"));
    EXPECT_TRUE(test("http://www.youtube.com/v/dQw4w9WgXcQ?start=4&fs=1", "http://www.youtube.com/embed/dQw4w9WgXcQ?start=4&fs=1"));

    // With an invalid query (see & instead of ?), we preserve and fix the query.
    EXPECT_TRUE(test("http://www.youtube.com/v/dQw4w9WgXcQ&start=4", "http://www.youtube.com/embed/dQw4w9WgXcQ?start=4"));
    EXPECT_TRUE(test("http://www.youtube.com/v/dQw4w9WgXcQ&start=4&fs=1", "http://www.youtube.com/embed/dQw4w9WgXcQ?start=4&fs=1"));

    // Non-Flash URL is untouched.
    EXPECT_TRUE(test("https://www.youtube.com/embed/dQw4w9WgXcQ", "https://www.youtube.com/embed/dQw4w9WgXcQ"));
    EXPECT_TRUE(test("http://www.youtube.com/embed/dQw4w9WgXcQ", "http://www.youtube.com/embed/dQw4w9WgXcQ"));
    EXPECT_TRUE(test("https://youtube.com/embed/dQw4w9WgXcQ", "https://youtube.com/embed/dQw4w9WgXcQ"));
    EXPECT_TRUE(test("http://youtube.com/embed/dQw4w9WgXcQ", "http://youtube.com/embed/dQw4w9WgXcQ"));
    // Even with extra parameters.
    EXPECT_TRUE(test("https://www.youtube.com/embed/dQw4w9WgXcQ?start=4", "https://www.youtube.com/embed/dQw4w9WgXcQ?start=4"));
    EXPECT_TRUE(test("http://www.youtube.com/embed/dQw4w9WgXcQ?enablejsapi=1", "http://www.youtube.com/embed/dQw4w9WgXcQ?enablejsapi=1"));
    // Even with an invalid "query".
    EXPECT_TRUE(test("https://www.youtube.com/embed/dQw4w9WgXcQ&start=4", "https://www.youtube.com/embed/dQw4w9WgXcQ&start=4"));

    // Don't transform anything with a non "/v/" path component immediately following the domain.
    EXPECT_TRUE(test("https://www.youtube.com/something/v/dQw4w9WgXcQ", "https://www.youtube.com/something/v/dQw4w9WgXcQ"));

    // Non-YouTube domain whose path looks like a Flash video shouldn't be transformed.
    EXPECT_TRUE(test("https://www.notyoutube.com/v/dQw4w9WgXcQ", "https://www.notyoutube.com/v/dQw4w9WgXcQ"));
}

} // namespace TestWebKitAPI
