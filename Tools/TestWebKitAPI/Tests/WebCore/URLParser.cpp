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

static bool eq(const String& s1, const String& s2) { return s1.utf8() == s2.utf8(); }
static void checkURL(const String& urlString, const ExpectedParts& parts)
{
    URLParser parser;
    auto url = parser.parse(urlString);
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port());
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));
    
    auto oldURL = URL(URL(), urlString);
    EXPECT_TRUE(eq(parts.protocol, oldURL.protocol()));
    EXPECT_TRUE(eq(parts.user, oldURL.user()));
    EXPECT_TRUE(eq(parts.password, oldURL.pass()));
    EXPECT_TRUE(eq(parts.host, oldURL.host()));
    EXPECT_EQ(parts.port, oldURL.port());
    EXPECT_TRUE(eq(parts.path, oldURL.path()));
    EXPECT_TRUE(eq(parts.query, oldURL.query()));
    EXPECT_TRUE(eq(parts.fragment, oldURL.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, oldURL.string()));
    
    EXPECT_TRUE(URLParser::allValuesEqual(url, oldURL));
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
    checkURL("http://webkit.org/", {"http", "", "", "webkit.org", 0, "/", "", "", "http://webkit.org/"});
    checkURL("http://webkit.org/path1/path2/index.html", {"http", "", "", "webkit.org", 0, "/path1/path2/index.html", "", "", "http://webkit.org/path1/path2/index.html"});
    checkURL("about:blank", {"about", "", "", "", 0, "blank", "", "", "about:blank"});
    checkURL("about:blank?query", {"about", "", "", "", 0, "blank", "query", "", "about:blank?query"});
    checkURL("about:blank#fragment", {"about", "", "", "", 0, "blank", "", "fragment", "about:blank#fragment"});
}

static void checkRelativeURL(const String& urlString, const String& baseURLString, const ExpectedParts& parts)
{
    URLParser baseParser;
    auto base = baseParser.parse(baseURLString);

    URLParser parser;
    auto url = parser.parse(urlString, base);
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port());
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));

    auto oldURL = URL(URL(URL(), baseURLString), urlString);
    EXPECT_TRUE(eq(parts.protocol, oldURL.protocol()));
    EXPECT_TRUE(eq(parts.user, oldURL.user()));
    EXPECT_TRUE(eq(parts.password, oldURL.pass()));
    EXPECT_TRUE(eq(parts.host, oldURL.host()));
    EXPECT_EQ(parts.port, oldURL.port());
    EXPECT_TRUE(eq(parts.path, oldURL.path()));
    EXPECT_TRUE(eq(parts.query, oldURL.query()));
    EXPECT_TRUE(eq(parts.fragment, oldURL.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, oldURL.string()));

    EXPECT_TRUE(URLParser::allValuesEqual(url, oldURL));
}

TEST_F(URLParserTest, ParseRelative)
{
    checkRelativeURL("/index.html", "http://webkit.org/path1/path2/", {"http", "", "", "webkit.org", 0, "/index.html", "", "", "http://webkit.org/index.html"});
    checkRelativeURL("http://whatwg.org/index.html", "http://webkit.org/path1/path2/", {"http", "", "", "whatwg.org", 0, "/index.html", "", "", "http://whatwg.org/index.html"});
    checkRelativeURL("index.html", "http://webkit.org/path1/path2/page.html?query#fragment", {"http", "", "", "webkit.org", 0, "/path1/path2/index.html", "", "", "http://webkit.org/path1/path2/index.html"});
    checkRelativeURL("//whatwg.org/index.html", "https://www.webkit.org/path", {"https", "", "", "whatwg.org", 0, "/index.html", "", "", "https://whatwg.org/index.html"});
}

