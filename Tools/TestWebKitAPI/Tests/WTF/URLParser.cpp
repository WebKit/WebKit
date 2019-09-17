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
#include "WTFStringUtilities.h"
#include <wtf/MainThread.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

class WTF_URLParser : public testing::Test {
public:
    void SetUp() final
    {
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

template<typename T, typename U>
bool eq(T&& s1, U&& s2)
{
    EXPECT_STREQ(s1.utf8().data(), s2.utf8().data());
    return s1.utf8() == s2.utf8();
}

static String insertTabAtLocation(const String& string, size_t location)
{
    ASSERT(location <= string.length());
    return makeString(string.substring(0, location), "\t", string.substring(location));
}

static ExpectedParts invalidParts(const String& urlStringWithTab)
{
    return {"", "", "", "", 0, "" , "", "", urlStringWithTab};
}

enum class TestTabs { No, Yes };

// Inserting tabs between surrogate pairs changes the encoded value instead of being skipped by the URLParser.
const TestTabs testTabsValueForSurrogatePairs = TestTabs::No;

static void checkURL(const String& urlString, const ExpectedParts& parts, TestTabs testTabs = TestTabs::Yes)
{
    auto url = URL(URL(), urlString);
    
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port().valueOr(0));
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));

    EXPECT_TRUE(WTF::URLParser::internalValuesConsistent(url));

    if (testTabs == TestTabs::No)
        return;

    for (size_t i = 0; i < urlString.length(); ++i) {
        String urlStringWithTab = insertTabAtLocation(urlString, i);
        checkURL(urlStringWithTab,
            parts.isInvalid() ? invalidParts(urlStringWithTab) : parts,
            TestTabs::No);
    }
}

static void checkRelativeURL(const String& urlString, const String& baseURLString, const ExpectedParts& parts, TestTabs testTabs = TestTabs::Yes)
{
    auto url = URL(URL(URL(), baseURLString), urlString);
    
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port().valueOr(0));
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));
    
    EXPECT_TRUE(WTF::URLParser::internalValuesConsistent(url));
    
    if (testTabs == TestTabs::No)
        return;
    
    for (size_t i = 0; i < urlString.length(); ++i) {
        String urlStringWithTab = insertTabAtLocation(urlString, i);
        checkRelativeURL(urlStringWithTab,
            baseURLString,
            parts.isInvalid() ? invalidParts(urlStringWithTab) : parts,
            TestTabs::No);
    }
}

