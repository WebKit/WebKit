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
    String string;
};

static const char* s(const String& s) { return s.utf8().data(); }
static void checkURL(const String& urlString, const ExpectedParts& parts)
{
    URLParser parser;
    auto url = parser.parse(urlString);
    EXPECT_STREQ(s(parts.protocol), s(url->protocol()));
    EXPECT_STREQ(s(parts.user), s(url->user()));
    EXPECT_STREQ(s(parts.password), s(url->pass()));
    EXPECT_STREQ(s(parts.host), s(url->host()));
    EXPECT_EQ(parts.port, url->port());
    EXPECT_STREQ(s(parts.path), s(url->path()));
    EXPECT_STREQ(s(parts.query), s(url->query()));
    EXPECT_STREQ(s(parts.fragment), s(url->fragmentIdentifier()));
    EXPECT_STREQ(s(parts.string), s(url->string()));
    
    auto oldURL = URL(URL(), urlString);
    EXPECT_STREQ(s(parts.protocol), s(oldURL.protocol()));
    EXPECT_STREQ(s(parts.user), s(oldURL.user()));
    EXPECT_STREQ(s(parts.password), s(oldURL.pass()));
    EXPECT_STREQ(s(parts.host), s(oldURL.host()));
    EXPECT_EQ(parts.port, oldURL.port());
    EXPECT_STREQ(s(parts.path), s(oldURL.path()));
    EXPECT_STREQ(s(parts.query), s(oldURL.query()));
    EXPECT_STREQ(s(parts.fragment), s(oldURL.fragmentIdentifier()));
    EXPECT_STREQ(s(parts.string), s(oldURL.string()));
    
    EXPECT_TRUE(URLParser::allValuesEqual(url.value(), oldURL));
}

TEST_F(URLParserTest, Parse)
{
    checkURL("http://user:pass@webkit.org:123/path?query#fragment", {"http", "user", "pass", "webkit.org", 123, "/path", "query", "fragment", "http://user:pass@webkit.org:123/path?query#fragment"});
    checkURL("http://user:pass@webkit.org:123/path?query", {"http", "user", "pass", "webkit.org", 123, "/path", "query", "", "http://user:pass@webkit.org:123/path?query"});
    checkURL("http://user:pass@webkit.org:123/path", {"http", "user", "pass", "webkit.org", 123, "/path", "", "", "http://user:pass@webkit.org:123/path"});
    checkURL("http://user:pass@webkit.org:123/", {"http", "user", "pass", "webkit.org", 123, "/", "", "", "http://user:pass@webkit.org:123/"});
    checkURL("http://user:pass@webkit.org:123", {"http", "user", "pass", "webkit.org", 123, "/", "", "", "http://user:pass@webkit.org:123/"});
    checkURL("http://user:pass@webkit.org", {"http", "user", "pass", "webkit.org", 0, "/", "", "", "http://user:pass@webkit.org/"});
    checkURL("http://webkit.org", {"http", "", "", "webkit.org", 0, "/", "", "", "http://webkit.org/"});
    checkURL("http://127.0.0.1", {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"});
}

static void checkURLDifferences(const String& urlString, const ExpectedParts& partsNew, const ExpectedParts& partsOld)
{
    URLParser parser;
    auto url = parser.parse(urlString);
    EXPECT_STREQ(s(partsNew.protocol), s(url->protocol()));
    EXPECT_STREQ(s(partsNew.user), s(url->user()));
    EXPECT_STREQ(s(partsNew.password), s(url->pass()));
    EXPECT_STREQ(s(partsNew.host), s(url->host()));
    EXPECT_EQ(partsNew.port, url->port());
    EXPECT_STREQ(s(partsNew.path), s(url->path()));
    EXPECT_STREQ(s(partsNew.query), s(url->query()));
    EXPECT_STREQ(s(partsNew.fragment), s(url->fragmentIdentifier()));
    EXPECT_STREQ(s(partsNew.string), s(url->string()));
    
    auto oldURL = URL(URL(), urlString);
    EXPECT_STREQ(s(partsOld.protocol), s(oldURL.protocol()));
    EXPECT_STREQ(s(partsOld.user), s(oldURL.user()));
    EXPECT_STREQ(s(partsOld.password), s(oldURL.pass()));
    EXPECT_STREQ(s(partsOld.host), s(oldURL.host()));
    EXPECT_EQ(partsOld.port, oldURL.port());
    EXPECT_STREQ(s(partsOld.path), s(oldURL.path()));
    EXPECT_STREQ(s(partsOld.query), s(oldURL.query()));
    EXPECT_STREQ(s(partsOld.fragment), s(oldURL.fragmentIdentifier()));
    EXPECT_STREQ(s(partsOld.string), s(oldURL.string()));
    
    EXPECT_FALSE(URLParser::allValuesEqual(url.value(), oldURL));
}

TEST_F(URLParserTest, ParserDifferences)
{
    checkURLDifferences("http://127.0.1",
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"},
        {"http", "", "", "127.0.1", 0, "/", "", "", "http://127.0.1/"});
    checkURLDifferences("http://011.11.0X11.0x011",
        {"http", "", "", "9.11.17.17", 0, "/", "", "", "http://9.11.17.17/"},
        {"http", "", "", "011.11.0x11.0x011", 0, "/", "", "", "http://011.11.0x11.0x011/"});
}

static void shouldFail(const String& urlString)
{
    URLParser parser;
    auto invalidURL = parser.parse(urlString);
    EXPECT_TRUE(invalidURL == Nullopt);
}
    
TEST_F(URLParserTest, ParserFailures)
{
    shouldFail("    ");
}

} // namespace TestWebKitAPI
