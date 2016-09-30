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
#include <wtf/text/StringBuilder.h>

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

    bool isInvalid() const
    {
        return protocol.isEmpty()
            && user.isEmpty()
            && password.isEmpty()
            && host.isEmpty()
            && !port
            && path.isEmpty()
            && query.isEmpty()
            && fragment.isEmpty();
    }
};

static bool eq(const String& s1, const String& s2)
{
    EXPECT_STREQ(s1.utf8().data(), s2.utf8().data());
    return s1.utf8() == s2.utf8();
}

static void checkURL(const String& urlString, const ExpectedParts& parts, bool checkTabs = true)
{
    bool wasEnabled = URLParser::enabled();
    URLParser::setEnabled(true);
    auto url = URL(URL(), urlString);
    URLParser::setEnabled(false);
    auto oldURL = URL(URL(), urlString);
    URLParser::setEnabled(wasEnabled);
    
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port());
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));
    
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
    EXPECT_TRUE(URLParser::internalValuesConsistent(url));
    EXPECT_TRUE(URLParser::internalValuesConsistent(oldURL));

    if (checkTabs) {
        for (size_t i = 0; i < urlString.length(); ++i) {
            String urlStringWithTab = makeString(urlString.substring(0, i), "\t", urlString.substring(i));
            ExpectedParts invalidPartsWithTab = {"", "", "", "", 0, "" , "", "", urlStringWithTab};
            checkURL(urlStringWithTab, parts.isInvalid() ? invalidPartsWithTab : parts, false);
        }
    }
}