static void checkURLDifferences(const String& urlString, const ExpectedParts& partsNew, const ExpectedParts& partsOld, TestTabs testTabs = TestTabs::Yes)
{
    UNUSED_PARAM(partsOld); // FIXME: Remove all the old expected parts.
    auto url = URL(URL(), urlString);
    
    EXPECT_TRUE(eq(partsNew.protocol, url.protocol()));
    EXPECT_TRUE(eq(partsNew.user, url.user()));
    EXPECT_TRUE(eq(partsNew.password, url.pass()));
    EXPECT_TRUE(eq(partsNew.host, url.host()));
    EXPECT_EQ(partsNew.port, url.port().valueOr(0));
    EXPECT_TRUE(eq(partsNew.path, url.path()));
    EXPECT_TRUE(eq(partsNew.query, url.query()));
    EXPECT_TRUE(eq(partsNew.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(partsNew.string, url.string()));
    
    EXPECT_TRUE(WTF::URLParser::internalValuesConsistent(url));
    
    if (testTabs == TestTabs::No)
        return;
    
    for (size_t i = 0; i < urlString.length(); ++i) {
        String urlStringWithTab = insertTabAtLocation(urlString, i);
        checkURLDifferences(urlStringWithTab,
            partsNew.isInvalid() ? invalidParts(urlStringWithTab) : partsNew,
            partsOld.isInvalid() ? invalidParts(urlStringWithTab) : partsOld,
            TestTabs::No);
    }
}

static void checkRelativeURLDifferences(const String& urlString, const String& baseURLString, const ExpectedParts& partsNew, const ExpectedParts& partsOld, TestTabs testTabs = TestTabs::Yes)
{
    UNUSED_PARAM(partsOld); // FIXME: Remove all the old expected parts.
    auto url = URL(URL(URL(), baseURLString), urlString);
    
    EXPECT_TRUE(eq(partsNew.protocol, url.protocol()));
    EXPECT_TRUE(eq(partsNew.user, url.user()));
    EXPECT_TRUE(eq(partsNew.password, url.pass()));
    EXPECT_TRUE(eq(partsNew.host, url.host()));
    EXPECT_EQ(partsNew.port, url.port().valueOr(0));
    EXPECT_TRUE(eq(partsNew.path, url.path()));
    EXPECT_TRUE(eq(partsNew.query, url.query()));
    EXPECT_TRUE(eq(partsNew.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(partsNew.string, url.string()));
    
    EXPECT_TRUE(WTF::URLParser::internalValuesConsistent(url));
    
    if (testTabs == TestTabs::No)
        return;
    
    for (size_t i = 0; i < urlString.length(); ++i) {
        String urlStringWithTab = insertTabAtLocation(urlString, i);
        checkRelativeURLDifferences(urlStringWithTab, baseURLString,
            partsNew.isInvalid() ? invalidParts(urlStringWithTab) : partsNew,
            partsOld.isInvalid() ? invalidParts(urlStringWithTab) : partsOld,
            TestTabs::No);
    }
}

static void shouldFail(const String& urlString)
{
    checkURL(urlString, {"", "", "", "", 0, "", "", "", urlString});
}

static void shouldFail(const String& urlString, const String& baseString)
{
    checkRelativeURL(urlString, baseString, {"", "", "", "", 0, "", "", "", urlString});
}

TEST_F(WTF_URLParser, Basic)
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
    checkURL("http://[0::0]/", {"http", "", "", "[::]", 0, "/", "", "", "http://[::]/"});
    checkURL("http://[0::]/", {"http", "", "", "[::]", 0, "/", "", "", "http://[::]/"});
    checkURL("http://[::]/", {"http", "", "", "[::]", 0, "/", "", "", "http://[::]/"});
    checkURL("http://[::0]/", {"http", "", "", "[::]", 0, "/", "", "", "http://[::]/"});
    checkURL("http://[::0:0]/", {"http", "", "", "[::]", 0, "/", "", "", "http://[::]/"});
    checkURL("http://[f::0:0]/", {"http", "", "", "[f::]", 0, "/", "", "", "http://[f::]/"});
    checkURL("http://[f:0::f]/", {"http", "", "", "[f::f]", 0, "/", "", "", "http://[f::f]/"});
    checkURL("http://[::0:ff]/", {"http", "", "", "[::ff]", 0, "/", "", "", "http://[::ff]/"});
    checkURL("http://[::00:0:0:0]/", {"http", "", "", "[::]", 0, "/", "", "", "http://[::]/"});
    checkURL("http://[::0:00:0:0]/", {"http", "", "", "[::]", 0, "/", "", "", "http://[::]/"});
    checkURL("http://[::0:0.0.0.0]/", {"http", "", "", "[::]", 0, "/", "", "", "http://[::]/"});
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
    checkURL("file://?Q", {"file", "", "", "", 0, "/", "Q", "", "file:///?Q"});
    checkURL("file://#F", {"file", "", "", "", 0, "/", "", "F", "file:///#F"});
    checkURL("file://host?Q", {"file", "", "", "host", 0, "/", "Q", "", "file://host/?Q"});
    checkURL("file://host#F", {"file", "", "", "host", 0, "/", "", "F", "file://host/#F"});
    checkURL("file://host\\P", {"file", "", "", "host", 0, "/P", "", "", "file://host/P"});
    checkURL("file://host\\?Q", {"file", "", "", "host", 0, "/", "Q", "", "file://host/?Q"});
    checkURL("file://host\\../P", {"file", "", "", "host", 0, "/P", "", "", "file://host/P"});
    checkURL("file://host\\/../P", {"file", "", "", "host", 0, "/P", "", "", "file://host/P"});
    checkURL("file://host\\/P", {"file", "", "", "host", 0, "//P", "", "", "file://host//P"});
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
    checkURL("ws://08./", {"ws", "", "", "08.", 0, "/", "", "", "ws://08./"});
    checkURL("http://[0:f::f:f:0:0]:123/path", {"http", "", "", "[0:f::f:f:0:0]", 123, "/path", "", "", "http://[0:f::f:f:0:0]:123/path"});
    checkURL("http://[0:f::f:f:0:0]:123", {"http", "", "", "[0:f::f:f:0:0]", 123, "/", "", "", "http://[0:f::f:f:0:0]:123/"});
    checkURL("http://[0:f:0:0:f:\t:]:123", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f:0:0:f::\t]:123", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f:0:0:f::]\t:123", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f:0:0:f::]:\t123", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f:0:0:f::]:1\t23", {"http", "", "", "[0:f:0:0:f::]", 123, "/", "", "", "http://[0:f:0:0:f::]:123/"});
    checkURL("http://[0:f::f:f:0:0]:/path", {"http", "", "", "[0:f::f:f:0:0]", 0, "/path", "", "", "http://[0:f::f:f:0:0]/path"});
    checkURL("a://[::2:]", {"a", "", "", "[::2]", 0, "", "", "", "a://[::2]"});
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
    checkURLDifferences("http://127.0.0.1.",
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"},
        {"http", "", "", "127.0.0.1.", 0, "/", "", "", "http://127.0.0.1./"});
    checkURLDifferences("http://127.0.0.1./",
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"},
        {"http", "", "", "127.0.0.1.", 0, "/", "", "", "http://127.0.0.1./"});
    checkURL("http://127.0.0.1../", {"http", "", "", "127.0.0.1..", 0, "/", "", "", "http://127.0.0.1../"});
    checkURLDifferences("http://0x100.0/",
        {"", "", "", "", 0, "", "", "", "http://0x100.0/"},
        {"http", "", "", "0x100.0", 0, "/", "", "", "http://0x100.0/"});
    checkURLDifferences("http://0.0.0x100.0/",
        {"", "", "", "", 0, "", "", "", "http://0.0.0x100.0/"},
        {"http", "", "", "0.0.0x100.0", 0, "/", "", "", "http://0.0.0x100.0/"});
    checkURLDifferences("http://0.0.0.0x100/",
        {"", "", "", "", 0, "", "", "", "http://0.0.0.0x100/"},
        {"http", "", "", "0.0.0.0x100", 0, "/", "", "", "http://0.0.0.0x100/"});
    checkURL("http://host:123?", {"http", "", "", "host", 123, "/", "", "", "http://host:123/?"});
    checkURL("http://host:123?query", {"http", "", "", "host", 123, "/", "query", "", "http://host:123/?query"});
    checkURL("http://host:123#", {"http", "", "", "host", 123, "/", "", "", "http://host:123/#"});
    checkURL("http://host:123#fragment", {"http", "", "", "host", 123, "/", "", "fragment", "http://host:123/#fragment"});
    checkURLDifferences("foo:////",
        {"foo", "", "", "", 0, "//", "", "", "foo:////"},
        {"foo", "", "", "", 0, "////", "", "", "foo:////"});
    checkURLDifferences("foo:///?",
        {"foo", "", "", "", 0, "/", "", "", "foo:///?"},
        {"foo", "", "", "", 0, "///", "", "", "foo:///?"});
    checkURLDifferences("foo:///#",
        {"foo", "", "", "", 0, "/", "", "", "foo:///#"},
        {"foo", "", "", "", 0, "///", "", "", "foo:///#"});
    checkURLDifferences("foo:///",
        {"foo", "", "", "", 0, "/", "", "", "foo:///"},
        {"foo", "", "", "", 0, "///", "", "", "foo:///"});
    checkURLDifferences("foo://?",
        {"foo", "", "", "", 0, "", "", "", "foo://?"},
        {"foo", "", "", "", 0, "//", "", "", "foo://?"});
    checkURLDifferences("foo://#",
        {"foo", "", "", "", 0, "", "", "", "foo://#"},
        {"foo", "", "", "", 0, "//", "", "", "foo://#"});
    checkURLDifferences("foo://",
        {"foo", "", "", "", 0, "", "", "", "foo://"},
        {"foo", "", "", "", 0, "//", "", "", "foo://"});
    checkURL("foo:/?", {"foo", "", "", "", 0, "/", "", "", "foo:/?"});
    checkURL("foo:/#", {"foo", "", "", "", 0, "/", "", "", "foo:/#"});
    checkURL("foo:/", {"foo", "", "", "", 0, "/", "", "", "foo:/"});
    checkURL("foo:?", {"foo", "", "", "", 0, "", "", "", "foo:?"});
    checkURL("foo:#", {"foo", "", "", "", 0, "", "", "", "foo:#"});
    checkURLDifferences("A://",
        {"a", "", "", "", 0, "", "", "", "a://"},
        {"a", "", "", "", 0, "//", "", "", "a://"});
    checkURLDifferences("aA://",
        {"aa", "", "", "", 0, "", "", "", "aa://"},
        {"aa", "", "", "", 0, "//", "", "", "aa://"});
    checkURL(utf16String(u"foo://host/#ПП\u0007 a</"), {"foo", "", "", "host", 0, "/", "", "%D0%9F%D0%9F%07 a</", "foo://host/#%D0%9F%D0%9F%07 a</"});
    checkURL(utf16String(u"foo://host/#\u0007 a</"), {"foo", "", "", "host", 0, "/", "", "%07 a</", "foo://host/#%07 a</"});
    checkURL(utf16String(u"http://host?ß😍#ß😍"), {"http", "", "", "host", 0, "/", "%C3%9F%F0%9F%98%8D", "%C3%9F%F0%9F%98%8D", "http://host/?%C3%9F%F0%9F%98%8D#%C3%9F%F0%9F%98%8D"}, testTabsValueForSurrogatePairs);
    checkURL(utf16String(u"http://host/path#💩\t💩"), {"http", "", "", "host", 0, "/path", "", "%F0%9F%92%A9%F0%9F%92%A9", "http://host/path#%F0%9F%92%A9%F0%9F%92%A9"}, testTabsValueForSurrogatePairs);
    checkURL(utf16String(u"http://host/#ПП\u0007 a</"), {"http", "", "", "host", 0, "/", "", "%D0%9F%D0%9F%07 a</", "http://host/#%D0%9F%D0%9F%07 a</"});
    checkURL(utf16String(u"http://host/#\u0007 a</"), {"http", "", "", "host", 0, "/", "", "%07 a</", "http://host/#%07 a</"});

    // This disagrees with the web platform test for http://:@www.example.com but agrees with Chrome and URL::parse,
    // and Firefox fails the web platform test differently. Maybe the web platform test ought to be changed.
    checkURL("http://:@host", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
}

