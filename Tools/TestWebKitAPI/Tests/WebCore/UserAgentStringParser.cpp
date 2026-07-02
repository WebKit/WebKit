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

#include "Helpers/PlatformUtilities.h"
#include <WebCore/UserAgentStringData.h>
#include <WebCore/UserAgentStringParser.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WebCore;

class UserAgentStringParserTest : public testing::Test {
public:
    void SetUp() final
    {
        WTF::initializeMainThread();
    }
};

static Ref<UserAgentStringData> parse(const String& userAgent)
{
    auto data = UserAgentStringParser::create(userAgent)->parse();
    RELEASE_ASSERT(data);
    return WTF::move(*data);
}

TEST_F(UserAgentStringParserTest, SafariBrowserVersionIsReported)
{
    auto data = parse("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Safari/605.1.15"_s);
    EXPECT_WK_STREQ("Safari", data->browserName);
    EXPECT_WK_STREQ("605.1.15", data->browserVersion);
}

TEST_F(UserAgentStringParserTest, FirefoxVersionNotClobberedByTrailingSafariToken)
{
    auto data = parse("Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0 Safari/605.1.15"_s);
    EXPECT_WK_STREQ("Firefox", data->browserName);
    EXPECT_WK_STREQ("120.0", data->browserVersion);
}

TEST_F(UserAgentStringParserTest, ChromeBrowserVersionIsReported)
{
    auto data = parse("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"_s);
    EXPECT_WK_STREQ("Google Chrome", data->browserName);
    EXPECT_WK_STREQ("120.0.0.0", data->browserVersion);
}

} // namespace TestWebKitAPI
