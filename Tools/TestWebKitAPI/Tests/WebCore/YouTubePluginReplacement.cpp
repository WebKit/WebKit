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
#include "PlatformUtilities.h"
#include <WebCore/YouTubePluginReplacement.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>

using namespace WebCore;

namespace TestWebKitAPI {

class YouTubePluginReplacementTest : public testing::Test {
public:
    void SetUp() final
    {
        WTF::initializeMainThread();
    }
};

static void test(ASCIILiteral inputURLString, ASCIILiteral expectedURLString)
{
    URL inputURL { inputURLString };
    String actualURLString = YouTubePluginReplacement::youTubeURLFromAbsoluteURL(inputURL, inputURLString);
    EXPECT_WK_STREQ(expectedURLString.characters(), actualURLString.utf8().data());
}

TEST_F(YouTubePluginReplacementTest, YouTubeURLFromAbsoluteURL)
{
    // YouTube non-video URL, not expected to be transformed.
    test("https://www.youtube.com"_s, "https://www.youtube.com"_s);

    // Basic YouTube video links, expected to be transformed.
    test("https://www.youtube.com/v/dQw4w9WgXcQ"_s, "https://www.youtube.com/embed/dQw4w9WgXcQ"_s);
    test("http://www.youtube.com/v/dQw4w9WgXcQ"_s, "http://www.youtube.com/embed/dQw4w9WgXcQ"_s);
    test("https://youtube.com/v/dQw4w9WgXcQ"_s, "https://youtube.com/embed/dQw4w9WgXcQ"_s);
    test("http://youtube.com/v/dQw4w9WgXcQ"_s, "http://youtube.com/embed/dQw4w9WgXcQ"_s);

    // With start time, preserved.
    test("http://www.youtube.com/v/dQw4w9WgXcQ?start=4"_s, "http://www.youtube.com/embed/dQw4w9WgXcQ?start=4"_s);
    test("http://www.youtube.com/v/dQw4w9WgXcQ?start=4&fs=1"_s, "http://www.youtube.com/embed/dQw4w9WgXcQ?start=4&fs=1"_s);

    // With an invalid query (see & instead of ?), we preserve and fix the query.
    test("http://www.youtube.com/v/dQw4w9WgXcQ&start=4"_s, "http://www.youtube.com/embed/dQw4w9WgXcQ?start=4"_s);
    test("http://www.youtube.com/v/dQw4w9WgXcQ&start=4&fs=1"_s, "http://www.youtube.com/embed/dQw4w9WgXcQ?start=4&fs=1"_s);

    // Non-Flash URL is untouched.
    test("https://www.youtube.com/embed/dQw4w9WgXcQ"_s, "https://www.youtube.com/embed/dQw4w9WgXcQ"_s);
    test("http://www.youtube.com/embed/dQw4w9WgXcQ"_s, "http://www.youtube.com/embed/dQw4w9WgXcQ"_s);
    test("https://youtube.com/embed/dQw4w9WgXcQ"_s, "https://youtube.com/embed/dQw4w9WgXcQ"_s);
    test("http://youtube.com/embed/dQw4w9WgXcQ"_s, "http://youtube.com/embed/dQw4w9WgXcQ"_s);
    // Even with extra parameters.
    test("https://www.youtube.com/embed/dQw4w9WgXcQ?start=4"_s, "https://www.youtube.com/embed/dQw4w9WgXcQ?start=4"_s);
    test("http://www.youtube.com/embed/dQw4w9WgXcQ?enablejsapi=1"_s, "http://www.youtube.com/embed/dQw4w9WgXcQ?enablejsapi=1"_s);
    // Even with an invalid "query".
    test("https://www.youtube.com/embed/dQw4w9WgXcQ&start=4"_s, "https://www.youtube.com/embed/dQw4w9WgXcQ&start=4"_s);

    // Don't transform anything with a non "/v/" path component immediately following the domain.
    test("https://www.youtube.com/something/v/dQw4w9WgXcQ"_s, "https://www.youtube.com/something/v/dQw4w9WgXcQ"_s);

    // Non-valid Youtube URLs should be dropped.
    test("https://www.notyoutube.com/v/dQw4w9WgXcQ"_s, ""_s);
    test("data:,Hello%2C%20World%21"_s, ""_s);
    test("javascript:foo()"_s, ""_s);
}

} // namespace TestWebKitAPI