static void testUserPass(const String& value, const String& decoded, const String& encoded)
{
    URL userURL(URL(), makeString("http://", value, "@example.com/"));
    URL passURL(URL(), makeString("http://user:", value, "@example.com/"));
    EXPECT_EQ(encoded, userURL.encodedUser());
    EXPECT_EQ(encoded, passURL.encodedPass());
    EXPECT_EQ(decoded, userURL.user());
    EXPECT_EQ(decoded, passURL.pass());
}

static void testUserPass(const String& value, const String& encoded)
{
    testUserPass(value, value, encoded);
}

TEST_F(WTF_URLParser, Credentials)
{
    auto validSurrogate = utf16String<3>({0xD800, 0xDD55, '\0'});
    auto invalidSurrogate = utf16String<3>({0xD800, 'A', '\0'});
    auto replacementA = utf16String<3>({0xFFFD, 'A', '\0'});

    testUserPass("a", "a");
    testUserPass("%", "%");
    testUserPass("%25", "%", "%25");
    testUserPass("%2525", "%25", "%2525");
    testUserPass("%FX", "%FX");
    testUserPass("%00", String::fromUTF8("\0", 1), "%00");
    testUserPass("%F%25", "%F%", "%F%25");
    testUserPass("%X%25", "%X%", "%X%25");
    testUserPass("%%25", "%%", "%%25");
    testUserPass("💩", "%C3%B0%C2%9F%C2%92%C2%A9");
    testUserPass("%💩", "%%C3%B0%C2%9F%C2%92%C2%A9");
    testUserPass(validSurrogate, "%F0%90%85%95");
    testUserPass(replacementA, "%EF%BF%BDA");
    testUserPass(invalidSurrogate, replacementA, "%EF%BF%BDA");
}

