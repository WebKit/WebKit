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
#include <WebCore/TextEncoding.h>
#include <wtf/MainThread.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace TestWebKitAPI {

class URLParserTextEncodingTest : public testing::Test {
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

static void checkURL(const String& urlString, const TextEncoding* encoding, const ExpectedParts& parts, TestTabs testTabs = TestTabs::Yes)
{
    auto url = URL({ }, urlString, encoding);
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port().valueOr(0));
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));
    
    if (testTabs == TestTabs::No)
        return;
    
    for (size_t i = 0; i < urlString.length(); ++i) {
        String urlStringWithTab = insertTabAtLocation(urlString, i);
        checkURL(urlStringWithTab, encoding,
            parts.isInvalid() ? invalidParts(urlStringWithTab) : parts,
            TestTabs::No);
    }
}

static void checkURL(const String& urlString, const String& baseURLString, const TextEncoding* encoding, const ExpectedParts& parts, TestTabs testTabs = TestTabs::Yes)
{
    auto url = URL(URL({ }, baseURLString), urlString, encoding);
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.pass()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port().valueOr(0));
    EXPECT_TRUE(eq(parts.path, url.path()));
    EXPECT_TRUE(eq(parts.query, url.query()));
    EXPECT_TRUE(eq(parts.fragment, url.fragmentIdentifier()));
    EXPECT_TRUE(eq(parts.string, url.string()));
    
    if (testTabs == TestTabs::No)
        return;
    
    for (size_t i = 0; i < urlString.length(); ++i) {
        String urlStringWithTab = insertTabAtLocation(urlString, i);
        checkURL(urlStringWithTab, baseURLString, encoding,
            parts.isInvalid() ? invalidParts(urlStringWithTab) : parts,
            TestTabs::No);
    }
}

TEST_F(URLParserTextEncodingTest, QueryEncoding)
{
    checkURL(utf16String(u"http://host?ß😍#ß😍"), nullptr, {"http", "", "", "host", 0, "/", "%C3%9F%F0%9F%98%8D", "%C3%9F%F0%9F%98%8D", utf16String(u"http://host/?%C3%9F%F0%9F%98%8D#%C3%9F%F0%9F%98%8D")}, testTabsValueForSurrogatePairs);

    TextEncoding latin1(String("latin1"));
    checkURL("http://host/?query with%20spaces", &latin1, {"http", "", "", "host", 0, "/", "query%20with%20spaces", "", "http://host/?query%20with%20spaces"});
    checkURL("http://host/?query", &latin1, {"http", "", "", "host", 0, "/", "query", "", "http://host/?query"});
    checkURL("http://host/?\tquery", &latin1, {"http", "", "", "host", 0, "/", "query", "", "http://host/?query"});
    checkURL("http://host/?q\tuery", &latin1, {"http", "", "", "host", 0, "/", "query", "", "http://host/?query"});
    checkURL("http://host/?query with SpAcEs#fragment", &latin1, {"http", "", "", "host", 0, "/", "query%20with%20SpAcEs", "fragment", "http://host/?query%20with%20SpAcEs#fragment"});
    checkURL("http://host/?que\rry\t\r\n#fragment", &latin1, {"http", "", "", "host", 0, "/", "query", "fragment", "http://host/?query#fragment"});

    TextEncoding unrecognized(String("unrecognized invalid encoding name"));
    checkURL("http://host/?query", &unrecognized, {"http", "", "", "host", 0, "/", "", "", "http://host/?"});
    checkURL("http://host/?", &unrecognized, {"http", "", "", "host", 0, "/", "", "", "http://host/?"});

    TextEncoding iso88591(String("ISO-8859-1"));
    String withUmlauts = utf16String<4>({0xDC, 0x430, 0x451, '\0'});
    checkURL(makeString("ws://host/path?", withUmlauts), &iso88591, {"ws", "", "", "host", 0, "/path", "%C3%9C%D0%B0%D1%91", "", "ws://host/path?%C3%9C%D0%B0%D1%91"});
    checkURL(makeString("wss://host/path?", withUmlauts), &iso88591, {"wss", "", "", "host", 0, "/path", "%C3%9C%D0%B0%D1%91", "", "wss://host/path?%C3%9C%D0%B0%D1%91"});
    checkURL(makeString("asdf://host/path?", withUmlauts), &iso88591, {"asdf", "", "", "host", 0, "/path", "%C3%9C%D0%B0%D1%91", "", "asdf://host/path?%C3%9C%D0%B0%D1%91"});
    checkURL(makeString("https://host/path?", withUmlauts), &iso88591, {"https", "", "", "host", 0, "/path", "%DC%26%231072%3B%26%231105%3B", "", "https://host/path?%DC%26%231072%3B%26%231105%3B"});
    checkURL(makeString("gopher://host/path?", withUmlauts), &iso88591, {"gopher", "", "", "host", 0, "/path", "%C3%9C%D0%B0%D1%91", "", "gopher://host/path?%C3%9C%D0%B0%D1%91"});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "ws://example.com/", &iso88591, {"ws", "", "", "example.com", 0, "/path", "%C3%9C%D0%B0%D1%91", "fragment", "ws://example.com/path?%C3%9C%D0%B0%D1%91#fragment"});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "wss://example.com/", &iso88591, {"wss", "", "", "example.com", 0, "/path", "%C3%9C%D0%B0%D1%91", "fragment", "wss://example.com/path?%C3%9C%D0%B0%D1%91#fragment"});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "asdf://example.com/", &iso88591, {"asdf", "", "", "example.com", 0, "/path", "%C3%9C%D0%B0%D1%91", "fragment", "asdf://example.com/path?%C3%9C%D0%B0%D1%91#fragment"});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "https://example.com/", &iso88591, {"https", "", "", "example.com", 0, "/path", "%DC%26%231072%3B%26%231105%3B", "fragment", "https://example.com/path?%DC%26%231072%3B%26%231105%3B#fragment"});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "gopher://example.com/", &iso88591, {"gopher", "", "", "example.com", 0, "/path", "%C3%9C%D0%B0%D1%91", "fragment", "gopher://example.com/path?%C3%9C%D0%B0%D1%91#fragment"});
    checkURL(makeString("gopher://host/path?", withUmlauts, "#fragment"), "asdf://example.com/?doesntmatter", &iso88591, {"gopher", "", "", "host", 0, "/path", "%C3%9C%D0%B0%D1%91", "fragment", "gopher://host/path?%C3%9C%D0%B0%D1%91#fragment"});
    checkURL(makeString("asdf://host/path?", withUmlauts, "#fragment"), "http://example.com/?doesntmatter", &iso88591, {"asdf", "", "", "host", 0, "/path", "%C3%9C%D0%B0%D1%91", "fragment", "asdf://host/path?%C3%9C%D0%B0%D1%91#fragment"});

    checkURL("http://host/pa'th?qu'ery#fr'agment", nullptr, {"http", "", "", "host", 0, "/pa'th", "qu%27ery", "fr'agment", "http://host/pa'th?qu%27ery#fr'agment"});
    checkURL("asdf://host/pa'th?qu'ery#fr'agment", nullptr, {"asdf", "", "", "host", 0, "/pa'th", "qu'ery", "fr'agment", "asdf://host/pa'th?qu'ery#fr'agment"});
    // FIXME: Add more tests with other encodings and things like non-ascii characters, emoji and unmatched surrogate pairs.
}

} // namespace TestWebKitAPI