static void checkURLDifferences(const String& urlString, const ExpectedParts& partsNew, const ExpectedParts& partsOld)
{
    URLParser parser;
    auto url = parser.parse(urlString);
    EXPECT_TRUE(eq(partsNew.protocol, url.protocol()));
    EXPECT_TRUE(eq(partsNew.user, url.user()));
    EXPECT_TRUE(eq(partsNew.password, url.pass()));
    EXPECT_TRUE(eq(partsNew.host, url.host()));
    EXPECT_EQ(partsNew.port, url.port());
    EXPECT_TRUE(eq(partsNew.path, url.path()));
    EXPECT_TRUE(eq(partsNew.query, url.query()));
    EXPECT_TRUE(eq(partsNew.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(partsNew.string, url.string()));
    
    auto oldURL = URL(URL(), urlString);
    EXPECT_TRUE(eq(partsOld.protocol, oldURL.protocol()));
    EXPECT_TRUE(eq(partsOld.user, oldURL.user()));
    EXPECT_TRUE(eq(partsOld.password, oldURL.pass()));
    EXPECT_TRUE(eq(partsOld.host, oldURL.host()));
    EXPECT_EQ(partsOld.port, oldURL.port());
    EXPECT_TRUE(eq(partsOld.path, oldURL.path()));
    EXPECT_TRUE(eq(partsOld.query, oldURL.query()));
    EXPECT_TRUE(eq(partsOld.fragment, oldURL.fragmentIdentifier()));
    EXPECT_TRUE(eq(partsOld.string, oldURL.string()));
    
    EXPECT_FALSE(URLParser::allValuesEqual(url, oldURL));
}

static void checkRelativeURLDifferences(const String& urlString, const String& baseURLString, const ExpectedParts& partsNew, const ExpectedParts& partsOld)
{
    URLParser baseParser;
    auto base = baseParser.parse(baseURLString);
    
    URLParser parser;
    auto url = parser.parse(urlString, base);
    EXPECT_TRUE(eq(partsNew.protocol, url.protocol()));
    EXPECT_TRUE(eq(partsNew.user, url.user()));
    EXPECT_TRUE(eq(partsNew.password, url.pass()));
    EXPECT_TRUE(eq(partsNew.host, url.host()));
    EXPECT_EQ(partsNew.port, url.port());
    EXPECT_TRUE(eq(partsNew.path, url.path()));
    EXPECT_TRUE(eq(partsNew.query, url.query()));
    EXPECT_TRUE(eq(partsNew.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(partsNew.string, url.string()));
    
    auto oldURL = URL(URL(URL(), baseURLString), urlString);
    EXPECT_TRUE(eq(partsOld.protocol, oldURL.protocol()));
    EXPECT_TRUE(eq(partsOld.user, oldURL.user()));
    EXPECT_TRUE(eq(partsOld.password, oldURL.pass()));
    EXPECT_TRUE(eq(partsOld.host, oldURL.host()));
    EXPECT_EQ(partsOld.port, oldURL.port());
    EXPECT_TRUE(eq(partsOld.path, oldURL.path()));
    EXPECT_TRUE(eq(partsOld.query, oldURL.query()));
    EXPECT_TRUE(eq(partsOld.fragment, oldURL.fragmentIdentifier()));
    EXPECT_TRUE(eq(partsOld.string, oldURL.string()));
    
    EXPECT_FALSE(URLParser::allValuesEqual(url, oldURL));
}

TEST_F(URLParserTest, ParserDifferences)
{
    checkURLDifferences("http://127.0.1",
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"},
        {"http", "", "", "127.0.1", 0, "/", "", "", "http://127.0.1/"});
    checkURLDifferences("http://011.11.0X11.0x011",
        {"http", "", "", "9.11.17.17", 0, "/", "", "", "http://9.11.17.17/"},
        {"http", "", "", "011.11.0x11.0x011", 0, "/", "", "", "http://011.11.0x11.0x011/"});

    // FIXME: This behavior ought to be specified in the standard.
    // With the existing URL::parse, WebKit returns "https:/", Firefox returns "https:///", and Chrome throws an error.
    checkRelativeURLDifferences("//", "https://www.webkit.org/path",
        {"https", "", "", "", 0, "", "", "", "https://"},
        {"https", "", "", "", 0, "/", "", "", "https:/"});
}

static void shouldFail(const String& urlString)
{
    URLParser parser;
    auto invalidURL = parser.parse(urlString);
    EXPECT_TRUE(URLParser::allValuesEqual(invalidURL, { }));
}
    
TEST_F(URLParserTest, ParserFailures)
{
    shouldFail("    ");
    shouldFail("");
}

} // namespace TestWebKitAPI