TEST_F(WTF_URLParser, ParseRelative)
{
    checkRelativeURL("/index.html", "http://webkit.org/path1/path2/", {"http", "", "", "webkit.org", 0, "/index.html", "", "", "http://webkit.org/index.html"});
    checkRelativeURL("http://whatwg.org/index.html", "http://webkit.org/path1/path2/", {"http", "", "", "whatwg.org", 0, "/index.html", "", "", "http://whatwg.org/index.html"});
    checkRelativeURL("index.html", "http://webkit.org/path1/path2/page.html?query#fragment", {"http", "", "", "webkit.org", 0, "/path1/path2/index.html", "", "", "http://webkit.org/path1/path2/index.html"});
    checkRelativeURL("//whatwg.org/index.html", "https://www.webkit.org/path", {"https", "", "", "whatwg.org", 0, "/index.html", "", "", "https://whatwg.org/index.html"});
    checkRelativeURL("http://example\t.\norg", "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/", "", "", "http://example.org/"});
    checkRelativeURL("test", "file:///path1/path2", {"file", "", "", "", 0, "/path1/test", "", "", "file:///path1/test"});
    checkRelativeURL(utf16String(u"http://www.foo。bar.com"), "http://other.com/", {"http", "", "", "www.foo.bar.com", 0, "/", "", "", "http://www.foo.bar.com/"});
    checkRelativeURLDifferences(utf16String(u"sc://ñ.test/"), "about:blank",
        {"sc", "", "", "%C3%B1.test", 0, "/", "", "", "sc://%C3%B1.test/"},
        {"sc", "", "", "xn--ida.test", 0, "/", "", "", "sc://xn--ida.test/"});
    checkRelativeURL("#fragment", "http://host/path", {"http", "", "", "host", 0, "/path", "", "fragment", "http://host/path#fragment"});
    checkRelativeURL("#fragment", "file:///path", {"file", "", "", "", 0, "/path", "", "fragment", "file:///path#fragment"});
    checkRelativeURL("#fragment", "file:///path#old", {"file", "", "", "", 0, "/path", "", "fragment", "file:///path#fragment"});
    checkRelativeURL("#", "file:///path#old", {"file", "", "", "", 0, "/path", "", "", "file:///path#"});
    checkRelativeURL("  ", "file:///path#old", {"file", "", "", "", 0, "/path", "", "", "file:///path"});
    checkRelativeURL("#", "file:///path", {"file", "", "", "", 0, "/path", "", "", "file:///path#"});
    checkRelativeURL("#", "file:///path?query", {"file", "", "", "", 0, "/path", "query", "", "file:///path?query#"});
    checkRelativeURL("#", "file:///path?query#old", {"file", "", "", "", 0, "/path", "query", "", "file:///path?query#"});
    checkRelativeURL("?query", "http://host/path", {"http", "", "", "host", 0, "/path", "query", "", "http://host/path?query"});
    checkRelativeURL("?query#fragment", "http://host/path", {"http", "", "", "host", 0, "/path", "query", "fragment", "http://host/path?query#fragment"});
    checkRelativeURL("?new", "file:///path?old#fragment", {"file", "", "", "", 0, "/path", "new", "", "file:///path?new"});
    checkRelativeURL("?", "file:///path?old#fragment", {"file", "", "", "", 0, "/path", "", "", "file:///path?"});
    checkRelativeURL("?", "file:///path", {"file", "", "", "", 0, "/path", "", "", "file:///path?"});
    checkRelativeURL("?query", "file:///path", {"file", "", "", "", 0, "/path", "query", "", "file:///path?query"});
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
    checkRelativeURL(String(), "http://host/#fragment", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkRelativeURL("", "http://host/#fragment", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkRelativeURL("  ", "http://host/#fragment", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkRelativeURL("  ", "http://host/path?query#fra#gment", {"http", "", "", "host", 0, "/path", "query", "", "http://host/path?query"});
    checkRelativeURL(" \a ", "http://host/#fragment", {"http", "", "", "host", 0, "/", "", "", "http://host/"});
    checkRelativeURLDifferences("foo://", "http://example.org/foo/bar",
        {"foo", "", "", "", 0, "", "", "", "foo://"},
        {"foo", "", "", "", 0, "//", "", "", "foo://"});
    checkRelativeURL(utf16String(u"#β"), "http://example.org/foo/bar", {"http", "", "", "example.org", 0, "/foo/bar", "", "%CE%B2", "http://example.org/foo/bar#%CE%B2"});
    checkRelativeURL("index.html", "applewebdata://Host/", {"applewebdata", "", "", "Host", 0, "/index.html", "", "", "applewebdata://Host/index.html"});
    checkRelativeURL("index.html", "applewebdata://Host", {"applewebdata", "", "", "Host", 0, "/index.html", "", "", "applewebdata://Host/index.html"});
    checkRelativeURL("", "applewebdata://Host", {"applewebdata", "", "", "Host", 0, "", "", "", "applewebdata://Host"});
    checkRelativeURL("?query", "applewebdata://Host", {"applewebdata", "", "", "Host", 0, "", "query", "", "applewebdata://Host?query"});
    checkRelativeURL("#fragment", "applewebdata://Host", {"applewebdata", "", "", "Host", 0, "", "", "fragment", "applewebdata://Host#fragment"});
    checkRelativeURL("notspecial://something?", "file:////var//containers//stuff/", {"notspecial", "", "", "something", 0, "", "", "", "notspecial://something?"}, TestTabs::No);
    checkRelativeURL("notspecial://something#", "file:////var//containers//stuff/", {"notspecial", "", "", "something", 0, "", "", "", "notspecial://something#"}, TestTabs::No);
    checkRelativeURL("http://something?", "file:////var//containers//stuff/", {"http", "", "", "something", 0, "/", "", "", "http://something/?"}, TestTabs::No);
    checkRelativeURL("http://something#", "file:////var//containers//stuff/", {"http", "", "", "something", 0, "/", "", "", "http://something/#"}, TestTabs::No);
    checkRelativeURL("file:", "file:///path?query#fragment", {"file", "", "", "", 0, "/path", "query", "", "file:///path?query"});
    checkRelativeURL("/", "file:///C:/a/b", {"file", "", "", "", 0, "/C:/", "", "", "file:///C:/"});
    checkRelativeURL("/abc", "file:///C:/a/b", {"file", "", "", "", 0, "/C:/abc", "", "", "file:///C:/abc"});
    checkRelativeURL("/abc", "file:///C:", {"file", "", "", "", 0, "/C:/abc", "", "", "file:///C:/abc"});
    checkRelativeURL("/abc", "file:///", {"file", "", "", "", 0, "/abc", "", "", "file:///abc"});
    checkRelativeURL("//d:", "file:///C:/a/b", {"file", "", "", "", 0, "/d:", "", "", "file:///d:"}, TestTabs::No);
    checkRelativeURL("//d|", "file:///C:/a/b", {"file", "", "", "", 0, "/d:", "", "", "file:///d:"}, TestTabs::No);
    checkRelativeURL("//A|", "file:///C:/a/b", {"file", "", "", "", 0, "/A:", "", "", "file:///A:"}, TestTabs::No);

    // The checking of slashes in SpecialAuthoritySlashes needed to get this to pass contradicts what is in the spec,
    // but it is included in the web platform tests.
    checkRelativeURL("http:\\\\host\\foo", "about:blank", {"http", "", "", "host", 0, "/foo", "", "", "http://host/foo"});
}

// These are differences between the new URLParser and the old URL::parse which make URLParser more standards compliant.
TEST_F(WTF_URLParser, ParserDifferences)
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
    checkURLDifferences("http://[::7f00:0001]/",
        {"http", "", "", "[::7f00:1]", 0, "/", "", "", "http://[::7f00:1]/"},
        {"http", "", "", "[::7f00:0001]", 0, "/", "", "", "http://[::7f00:0001]/"});
    checkURLDifferences("http://[::7f00:00]/",
        {"http", "", "", "[::7f00:0]", 0, "/", "", "", "http://[::7f00:0]/"},
        {"http", "", "", "[::7f00:00]", 0, "/", "", "", "http://[::7f00:00]/"});
    checkURLDifferences("http://[::0:7f00:0001]/",
        {"http", "", "", "[::7f00:1]", 0, "/", "", "", "http://[::7f00:1]/"},
        {"http", "", "", "[::0:7f00:0001]", 0, "/", "", "", "http://[::0:7f00:0001]/"});
    checkURLDifferences("http://127.00.0.1/",
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"},
        {"http", "", "", "127.00.0.1", 0, "/", "", "", "http://127.00.0.1/"});
    checkURLDifferences("http://127.0.0.01/",
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1/"},
        {"http", "", "", "127.0.0.01", 0, "/", "", "", "http://127.0.0.01/"});
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
    checkURL("http://example.com/path1/path2/A%2e%2e#fragment", {"http", "", "", "example.com", 0, "/path1/path2/A%2e%2e", "", "fragment", "http://example.com/path1/path2/A%2e%2e#fragment"});
    checkURLDifferences("file://[0:a:0:0:b:c:0:0]/path",
        {"file", "", "", "[0:a::b:c:0:0]", 0, "/path", "", "", "file://[0:a::b:c:0:0]/path"},
        {"file", "", "", "[0:a:0:0:b:c:0:0]", 0, "/path", "", "", "file://[0:a:0:0:b:c:0:0]/path"});
    checkURLDifferences("http://",
        {"", "", "", "", 0, "", "", "", "http://"},
        {"http", "", "", "", 0, "/", "", "", "http:/"});
    checkRelativeURLDifferences("//", "https://www.webkit.org/path",
        {"", "", "", "", 0, "", "", "", "//"},
        {"https", "", "", "", 0, "/", "", "", "https:/"});
    checkURLDifferences("http://127.0.0.1:65536/path",
        {"", "", "", "", 0, "", "", "", "http://127.0.0.1:65536/path"},
        {"http", "", "", "127.0.0.1", 0, "/path", "", "", "http://127.0.0.1:65536/path"});
    checkURLDifferences("http://host:65536",
        {"", "", "", "", 0, "", "", "", "http://host:65536"},
        {"http", "", "", "host", 0, "/", "", "", "http://host:65536/"});
    checkURLDifferences("http://127.0.0.1:65536",
        {"", "", "", "", 0, "", "", "", "http://127.0.0.1:65536"},
        {"http", "", "", "127.0.0.1", 0, "/", "", "", "http://127.0.0.1:65536/"});
    checkURLDifferences("http://[0:f::f:f:0:0]:65536",
        {"", "", "", "", 0, "", "", "", "http://[0:f::f:f:0:0]:65536"},
        {"http", "", "", "[0:f::f:f:0:0]", 0, "/", "", "", "http://[0:f::f:f:0:0]:65536/"});
    checkRelativeURLDifferences(":foo.com\\", "notspecial://example.org/foo/bar",
        {"notspecial", "", "", "example.org", 0, "/foo/:foo.com\\", "", "", "notspecial://example.org/foo/:foo.com\\"},
        {"notspecial", "", "", "example.org", 0, "/foo/:foo.com/", "", "", "notspecial://example.org/foo/:foo.com/"});
    checkURL("sc://pa", {"sc", "", "", "pa", 0, "", "", "", "sc://pa"});
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
    checkURLDifferences("http://example.com%A0",
        {"", "", "", "", 0, "", "", "", "http://example.com%A0"},
        {"http", "", "", "example.com%a0", 0, "/", "", "", "http://example.com%a0/"});
    checkURLDifferences("http://%E2%98%83",
        {"http", "", "", "xn--n3h", 0, "/", "", "", "http://xn--n3h/"},
        {"http", "", "", "%e2%98%83", 0, "/", "", "", "http://%e2%98%83/"});
    checkURLDifferences("http://host%73",
        {"http", "", "", "hosts", 0, "/", "", "", "http://hosts/"},
        {"http", "", "", "host%73", 0, "/", "", "", "http://host%73/"});
    checkURLDifferences("http://host%53",
        {"http", "", "", "hosts", 0, "/", "", "", "http://hosts/"},
        {"http", "", "", "host%53", 0, "/", "", "", "http://host%53/"});
    checkURLDifferences("http://%",
        {"", "", "", "", 0, "", "", "", "http://%"},
        {"http", "", "", "%", 0, "/", "", "", "http://%/"});
    checkURLDifferences("http://%7",
        {"", "", "", "", 0, "", "", "", "http://%7"},
        {"http", "", "", "%7", 0, "/", "", "", "http://%7/"});
    checkURLDifferences("http://%7s",
        {"", "", "", "", 0, "", "", "", "http://%7s"},
        {"http", "", "", "%7s", 0, "/", "", "", "http://%7s/"});
    checkURLDifferences("http://%73",
        {"http", "", "", "s", 0, "/", "", "", "http://s/"},
        {"http", "", "", "%73", 0, "/", "", "", "http://%73/"});
    checkURLDifferences("http://abcdefg%",
        {"", "", "", "", 0, "", "", "", "http://abcdefg%"},
        {"http", "", "", "abcdefg%", 0, "/", "", "", "http://abcdefg%/"});
    checkURLDifferences("http://abcd%7Xefg",
        {"", "", "", "", 0, "", "", "", "http://abcd%7Xefg"},
        {"http", "", "", "abcd%7xefg", 0, "/", "", "", "http://abcd%7xefg/"});
    checkURL("ws://äAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", {"ws", "", "", "xn--aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-rsb254a", 0, "/", "", "", "ws://xn--aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-rsb254a/"}, TestTabs::No);
    shouldFail("ws://äAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    checkURL("ws://&äAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", {"ws", "", "", "xn--&aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-ssb254a", 0, "/", "", "", "ws://xn--&aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-ssb254a/"}, TestTabs::No);
    shouldFail("ws://&äAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

    // URLParser matches Chrome and the spec, but not URL::parse or Firefox.
    checkURLDifferences(utf16String(u"http://０Ｘｃ０．０２５０．０１"),
        {"http", "", "", "192.168.0.1", 0, "/", "", "", "http://192.168.0.1/"},
        {"http", "", "", "0xc0.0250.01", 0, "/", "", "", "http://0xc0.0250.01/"});

    checkURL("http://host/path%2e.%2E", {"http", "", "", "host", 0, "/path%2e.%2E", "", "", "http://host/path%2e.%2E"});

    checkRelativeURLDifferences(utf16String(u"http://foo:💩@example.com/bar"), "http://other.com/",
        {"http", "foo", utf16String(u"💩"), "example.com", 0, "/bar", "", "", "http://foo:%F0%9F%92%A9@example.com/bar"},
        {"", "", "", "", 0, "", "", "", utf16String(u"http://foo:💩@example.com/bar")}, testTabsValueForSurrogatePairs);
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
        {"notspecial", "@test@test@example", "800\\path", "end", 0, "", "", "", "notspecial://%40test%40test%40example:800%5Cpath@end"},
        {"", "", "", "", 0, "", "", "", "notspecial://@test@test@example:800\\path@end"});
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
    checkURLDifferences("http://://",
        {"", "", "", "", 0, "", "", "", "http://://"},
        {"http", "", "", "", 0, "//", "", "", "http://://"});
    checkURLDifferences("http://:123?",
        {"", "", "", "", 0, "", "", "", "http://:123?"},
        {"http", "", "", "", 123, "/", "", "", "http://:123/?"});
    checkURLDifferences("http:/:",
        {"", "", "", "", 0, "", "", "", "http:/:"},
        {"http", "", "", "", 0, "/", "", "", "http://:/"});
    checkURLDifferences("asdf://:",
        {"", "", "", "", 0, "", "", "", "asdf://:"},
        {"asdf", "", "", "", 0, "", "", "", "asdf://:"});
    checkURLDifferences("http://:",
        {"", "", "", "", 0, "", "", "", "http://:"},
        {"http", "", "", "", 0, "/", "", "", "http://:/"});
    checkURLDifferences("http:##foo",
        {"", "", "", "", 0, "", "", "", "http:##foo"},
        {"http", "", "", "", 0, "/", "", "#foo", "http:/##foo"});
    checkURLDifferences("http:??bar",
        {"", "", "", "", 0, "", "", "", "http:??bar"},
        {"http", "", "", "", 0, "/", "?bar", "", "http:/??bar"});
    checkURL("asdf:##foo", {"asdf", "", "", "", 0, "", "", "#foo", "asdf:##foo"});
    checkURL("asdf:??bar", {"asdf", "", "", "", 0, "", "?bar", "", "asdf:??bar"});
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
    checkURL("notspecial://HoSt", {"notspecial", "", "", "HoSt", 0, "", "", "", "notspecial://HoSt"});
    checkURL("notspecial://H%6FSt", {"notspecial", "", "", "H%6FSt", 0, "", "", "", "notspecial://H%6FSt"});
    checkURL("notspecial://H%4fSt", {"notspecial", "", "", "H%4fSt", 0, "", "", "", "notspecial://H%4fSt"});
    checkURLDifferences(utf16String(u"notspecial://H😍ßt"),
        {"notspecial", "", "", "H%F0%9F%98%8D%C3%9Ft", 0, "", "", "", "notspecial://H%F0%9F%98%8D%C3%9Ft"},
        {"notspecial", "", "", "xn--hsst-qc83c", 0, "", "", "", "notspecial://xn--hsst-qc83c"}, testTabsValueForSurrogatePairs);
    checkURLDifferences("http://[ffff:aaaa:cccc:eeee:bbbb:dddd:255.255.255.255]/",
        {"http", "", "", "[ffff:aaaa:cccc:eeee:bbbb:dddd:ffff:ffff]", 0, "/", "", "", "http://[ffff:aaaa:cccc:eeee:bbbb:dddd:ffff:ffff]/"},
        {"http", "", "", "[ffff:aaaa:cccc:eeee:bbbb:dddd:255.255.255.255]", 0, "/", "", "", "http://[ffff:aaaa:cccc:eeee:bbbb:dddd:255.255.255.255]/"}, TestTabs::No);
    checkURLDifferences("http://[::123.234.12.210]/",
        {"http", "", "", "[::7bea:cd2]", 0, "/", "", "", "http://[::7bea:cd2]/"},
        {"http", "", "", "[::123.234.12.210]", 0, "/", "", "", "http://[::123.234.12.210]/"});
    checkURLDifferences("http://[::a:255.255.255.255]/",
        {"http", "", "", "[::a:ffff:ffff]", 0, "/", "", "", "http://[::a:ffff:ffff]/"},
        {"http", "", "", "[::a:255.255.255.255]", 0, "/", "", "", "http://[::a:255.255.255.255]/"});
    checkURLDifferences("http://[::0.00.255.255]/",
        {"", "", "", "", 0, "", "", "", "http://[::0.00.255.255]/"},
        {"http", "", "", "[::0.00.255.255]", 0, "/", "", "", "http://[::0.00.255.255]/"});
    checkURLDifferences("http://[::0.0.255.255]/",
        {"http", "", "", "[::ffff]", 0, "/", "", "", "http://[::ffff]/"},
        {"http", "", "", "[::0.0.255.255]", 0, "/", "", "", "http://[::0.0.255.255]/"});
    checkURLDifferences("http://[::0:1.0.255.255]/",
        {"http", "", "", "[::100:ffff]", 0, "/", "", "", "http://[::100:ffff]/"},
        {"http", "", "", "[::0:1.0.255.255]", 0, "/", "", "", "http://[::0:1.0.255.255]/"});
    checkURLDifferences("http://[::A:1.0.255.255]/",
        {"http", "", "", "[::a:100:ffff]", 0, "/", "", "", "http://[::a:100:ffff]/"},
        {"http", "", "", "[::a:1.0.255.255]", 0, "/", "", "", "http://[::a:1.0.255.255]/"});
    checkURLDifferences("http://[:127.0.0.1]",
        {"", "", "", "", 0, "", "", "", "http://[:127.0.0.1]"},
        {"http", "", "", "[:127.0.0.1]", 0, "/", "", "", "http://[:127.0.0.1]/"});
    checkURLDifferences("http://[127.0.0.1]",
        {"", "", "", "", 0, "", "", "", "http://[127.0.0.1]"},
        {"http", "", "", "[127.0.0.1]", 0, "/", "", "", "http://[127.0.0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.1]",
        {"http", "", "", "[a:b:c:d:e:f:7f00:1]", 0, "/", "", "", "http://[a:b:c:d:e:f:7f00:1]/"},
        {"http", "", "", "[a:b:c:d:e:f:127.0.0.1]", 0, "/", "", "", "http://[a:b:c:d:e:f:127.0.0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.101]",
        {"http", "", "", "[a:b:c:d:e:f:7f00:65]", 0, "/", "", "", "http://[a:b:c:d:e:f:7f00:65]/"},
        {"http", "", "", "[a:b:c:d:e:f:127.0.0.101]", 0, "/", "", "", "http://[a:b:c:d:e:f:127.0.0.101]/"});
    checkURLDifferences("http://[::a:b:c:d:e:f:127.0.0.1]",
        {"", "", "", "", 0, "", "", "", "http://[::a:b:c:d:e:f:127.0.0.1]"},
        {"http", "", "", "[::a:b:c:d:e:f:127.0.0.1]", 0, "/", "", "", "http://[::a:b:c:d:e:f:127.0.0.1]/"});
    checkURLDifferences("http://[a:b::c:d:e:f:127.0.0.1]",
        {"", "", "", "", 0, "", "", "", "http://[a:b::c:d:e:f:127.0.0.1]"},
        {"http", "", "", "[a:b::c:d:e:f:127.0.0.1]", 0, "/", "", "", "http://[a:b::c:d:e:f:127.0.0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:127.0.0.1]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:127.0.0.1]"},
        {"http", "", "", "[a:b:c:d:e:127.0.0.1]", 0, "/", "", "", "http://[a:b:c:d:e:127.0.0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.0.1]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f:127.0.0.0.1]"},
        {"http", "", "", "[a:b:c:d:e:f:127.0.0.0.1]", 0, "/", "", "", "http://[a:b:c:d:e:f:127.0.0.0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.1]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f:127.0.1]"},
        {"http", "", "", "[a:b:c:d:e:f:127.0.1]", 0, "/", "", "", "http://[a:b:c:d:e:f:127.0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.011]", // Chrome treats this as octal, Firefox and the spec fail
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f:127.0.0.011]"},
        {"http", "", "", "[a:b:c:d:e:f:127.0.0.011]", 0, "/", "", "", "http://[a:b:c:d:e:f:127.0.0.011]/"});
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.00.1]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f:127.0.00.1]"},
        {"http", "", "", "[a:b:c:d:e:f:127.0.00.1]", 0, "/", "", "", "http://[a:b:c:d:e:f:127.0.00.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.1.]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f:127.0.0.1.]"},
        {"http", "", "", "[a:b:c:d:e:f:127.0.0.1.]", 0, "/", "", "", "http://[a:b:c:d:e:f:127.0.0.1.]/"});
    checkURLDifferences("http://[a:b:c:d:e:f:127.0..0.1]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f:127.0..0.1]"},
        {"http", "", "", "[a:b:c:d:e:f:127.0..0.1]", 0, "/", "", "", "http://[a:b:c:d:e:f:127.0..0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:f::127.0.0.1]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f::127.0.0.1]"},
        {"http", "", "", "[a:b:c:d:e:f::127.0.0.1]", 0, "/", "", "", "http://[a:b:c:d:e:f::127.0.0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e::127.0.0.1]",
        {"http", "", "", "[a:b:c:d:e:0:7f00:1]", 0, "/", "", "", "http://[a:b:c:d:e:0:7f00:1]/"},
        {"http", "", "", "[a:b:c:d:e::127.0.0.1]", 0, "/", "", "", "http://[a:b:c:d:e::127.0.0.1]/"});
    checkURLDifferences("http://[a:b:c:d::e:127.0.0.1]",
        {"http", "", "", "[a:b:c:d:0:e:7f00:1]", 0, "/", "", "", "http://[a:b:c:d:0:e:7f00:1]/"},
        {"http", "", "", "[a:b:c:d::e:127.0.0.1]", 0, "/", "", "", "http://[a:b:c:d::e:127.0.0.1]/"});
    checkURLDifferences("http://[a:b:c:d:e:f::127.0.0.]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f::127.0.0.]"},
        {"http", "", "", "[a:b:c:d:e:f::127.0.0.]", 0, "/", "", "", "http://[a:b:c:d:e:f::127.0.0.]/"});
    checkURLDifferences("http://[a:b:c:d:e:f::127.0.0.256]",
        {"", "", "", "", 0, "", "", "", "http://[a:b:c:d:e:f::127.0.0.256]"},
        {"http", "", "", "[a:b:c:d:e:f::127.0.0.256]", 0, "/", "", "", "http://[a:b:c:d:e:f::127.0.0.256]/"});
    checkURLDifferences("http://123456", {"http", "", "", "0.1.226.64", 0, "/", "", "", "http://0.1.226.64/"}, {"http", "", "", "123456", 0, "/", "", "", "http://123456/"});
    checkURL("asdf://123456", {"asdf", "", "", "123456", 0, "", "", "", "asdf://123456"});
    checkURLDifferences("http://[0:0:0:0:a:b:c:d]",
        {"http", "", "", "[::a:b:c:d]", 0, "/", "", "", "http://[::a:b:c:d]/"},
        {"http", "", "", "[0:0:0:0:a:b:c:d]", 0, "/", "", "", "http://[0:0:0:0:a:b:c:d]/"});
    checkURLDifferences("asdf://[0:0:0:0:a:b:c:d]",
        {"asdf", "", "", "[::a:b:c:d]", 0, "", "", "", "asdf://[::a:b:c:d]"},
        {"asdf", "", "", "[0:0:0:0:a:b:c:d]", 0, "", "", "", "asdf://[0:0:0:0:a:b:c:d]"}, TestTabs::No);
    shouldFail("a://%:a");
    checkURL("a://%:/", {"a", "", "", "%", 0, "/", "", "", "a://%/"});
    checkURL("a://%:", {"a", "", "", "%", 0, "", "", "", "a://%"});
    checkURL("a://%:1/", {"a", "", "", "%", 1, "/", "", "", "a://%:1/"});
    checkURLDifferences("http://%:",
        {"", "", "", "", 0, "", "", "", "http://%:"},
        {"http", "", "", "%", 0, "/", "", "", "http://%/"});
    checkURL("a://123456", {"a", "", "", "123456", 0, "", "", "", "a://123456"});
    checkURL("a://123456:7", {"a", "", "", "123456", 7, "", "", "", "a://123456:7"});
    checkURL("a://123456:7/", {"a", "", "", "123456", 7, "/", "", "", "a://123456:7/"});
    checkURL("a://A", {"a", "", "", "A", 0, "", "", "", "a://A"});
    checkURL("a://A:2", {"a", "", "", "A", 2, "", "", "", "a://A:2"});
    checkURL("a://A:2/", {"a", "", "", "A", 2, "/", "", "", "a://A:2/"});
    checkURLDifferences(u8"asd://ß",
        {"asd", "", "", "%C3%83%C2%9F", 0, "", "", "", "asd://%C3%83%C2%9F"},
        {"", "", "", "", 0, "", "", "", "about:blank"}, TestTabs::No);
    checkURLDifferences(u8"asd://ß:4",
        {"asd", "", "", "%C3%83%C2%9F", 4, "", "", "", "asd://%C3%83%C2%9F:4"},
        {"", "", "", "", 0, "", "", "", "about:blank"}, TestTabs::No);
    checkURLDifferences(u8"asd://ß:4/",
        {"asd", "", "", "%C3%83%C2%9F", 4, "/", "", "", "asd://%C3%83%C2%9F:4/"},
        {"", "", "", "", 0, "", "", "", "about:blank"}, TestTabs::No);
    checkURLDifferences("a://[A::b]:4",
        {"a", "", "", "[a::b]", 4, "", "", "", "a://[a::b]:4"},
        {"a", "", "", "[A::b]", 4, "", "", "", "a://[A::b]:4"});
    shouldFail("http://[~]");
    shouldFail("a://[~]");
    checkRelativeURLDifferences("a://b", "//[aBc]",
        {"a", "", "", "b", 0, "", "", "", "a://b"},
        {"", "", "", "", 0, "", "", "", "a://b"});
    checkURL(utf16String(u"http://öbb.at"), {"http", "", "", "xn--bb-eka.at", 0, "/", "", "", "http://xn--bb-eka.at/"});
    checkURL(utf16String(u"http://ÖBB.at"), {"http", "", "", "xn--bb-eka.at", 0, "/", "", "", "http://xn--bb-eka.at/"});
    checkURL(utf16String(u"http://√.com"), {"http", "", "", "xn--19g.com", 0, "/", "", "", "http://xn--19g.com/"});
    checkURLDifferences(utf16String(u"http://faß.de"),
        {"http", "", "", "xn--fa-hia.de", 0, "/", "", "", "http://xn--fa-hia.de/"},
        {"http", "", "", "fass.de", 0, "/", "", "", "http://fass.de/"});
    checkURL(utf16String(u"http://ԛәлп.com"), {"http", "", "", "xn--k1ai47bhi.com", 0, "/", "", "", "http://xn--k1ai47bhi.com/"});
    checkURLDifferences(utf16String(u"http://Ⱥbby.com"),
        {"http", "", "", "xn--bby-iy0b.com", 0, "/", "", "", "http://xn--bby-iy0b.com/"},
        {"http", "", "", "xn--bby-spb.com", 0, "/", "", "", "http://xn--bby-spb.com/"});
    checkURLDifferences(utf16String(u"http://\u2132"),
        {"", "", "", "", 0, "", "", "", utf16String(u"http://Ⅎ")},
        {"http", "", "", "xn--f3g", 0, "/", "", "", "http://xn--f3g/"});
    checkURLDifferences(utf16String(u"http://\u05D9\u05B4\u05D5\u05D0\u05B8/"),
        {"http", "", "", "xn--cdbi5etas", 0, "/", "", "", "http://xn--cdbi5etas/"},
        {"", "", "", "", 0, "", "", "", "about:blank"}, TestTabs::No);
    checkURLDifferences(utf16String(u"http://bidirectional\u0786\u07AE\u0782\u07B0\u0795\u07A9\u0793\u07A6\u0783\u07AA/"),
        {"", "", "", "", 0, "", "", "", utf16String(u"http://bidirectionalކޮންޕީޓަރު/")},
        {"", "", "", "", 0, "", "", "", "about:blank"}, TestTabs::No);
    checkURLDifferences(utf16String(u"http://contextj\u200D"),
        {"", "", "", "", 0, "", "", "", utf16String(u"http://contextj\u200D")},
        {"http", "", "", "contextj", 0, "/", "", "", "http://contextj/"});
    checkURL(utf16String(u"http://contexto\u30FB"), {"http", "", "", "xn--contexto-wg5g", 0, "/", "", "", "http://xn--contexto-wg5g/"});
    checkURLDifferences(utf16String(u"http://\u321D\u321E/"),
        {"http", "", "", "xn--()()-bs0sc174agx4b", 0, "/", "", "", "http://xn--()()-bs0sc174agx4b/"},
        {"http", "", "", "xn--5mkc", 0, "/", "", "", "http://xn--5mkc/"});
}

