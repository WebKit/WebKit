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

#include "Test.h"
#include "WTFTestUtilities.h"
#include <pal/text/TextEncoding.h>
#include <wtf/MainThread.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringBuilder.h>

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

static String insertTabAtLocation(StringView string, size_t location)
{
    ASSERT(location <= string.length());
    return makeString(string.left(location), '\t', string.substring(location));
}

static ExpectedParts invalidParts(const String& urlStringWithTab)
{
    return {emptyString(), emptyString(), emptyString(), emptyString(), 0, emptyString(), emptyString(), emptyString(), urlStringWithTab};
}

enum class TestTabs { No, Yes };

// Inserting tabs between surrogate pairs changes the encoded value instead of being skipped by the URLParser.
const TestTabs testTabsValueForSurrogatePairs = TestTabs::No;

static void checkURL(const String& urlString, const PAL::TextEncoding* encoding, const ExpectedParts& parts, TestTabs testTabs = TestTabs::Yes)
{
    auto url = URL({ }, urlString, encoding);
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.password()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port().value_or(0));
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

static void checkURL(const String& urlString, const String& baseURLString, const PAL::TextEncoding* encoding, const ExpectedParts& parts, TestTabs testTabs = TestTabs::Yes)
{
    auto url = URL(URL({ }, baseURLString), urlString, encoding);
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.password()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port().value_or(0));
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
    checkURL(utf16String(u"http://host?ß😍#ß😍"), nullptr, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, "%C3%9F%F0%9F%98%8D"_s, "%C3%9F%F0%9F%98%8D"_s, utf16String(u"http://host/?%C3%9F%F0%9F%98%8D#%C3%9F%F0%9F%98%8D")}, testTabsValueForSurrogatePairs);

    PAL::TextEncoding latin1("latin1");
    checkURL("http://host/?query with%20spaces"_s, &latin1, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, "query%20with%20spaces"_s, emptyString(), "http://host/?query%20with%20spaces"_s});
    checkURL("http://host/?query"_s, &latin1, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, "query"_s, emptyString(), "http://host/?query"_s});
    checkURL("http://host/?\tquery"_s, &latin1, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, "query"_s, emptyString(), "http://host/?query"_s});
    checkURL("http://host/?q\tuery"_s, &latin1, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, "query"_s, emptyString(), "http://host/?query"_s});
    checkURL("http://host/?query with SpAcEs#fragment"_s, &latin1, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, "query%20with%20SpAcEs"_s, "fragment"_s, "http://host/?query%20with%20SpAcEs#fragment"_s});
    checkURL("http://host/?que\rry\t\r\n#fragment"_s, &latin1, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, "query"_s, "fragment"_s, "http://host/?query#fragment"_s});

    PAL::TextEncoding unrecognized(String("unrecognized invalid encoding name"_s));
    checkURL("http://host/?query"_s, &unrecognized, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, emptyString(), emptyString(), "http://host/?"_s});
    checkURL("http://host/?"_s, &unrecognized, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/"_s, emptyString(), emptyString(), "http://host/?"_s});

    PAL::TextEncoding iso88591("ISO-8859-1");
    String withUmlauts = utf16String<4>({0xDC, 0x430, 0x451, '\0'});
    checkURL(makeString("ws://host/path?", withUmlauts), &iso88591, {"ws"_s, emptyString(), emptyString(), "host"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, emptyString(), "ws://host/path?%C3%9C%D0%B0%D1%91"_s});
    checkURL(makeString("wss://host/path?", withUmlauts), &iso88591, {"wss"_s, emptyString(), emptyString(), "host"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, emptyString(), "wss://host/path?%C3%9C%D0%B0%D1%91"_s});
    checkURL(makeString("asdf://host/path?", withUmlauts), &iso88591, {"asdf"_s, emptyString(), emptyString(), "host"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, emptyString(), "asdf://host/path?%C3%9C%D0%B0%D1%91"_s});
    checkURL(makeString("https://host/path?", withUmlauts), &iso88591, {"https"_s, emptyString(), emptyString(), "host"_s, 0, "/path"_s, "%DC%26%231072%3B%26%231105%3B"_s, emptyString(), "https://host/path?%DC%26%231072%3B%26%231105%3B"_s});
    checkURL(makeString("gopher://host/path?", withUmlauts), &iso88591, {"gopher"_s, emptyString(), emptyString(), "host"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, emptyString(), "gopher://host/path?%C3%9C%D0%B0%D1%91"_s});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "ws://example.com/"_s, &iso88591, {"ws"_s, emptyString(), emptyString(), "example.com"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, "fragment"_s, "ws://example.com/path?%C3%9C%D0%B0%D1%91#fragment"_s});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "wss://example.com/"_s, &iso88591, {"wss"_s, emptyString(), emptyString(), "example.com"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, "fragment"_s, "wss://example.com/path?%C3%9C%D0%B0%D1%91#fragment"_s});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "asdf://example.com/"_s, &iso88591, {"asdf"_s, emptyString(), emptyString(), "example.com"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, "fragment"_s, "asdf://example.com/path?%C3%9C%D0%B0%D1%91#fragment"_s});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "https://example.com/"_s, &iso88591, {"https"_s, emptyString(), emptyString(), "example.com"_s, 0, "/path"_s, "%DC%26%231072%3B%26%231105%3B"_s, "fragment"_s, "https://example.com/path?%DC%26%231072%3B%26%231105%3B#fragment"_s});
    checkURL(makeString("/path?", withUmlauts, "#fragment"), "gopher://example.com/"_s, &iso88591, {"gopher"_s, emptyString(), emptyString(), "example.com"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, "fragment"_s, "gopher://example.com/path?%C3%9C%D0%B0%D1%91#fragment"_s});
    checkURL(makeString("gopher://host/path?", withUmlauts, "#fragment"), "asdf://example.com/?doesntmatter"_s, &iso88591, {"gopher"_s, emptyString(), emptyString(), "host"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, "fragment"_s, "gopher://host/path?%C3%9C%D0%B0%D1%91#fragment"_s});
    checkURL(makeString("asdf://host/path?", withUmlauts, "#fragment"), "http://example.com/?doesntmatter"_s, &iso88591, {"asdf"_s, emptyString(), emptyString(), "host"_s, 0, "/path"_s, "%C3%9C%D0%B0%D1%91"_s, "fragment"_s, "asdf://host/path?%C3%9C%D0%B0%D1%91#fragment"_s});

    checkURL("http://host/pa'th?qu'ery#fr'agment"_s, nullptr, {"http"_s, emptyString(), emptyString(), "host"_s, 0, "/pa'th"_s, "qu%27ery"_s, "fr'agment"_s, "http://host/pa'th?qu%27ery#fr'agment"_s});
    checkURL("asdf://host/pa'th?qu'ery#fr'agment"_s, nullptr, {"asdf"_s, emptyString(), emptyString(), "host"_s, 0, "/pa'th"_s, "qu'ery"_s, "fr'agment"_s, "asdf://host/pa'th?qu'ery#fr'agment"_s});
    // FIXME: Add more tests with other encodings and things like non-ascii characters, emoji and unmatched surrogate pairs.
}

} // namespace TestWebKitAPI
