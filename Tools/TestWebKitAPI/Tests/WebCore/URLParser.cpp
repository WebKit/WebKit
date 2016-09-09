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

static bool eq(const String& s1, const String& s2)
{
    EXPECT_STREQ(s1.utf8().data(), s2.utf8().data());
    return s1.utf8() == s2.utf8();
}

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

template<size_t length>
static String wideString(const wchar_t (&url)[length])
{
    StringBuilder builder;
    builder.reserveCapacity(length - 1);
    for (size_t i = 0; i < length - 1; ++i)
        builder.append(url[i]);
    return builder.toString();
}

TEST_F(URLParserTest, Basic)
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
    checkURL("http://[0:f::f:f:0:0]", {"http", "", "", "[0:f::f:f:0:0]", 0, "/", "", "", "http://[0:f::f:f:0:0]/"});
    checkURL("http://[0:f:0:0:f::]", {"http", "", "", "[0:f:0:0:f::]", 0, "/", "", "", "http://[0:f:0:0:f::]/"});
    checkURL("http://[::f:0:0:f:0:0]", {"http", "", "", "[::f:0:0:f:0:0]", 0, "/", "", "", "http://[::f:0:0:f:0:0]/"});
    checkURL("http://example.com/path1/path2/.", {"http", "", "", "example.com", 0, "/path1/path2/", "", "", "http://example.com/path1/path2/"});
    checkURL("http://example.com/path1/path2/..", {"http", "", "", "example.com", 0, "/path1/", "", "", "http://example.com/path1/"});
    checkURL("http://example.com/path1/path2/./path3", {"http", "", "", "example.com", 0, "/path1/path2/path3", "", "", "http://example.com/path1/path2/path3"});
    checkURL("http://example.com/path1/path2/.\\path3", {"http", "", "", "example.com", 0, "/path1/path2/path3", "", "", "http://example.com/path1/path2/path3"});
    checkURL("http://example.com/path1/path2/../path3", {"http", "", "", "example.com", 0, "/path1/path3", "", "", "http://example.com/path1/path3"});
    checkURL("http://example.com/path1/path2/..\\path3", {"http", "", "", "example.com", 0, "/path1/path3", "", "", "http://example.com/path1/path3"});
    checkURL("http://example.com/.", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkURL("http://example.com/..", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkURL("http://example.com/./path1", {"http", "", "", "example.com", 0, "/path1", "", "", "http://example.com/path1"});
    checkURL("http://example.com/../path1", {"http", "", "", "example.com", 0, "/path1", "", "", "http://example.com/path1"});
    checkURL("http://example.com/../path1/../../path2/path3/../path4", {"http", "", "", "example.com", 0, "/path2/path4", "", "", "http://example.com/path2/path4"});
    checkURL("http://example.com/path1/.%2", {"http", "", "", "example.com", 0, "/path1/.%2", "", "", "http://example.com/path1/.%2"});
    checkURL("http://example.com/path1/%2", {"http", "", "", "example.com", 0, "/path1/%2", "", "", "http://example.com/path1/%2"});
    checkURL("http://example.com/path1/%", {"http", "", "", "example.com", 0, "/path1/%", "", "", "http://example.com/path1/%"});
    checkURL("http://example.com/path1/.%", {"http", "", "", "example.com", 0, "/path1/.%", "", "", "http://example.com/path1/.%"});
    checkURL("http://example.com//.", {"http", "", "", "example.com", 0, "//", "", "", "http://example.com//"});
    checkURL("http://example.com//./", {"http", "", "", "example.com", 0, "//", "", "", "http://example.com//"});
    checkURL("http://example.com//.//", {"http", "", "", "example.com", 0, "///", "", "", "http://example.com///"});
    checkURL("http://example.com//..", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkURL("http://example.com//../", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkURL("http://example.com//..//", {"http", "", "", "example.com", 0, "//", "", "", "http://example.com//"});
    checkURL("http://example.com//..", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkURL("http://example.com/.//", {"http", "", "", "example.com", 0, "//", "", "", "http://example.com//"});
    checkURL("http://example.com/..//", {"http", "", "", "example.com", 0, "//", "", "", "http://example.com//"});
    checkURL("http://example.com/./", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkURL("http://example.com/../", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkURL("http://example.com/path1/.../path3", {"http", "", "", "example.com", 0, "/path1/.../path3", "", "", "http://example.com/path1/.../path3"});
    checkURL("http://example.com/path1/...", {"http", "", "", "example.com", 0, "/path1/...", "", "", "http://example.com/path1/..."});
    checkURL("http://example.com/path1/.../", {"http", "", "", "example.com", 0, "/path1/.../", "", "", "http://example.com/path1/.../"});
    checkURL("http://example.com/.path1/", {"http", "", "", "example.com", 0, "/.path1/", "", "", "http://example.com/.path1/"});
    checkURL("http://example.com/..path1/", {"http", "", "", "example.com", 0, "/..path1/", "", "", "http://example.com/..path1/"});
    checkURL("http://example.com/path1/.path2", {"http", "", "", "example.com", 0, "/path1/.path2", "", "", "http://example.com/path1/.path2"});
    checkURL("http://example.com/path1/..path2", {"http", "", "", "example.com", 0, "/path1/..path2", "", "", "http://example.com/path1/..path2"});
    checkURL("http://example.com/path1/path2/.?query", {"http", "", "", "example.com", 0, "/path1/path2/", "query", "", "http://example.com/path1/path2/?query"});
    checkURL("http://example.com/path1/path2/..?query", {"http", "", "", "example.com", 0, "/path1/", "query", "", "http://example.com/path1/?query"});
    checkURL("http://example.com/path1/path2/.#fragment", {"http", "", "", "example.com", 0, "/path1/path2/", "", "fragment", "http://example.com/path1/path2/#fragment"});
    checkURL("http://example.com/path1/path2/..#fragment", {"http", "", "", "example.com", 0, "/path1/", "", "fragment", "http://example.com/path1/#fragment"});

    checkURL("file:", {"file", "", "", "", 0, "/", "", "", "file:///"});
    checkURL("file:/", {"file", "", "", "", 0, "/", "", "", "file:///"});
    checkURL("file://", {"file", "", "", "", 0, "/", "", "", "file:///"});
    checkURL("file:///", {"file", "", "", "", 0, "/", "", "", "file:///"});
    checkURL("file:////", {"file", "", "", "", 0, "//", "", "", "file:////"}); // This matches Firefox and URL::parse which I believe are correct, but not Chrome.
    checkURL("file:/path", {"file", "", "", "", 0, "/path", "", "", "file:///path"});
    checkURL("file://host/path", {"file", "", "", "host", 0, "/path", "", "", "file://host/path"});
    checkURL("file://host", {"file", "", "", "host", 0, "/", "", "", "file://host/"});
    checkURL("file://host/", {"file", "", "", "host", 0, "/", "", "", "file://host/"});
    checkURL("file:///path", {"file", "", "", "", 0, "/path", "", "", "file:///path"});
    checkURL("file:////path", {"file", "", "", "", 0, "//path", "", "", "file:////path"});
    checkURL("file://localhost/path", {"file", "", "", "", 0, "/path", "", "", "file:///path"});
    checkURL("file://localhost/", {"file", "", "", "", 0, "/", "", "", "file:///"});
    checkURL("file://localhost", {"file", "", "", "", 0, "/", "", "", "file:///"});
    // FIXME: check file://lOcAlHoSt etc.
    checkURL("file:?query", {"file", "", "", "", 0, "/", "query", "", "file:///?query"});
    checkURL("file:#fragment", {"file", "", "", "", 0, "/", "", "fragment", "file:///#fragment"});
    checkURL("file:?query#fragment", {"file", "", "", "", 0, "/", "query", "fragment", "file:///?query#fragment"});
    checkURL("file:#fragment?notquery", {"file", "", "", "", 0, "/", "", "fragment?notquery", "file:///#fragment?notquery"});
    checkURL("file:/?query", {"file", "", "", "", 0, "/", "query", "", "file:///?query"});
    checkURL("file:/#fragment", {"file", "", "", "", 0, "/", "", "fragment", "file:///#fragment"});
    checkURL("file://?query", {"file", "", "", "", 0, "/", "query", "", "file:///?query"});
    checkURL("file://#fragment", {"file", "", "", "", 0, "/", "", "fragment", "file:///#fragment"});
    checkURL("file:///?query", {"file", "", "", "", 0, "/", "query", "", "file:///?query"});
    checkURL("file:///#fragment", {"file", "", "", "", 0, "/", "", "fragment", "file:///#fragment"});
    checkURL("file:////?query", {"file", "", "", "", 0, "//", "query", "", "file:////?query"});
    checkURL("file:////#fragment", {"file", "", "", "", 0, "//", "", "fragment", "file:////#fragment"});
    checkURL("http://host/A b", {"http", "", "", "host", 0, "/A%20b", "", "", "http://host/A%20b"});
    checkURL("http://host/a%20B", {"http", "", "", "host", 0, "/a%20B", "", "", "http://host/a%20B"});
    checkURL("http://host?q=@ <>!#fragment", {"http", "", "", "host", 0, "/", "q=@%20%3C%3E!", "fragment", "http://host/?q=@%20%3C%3E!#fragment"});
    checkURL("http://user:@host", {"http", "user", "", "host", 0, "/", "", "", "http://user@host/"});
    checkURL("http://127.0.0.1:10100/path", {"http", "", "", "127.0.0.1", 10100, "/path", "", "", "http://127.0.0.1:10100/path"});
    checkURL("http://127.0.0.1:/path", {"http", "", "", "127.0.0.1", 0, "/path", "", "", "http://127.0.0.1/path"});
    checkURL("http://127.0.0.1:123", {"http", "", "", "127.0.0.1", 123, "/", "", "", "http://127.0.0.1:123/"});
    checkURL("http://127.0.0.1:", {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"});
    checkURL("http://[0:f::f:f:0:0]:123/path", {"http", "", "", "[0:f::f:f:0:0]", 123, "/path", "", "", "http://[0:f::f:f:0:0]:123/path"});
    checkURL("http://[0:f::f:f:0:0]:123", {"http", "", "", "[0:f::f:f:0:0]", 123, "/", "", "", "http://[0:f::f:f:0:0]:123/"});
    checkURL("http://[0:f::f:f:0:0]:/path", {"http", "", "", "[0:f::f:f:0:0]", 0, "/path", "", "", "http://[0:f::f:f:0:0]/path"});
    checkURL("http://[0:f::f:f:0:0]:", {"http", "", "", "[0:f::f:f:0:0]", 0, "/", "", "", "http://[0:f::f:f:0:0]/"});
    checkURL("http://host:10100/path", {"http", "", "", "host", 10100, "/path", "", "", "http://host:10100/path"});
    checkURL("http://host:/path", {"http", "", "", "host", 0, "/path", "", "", "http://host/path"});
    checkURL("http://host:123", {"http", "", "", "host", 123, "/", "", "", "http://host:123/"});
    checkURL("http://host:", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkURL("http://hos\tt\n:\t1\n2\t3\t/\npath", {"http", "", "", "host", 123, "/path", "", "", "http://host:123/path"});
    checkURL("http://user@example.org/path3", {"http", "user", "", "example.org", 0, "/path3", "", "", "http://user@example.org/path3"});
    checkURL("sc:/pa/pa", {"sc", "", "", "", 0, "/pa/pa", "", "", "sc:/pa/pa"});
    checkURL("sc:/pa", {"sc", "", "", "", 0, "/pa", "", "", "sc:/pa"});
    checkURL("sc:/pa/", {"sc", "", "", "", 0, "/pa/", "", "", "sc:/pa/"});
    checkURL("sc://pa/", {"sc", "", "", "pa", 0, "/", "", "", "sc://pa/"});

    // This disagrees with the web platform test for http://:@www.example.com but agrees with Chrome and URL::parse,
    // and Firefox fails the web platform test differently. Maybe the web platform test ought to be changed.
    checkURL("http://:@host", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
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
    checkRelativeURL("http://example\t.\norg", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/", "", "", "http://example.org/"});
    checkRelativeURL("test", "file:///path1/path2", {"file", "", "", "", 0, "/path1/test", "", "", "file:///path1/test"});
    checkRelativeURL(wideString(L"http://www.foo。bar.com"), "http://other.com/", {"http", "", "", "www.foo.bar.com", 0, "/", "", "", "http://www.foo.bar.com/"});
    checkRelativeURL(wideString(L"sc://ñ.test/"), "about:blank", {"sc", "", "", "xn--ida.test", 0, "/", "", "", "sc://xn--ida.test/"});
    checkRelativeURL("#fragment", "http://host/path", {"http", "", "", "host", 0, "/path", "", "fragment", "http://host/path#fragment"});
    checkRelativeURL("?query", "http://host/path", {"http", "", "", "host", 0, "/path", "query", "", "http://host/path?query"});
    checkRelativeURL("?query#fragment", "http://host/path", {"http", "", "", "host", 0, "/path", "query", "fragment", "http://host/path?query#fragment"});
    checkRelativeURL(wideString(L"?β"), "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "%CE%B2", "", "http://example.org/foo/bar?%CE%B2"});
    checkRelativeURL("?", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar?"});
    checkRelativeURL("#", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar#"});
    checkRelativeURL("?#", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar?#"});
    checkRelativeURL("#?", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "?", "http://example.org/foo/bar#?"});
    checkRelativeURL("/", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/", "", "", "http://example.org/"});
    checkRelativeURL("http://@host", "about:blank", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkRelativeURL("http://:@host", "about:blank", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkRelativeURL("http://foo.com/\\@", "http://example.org/foo/bar", {"http", "", "", "foo.com", 0, "//@", "", "", "http://foo.com//@"});
    checkRelativeURL("\\@", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/@", "", "", "http://example.org/@"});
    checkRelativeURL("/path3", "http://user@example.org/path1/path2", {"http", "user", "", "example.org", 0, "/path3", "", "", "http://user@example.org/path3"});
    checkRelativeURL("", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar"});
    checkRelativeURL("  \a  \t\n", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar"});
    checkRelativeURL(":foo.com\\", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/:foo.com/", "", "", "http://example.org/foo/:foo.com/"});
    checkRelativeURL("http:/example.com/", "about:blank", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkRelativeURL("http:example.com/", "about:blank", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkRelativeURL("http:\\\\foo.com\\", "http://example.org/foo/bar", {"http", "", "", "foo.com", 0, "/", "", "", "http://foo.com/"});
    checkRelativeURL("http:\\\\foo.com/", "http://example.org/foo/bar", {"http", "", "", "foo.com", 0, "/", "", "", "http://foo.com/"});
    checkRelativeURL("http:\\\\foo.com", "http://example.org/foo/bar", {"http", "", "", "foo.com", 0, "/", "", "", "http://foo.com/"});
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

// These are differences between the new URLParser and the old URL::parse which make URLParser more standards compliant.
TEST_F(URLParserTest, ParserDifferences)
{
    checkURLDifferences("http://127.0.1",
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"},
        {"http", "", "", "127.0.1", 0, "/", "", "", "http://127.0.1/"});
    checkURLDifferences("http://011.11.0X11.0x011",
        {"http", "", "", "9.11.17.17", 0, "/", "", "", "http://9.11.17.17/"},
        {"http", "", "", "011.11.0x11.0x011", 0, "/", "", "", "http://011.11.0x11.0x011/"});
    checkURLDifferences("http://[1234:0078:90AB:CdEf:0123:0007:89AB:0000]",
        {"http", "", "", "[1234:78:90ab:cdef:123:7:89ab:0]", 0, "/", "", "", "http://[1234:78:90ab:cdef:123:7:89ab:0]/"},
        {"http", "", "", "[1234:0078:90ab:cdef:0123:0007:89ab:0000]", 0, "/", "", "", "http://[1234:0078:90ab:cdef:0123:0007:89ab:0000]/"});
    checkURLDifferences("http://[0:f:0:0:f:f:0:0]",
        {"http", "", "", "[0:f::f:f:0:0]", 0, "/", "", "", "http://[0:f::f:f:0:0]/"},
        {"http", "", "", "[0:f:0:0:f:f:0:0]", 0, "/", "", "", "http://[0:f:0:0:f:f:0:0]/"});
    checkURLDifferences("http://[0:f:0:0:f:0:0:0]",
        {"http", "", "", "[0:f:0:0:f::]", 0, "/", "", "", "http://[0:f:0:0:f::]/"},
        {"http", "", "", "[0:f:0:0:f:0:0:0]", 0, "/", "", "", "http://[0:f:0:0:f:0:0:0]/"});
    checkURLDifferences("http://[0:0:f:0:0:f:0:0]",
        {"http", "", "", "[::f:0:0:f:0:0]", 0, "/", "", "", "http://[::f:0:0:f:0:0]/"},
        {"http", "", "", "[0:0:f:0:0:f:0:0]", 0, "/", "", "", "http://[0:0:f:0:0:f:0:0]/"});
    checkURLDifferences("http://example.com/path1/.%2e",
        {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"},
        {"http", "", "", "example.com", 0, "/path1/.%2e", "", "", "http://example.com/path1/.%2e"});
    checkURLDifferences("http://example.com/path1/.%2E",
        {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"},
        {"http", "", "", "example.com", 0, "/path1/.%2E", "", "", "http://example.com/path1/.%2E"});
    checkURLDifferences("http://example.com/path1/.%2E/",
        {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"},
        {"http", "", "", "example.com", 0, "/path1/.%2E/", "", "", "http://example.com/path1/.%2E/"});
    checkURLDifferences("http://example.com/path1/%2e.",
        {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"},
        {"http", "", "", "example.com", 0, "/path1/%2e.", "", "", "http://example.com/path1/%2e."});
    checkURLDifferences("http://example.com/path1/%2E%2e",
        {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"},
        {"http", "", "", "example.com", 0, "/path1/%2E%2e", "", "", "http://example.com/path1/%2E%2e"});
    checkURLDifferences("http://example.com/path1/%2e",
        {"http", "", "", "example.com", 0, "/path1/", "", "", "http://example.com/path1/"},
        {"http", "", "", "example.com", 0, "/path1/%2e", "", "", "http://example.com/path1/%2e"});
    checkURLDifferences("http://example.com/path1/%2E",
        {"http", "", "", "example.com", 0, "/path1/", "", "", "http://example.com/path1/"},
        {"http", "", "", "example.com", 0, "/path1/%2E", "", "", "http://example.com/path1/%2E"});
    checkURLDifferences("http://example.com/path1/%2E/",
        {"http", "", "", "example.com", 0, "/path1/", "", "", "http://example.com/path1/"},
        {"http", "", "", "example.com", 0, "/path1/%2E/", "", "", "http://example.com/path1/%2E/"});
    checkURLDifferences("http://example.com/path1/path2/%2e?query",
        {"http", "", "", "example.com", 0, "/path1/path2/", "query", "", "http://example.com/path1/path2/?query"},
        {"http", "", "", "example.com", 0, "/path1/path2/%2e", "query", "", "http://example.com/path1/path2/%2e?query"});
    checkURLDifferences("http://example.com/path1/path2/%2e%2e?query",
        {"http", "", "", "example.com", 0, "/path1/", "query", "", "http://example.com/path1/?query"},
        {"http", "", "", "example.com", 0, "/path1/path2/%2e%2e", "query", "", "http://example.com/path1/path2/%2e%2e?query"});
    checkURLDifferences("http://example.com/path1/path2/%2e#fragment",
        {"http", "", "", "example.com", 0, "/path1/path2/", "", "fragment", "http://example.com/path1/path2/#fragment"},
        {"http", "", "", "example.com", 0, "/path1/path2/%2e", "", "fragment", "http://example.com/path1/path2/%2e#fragment"});
    checkURLDifferences("http://example.com/path1/path2/%2e%2e#fragment",
        {"http", "", "", "example.com", 0, "/path1/", "", "fragment", "http://example.com/path1/#fragment"},
        {"http", "", "", "example.com", 0, "/path1/path2/%2e%2e", "", "fragment", "http://example.com/path1/path2/%2e%2e#fragment"});
    checkURLDifferences("file://[0:a:0:0:b:c:0:0]/path",
        {"file", "", "", "[0:a::b:c:0:0]", 0, "/path", "", "", "file://[0:a::b:c:0:0]/path"},
        {"file", "", "", "[0:a:0:0:b:c:0:0]", 0, "/path", "", "", "file://[0:a:0:0:b:c:0:0]/path"});
    checkRelativeURLDifferences(wideString(L"#β"), "http://example.org/foo/bar",
        {"http", "", "", "example.org", 0, "/foo/bar", "", wideString(L"β"), wideString(L"http://example.org/foo/bar#β")},
        {"http", "", "", "example.org", 0, "/foo/bar", "", "%CE%B2", "http://example.org/foo/bar#%CE%B2"});
    checkURLDifferences("http://",
        {"", "", "", "", 0, "", "", "", "http://"},
        {"http", "", "", "", 0, "/", "", "", "http:/"});
    checkRelativeURLDifferences("//", "https://www.webkit.org/path",
        {"", "", "", "", 0, "", "", "", "//"},
        {"https", "", "", "", 0, "/", "", "", "https:/"});
    checkURLDifferences("http://127.0.0.1:65536/path",
        {"", "", "", "", 0, "", "", "", "http://127.0.0.1:65536/path"},
        {"http", "", "", "127.0.0.1", 65535, "/path", "", "", "http://127.0.0.1:65536/path"});
    checkURLDifferences("http://host:65536",
        {"", "", "", "", 0, "", "", "", "http://host:65536"},
        {"http", "", "", "host", 65535, "/", "", "", "http://host:65536/"});
    checkURLDifferences("http://127.0.0.1:65536",
        {"", "", "", "", 0, "", "", "", "http://127.0.0.1:65536"},
        {"http", "", "", "127.0.0.1", 65535, "/", "", "", "http://127.0.0.1:65536/"});
    checkURLDifferences("http://[0:f::f:f:0:0]:65536",
        {"", "", "", "", 0, "", "", "", "http://[0:f::f:f:0:0]:65536"},
        {"http", "", "", "[0:f::f:f:0:0]", 65535, "/", "", "", "http://[0:f::f:f:0:0]:65536/"});
    checkRelativeURLDifferences(":foo.com\\", "notspecial://example.org/foo/bar",
        {"notspecial", "", "", "example.org", 0, "/foo/:foo.com\\", "", "", "notspecial://example.org/foo/:foo.com\\"},
        {"notspecial", "", "", "example.org", 0, "/foo/:foo.com/", "", "", "notspecial://example.org/foo/:foo.com/"});
    checkURLDifferences("sc://pa",
        {"sc", "", "", "pa", 0, "/", "", "", "sc://pa/"},
        {"sc", "", "", "pa", 0, "", "", "", "sc://pa"});
    checkRelativeURLDifferences("notspecial:\\\\foo.com\\", "http://example.org/foo/bar",
        {"notspecial", "", "", "", 0, "\\\\foo.com\\", "", "", "notspecial:\\\\foo.com\\"},
        {"notspecial", "", "", "foo.com", 0, "/", "", "", "notspecial://foo.com/"});
    checkRelativeURLDifferences("notspecial:\\\\foo.com/", "http://example.org/foo/bar",
        {"notspecial", "", "", "", 0, "\\\\foo.com/", "", "", "notspecial:\\\\foo.com/"},
        {"notspecial", "", "", "foo.com", 0, "/", "", "", "notspecial://foo.com/"});
    checkRelativeURLDifferences("notspecial:\\\\foo.com", "http://example.org/foo/bar",
        {"notspecial", "", "", "", 0, "\\\\foo.com", "", "", "notspecial:\\\\foo.com"},
        {"notspecial", "", "", "foo.com", 0, "", "", "", "notspecial://foo.com"});
    checkURLDifferences("file://notuser:notpassword@test",
        {"", "", "", "", 0, "", "", "", "file://notuser:notpassword@test"},
        {"file", "notuser", "notpassword", "test", 0, "/", "", "", "file://notuser:notpassword@test/"});
    checkURLDifferences("file://notuser:notpassword@test/",
        {"", "", "", "", 0, "", "", "", "file://notuser:notpassword@test/"},
        {"file", "notuser", "notpassword", "test", 0, "/", "", "", "file://notuser:notpassword@test/"});
    
    // This behavior matches Chrome and Firefox, but not WebKit using URL::parse.
    // The behavior of URL::parse is clearly wrong because reparsing file://path would make path the host.
    // The spec is unclear.
    checkURLDifferences("file:path",
        {"file", "", "", "", 0, "/path", "", "", "file:///path"},
        {"file", "", "", "", 0, "path", "", "", "file://path"});
    
    // FIXME: Fix and test incomplete percent encoded characters in the middle and end of the input string.
    // FIXME: Fix and test percent encoded upper case characters in the host.
    checkURLDifferences("http://host%73",
        {"http", "", "", "hosts", 0, "/", "", "", "http://hosts/"},
        {"http", "", "", "host%73", 0, "/", "", "", "http://host%73/"});
    
    // URLParser matches Chrome and the spec, but not URL::parse or Firefox.
    checkURLDifferences(wideString(L"http://０Ｘｃ０．０２５０．０１"),
        {"http", "", "", "192.168.0.1", 0, "/", "", "", "http://192.168.0.1/"},
        {"http", "", "", "0xc0.0250.01", 0, "/", "", "", "http://0xc0.0250.01/"});
    checkURLDifferences("http://host/path%2e.%2E",
        {"http", "", "", "host", 0, "/path...", "", "", "http://host/path..."},
        {"http", "", "", "host", 0, "/path%2e.%2E", "", "", "http://host/path%2e.%2E"});
}

TEST_F(URLParserTest, DefaultPort)
{
    checkURL("ftp://host:21/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("ftp://host:22/", {"ftp", "", "", "host", 22, "/", "", "", "ftp://host:22/"});
    checkURLDifferences("ftp://host:21",
        {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"},
        {"ftp", "", "", "host", 0, "", "", "", "ftp://host"});
    checkURLDifferences("ftp://host:22",
        {"ftp", "", "", "host", 22, "/", "", "", "ftp://host:22/"},
        {"ftp", "", "", "host", 22, "", "", "", "ftp://host:22"});
    
    checkURL("gopher://host:70/", {"gopher", "", "", "host", 0, "/", "", "", "gopher://host/"});
    checkURL("gopher://host:71/", {"gopher", "", "", "host", 71, "/", "", "", "gopher://host:71/"});
    // Spec, Chrome, Firefox, and URLParser have "/", URL::parse does not.
    // Spec, Chrome, URLParser, URL::parse recognize gopher default port, Firefox does not.
    checkURLDifferences("gopher://host:70",
        {"gopher", "", "", "host", 0, "/", "", "", "gopher://host/"},
        {"gopher", "", "", "host", 0, "", "", "", "gopher://host"});
    checkURLDifferences("gopher://host:71",
        {"gopher", "", "", "host", 71, "/", "", "", "gopher://host:71/"},
        {"gopher", "", "", "host", 71, "", "", "", "gopher://host:71"});
    
    checkURL("http://host:80", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkURL("http://host:80/", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkURL("http://host:81", {"http", "", "", "host", 81, "/", "", "", "http://host:81/"});
    checkURL("http://host:81/", {"http", "", "", "host", 81, "/", "", "", "http://host:81/"});
    
    checkURL("https://host:443", {"https", "", "", "host", 0, "/", "", "", "https://host/"});
    checkURL("https://host:443/", {"https", "", "", "host", 0, "/", "", "", "https://host/"});
    checkURL("https://host:444", {"https", "", "", "host", 444, "/", "", "", "https://host:444/"});
    checkURL("https://host:444/", {"https", "", "", "host", 444, "/", "", "", "https://host:444/"});
    
    checkURL("ws://host:80/", {"ws", "", "", "host", 0, "/", "", "", "ws://host/"});
    checkURL("ws://host:81/", {"ws", "", "", "host", 81, "/", "", "", "ws://host:81/"});
    // URLParser matches Chrome and Firefox, but not URL::parse
    checkURLDifferences("ws://host:80",
        {"ws", "", "", "host", 0, "/", "", "", "ws://host/"},
        {"ws", "", "", "host", 0, "", "", "", "ws://host"});
    checkURLDifferences("ws://host:81",
        {"ws", "", "", "host", 81, "/", "", "", "ws://host:81/"},
        {"ws", "", "", "host", 81, "", "", "", "ws://host:81"});
    
    checkURL("wss://host:443/", {"wss", "", "", "host", 0, "/", "", "", "wss://host/"});
    checkURL("wss://host:444/", {"wss", "", "", "host", 444, "/", "", "", "wss://host:444/"});
    // URLParser matches Chrome and Firefox, but not URL::parse
    checkURLDifferences("wss://host:443",
        {"wss", "", "", "host", 0, "/", "", "", "wss://host/"},
        {"wss", "", "", "host", 0, "", "", "", "wss://host"});
    checkURLDifferences("wss://host:444",
        {"wss", "", "", "host", 444, "/", "", "", "wss://host:444/"},
        {"wss", "", "", "host", 444, "", "", "", "wss://host:444"});

    // 990 is the default ftps port in URL::parse, but it's not in the URL spec. Maybe it should be.
    checkURL("ftps://host:990/", {"ftps", "", "", "host", 990, "/", "", "", "ftps://host:990/"});
    checkURL("ftps://host:991/", {"ftps", "", "", "host", 991, "/", "", "", "ftps://host:991/"});
    checkURLDifferences("ftps://host:990",
        {"ftps", "", "", "host", 990, "/", "", "", "ftps://host:990/"},
        {"ftps", "", "", "host", 990, "", "", "", "ftps://host:990"});
    checkURLDifferences("ftps://host:991",
        {"ftps", "", "", "host", 991, "/", "", "", "ftps://host:991/"},
        {"ftps", "", "", "host", 991, "", "", "", "ftps://host:991"});

    checkURL("unknown://host:80/", {"unknown", "", "", "host", 80, "/", "", "", "unknown://host:80/"});
    checkURL("unknown://host:81/", {"unknown", "", "", "host", 81, "/", "", "", "unknown://host:81/"});
    checkURLDifferences("unknown://host:80",
        {"unknown", "", "", "host", 80, "/", "", "", "unknown://host:80/"},
        {"unknown", "", "", "host", 80, "", "", "", "unknown://host:80"});
    checkURLDifferences("unknown://host:81",
        {"unknown", "", "", "host", 81, "/", "", "", "unknown://host:81/"},
        {"unknown", "", "", "host", 81, "", "", "", "unknown://host:81"});
}
    
static void shouldFail(const String& urlString)
{
    URLParser parser;
    auto invalidURL = parser.parse(urlString);
    checkURL(urlString, {"", "", "", "", 0, "", "", "", urlString});
}

static void shouldFail(const String& urlString, const String& baseString)
{
    URLParser parser;
    auto invalidURL = parser.parse(urlString);
    checkRelativeURL(urlString, baseString, {"", "", "", "", 0, "", "", "", urlString});
}
    
TEST_F(URLParserTest, ParserFailures)
{
    shouldFail("    ");
    shouldFail("");
    shouldFail("http://127.0.0.1:abc");
    shouldFail("http://host:abc");
    shouldFail("http://a:@", "about:blank");
    shouldFail("http://:b@", "about:blank");
    shouldFail("http://:@", "about:blank");
    shouldFail("http://a:@");
    shouldFail("http://:b@");
    shouldFail("http://@");
    shouldFail("http://[0:f::f:f:0:0]:abc");
}

} // namespace TestWebKitAPI