TEST_F(WTF_URLParser, DefaultPort)
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
    
    checkURL("gOpHeR://host:70/", {"gopher", "", "", "host", 70, "/", "", "", "gopher://host:70/"});
    checkURL("gopher://host:70/", {"gopher", "", "", "host", 70, "/", "", "", "gopher://host:70/"});
    checkURL("gopher://host:71/", {"gopher", "", "", "host", 71, "/", "", "", "gopher://host:71/"});
    
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

    checkURL("fTpS://host:990/", {"ftps", "", "", "host", 990, "/", "", "", "ftps://host:990/"});
    checkURL("ftps://host:990/", {"ftps", "", "", "host", 990, "/", "", "", "ftps://host:990/"});
    checkURL("ftps://host:991/", {"ftps", "", "", "host", 991, "/", "", "", "ftps://host:991/"});
    checkURL("ftps://host:990", {"ftps", "", "", "host", 990, "", "", "", "ftps://host:990"});
    checkURL("ftps://host:991", {"ftps", "", "", "host", 991, "", "", "", "ftps://host:991"});

    checkURL("uNkNoWn://host:80/", {"unknown", "", "", "host", 80, "/", "", "", "unknown://host:80/"});
    checkURL("unknown://host:80/", {"unknown", "", "", "host", 80, "/", "", "", "unknown://host:80/"});
    checkURL("unknown://host:81/", {"unknown", "", "", "host", 81, "/", "", "", "unknown://host:81/"});
    checkURL("unknown://host:80", {"unknown", "", "", "host", 80, "", "", "", "unknown://host:80"});
    checkURL("unknown://host:81", {"unknown", "", "", "host", 81, "", "", "", "unknown://host:81"});

    checkURL("file://host:0", {"file", "", "", "host", 0, "/", "", "", "file://host:0/"});
    checkURL("file://host:80", {"file", "", "", "host", 80, "/", "", "", "file://host:80/"});
    checkURL("file://host:80/path", {"file", "", "", "host", 80, "/path", "", "", "file://host:80/path"});
    checkURLDifferences("file://:80/path",
        {"", "", "", "", 0, "", "", "", "file://:80/path"},
        {"file", "", "", "", 80, "/path", "", "", "file://:80/path"});
    checkURLDifferences("file://:0/path",
        {"", "", "", "", 0, "", "", "", "file://:0/path"},
        {"file", "", "", "", 0, "/path", "", "", "file://:0/path"});
    
    checkURL("http://example.com:0000000000000077", {"http", "", "", "example.com", 77, "/", "", "", "http://example.com:77/"});
    checkURL("http://example.com:0000000000000080", {"http", "", "", "example.com", 0, "/", "", "", "http://example.com/"});
}