template<size_t length>
static String utf16String(const char16_t (&url)[length])
{
    StringBuilder builder;
    builder.reserveCapacity(length - 1);
    for (size_t i = 0; i < length - 1; ++i)
        builder.append(static_cast<UChar>(url[i]));
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
    checkURL("http://user:\t\t\tpass@webkit.org", {"http", "user", "pass", "webkit.org", 0, "/", "", "", "http://user:pass@webkit.org/"});
    checkURL("http://us\ter:pass@webkit.org", {"http", "user", "pass", "webkit.org", 0, "/", "", "", "http://user:pass@webkit.org/"});
    checkURL("http://user:pa\tss@webkit.org", {"http", "user", "pass", "webkit.org", 0, "/", "", "", "http://user:pass@webkit.org/"});
    checkURL("http://user:pass\t@webkit.org", {"http", "user", "pass", "webkit.org", 0, "/", "", "", "http://user:pass@webkit.org/"});
    checkURL("http://\tuser:pass@webkit.org", {"http", "user", "pass", "webkit.org", 0, "/", "", "", "http://user:pass@webkit.org/"});
    checkURL("http://user\t:pass@webkit.org", {"http", "user", "pass", "webkit.org", 0, "/", "", "", "http://user:pass@webkit.org/"});
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
    checkURL("http://[0:f:0:0:f::]:", {"http", "", "", "[0:f:0:0:f::]", 0, "/", "", "", "http://[0:f:0:0:f::]/"});
    checkURL("http://[0:f:0:0:f::]:\t", {"http", "", "", "[0:f:0:0:f::]", 0, "/", "", "", "http://[0:f:0:0:f::]/"});
    checkURL("http://[0:f:0:0:f::]\t:", {"http", "", "", "[0:f:0:0:f::]", 0, "/", "", "", "http://[0:f:0:0:f::]/"});
    checkURL("http://\t[::f:0:0:f:0:0]", {"http", "", "", "[::f:0:0:f:0:0]", 0, "/", "", "", "http://[::f:0:0:f:0:0]/"});
    checkURL("http://[\t::f:0:0:f:0:0]", {"http", "", "", "[::f:0:0:f:0:0]", 0, "/", "", "", "http://[::f:0:0:f:0:0]/"});
    checkURL("http://[:\t:f:0:0:f:0:0]", {"http", "", "", "[::f:0:0:f:0:0]", 0, "/", "", "", "http://[::f:0:0:f:0:0]/"});
    checkURL("http://[::\tf:0:0:f:0:0]", {"http", "", "", "[::f:0:0:f:0:0]", 0, "/", "", "", "http://[::f:0:0:f:0:0]/"});
    checkURL("http://[::f\t:0:0:f:0:0]", {"http", "", "", "[::f:0:0:f:0:0]", 0, "/", "", "", "http://[::f:0:0:f:0:0]/"});
    checkURL("http://[::f:\t0:0:f:0:0]", {"http", "", "", "[::f:0:0:f:0:0]", 0, "/", "", "", "http://[::f:0:0:f:0:0]/"});
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
    checkURL("file://lOcAlHoSt", {"file", "", "", "", 0, "/", "", "", "file:///"});
    checkURL("file://lOcAlHoSt/", {"file", "", "", "", 0, "/", "", "", "file:///"});
    checkURL("file:/pAtH/", {"file", "", "", "", 0, "/pAtH/", "", "", "file:///pAtH/"});
    checkURL("file:/pAtH", {"file", "", "", "", 0, "/pAtH", "", "", "file:///pAtH"});
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
    checkURL("http://user:@\thost", {"http", "user", "", "host", 0, "/", "", "", "http://user@host/"});
    checkURL("http://user:\t@host", {"http", "user", "", "host", 0, "/", "", "", "http://user@host/"});
    checkURL("http://user\t:@host", {"http", "user", "", "host", 0, "/", "", "", "http://user@host/"});
    checkURL("http://use\tr:@host", {"http", "user", "", "host", 0, "/", "", "", "http://user@host/"});
    checkURL("http://127.0.0.1:10100/path", {"http", "", "", "127.0.0.1", 10100, "/path", "", "", "http://127.0.0.1:10100/path"});
    checkURL("http://127.0.0.1:/path", {"http", "", "", "127.0.0.1", 0, "/path", "", "", "http://127.0.0.1/path"});
    checkURL("http://127.0.0.1\t:/path", {"http", "", "", "127.0.0.1", 0, "/path", "", "", "http://127.0.0.1/path"});
    checkURL("http://127.0.0.1:\t/path", {"http", "", "", "127.0.0.1", 0, "/path", "", "", "http://127.0.0.1/path"});
    checkURL("http://127.0.0.1:/\tpath", {"http", "", "", "127.0.0.1", 0, "/path", "", "", "http://127.0.0.1/path"});
    checkURL("http://127.0.0.1:123", {"http", "", "", "127.0.0.1", 123, "/", "", "", "http://127.0.0.1:123/"});
    checkURL("http://127.0.0.1:", {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"});
    checkURL("http://[0:f::f:f:0:0]:123/path", {"http", "", "", "[0:f::f:f:0:0]", 123, "/path", "", "", "http://[0:f::f:f:0:0]:123/path"});
    checkURL("http://[0:f::f:f:0:0]:123", {"http", "", "", "[0:f::f:f:0:0]", 123, "/", "", "", "http://[0:f::f:f:0:0]:123/"});
    checkURL("http://[0:f:0:0:f:\t:]:123", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f:0:0:f::\t]:123", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f:0:0:f::]\t:123", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f:0:0:f::]:\t123", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f:0:0:f::]:1\t23", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
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
    checkURL("notspecial:/notuser:notpassword@nothost", {"notspecial", "", "", "", 0, "/notuser:notpassword@nothost", "", "", "notspecial:/notuser:notpassword@nothost"});
    checkURL("sc://pa/", {"sc", "", "", "pa", 0, "/", "", "", "sc://pa/"});
    checkURL("sc://\tpa/", {"sc", "", "", "pa", 0, "/", "", "", "sc://pa/"});
    checkURL("sc:/\t/pa/", {"sc", "", "", "pa", 0, "/", "", "", "sc://pa/"});
    checkURL("sc:\t//pa/", {"sc", "", "", "pa", 0, "/", "", "", "sc://pa/"});
    checkURL("http://host   \a   ", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkURL("notspecial:/a", {"notspecial", "", "", "", 0, "/a", "", "", "notspecial:/a"});
    checkURL("notspecial:", {"notspecial", "", "", "", 0, "", "", "", "notspecial:"});
    checkURL("http:/a", {"http", "", "", "a", 0, "/", "", "", "http://a/"});
    checkURL("http://256../", {"http", "", "", "256..", 0, "/", "", "", "http://256../"});
    checkURL("http://256..", {"http", "", "", "256..", 0, "/", "", "", "http://256../"});
    checkURL("http://127..1/", {"http", "", "", "127..1", 0, "/", "", "", "http://127..1/"});
    checkURL("http://127.a.0.1/", {"http", "", "", "127.a.0.1", 0, "/", "", "", "http://127.a.0.1/"});
    checkURL("http://127.0.0.1/", {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"});
    checkURL("http://12\t7.0.0.1/", {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"});
    checkURL("http://127.\t0.0.1/", {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"});
    checkURL("http://./", {"http", "", "", ".", 0, "/", "", "", "http://./"});
    checkURL("http://.", {"http", "", "", ".", 0, "/", "", "", "http://./"});
    checkURL("http://123\t.256/", {"http", "", "", "123.256", 0, "/", "", "", "http://123.256/"});
    checkURL("http://123.\t256/", {"http", "", "", "123.256", 0, "/", "", "", "http://123.256/"});
    checkURL("notspecial:/a", {"notspecial", "", "", "", 0, "/a", "", "", "notspecial:/a"});
    checkURL("notspecial:", {"notspecial", "", "", "", 0, "", "", "", "notspecial:"});
    checkURL("notspecial:/", {"notspecial", "", "", "", 0, "/", "", "", "notspecial:/"});
    checkURL("data:image/png;base64,encoded-data-follows-here", {"data", "", "", "", 0, "image/png;base64,encoded-data-follows-here", "", "", "data:image/png;base64,encoded-data-follows-here"});
    checkURL("data:image/png;base64,encoded/data-with-slash", {"data", "", "", "", 0, "image/png;base64,encoded/data-with-slash", "", "", "data:image/png;base64,encoded/data-with-slash"});
    checkURL("about:~", {"about", "", "", "", 0, "~", "", "", "about:~"});
    checkURL("https://@test@test@example:800\\path@end", {"", "", "", "", 0, "", "", "", "https://@test@test@example:800\\path@end"});
    checkURL("http://www.example.com/#a\nb\rc\td", {"http", "", "", "www.example.com", 0, "/", "", "abcd", "http://www.example.com/#abcd"});
    checkURL("http://[A:b:c:DE:fF:0:1:aC]/", {"http", "", "", "[a:b:c:de:ff:0:1:ac]", 0, "/", "", "", "http://[a:b:c:de:ff:0:1:ac]/"});
    checkURL("http:////////user:@webkit.org:99?foo", {"http", "user", "", "webkit.org", 99, "/", "foo", "", "http://user@webkit.org:99/?foo"});
    checkURL("http:////////user:@webkit.org:99#foo", {"http", "user", "", "webkit.org", 99, "/", "", "foo", "http://user@webkit.org:99/#foo"});
    checkURL("http:////\t////user:@webkit.org:99?foo", {"http", "user", "", "webkit.org", 99, "/", "foo", "", "http://user@webkit.org:99/?foo"});
    checkURL("http://\t//\\///user:@webkit.org:99?foo", {"http", "user", "", "webkit.org", 99, "/", "foo", "", "http://user@webkit.org:99/?foo"});
    checkURL("http:/\\user:@webkit.org:99?foo", {"http", "user", "", "webkit.org", 99, "/", "foo", "", "http://user@webkit.org:99/?foo"});
    checkURL("http://127.0.0.1", {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"});
    checkURL("http://127.0.0.1.", {"http", "", "", "127.0.0.1.", 0, "/", "", "", "http://127.0.0.1./"});
    checkURL("http://127.0.0.1./", {"http", "", "", "127.0.0.1.", 0, "/", "", "", "http://127.0.0.1./"});

    // This disagrees with the web platform test for http://:@www.example.com but agrees with Chrome and URL::parse,
    // and Firefox fails the web platform test differently. Maybe the web platform test ought to be changed.
    checkURL("http://:@host", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
}

static void checkRelativeURL(const String& urlString, const String& baseURLString, const ExpectedParts& parts, bool checkTabs = true)
{
    bool wasEnabled = URLParser::enabled();
    URLParser::setEnabled(true);
    auto url = URL(URL(URL(), baseURLString), urlString);
    URLParser::setEnabled(false);
    auto oldURL = URL(URL(URL(), baseURLString), urlString);
    URLParser::setEnabled(wasEnabled);

    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port());
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));

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
    EXPECT_TRUE(URLParser::internalValuesConsistent(url));
    EXPECT_TRUE(URLParser::internalValuesConsistent(oldURL));
    
    if (checkTabs) {
        for (size_t i = 0; i < urlString.length(); ++i) {
            String urlStringWithTab = makeString(urlString.substring(0, i), "\t", urlString.substring(i));
            ExpectedParts invalidPartsWithTab = {"", "", "", "", 0, "" , "", "", urlStringWithTab};
            checkRelativeURL(urlStringWithTab, baseURLString, parts.isInvalid() ? invalidPartsWithTab : parts, false);
        }
    }
}

TEST_F(URLParserTest, ParseRelative)
{
    checkRelativeURL("/index.html", "http://webkit.org/path1/path2/", {"http", "", "", "webkit.org", 0, "/index.html", "", "", "http://webkit.org/index.html"});
    checkRelativeURL("http://whatwg.org/index.html", "http://webkit.org/path1/path2/", {"http", "", "", "whatwg.org", 0, "/index.html", "", "", "http://whatwg.org/index.html"});
    checkRelativeURL("index.html", "http://webkit.org/path1/path2/page.html?query#fragment", {"http", "", "", "webkit.org", 0, "/path1/path2/index.html", "", "", "http://webkit.org/path1/path2/index.html"});
    checkRelativeURL("//whatwg.org/index.html", "https://www.webkit.org/path", {"https", "", "", "whatwg.org", 0, "/index.html", "", "", "https://whatwg.org/index.html"});
    checkRelativeURL("http://example\t.\norg", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/", "", "", "http://example.org/"});
    checkRelativeURL("test", "file:///path1/path2", {"file", "", "", "", 0, "/path1/test", "", "", "file:///path1/test"});
    checkRelativeURL(utf16String(u"http://www.foo。bar.com"), "http://other.com/", {"http", "", "", "www.foo.bar.com", 0, "/", "", "", "http://www.foo.bar.com/"});
    checkRelativeURL(utf16String(u"sc://ñ.test/"), "about:blank", {"sc", "", "", "xn--ida.test", 0, "/", "", "", "sc://xn--ida.test/"});
    checkRelativeURL("#fragment", "http://host/path", {"http", "", "", "host", 0, "/path", "", "fragment", "http://host/path#fragment"});
    checkRelativeURL("?query", "http://host/path", {"http", "", "", "host", 0, "/path", "query", "", "http://host/path?query"});
    checkRelativeURL("?query#fragment", "http://host/path", {"http", "", "", "host", 0, "/path", "query", "fragment", "http://host/path?query#fragment"});
    checkRelativeURL(utf16String(u"?β"), "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "%CE%B2", "", "http://example.org/foo/bar?%CE%B2"});
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
    checkRelativeURL("\t", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar"});
    checkRelativeURL(" ", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar"});
    checkRelativeURL("  \a  \t\n", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar"});
    checkRelativeURL(":foo.com\\", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/:foo.com/", "", "", "http://example.org/foo/:foo.com/"});
    checkRelativeURL("http:/example.com/", "about:blank", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkRelativeURL("http:example.com/", "about:blank", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkRelativeURL("http:\\\\foo.com\\", "http://example.org/foo/bar", {"http", "", "", "foo.com", 0, "/", "", "", "http://foo.com/"});
    checkRelativeURL("http:\\\\foo.com/", "http://example.org/foo/bar", {"http", "", "", "foo.com", 0, "/", "", "", "http://foo.com/"});
    checkRelativeURL("http:\\\\foo.com", "http://example.org/foo/bar", {"http", "", "", "foo.com", 0, "/", "", "", "http://foo.com/"});
    checkRelativeURL("http://ExAmPlE.CoM", "http://other.com", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    checkRelativeURL("http:", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "", "http://example.org/foo/bar"});
    checkRelativeURL("#x", "data:,", {"data", "", "", "", 0, ",", "", "x", "data:,#x"});
    checkRelativeURL("#x", "about:blank", {"about", "", "", "", 0, "blank", "", "x", "about:blank#x"});
    checkRelativeURL("  foo.com  ", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/foo.com", "", "", "http://example.org/foo/foo.com"});
    checkRelativeURL(" \a baz", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/baz", "", "", "http://example.org/foo/baz"});
    checkRelativeURL("~", "http://example.org", {"http", "", "", "example.org", 0, "/~", "", "", "http://example.org/~"});
    checkRelativeURL("notspecial:", "about:blank", {"notspecial", "", "", "", 0, "", "", "", "notspecial:"});
    checkRelativeURL("notspecial:", "http://host", {"notspecial", "", "", "", 0, "", "", "", "notspecial:"});
    checkRelativeURL("http:", "http://host", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkRelativeURL("i", "sc:/pa/po", {"sc", "", "", "", 0, "/pa/i", "", "", "sc:/pa/i"});
    checkRelativeURL("i    ", "sc:/pa/po", {"sc", "", "", "", 0, "/pa/i", "", "", "sc:/pa/i"});
    checkRelativeURL("i\t\n  ", "sc:/pa/po", {"sc", "", "", "", 0, "/pa/i", "", "", "sc:/pa/i"});
    checkRelativeURL("i", "sc://ho/pa", {"sc", "", "", "ho", 0, "/i", "", "", "sc://ho/i"});
    checkRelativeURL("!", "sc://ho/pa", {"sc", "", "", "ho", 0, "/!", "", "", "sc://ho/!"});
    checkRelativeURL("!", "sc:/ho/pa", {"sc", "", "", "", 0, "/ho/!", "", "", "sc:/ho/!"});
    checkRelativeURL("notspecial:/", "about:blank", {"notspecial", "", "", "", 0, "/", "", "", "notspecial:/"});
    checkRelativeURL("notspecial:/", "http://host", {"notspecial", "", "", "", 0, "/", "", "", "notspecial:/"});
    checkRelativeURL("foo:/", "http://example.org/foo/bar", {"foo", "", "", "", 0, "/", "", "", "foo:/"});
    checkRelativeURL("://:0/", "http://webkit.org/", {"http", "", "", "webkit.org", 0, "/://:0/", "", "", "http://webkit.org/://:0/"});
    checkRelativeURL(String(), "http://webkit.org/", {"http", "", "", "webkit.org", 0, "/", "", "", "http://webkit.org/"});
    checkRelativeURL("https://@test@test@example:800\\path@end", "http://doesnotmatter/", {"", "", "", "", 0, "", "", "", "https://@test@test@example:800\\path@end"});
    checkRelativeURL("http://f:0/c", "http://example.org/foo/bar", {"http", "", "", "f", 0, "/c", "", "", "http://f:0/c"});

    // The checking of slashes in SpecialAuthoritySlashes needed to get this to pass contradicts what is in the spec,
    // but it is included in the web platform tests.
    checkRelativeURL("http:\\\\host\\foo", "about:blank", {"http", "", "", "host", 0, "/foo", "", "", "http://host/foo"});
}

static void checkURLDifferences(const String& urlString, const ExpectedParts& partsNew, const ExpectedParts& partsOld)
{
    bool wasEnabled = URLParser::enabled();
    URLParser::setEnabled(true);
    auto url = URL(URL(), urlString);
    URLParser::setEnabled(false);
    auto oldURL = URL(URL(), urlString);
    URLParser::setEnabled(wasEnabled);

    EXPECT_TRUE(eq(partsNew.protocol, url.protocol()));
    EXPECT_TRUE(eq(partsNew.user, url.user()));
    EXPECT_TRUE(eq(partsNew.password, url.pass()));
    EXPECT_TRUE(eq(partsNew.host, url.host()));
    EXPECT_EQ(partsNew.port, url.port());
    EXPECT_TRUE(eq(partsNew.path, url.path()));
    EXPECT_TRUE(eq(partsNew.query, url.query()));
    EXPECT_TRUE(eq(partsNew.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(partsNew.string, url.string()));
    
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
    EXPECT_TRUE(URLParser::internalValuesConsistent(url));
    EXPECT_TRUE(URLParser::internalValuesConsistent(oldURL));
    
    // FIXME: check tabs here like we do for checkURL and checkRelativeURL.
}

static void checkRelativeURLDifferences(const String& urlString, const String& baseURLString, const ExpectedParts& partsNew, const ExpectedParts& partsOld)
{
    bool wasEnabled = URLParser::enabled();
    URLParser::setEnabled(true);
    auto url = URL(URL(URL(), baseURLString), urlString);
    URLParser::setEnabled(false);
    auto oldURL = URL(URL(URL(), baseURLString), urlString);
    URLParser::setEnabled(wasEnabled);

    EXPECT_TRUE(eq(partsNew.protocol, url.protocol()));
    EXPECT_TRUE(eq(partsNew.user, url.user()));
    EXPECT_TRUE(eq(partsNew.password, url.pass()));
    EXPECT_TRUE(eq(partsNew.host, url.host()));
    EXPECT_EQ(partsNew.port, url.port());
    EXPECT_TRUE(eq(partsNew.path, url.path()));
    EXPECT_TRUE(eq(partsNew.query, url.query()));
    EXPECT_TRUE(eq(partsNew.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(partsNew.string, url.string()));
    
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
    EXPECT_TRUE(URLParser::internalValuesConsistent(url));
    EXPECT_TRUE(URLParser::internalValuesConsistent(oldURL));

    // FIXME: check tabs here like we do for checkURL and checkRelativeURL.
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
    checkURLDifferences("http://[a:0:0:0:b:c::d]",
        {"http", "", "", "[a::b:c:0:d]", 0, "/", "", "", "http://[a::b:c:0:d]/"},
        {"http", "", "", "[a:0:0:0:b:c::d]", 0, "/", "", "", "http://[a:0:0:0:b:c::d]/"});
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
    checkRelativeURLDifferences(utf16String(u"#β"), "http://example.org/foo/bar",
        {"http", "", "", "example.org", 0, "/foo/bar", "", utf16String(u"β"), utf16String(u"http://example.org/foo/bar#β")},
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
    checkRelativeURLDifferences("http:/", "about:blank",
        {"", "", "", "", 0, "", "", "", "http:/"},
        {"http", "", "", "", 0, "/", "", "", "http:/"});
    checkRelativeURLDifferences("http:", "about:blank",
        {"http", "", "", "", 0, "", "", "", "http:"},
        {"http", "", "", "", 0, "/", "", "", "http:/"});
    checkRelativeURLDifferences("http:/", "http://host",
        {"", "", "", "", 0, "", "", "", "http:/"},
        {"http", "", "", "", 0, "/", "", "", "http:/"});
    checkURLDifferences("http:/",
        {"", "", "", "", 0, "", "", "", "http:/"},
        {"http", "", "", "", 0, "/", "", "", "http:/"});
    checkURLDifferences("http:",
        {"http", "", "", "", 0, "", "", "", "http:"},
        {"http", "", "", "", 0, "/", "", "", "http:/"});
    checkRelativeURLDifferences("http:/example.com/", "http://example.org/foo/bar",
        {"http", "", "", "example.org", 0, "/example.com/", "", "", "http://example.org/example.com/"},
        {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
    
    // This behavior matches Chrome and Firefox, but not WebKit using URL::parse.
    // The behavior of URL::parse is clearly wrong because reparsing file://path would make path the host.
    // The spec is unclear.
    checkURLDifferences("file:path",
        {"file", "", "", "", 0, "/path", "", "", "file:///path"},
        {"file", "", "", "", 0, "path", "", "", "file://path"});
    checkURLDifferences("file:pAtH",
        {"file", "", "", "", 0, "/pAtH", "", "", "file:///pAtH"},
        {"file", "", "", "", 0, "pAtH", "", "", "file://pAtH"});
    checkURLDifferences("file:pAtH/",
        {"file", "", "", "", 0, "/pAtH/", "", "", "file:///pAtH/"},
        {"file", "", "", "", 0, "pAtH/", "", "", "file://pAtH/"});
    
    // FIXME: Fix and test incomplete percent encoded characters in the middle and end of the input string.
    // FIXME: Fix and test percent encoded upper case characters in the host.
    checkURLDifferences("http://host%73",
        {"http", "", "", "hosts", 0, "/", "", "", "http://hosts/"},
        {"http", "", "", "host%73", 0, "/", "", "", "http://host%73/"});
    
    // URLParser matches Chrome and the spec, but not URL::parse or Firefox.
    checkURLDifferences(utf16String(u"http://０Ｘｃ０．０２５０．０１"),
        {"http", "", "", "192.168.0.1", 0, "/", "", "", "http://192.168.0.1/"},
        {"http", "", "", "0xc0.0250.01", 0, "/", "", "", "http://0xc0.0250.01/"});
    checkURLDifferences("http://host/path%2e.%2E",
        {"http", "", "", "host", 0, "/path...", "", "", "http://host/path..."},
        {"http", "", "", "host", 0, "/path%2e.%2E", "", "", "http://host/path%2e.%2E"});

    checkRelativeURLDifferences(utf16String(u"http://foo:💩@example.com/bar"), "http://other.com/",
        {"http", "foo", utf16String(u"💩"), "example.com", 0, "/bar", "", "", "http://foo:%F0%9F%92%A9@example.com/bar"},
        {"", "", "", "", 0, "", "", "", utf16String(u"http://foo:💩@example.com/bar")});
    checkRelativeURLDifferences("http://&a:foo(b]c@d:2/", "http://example.org/foo/bar",
        {"http", "&a", "foo(b]c", "d", 2, "/", "", "", "http://&a:foo(b%5Dc@d:2/"},
        {"", "", "", "", 0, "", "", "", "http://&a:foo(b]c@d:2/"});
    checkRelativeURLDifferences("http://`{}:`{}@h/`{}?`{}", "http://doesnotmatter/",
        {"http", "`{}", "`{}", "h", 0, "/%60%7B%7D", "`{}", "", "http://%60%7B%7D:%60%7B%7D@h/%60%7B%7D?`{}"},
        {"", "", "", "", 0, "", "", "", "http://`{}:`{}@h/`{}?`{}"});
    checkURLDifferences("http://[0:f::f::f]",
        {"", "", "", "", 0, "" , "", "", "http://[0:f::f::f]"},
        {"http", "", "", "[0:f::f::f]", 0, "/" , "", "", "http://[0:f::f::f]/"});
    checkURLDifferences("http://123",
        {"http", "", "", "0.0.0.123", 0, "/", "", "", "http://0.0.0.123/"},
        {"http", "", "", "123", 0, "/", "", "", "http://123/"});
    checkURLDifferences("http://123.234/",
        {"http", "", "", "123.0.0.234", 0, "/", "", "", "http://123.0.0.234/"},
        {"http", "", "", "123.234", 0, "/", "", "", "http://123.234/"});
    checkURLDifferences("http://123.234.012",
        {"http", "", "", "123.234.0.10", 0, "/", "", "", "http://123.234.0.10/"},
        {"http", "", "", "123.234.012", 0, "/", "", "", "http://123.234.012/"});
    checkURLDifferences("http://123.234.12",
        {"http", "", "", "123.234.0.12", 0, "/", "", "", "http://123.234.0.12/"},
        {"http", "", "", "123.234.12", 0, "/", "", "", "http://123.234.12/"});
    checkRelativeURLDifferences("file:c:\\foo\\bar.html", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/c:/foo/bar.html", "", "", "file:///c:/foo/bar.html"},
        {"file", "", "", "", 0, "/tmp/mock/c:/foo/bar.html", "", "", "file:///tmp/mock/c:/foo/bar.html"});
    checkRelativeURLDifferences("  File:c|////foo\\bar.html", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/c:////foo/bar.html", "", "", "file:///c:////foo/bar.html"},
        {"file", "", "", "", 0, "/tmp/mock/c|////foo/bar.html", "", "", "file:///tmp/mock/c|////foo/bar.html"});
    checkRelativeURLDifferences("  Fil\t\n\te\n\t\n:\t\n\tc\t\n\t|\n\t\n/\t\n\t/\n\t\n//foo\\bar.html", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/c:////foo/bar.html", "", "", "file:///c:////foo/bar.html"},
        {"file", "", "", "", 0, "/tmp/mock/c|////foo/bar.html", "", "", "file:///tmp/mock/c|////foo/bar.html"});
    checkRelativeURLDifferences("C|/foo/bar", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/C:/foo/bar", "", "", "file:///C:/foo/bar"},
        {"file", "", "", "", 0, "/tmp/mock/C|/foo/bar", "", "", "file:///tmp/mock/C|/foo/bar"});
    checkRelativeURLDifferences("/C|/foo/bar", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/C:/foo/bar", "", "", "file:///C:/foo/bar"},
        {"file", "", "", "", 0, "/C|/foo/bar", "", "", "file:///C|/foo/bar"});
    checkRelativeURLDifferences("https://@test@test@example:800/", "http://doesnotmatter/",
        {"https", "@test@test", "", "example", 800, "/", "", "", "https://%40test%40test@example:800/"},
        {"", "", "", "", 0, "", "", "", "https://@test@test@example:800/"});
    checkRelativeURLDifferences("https://@test@test@example:800/path@end", "http://doesnotmatter/",
        {"https", "@test@test", "", "example", 800, "/path@end", "", "", "https://%40test%40test@example:800/path@end"},
        {"", "", "", "", 0, "", "", "", "https://@test@test@example:800/path@end"});
    checkURLDifferences("notspecial://@test@test@example:800/path@end",
        {"notspecial", "@test@test", "", "example", 800, "/path@end", "", "", "notspecial://%40test%40test@example:800/path@end"},
        {"", "", "", "", 0, "", "", "", "notspecial://@test@test@example:800/path@end"});
    checkURLDifferences("notspecial://@test@test@example:800\\path@end",
        {"notspecial", "@test@test@example", "800\\path", "end", 0, "/", "", "", "notspecial://%40test%40test%40example:800%5Cpath@end/"},
        {"", "", "", "", 0, "", "", "", "notspecial://@test@test@example:800\\path@end"});
    checkRelativeURLDifferences("foo://", "http://example.org/foo/bar",
        {"foo", "", "", "", 0, "/", "", "", "foo:///"},
        {"foo", "", "", "", 0, "//", "", "", "foo://"});
    checkURLDifferences(utf16String(u"http://host?ß😍#ß😍"),
        {"http", "", "", "host", 0, "/", "%C3%9F%F0%9F%98%8D", utf16String(u"ß😍"), utf16String(u"http://host/?%C3%9F%F0%9F%98%8D#ß😍")},
        {"http", "", "", "host", 0, "/", "%C3%9F%F0%9F%98%8D", "%C3%9F%F0%9F%98%8D", "http://host/?%C3%9F%F0%9F%98%8D#%C3%9F%F0%9F%98%8D"});
    checkURLDifferences(utf16String(u"http://host/path#💩\t💩"),
        {"http", "", "", "host", 0, "/path", "", utf16String(u"💩💩"), utf16String(u"http://host/path#💩💩")},
        {"http", "", "", "host", 0, "/path", "", "%F0%9F%92%A9%F0%9F%92%A9", "http://host/path#%F0%9F%92%A9%F0%9F%92%A9"});
    checkURLDifferences("http://%48OsT",
        {"http", "", "", "host", 0, "/", "", "", "http://host/"},
        {"http", "", "", "%48ost", 0, "/", "", "", "http://%48ost/"});
    checkURLDifferences("http://h%4FsT",
        {"http", "", "", "host", 0, "/", "", "", "http://host/"},
        {"http", "", "", "h%4fst", 0, "/", "", "", "http://h%4fst/"});
    checkURLDifferences("http://h%4fsT",
        {"http", "", "", "host", 0, "/", "", "", "http://host/"},
        {"http", "", "", "h%4fst", 0, "/", "", "", "http://h%4fst/"});
    checkURLDifferences("http://h%6fsT",
        {"http", "", "", "host", 0, "/", "", "", "http://host/"},
        {"http", "", "", "h%6fst", 0, "/", "", "", "http://h%6fst/"});
    checkURLDifferences("http://host/`",
        {"http", "", "", "host", 0, "/%60", "", "", "http://host/%60"},
        {"http", "", "", "host", 0, "/`", "", "", "http://host/`"});
    checkURLDifferences("aA://",
        {"aa", "", "", "", 0, "/", "", "", "aa:///"},
        {"aa", "", "", "", 0, "//", "", "", "aa://"});
    checkURLDifferences("A://",
        {"a", "", "", "", 0, "/", "", "", "a:///"},
        {"a", "", "", "", 0, "//", "", "", "a://"});
    checkRelativeURLDifferences("//C|/foo/bar", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/C:/foo/bar", "", "", "file:///C:/foo/bar"},
        {"", "", "", "", 0, "", "", "", "//C|/foo/bar"});
    checkRelativeURLDifferences("//C:/foo/bar", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/C:/foo/bar", "", "", "file:///C:/foo/bar"},
        {"file", "", "", "c", 0, "/foo/bar", "", "", "file://c/foo/bar"});
    checkRelativeURLDifferences("//C|?foo/bar", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/C:/", "foo/bar", "", "file:///C:/?foo/bar"},
        {"", "", "", "", 0, "", "", "", "//C|?foo/bar"});
    checkRelativeURLDifferences("//C|#foo/bar", "file:///tmp/mock/path",
        {"file", "", "", "", 0, "/C:/", "", "foo/bar", "file:///C:/#foo/bar"},
        {"", "", "", "", 0, "", "", "", "//C|#foo/bar"});
    checkURLDifferences("http://0xFFFFFfFF/",
        {"http", "", "", "255.255.255.255", 0, "/", "", "", "http://255.255.255.255/"},
        {"http", "", "", "0xffffffff", 0, "/", "", "", "http://0xffffffff/"});
    checkURLDifferences("http://0000000000000000037777777777/",
        {"http", "", "", "255.255.255.255", 0, "/", "", "", "http://255.255.255.255/"},
        {"http", "", "", "0000000000000000037777777777", 0, "/", "", "", "http://0000000000000000037777777777/"});
    checkURLDifferences("http://4294967295/",
        {"http", "", "", "255.255.255.255", 0, "/", "", "", "http://255.255.255.255/"},
        {"http", "", "", "4294967295", 0, "/", "", "", "http://4294967295/"});
    checkURLDifferences("http://256/",
        {"http", "", "", "0.0.1.0", 0, "/", "", "", "http://0.0.1.0/"},
        {"http", "", "", "256", 0, "/", "", "", "http://256/"});
    checkURLDifferences("http://256./",
        {"http", "", "", "0.0.1.0", 0, "/", "", "", "http://0.0.1.0/"},
        {"http", "", "", "256.", 0, "/", "", "", "http://256./"});
    checkURLDifferences("http://123.256/",
        {"http", "", "", "123.0.1.0", 0, "/", "", "", "http://123.0.1.0/"},
        {"http", "", "", "123.256", 0, "/", "", "", "http://123.256/"});
    checkURLDifferences("http://127.%.0.1/",
        {"", "", "", "", 0, "", "", "", "http://127.%.0.1/"},
        {"http", "", "", "127.%.0.1", 0, "/", "", "", "http://127.%.0.1/"});
    checkURLDifferences("http://[1:2:3:4:5:6:7:8:]/",
        {"", "", "", "", 0, "", "", "", "http://[1:2:3:4:5:6:7:8:]/"},
        {"http", "", "", "[1:2:3:4:5:6:7:8:]", 0, "/", "", "", "http://[1:2:3:4:5:6:7:8:]/"});
    checkURLDifferences("http://[:2:3:4:5:6:7:8:]/",
        {"", "", "", "", 0, "", "", "", "http://[:2:3:4:5:6:7:8:]/"},
        {"http", "", "", "[:2:3:4:5:6:7:8:]", 0, "/", "", "", "http://[:2:3:4:5:6:7:8:]/"});
    checkURLDifferences("http://[1:2:3:4:5:6:7::]/",
        {"http", "", "", "[1:2:3:4:5:6:7:0]", 0, "/", "", "", "http://[1:2:3:4:5:6:7:0]/"},
        {"http", "", "", "[1:2:3:4:5:6:7::]", 0, "/", "", "", "http://[1:2:3:4:5:6:7::]/"});
    checkURLDifferences("http://[1:2:3:4:5:6:7:::]/",
        {"", "", "", "", 0, "", "", "", "http://[1:2:3:4:5:6:7:::]/"},
        {"http", "", "", "[1:2:3:4:5:6:7:::]", 0, "/", "", "", "http://[1:2:3:4:5:6:7:::]/"});
    checkURLDifferences("http://127.0.0.1~/",
        {"http", "", "", "127.0.0.1~", 0, "/", "", "", "http://127.0.0.1~/"},
        {"", "", "", "", 0, "", "", "", "http://127.0.0.1~/"});
    checkURLDifferences("http://127.0.1~/",
        {"http", "", "", "127.0.1~", 0, "/", "", "", "http://127.0.1~/"},
        {"", "", "", "", 0, "", "", "", "http://127.0.1~/"});
    checkURLDifferences("http://127.0.1./",
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"},
        {"http", "", "", "127.0.1.", 0, "/", "", "", "http://127.0.1./"});
    checkURLDifferences("http://127.0.1.~/",
        {"http", "", "", "127.0.1.~", 0, "/", "", "", "http://127.0.1.~/"},
        {"", "", "", "", 0, "", "", "", "http://127.0.1.~/"});
    checkURLDifferences("http://127.0.1.~",
        {"http", "", "", "127.0.1.~", 0, "/", "", "", "http://127.0.1.~/"},
        {"", "", "", "", 0, "", "", "", "http://127.0.1.~"});
    checkRelativeURLDifferences("http://f:000/c", "http://example.org/foo/bar",
        {"http", "", "", "f", 0, "/c", "", "", "http://f:0/c"},
        {"http", "", "", "f", 0, "/c", "", "", "http://f:000/c"});
    checkRelativeURLDifferences("http://f:010/c", "http://example.org/foo/bar",
        {"http", "", "", "f", 10, "/c", "", "", "http://f:10/c"},
        {"http", "", "", "f", 10, "/c", "", "", "http://f:010/c"});
    checkURL("http://0.0.0.0x100/", {"http", "", "", "0.0.0.0x100", 0, "/", "", "", "http://0.0.0.0x100/"});
}

TEST_F(URLParserTest, DefaultPort)
{
    checkURL("FtP://host:21/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("ftp://host:21/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("f\ttp://host:21/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("f\ttp://host\t:21/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("f\ttp://host:\t21/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("f\ttp://host:2\t1/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("f\ttp://host:21\t/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("ftp://host\t:21/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("ftp://host:\t21/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("ftp://host:2\t1/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("ftp://host:21\t/", {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"});
    checkURL("ftp://host:22/", {"ftp", "", "", "host", 22, "/", "", "", "ftp://host:22/"});
    checkURLDifferences("ftp://host:21",
        {"ftp", "", "", "host", 0, "/", "", "", "ftp://host/"},
        {"ftp", "", "", "host", 0, "", "", "", "ftp://host"});
    checkURLDifferences("ftp://host:22",
        {"ftp", "", "", "host", 22, "/", "", "", "ftp://host:22/"},
        {"ftp", "", "", "host", 22, "", "", "", "ftp://host:22"});
    
    checkURL("gOpHeR://host:70/", {"gopher", "", "", "host", 0, "/", "", "", "gopher://host/"});
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
    
    checkURL("hTtP://host:80", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkURL("http://host:80", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkURL("http://host:80/", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkURL("http://host:81", {"http", "", "", "host", 81, "/", "", "", "http://host:81/"});
    checkURL("http://host:81/", {"http", "", "", "host", 81, "/", "", "", "http://host:81/"});
    
    checkURL("hTtPs://host:443", {"https", "", "", "host", 0, "/", "", "", "https://host/"});
    checkURL("https://host:443", {"https", "", "", "host", 0, "/", "", "", "https://host/"});
    checkURL("https://host:443/", {"https", "", "", "host", 0, "/", "", "", "https://host/"});
    checkURL("https://host:444", {"https", "", "", "host", 444, "/", "", "", "https://host:444/"});
    checkURL("https://host:444/", {"https", "", "", "host", 444, "/", "", "", "https://host:444/"});
    
    checkURL("wS://host:80/", {"ws", "", "", "host", 0, "/", "", "", "ws://host/"});
    checkURL("ws://host:80/", {"ws", "", "", "host", 0, "/", "", "", "ws://host/"});
    checkURL("ws://host:81/", {"ws", "", "", "host", 81, "/", "", "", "ws://host:81/"});
    // URLParser matches Chrome and Firefox, but not URL::parse
    checkURLDifferences("ws://host:80",
        {"ws", "", "", "host", 0, "/", "", "", "ws://host/"},
        {"ws", "", "", "host", 0, "", "", "", "ws://host"});
    checkURLDifferences("ws://host:81",
        {"ws", "", "", "host", 81, "/", "", "", "ws://host:81/"},
        {"ws", "", "", "host", 81, "", "", "", "ws://host:81"});
    
    checkURL("WsS://host:443/", {"wss", "", "", "host", 0, "/", "", "", "wss://host/"});
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
    checkURL("fTpS://host:990/", {"ftps", "", "", "host", 990, "/", "", "", "ftps://host:990/"});
    checkURL("ftps://host:990/", {"ftps", "", "", "host", 990, "/", "", "", "ftps://host:990/"});
    checkURL("ftps://host:991/", {"ftps", "", "", "host", 991, "/", "", "", "ftps://host:991/"});
    checkURLDifferences("ftps://host:990",
        {"ftps", "", "", "host", 990, "/", "", "", "ftps://host:990/"},
        {"ftps", "", "", "host", 990, "", "", "", "ftps://host:990"});
    checkURLDifferences("ftps://host:991",
        {"ftps", "", "", "host", 991, "/", "", "", "ftps://host:991/"},
        {"ftps", "", "", "host", 991, "", "", "", "ftps://host:991"});

    checkURL("uNkNoWn://host:80/", {"unknown", "", "", "host", 80, "/", "", "", "unknown://host:80/"});
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
    checkURL(urlString, {"", "", "", "", 0, "", "", "", urlString});
}

static void shouldFail(const String& urlString, const String& baseString)
{
    checkRelativeURL(urlString, baseString, {"", "", "", "", 0, "", "", "", urlString});
}

TEST_F(URLParserTest, ParserFailures)
{
    shouldFail("    ");
    shouldFail("  \a  ");
    shouldFail("");
    shouldFail(String());
    shouldFail("", "about:blank");
    shouldFail(String(), "about:blank");
    shouldFail("http://127.0.0.1:abc");
    shouldFail("http://host:abc");
    shouldFail("http://a:@", "about:blank");
    shouldFail("http://:b@", "about:blank");
    shouldFail("http://:@", "about:blank");
    shouldFail("http://a:@");
    shouldFail("http://:b@");
    shouldFail("http://@");
    shouldFail("http://[0:f::f:f:0:0]:abc");
    shouldFail("../i", "sc:sd");
    shouldFail("../i", "sc:sd/sd");
    shouldFail("/i", "sc:sd");
    shouldFail("/i", "sc:sd/sd");
    shouldFail("?i", "sc:sd");
    shouldFail("?i", "sc:sd/sd");
    shouldFail("http://example example.com", "http://other.com/");
    shouldFail("http://[www.example.com]/", "about:blank");
    shouldFail("http://192.168.0.1 hello", "http://other.com/");
    shouldFail("http://[example.com]", "http://other.com/");
    shouldFail("i", "sc:sd");
    shouldFail("i", "sc:sd/sd");
    shouldFail("i");
    shouldFail("asdf");
    shouldFail("~");
    shouldFail("~", "about:blank");
    shouldFail("~~~");
    shouldFail("://:0/");
    shouldFail("://:0/", "");
    shouldFail("://:0/", "about:blank");
    shouldFail("about~");
    shouldFail("//C:asdf/foo/bar", "file:///tmp/mock/path");
    shouldFail("http://[1234::ab#]");
    shouldFail("http://[1234::ab/]");
    shouldFail("http://[1234::ab?]");
    shouldFail("http://[1234::ab@]");
    shouldFail("http://[1234::ab~]");
    shouldFail("http://[2001::1");
    shouldFail("http://[1:2:3:4:5:6:7:8~]/");
}

// These are in the spec but not in the web platform tests.
TEST_F(URLParserTest, AdditionalTests)
{
    checkURL("about:\a\aabc", {"about", "", "", "", 0, "%07%07abc", "", "", "about:%07%07abc"});
    checkURL("notspecial:\t\t\n\t", {"notspecial", "", "", "", 0, "", "", "", "notspecial:"});
    checkURLDifferences("notspecial\t\t\n\t:\t\t\n\t/\t\t\n\t/\t\t\n\thost",
        {"notspecial", "", "", "host", 0, "/", "", "", "notspecial://host/"},
        {"notspecial", "", "", "host", 0, "", "", "", "notspecial://host"});
    checkRelativeURL("http:", "http://example.org/foo/bar?query#fragment", {"http", "", "", "example.org", 0, "/foo/bar", "query", "", "http://example.org/foo/bar?query"});
    checkRelativeURLDifferences("ws:", "http://example.org/foo/bar",
        {"ws", "", "", "", 0, "", "", "", "ws:"},
        {"ws", "", "", "", 0, "s:", "", "", "ws:s:"});
    checkRelativeURL("notspecial:", "http://example.org/foo/bar", {"notspecial", "", "", "", 0, "", "", "", "notspecial:"});

    const wchar_t surrogateBegin = 0xD800;
    const wchar_t validSurrogateEnd = 0xDD55;
    const wchar_t invalidSurrogateEnd = 'A';
    checkURL(utf16String<12>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', surrogateBegin, validSurrogateEnd, '\0'}),
        {"http", "", "", "w", 0, "/%F0%90%85%95", "", "", "http://w/%F0%90%85%95"}, false);

    // URLParser matches Chrome and Firefox but not URL::parse.
    checkURLDifferences(utf16String<12>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', surrogateBegin, invalidSurrogateEnd}),
        {"http", "", "", "w", 0, "/%EF%BF%BDA", "", "", "http://w/%EF%BF%BDA"},
        {"http", "", "", "w", 0, "/%ED%A0%80A", "", "", "http://w/%ED%A0%80A"});
    checkURLDifferences(utf16String<13>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', '?', surrogateBegin, invalidSurrogateEnd, '\0'}),
        {"http", "", "", "w", 0, "/", "%EF%BF%BDA", "", "http://w/?%EF%BF%BDA"},
        {"http", "", "", "w", 0, "/", "%ED%A0%80A", "", "http://w/?%ED%A0%80A"});
    checkURLDifferences(utf16String<11>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', surrogateBegin, '\0'}),
        {"http", "", "", "w", 0, "/%EF%BF%BD", "", "", "http://w/%EF%BF%BD"},
        {"http", "", "", "w", 0, "/%ED%A0%80", "", "", "http://w/%ED%A0%80"});
    checkURLDifferences(utf16String<12>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', '?', surrogateBegin, '\0'}),
        {"http", "", "", "w", 0, "/", "%EF%BF%BD", "", "http://w/?%EF%BF%BD"},
        {"http", "", "", "w", 0, "/", "%ED%A0%80", "", "http://w/?%ED%A0%80"});
    checkURLDifferences(utf16String<13>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', '?', surrogateBegin, ' ', '\0'}),
        {"http", "", "", "w", 0, "/", "%EF%BF%BD", "", "http://w/?%EF%BF%BD"},
        {"http", "", "", "w", 0, "/", "%ED%A0%80", "", "http://w/?%ED%A0%80"});
}

static void checkURL(const String& urlString, const TextEncoding& encoding, const ExpectedParts& parts, bool checkTabs = true)
{
    URLParser parser(urlString, { }, encoding);
    auto url = parser.result();
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port());
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));

    if (checkTabs) {
        for (size_t i = 0; i < urlString.length(); ++i) {
            String urlStringWithTab = makeString(urlString.substring(0, i), "\t", urlString.substring(i));
            ExpectedParts invalidPartsWithTab = {"", "", "", "", 0, "" , "", "", urlStringWithTab};
            checkURL(urlStringWithTab, encoding, parts.isInvalid() ? invalidPartsWithTab : parts, false);
        }
    }
}

TEST_F(URLParserTest, QueryEncoding)
{
    checkURL(utf16String(u"http://host?ß😍#ß😍"), UTF8Encoding(), {"http", "", "", "host", 0, "/", "%C3%9F%F0%9F%98%8D", utf16String(u"ß😍"), utf16String(u"http://host/?%C3%9F%F0%9F%98%8D#ß😍")}, false);
    checkURL(utf16String(u"http://host?ß😍#ß😍"), UTF8Encoding(), {"http", "", "", "host", 0, "/", "%C3%9F%F0%9F%98%8D", utf16String(u"ß😍"), utf16String(u"http://host/?%C3%9F%F0%9F%98%8D#ß😍")}, false);

    TextEncoding latin1(String("latin1"));
    checkURL("http://host/?query with%20spaces", latin1, {"http", "", "", "host", 0, "/", "query%20with%20spaces", "", "http://host/?query%20with%20spaces"});
    checkURL("http://host/?query", latin1, {"http", "", "", "host", 0, "/", "query", "", "http://host/?query"});
    checkURL("http://host/?\tquery", latin1, {"http", "", "", "host", 0, "/", "query", "", "http://host/?query"});
    checkURL("http://host/?q\tuery", latin1, {"http", "", "", "host", 0, "/", "query", "", "http://host/?query"});
    checkURL("http://host/?query with SpAcEs#fragment", latin1, {"http", "", "", "host", 0, "/", "query%20with%20SpAcEs", "fragment", "http://host/?query%20with%20SpAcEs#fragment"});

    TextEncoding unrecognized(String("unrecognized invalid encoding name"));
    checkURL("http://host/?query", unrecognized, {"http", "", "", "host", 0, "/", "", "", "http://host/?"});
    checkURL("http://host/?", unrecognized, {"http", "", "", "host", 0, "/", "", "", "http://host/?"});

    // FIXME: Add more tests with other encodings and things like non-ascii characters, emoji and unmatched surrogate pairs.
}

} // namespace TestWebKitAPI
