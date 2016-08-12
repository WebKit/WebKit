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
#include <WebCore/URLParser.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace TestWebKitAPI {

class URLParserTest : public testing::Test {
public:
    void SetUp() final {
        WTF::initializeMainThread();
    }
};

struct ExpectedParts {
    String protocol;
    String user;
    String password;
    String host;
    unsigned short port;
    String path;
    String query;
    String fragment;
};
    
static void eq(const String& s1, const String& s2) { EXPECT_STREQ(s1.utf8().data(), s2.utf8().data()); }
static void checkURL(const URL& url, const ExpectedParts& parts)
{
    eq(url.protocol(), parts.protocol);
    eq(url.user(), parts.user);
    eq(url.pass(), parts.password);
    eq(url.host(), parts.host);
    EXPECT_EQ(url.port(), parts.port);
    eq(url.path(), parts.path);
    eq(url.query(), parts.query);
    eq(url.fragmentIdentifier(), parts.fragment);
}

TEST_F(URLParserTest, Parse)
{
    auto url = URLParser::parse("http://user:pass@webkit.org:123/path?query#fragment");
    checkURL(url.value(), {"http", "user", "pass", "webkit.org", 123, "/path", "query", "fragment"});
}

} // namespace TestWebKitAPI