TEST_F(WTF_URLParser, ParserFailures)
{
    shouldFail("    ");
    shouldFail("  \a  ");
    shouldFail("");
    shouldFail(String());
    shouldFail("", "about:blank");
    shouldFail(String(), "about:blank");
    shouldFail("http://127.0.0.1:abc");
    shouldFail("http://host:abc");
    shouldFail("http://:abc");
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
    shouldFail("%");
    shouldFail("//%");
    shouldFail("~", "about:blank");
    shouldFail("~~~");
    shouldFail("://:0/");
    shouldFail("://:0/", "");
    shouldFail("://:0/", "about:blank");
    shouldFail("about~");
    shouldFail("//C:asdf/foo/bar", "file:///tmp/mock/path");
    shouldFail("wss://[c::]abc/");
    shouldFail("abc://[c::]:abc/");
    shouldFail("abc://[c::]01");
    shouldFail("http://[1234::ab#]");
    shouldFail("http://[1234::ab/]");
    shouldFail("http://[1234::ab?]");
    shouldFail("http://[1234::ab@]");
    shouldFail("http://[1234::ab~]");
    shouldFail("http://[2001::1");
    shouldFail("http://4:b\xE1");
    shouldFail("http://[1:2:3:4:5:6:7:8~]/");
    shouldFail("http://[a:b:c:d:e:f:g:127.0.0.1]");
    shouldFail("http://[a:b:c:d:e:f:g:h:127.0.0.1]");
    shouldFail("http://[a:b:c:d:e:f:127.0.0.0x11]"); // Chrome treats this as hex, Firefox and the spec fail
    shouldFail("http://[a:b:c:d:e:f:127.0.-0.1]");
    shouldFail("asdf://space In\aHost");
    shouldFail("asdf://[0:0:0:0:a:b:c:d");
    shouldFail("http://[::0:0.0.00000.0]/");
}

// These are in the spec but not in the web platform tests.
TEST_F(WTF_URLParser, AdditionalTests)
{
    checkURL("about:\a\aabc", {"about", "", "", "", 0, "%07%07abc", "", "", "about:%07%07abc"});
    checkURL("notspecial:\t\t\n\t", {"notspecial", "", "", "", 0, "", "", "", "notspecial:"});
    checkURL("notspecial\t\t\n\t:\t\t\n\t/\t\t\n\t/\t\t\n\thost", {"notspecial", "", "", "host", 0, "", "", "", "notspecial://host"});
    checkRelativeURL("http:", "http://example.org/foo/bar?query#fragment", {"http", "", "", "example.org", 0, "/foo/bar", "query", "", "http://example.org/foo/bar?query"});
    checkRelativeURLDifferences("ws:", "http://example.org/foo/bar",
        {"ws", "", "", "", 0, "", "", "", "ws:"},
        {"ws", "", "", "", 0, "s:", "", "", "ws:s:"});
    checkRelativeURL("notspecial:", "http://example.org/foo/bar", {"notspecial", "", "", "", 0, "", "", "", "notspecial:"});

    const wchar_t surrogateBegin = 0xD800;
    const wchar_t validSurrogateEnd = 0xDD55;
    const wchar_t invalidSurrogateEnd = 'A';
    const wchar_t replacementCharacter = 0xFFFD;
    checkURL(utf16String<12>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', surrogateBegin, validSurrogateEnd, '\0'}),
        {"http", "", "", "w", 0, "/%F0%90%85%95", "", "", "http://w/%F0%90%85%95"}, testTabsValueForSurrogatePairs);
    shouldFail(utf16String<10>({'h', 't', 't', 'p', ':', '/', surrogateBegin, invalidSurrogateEnd, '/', '\0'}));
    shouldFail(utf16String<9>({'h', 't', 't', 'p', ':', '/', replacementCharacter, '/', '\0'}));
    
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
    
    // FIXME: Write more invalid surrogate pair tests based on feedback from https://bugs.webkit.org/show_bug.cgi?id=162105
}

} // namespace TestWebKitAPI
