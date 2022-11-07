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
    StringView protocol;
    StringView user;
    StringView password;
    StringView host;
    unsigned short port;
    StringView path;
    StringView query;
    StringView fragment;
    StringView string;

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

bool eq(StringView s1, StringView s2)
{
    EXPECT_STREQ(s1.utf8().data(), s2.utf8().data());
    return s1.utf8() == s2.utf8();
}

static String insertTabAtLocation(StringView string, size_t location)
{
    ASSERT(location <= string.length());
    return makeString(string.left(location), '\t', string.substring(location));
}

static ExpectedParts invalidParts(StringView urlStringWithTab)
{
    return { ""_s, ""_s, ""_s, ""_s, 0, ""_s , ""_s, ""_s, urlStringWithTab };
}

enum class TestTabs { No, Yes };

// Inserting tabs between surrogate pairs changes the encoded value instead of being skipped by the URLParser.
const TestTabs testTabsValueForSurrogatePairs = TestTabs::No;

static void checkURL(StringView urlString, const ExpectedParts& parts, TestTabs testTabs = TestTabs::Yes)
{
    URL url { urlString.toString() };
    
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.password()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port().value_or(0));
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

static void checkRelativeURL(StringView urlString, StringView baseURL, const ExpectedParts& parts, TestTabs testTabs = TestTabs::Yes)
{
    auto baseURLString = baseURL.toString();
    auto url = URL(URL(baseURLString), urlString.toString());
    
    EXPECT_TRUE(eq(parts.protocol, url.protocol()));
    EXPECT_TRUE(eq(parts.user, url.user()));
    EXPECT_TRUE(eq(parts.password, url.password()));
    EXPECT_TRUE(eq(parts.host, url.host()));
    EXPECT_EQ(parts.port, url.port().value_or(0));
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

static void checkURLDifferences(StringView urlString, const ExpectedParts& partsNew, const ExpectedParts& partsOld, TestTabs testTabs = TestTabs::Yes)
{
    UNUSED_PARAM(partsOld); // FIXME: Remove all the old expected parts.
    URL url { urlString.toString() };
    
    EXPECT_TRUE(eq(partsNew.protocol, url.protocol()));
    EXPECT_TRUE(eq(partsNew.user, url.user()));
    EXPECT_TRUE(eq(partsNew.password, url.password()));
    EXPECT_TRUE(eq(partsNew.host, url.host()));
    EXPECT_EQ(partsNew.port, url.port().value_or(0));
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

static void checkRelativeURLDifferences(StringView urlString, StringView baseURLString, const ExpectedParts& partsNew, const ExpectedParts& partsOld, TestTabs testTabs = TestTabs::Yes)
{
    UNUSED_PARAM(partsOld); // FIXME: Remove all the old expected parts.
    auto url = URL(URL(baseURLString.toString()), urlString.toString());
    
    EXPECT_TRUE(eq(partsNew.protocol, url.protocol()));
    EXPECT_TRUE(eq(partsNew.user, url.user()));
    EXPECT_TRUE(eq(partsNew.password, url.password()));
    EXPECT_TRUE(eq(partsNew.host, url.host()));
    EXPECT_EQ(partsNew.port, url.port().value_or(0));
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

static void shouldFail(StringView urlString)
{
    checkURL(urlString, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, urlString });
}

static void shouldFail(StringView urlString, StringView baseString)
{
    checkRelativeURL(urlString, baseString, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, urlString });
}

TEST_F(WTF_URLParser, Idempotence)
{
    checkURL("a://"_s, { "a"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "a://"_s });
    checkURL("b:///"_s, { "b"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "b:///"_s });
    checkURL("c:/.//"_s, { "c"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "c:/.//"_s });
    checkURL("d:/..//"_s, { "d"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "d:/.//"_s });
    checkURL("e:/../..//"_s, { "e"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "e:/.//"_s });
    checkURL("f:/../../"_s, { "f"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "f:/"_s });
    checkURL("g:/././"_s, { "g"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "g:/"_s });
    checkURL("h:/./../"_s, { "h"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "h:/"_s });
    checkURL("i:/.././"_s, { "i"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "i:/"_s });
    checkURL("j:/./..//"_s, { "j"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "j:/.//"_s });
    checkURL("k:/.././/"_s, { "k"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "k:/.//"_s });
    checkURL("l:/.?"_s, { "l"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "l:/?"_s });
    checkURL("m:/./?"_s, { "m"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "m:/?"_s });
    checkURL("n:/.//?"_s, { "n"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "n:/.//?"_s });
    checkURL("o:/.#"_s, { "o"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "o:/#"_s });
    checkURL("p:/%2e//"_s, { "p"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "p:/.//"_s });
    checkURL("q:/%2e%2e//"_s, { "q"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "q:/.//"_s });
    checkURL("r:/%2e%2e/"_s, { "r"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "r:/"_s });
    checkURL("s:/%2e/"_s, { "s"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "s:/"_s });
    checkURL("t:/.//p/../../../..//x"_s, { "t"_s, ""_s, ""_s, ""_s, 0, "//x"_s, ""_s, ""_s, "t:/.//x"_s });
    checkRelativeURL("../path"_s, "u:/.//p"_s, { "u"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "u:/path"_s });
    checkURL("v:/.//.."_s, { "v"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "v:/"_s });
    checkURL("w:/.//..//"_s, { "w"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "w:/.//"_s });
    checkURL("x:/.//../a"_s, { "x"_s, ""_s, ""_s, ""_s, 0, "/a"_s, ""_s, ""_s, "x:/a"_s });
    checkURL("http://host/./"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkURL("http://host/../"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkURL("http://host/.../"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/.../"_s, ""_s, ""_s, "http://host/.../"_s });
    checkURL("http://host/.."_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkURL("http://host/."_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
}

TEST_F(WTF_URLParser, Basic)
{
    checkURL("http://user:pass@webkit.org:123/path?query#fragment"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 123, "/path"_s, "query"_s, "fragment"_s, "http://user:pass@webkit.org:123/path?query#fragment"_s });
    checkURL("http://user:pass@webkit.org:123/path?query"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 123, "/path"_s, "query"_s, ""_s, "http://user:pass@webkit.org:123/path?query"_s });
    checkURL("http://user:pass@webkit.org:123/path"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 123, "/path"_s, ""_s, ""_s, "http://user:pass@webkit.org:123/path"_s });
    checkURL("http://user:pass@webkit.org:123/"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 123, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org:123/"_s });
    checkURL("http://user:pass@webkit.org:123"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 123, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org:123/"_s });
    checkURL("http://user:pass@webkit.org"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org/"_s });
    checkURL("http://user:\t\t\tpass@webkit.org"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org/"_s });
    checkURL("http://us\ter:pass@webkit.org"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org/"_s });
    checkURL("http://user:pa\tss@webkit.org"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org/"_s });
    checkURL("http://user:pass\t@webkit.org"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org/"_s });
    checkURL("http://\tuser:pass@webkit.org"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org/"_s });
    checkURL("http://user\t:pass@webkit.org"_s, { "http"_s, "user"_s, "pass"_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://user:pass@webkit.org/"_s });
    checkURL("http://webkit.org"_s, { "http"_s, ""_s, ""_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://webkit.org/"_s });
    checkURL("http://127.0.0.1"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s });
    checkURL("http://webkit.org/"_s, { "http"_s, ""_s, ""_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://webkit.org/"_s });
    checkURL("http://webkit.org/path1/path2/index.html"_s, { "http"_s, ""_s, ""_s, "webkit.org"_s, 0, "/path1/path2/index.html"_s, ""_s, ""_s, "http://webkit.org/path1/path2/index.html"_s });
    checkURL("about:blank"_s, { "about"_s, ""_s, ""_s, ""_s, 0, "blank"_s, ""_s, ""_s, "about:blank"_s });
    checkURL("about:blank?query"_s, { "about"_s, ""_s, ""_s, ""_s, 0, "blank"_s, "query"_s, ""_s, "about:blank?query"_s });
    checkURL("about:blank#fragment"_s, { "about"_s, ""_s, ""_s, ""_s, 0, "blank"_s, ""_s, "fragment"_s, "about:blank#fragment"_s });
    checkURL("http://[0::0]/"_s, { "http"_s, ""_s, ""_s, "[::]"_s, 0, "/"_s, ""_s, ""_s, "http://[::]/"_s });
    checkURL("http://[0::]/"_s, { "http"_s, ""_s, ""_s, "[::]"_s, 0, "/"_s, ""_s, ""_s, "http://[::]/"_s });
    checkURL("http://[::]/"_s, { "http"_s, ""_s, ""_s, "[::]"_s, 0, "/"_s, ""_s, ""_s, "http://[::]/"_s });
    checkURL("http://[::0]/"_s, { "http"_s, ""_s, ""_s, "[::]"_s, 0, "/"_s, ""_s, ""_s, "http://[::]/"_s });
    checkURL("http://[::0:0]/"_s, { "http"_s, ""_s, ""_s, "[::]"_s, 0, "/"_s, ""_s, ""_s, "http://[::]/"_s });
    checkURL("http://[f::0:0]/"_s, { "http"_s, ""_s, ""_s, "[f::]"_s, 0, "/"_s, ""_s, ""_s, "http://[f::]/"_s });
    checkURL("http://[f:0::f]/"_s, { "http"_s, ""_s, ""_s, "[f::f]"_s, 0, "/"_s, ""_s, ""_s, "http://[f::f]/"_s });
    checkURL("http://[::0:ff]/"_s, { "http"_s, ""_s, ""_s, "[::ff]"_s, 0, "/"_s, ""_s, ""_s, "http://[::ff]/"_s });
    checkURL("http://[::00:0:0:0]/"_s, { "http"_s, ""_s, ""_s, "[::]"_s, 0, "/"_s, ""_s, ""_s, "http://[::]/"_s });
    checkURL("http://[::0:00:0:0]/"_s, { "http"_s, ""_s, ""_s, "[::]"_s, 0, "/"_s, ""_s, ""_s, "http://[::]/"_s });
    checkURL("http://[::0:0.0.0.0]/"_s, { "http"_s, ""_s, ""_s, "[::]"_s, 0, "/"_s, ""_s, ""_s, "http://[::]/"_s });
    checkURL("http://[0:f::f:f:0:0]"_s, { "http"_s, ""_s, ""_s, "[0:f::f:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f::f:f:0:0]/"_s });
    checkURL("http://[0:f:0:0:f::]"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]/"_s });
    checkURL("http://[::f:0:0:f:0:0]"_s, { "http"_s, ""_s, ""_s, "[::f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::f:0:0:f:0:0]/"_s });
    checkURL("http://[0:f:0:0:f::]:"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]/"_s });
    checkURL("http://[0:f:0:0:f::]:\t"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]/"_s });
    checkURL("http://[0:f:0:0:f::]\t:"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]/"_s });
    checkURL("http://\t[::f:0:0:f:0:0]"_s, { "http"_s, ""_s, ""_s, "[::f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::f:0:0:f:0:0]/"_s });
    checkURL("http://[\t::f:0:0:f:0:0]"_s, { "http"_s, ""_s, ""_s, "[::f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::f:0:0:f:0:0]/"_s });
    checkURL("http://[:\t:f:0:0:f:0:0]"_s, { "http"_s, ""_s, ""_s, "[::f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::f:0:0:f:0:0]/"_s });
    checkURL("http://[::\tf:0:0:f:0:0]"_s, { "http"_s, ""_s, ""_s, "[::f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::f:0:0:f:0:0]/"_s });
    checkURL("http://[::f\t:0:0:f:0:0]"_s, { "http"_s, ""_s, ""_s, "[::f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::f:0:0:f:0:0]/"_s });
    checkURL("http://[::f:\t0:0:f:0:0]"_s, { "http"_s, ""_s, ""_s, "[::f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::f:0:0:f:0:0]/"_s });
    checkURL("http://example.com/path1/path2/."_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/"_s, ""_s, ""_s, "http://example.com/path1/path2/"_s });
    checkURL("http://example.com/path1/path2/.."_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/"_s, ""_s, ""_s, "http://example.com/path1/"_s });
    checkURL("http://example.com/path1/path2/./path3"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/path3"_s, ""_s, ""_s, "http://example.com/path1/path2/path3"_s });
    checkURL("http://example.com/path1/path2/.\\path3"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/path3"_s, ""_s, ""_s, "http://example.com/path1/path2/path3"_s });
    checkURL("http://example.com/path1/path2/../path3"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path3"_s, ""_s, ""_s, "http://example.com/path1/path3"_s });
    checkURL("http://example.com/path1/path2/..\\path3"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path3"_s, ""_s, ""_s, "http://example.com/path1/path3"_s });
    checkURL("http://example.com/."_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkURL("http://example.com/.."_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkURL("http://example.com/./path1"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1"_s, ""_s, ""_s, "http://example.com/path1"_s });
    checkURL("http://example.com/../path1"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1"_s, ""_s, ""_s, "http://example.com/path1"_s });
    checkURL("http://example.com/../path1/../../path2/path3/../path4"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path2/path4"_s, ""_s, ""_s, "http://example.com/path2/path4"_s });
    checkURL("http://example.com/path1/.%2"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/.%2"_s, ""_s, ""_s, "http://example.com/path1/.%2"_s });
    checkURL("http://example.com/path1/%2"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/%2"_s, ""_s, ""_s, "http://example.com/path1/%2"_s });
    checkURL("http://example.com/path1/%"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/%"_s, ""_s, ""_s, "http://example.com/path1/%"_s });
    checkURL("http://example.com/path1/.%"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/.%"_s, ""_s, ""_s, "http://example.com/path1/.%"_s });
    checkURL("http://example.com//."_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "//"_s, ""_s, ""_s, "http://example.com//"_s });
    checkURL("http://example.com//./"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "//"_s, ""_s, ""_s, "http://example.com//"_s });
    checkURL("http://example.com//.//"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "///"_s, ""_s, ""_s, "http://example.com///"_s });
    checkURL("http://example.com//.."_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkURL("http://example.com//../"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkURL("http://example.com//..//"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "//"_s, ""_s, ""_s, "http://example.com//"_s });
    checkURL("http://example.com//.."_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkURL("http://example.com/.//"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "//"_s, ""_s, ""_s, "http://example.com//"_s });
    checkURL("http://example.com/..//"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "//"_s, ""_s, ""_s, "http://example.com//"_s });
    checkURL("http://example.com/./"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkURL("http://example.com/../"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkURL("http://example.com/path1/.../path3"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/.../path3"_s, ""_s, ""_s, "http://example.com/path1/.../path3"_s });
    checkURL("http://example.com/path1/..."_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/..."_s, ""_s, ""_s, "http://example.com/path1/..."_s });
    checkURL("http://example.com/path1/.../"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/.../"_s, ""_s, ""_s, "http://example.com/path1/.../"_s });
    checkURL("http://example.com/.path1/"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/.path1/"_s, ""_s, ""_s, "http://example.com/.path1/"_s });
    checkURL("http://example.com/..path1/"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/..path1/"_s, ""_s, ""_s, "http://example.com/..path1/"_s });
    checkURL("http://example.com/path1/.path2"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/.path2"_s, ""_s, ""_s, "http://example.com/path1/.path2"_s });
    checkURL("http://example.com/path1/..path2"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/..path2"_s, ""_s, ""_s, "http://example.com/path1/..path2"_s });
    checkURL("http://example.com/path1/path2/.?query"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/"_s, "query"_s, ""_s, "http://example.com/path1/path2/?query"_s });
    checkURL("http://example.com/path1/path2/..?query"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/"_s, "query"_s, ""_s, "http://example.com/path1/?query"_s });
    checkURL("http://example.com/path1/path2/.#fragment"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/"_s, ""_s, "fragment"_s, "http://example.com/path1/path2/#fragment"_s });
    checkURL("http://example.com/path1/path2/..#fragment"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/"_s, ""_s, "fragment"_s, "http://example.com/path1/#fragment"_s });

    checkURL("file:"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "file:///"_s });
    checkURL("file:/"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "file:///"_s });
    checkURL("file://"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "file:///"_s });
    checkURL("file:///"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "file:///"_s });
    checkURL("file:////"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "file:////"_s }); // This matches Firefox and URL::parse which I believe are correct, but not Chrome.
    checkURL("file:/path"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path"_s });
    checkURL("file://host/path"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/path"_s, ""_s, ""_s, "file://host/path"_s });
    checkURL("file://host"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "file://host/"_s });
    checkURL("file://host/"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "file://host/"_s });
    checkURL("file:///path"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path"_s });
    checkURL("file:////path"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "//path"_s, ""_s, ""_s, "file:////path"_s });
    checkURL("file://localhost/path"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path"_s });
    checkURL("file://localhost/"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "file:///"_s });
    checkURL("file://localhost"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "file:///"_s });
    checkURL("file://lOcAlHoSt"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "file:///"_s });
    checkURL("file://lOcAlHoSt/"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "file:///"_s });
    checkURL("file:/pAtH/"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/pAtH/"_s, ""_s, ""_s, "file:///pAtH/"_s });
    checkURL("file:/pAtH"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/pAtH"_s, ""_s, ""_s, "file:///pAtH"_s });
    checkURL("file:?query"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, "query"_s, ""_s, "file:///?query"_s });
    checkURL("file:#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, "fragment"_s, "file:///#fragment"_s });
    checkURL("file:?query#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, "query"_s, "fragment"_s, "file:///?query#fragment"_s });
    checkURL("file:#fragment?notquery"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, "fragment?notquery"_s, "file:///#fragment?notquery"_s });
    checkURL("file:/?query"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, "query"_s, ""_s, "file:///?query"_s });
    checkURL("file:/#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, "fragment"_s, "file:///#fragment"_s });
    checkURL("file://?query"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, "query"_s, ""_s, "file:///?query"_s });
    checkURL("file://#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, "fragment"_s, "file:///#fragment"_s });
    checkURL("file:///?query"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, "query"_s, ""_s, "file:///?query"_s });
    checkURL("file:///#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, "fragment"_s, "file:///#fragment"_s });
    checkURL("file:////?query"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "//"_s, "query"_s, ""_s, "file:////?query"_s });
    checkURL("file:////#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, "fragment"_s, "file:////#fragment"_s });
    checkURL("file://?Q"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, "Q"_s, ""_s, "file:///?Q"_s });
    checkURL("file://#F"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, "F"_s, "file:///#F"_s });
    checkURL("file://host?Q"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/"_s, "Q"_s, ""_s, "file://host/?Q"_s });
    checkURL("file://host#F"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, "F"_s, "file://host/#F"_s });
    checkURL("file://host\\P"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/P"_s, ""_s, ""_s, "file://host/P"_s });
    checkURL("file://host\\?Q"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/"_s, "Q"_s, ""_s, "file://host/?Q"_s });
    checkURL("file://host\\../P"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/P"_s, ""_s, ""_s, "file://host/P"_s });
    checkURL("file://host\\/../P"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/P"_s, ""_s, ""_s, "file://host/P"_s });
    checkURL("file://host\\/P"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "//P"_s, ""_s, ""_s, "file://host//P"_s });
    checkURL("http://host/A b"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/A%20b"_s, ""_s, ""_s, "http://host/A%20b"_s });
    checkURL("http://host/a%20B"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/a%20B"_s, ""_s, ""_s, "http://host/a%20B"_s });
    checkURL("http://host?q=@ <>!#fragment"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, "q=@%20%3C%3E!"_s, "fragment"_s, "http://host/?q=@%20%3C%3E!#fragment"_s });
    checkURL("http://user:@host"_s, { "http"_s, "user"_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://user@host/"_s });
    checkURL("http://user:@\thost"_s, { "http"_s, "user"_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://user@host/"_s });
    checkURL("http://user:\t@host"_s, { "http"_s, "user"_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://user@host/"_s });
    checkURL("http://user\t:@host"_s, { "http"_s, "user"_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://user@host/"_s });
    checkURL("http://use\tr:@host"_s, { "http"_s, "user"_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://user@host/"_s });
    checkURL("http://127.0.0.1:10100/path"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 10100, "/path"_s, ""_s, ""_s, "http://127.0.0.1:10100/path"_s });
    checkURL("http://127.0.0.1:/path"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/path"_s, ""_s, ""_s, "http://127.0.0.1/path"_s });
    checkURL("http://127.0.0.1\t:/path"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/path"_s, ""_s, ""_s, "http://127.0.0.1/path"_s });
    checkURL("http://127.0.0.1:\t/path"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/path"_s, ""_s, ""_s, "http://127.0.0.1/path"_s });
    checkURL("http://127.0.0.1:/\tpath"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/path"_s, ""_s, ""_s, "http://127.0.0.1/path"_s });
    checkURL("http://127.0.0.1:123"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 123, "/"_s, ""_s, ""_s, "http://127.0.0.1:123/"_s });
    checkURL("http://127.0.0.1:"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s });
    shouldFail("ws://08./"_s);
    checkURL("http://[0:f::f:f:0:0]:123/path"_s, { "http"_s, ""_s, ""_s, "[0:f::f:f:0:0]"_s, 123, "/path"_s, ""_s, ""_s, "http://[0:f::f:f:0:0]:123/path"_s });
    checkURL("http://[0:f::f:f:0:0]:123"_s, { "http"_s, ""_s, ""_s, "[0:f::f:f:0:0]"_s, 123, "/"_s, ""_s, ""_s, "http://[0:f::f:f:0:0]:123/"_s });
    checkURL("http://[0:f:0:0:f:\t:]:123"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 123, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]:123/"_s });
    checkURL("http://[0:f:0:0:f::\t]:123"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 123, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]:123/"_s });
    checkURL("http://[0:f:0:0:f::]\t:123"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 123, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]:123/"_s });
    checkURL("http://[0:f:0:0:f::]:\t123"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 123, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]:123/"_s });
    checkURL("http://[0:f:0:0:f::]:1\t23"_s, { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 123, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]:123/"_s });
    checkURL("http://[0:f::f:f:0:0]:/path"_s, { "http"_s, ""_s, ""_s, "[0:f::f:f:0:0]"_s, 0, "/path"_s, ""_s, ""_s, "http://[0:f::f:f:0:0]/path"_s });
    checkURL("a://[::2:]"_s, { "a"_s, ""_s, ""_s, "[::2]"_s, 0, ""_s, ""_s, ""_s, "a://[::2]"_s });
    checkURL("http://[0:f::f:f:0:0]:"_s, { "http"_s, ""_s, ""_s, "[0:f::f:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f::f:f:0:0]/"_s });
    checkURL("http://host:10100/path"_s, { "http"_s, ""_s, ""_s, "host"_s, 10100, "/path"_s, ""_s, ""_s, "http://host:10100/path"_s });
    checkURL("http://host:/path"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/path"_s, ""_s, ""_s, "http://host/path"_s });
    checkURL("http://host:123"_s, { "http"_s, ""_s, ""_s, "host"_s, 123, "/"_s, ""_s, ""_s, "http://host:123/"_s });
    checkURL("http://host:"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkURL("http://hos\tt\n:\t1\n2\t3\t/\npath"_s, { "http"_s, ""_s, ""_s, "host"_s, 123, "/path"_s, ""_s, ""_s, "http://host:123/path"_s });
    checkURL("http://user@example.org/path3"_s, { "http"_s, "user"_s, ""_s, "example.org"_s, 0, "/path3"_s, ""_s, ""_s, "http://user@example.org/path3"_s });
    checkURL("sc:/pa/pa"_s, { "sc"_s, ""_s, ""_s, ""_s, 0, "/pa/pa"_s, ""_s, ""_s, "sc:/pa/pa"_s });
    checkURL("sc:/pa"_s, { "sc"_s, ""_s, ""_s, ""_s, 0, "/pa"_s, ""_s, ""_s, "sc:/pa"_s });
    checkURL("sc:/pa/"_s, { "sc"_s, ""_s, ""_s, ""_s, 0, "/pa/"_s, ""_s, ""_s, "sc:/pa/"_s });
    checkURL("notspecial:/notuser:notpassword@nothost"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, "/notuser:notpassword@nothost"_s, ""_s, ""_s, "notspecial:/notuser:notpassword@nothost"_s });
    checkURL("sc://pa/"_s, { "sc"_s, ""_s, ""_s, "pa"_s, 0, "/"_s, ""_s, ""_s, "sc://pa/"_s });
    checkURL("sc://\tpa/"_s, { "sc"_s, ""_s, ""_s, "pa"_s, 0, "/"_s, ""_s, ""_s, "sc://pa/"_s });
    checkURL("sc:/\t/pa/"_s, { "sc"_s, ""_s, ""_s, "pa"_s, 0, "/"_s, ""_s, ""_s, "sc://pa/"_s });
    checkURL("sc:\t//pa/"_s, { "sc"_s, ""_s, ""_s, "pa"_s, 0, "/"_s, ""_s, ""_s, "sc://pa/"_s });
    checkURL("http://host   \a   "_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkURL("notspecial:/a"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, "/a"_s, ""_s, ""_s, "notspecial:/a"_s });
    checkURL("notspecial:"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "notspecial:"_s });
    checkURL("http:/a"_s, { "http"_s, ""_s, ""_s, "a"_s, 0, "/"_s, ""_s, ""_s, "http://a/"_s });
    checkURL("http://256../"_s, { "http"_s, ""_s, ""_s, "256.."_s, 0, "/"_s, ""_s, ""_s, "http://256../"_s });
    checkURL("http://256.."_s, { "http"_s, ""_s, ""_s, "256.."_s, 0, "/"_s, ""_s, ""_s, "http://256../"_s });
    shouldFail("http://127..1/"_s);
    shouldFail("http://127.a.0.1/"_s);
    checkURL("http://127.0.0.1/"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s });
    checkURL("http://12\t7.0.0.1/"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s });
    checkURL("http://127.\t0.0.1/"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s });
    checkURL("http://./"_s, { "http"_s, ""_s, ""_s, "."_s, 0, "/"_s, ""_s, ""_s, "http://./"_s });
    checkURL("http://."_s, { "http"_s, ""_s, ""_s, "."_s, 0, "/"_s, ""_s, ""_s, "http://./"_s });
    checkURL("notspecial:/a"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, "/a"_s, ""_s, ""_s, "notspecial:/a"_s });
    checkURL("notspecial:"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "notspecial:"_s });
    checkURL("notspecial:/"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "notspecial:/"_s });
    checkURL("data:image/png;base64,encoded-data-follows-here"_s, { "data"_s, ""_s, ""_s, ""_s, 0, "image/png;base64,encoded-data-follows-here"_s, ""_s, ""_s, "data:image/png;base64,encoded-data-follows-here"_s });
    checkURL("data:image/png;base64,encoded/data-with-slash"_s, { "data"_s, ""_s, ""_s, ""_s, 0, "image/png;base64,encoded/data-with-slash"_s, ""_s, ""_s, "data:image/png;base64,encoded/data-with-slash"_s });
    checkURL("about:~"_s, { "about"_s, ""_s, ""_s, ""_s, 0, "~"_s, ""_s, ""_s, "about:~"_s });
    checkURL("https://@test@test@example:800\\path@end"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "https://@test@test@example:800\\path@end"_s });
    checkURL("http://www.example.com/#a\nb\rc\td"_s, { "http"_s, ""_s, ""_s, "www.example.com"_s, 0, "/"_s, ""_s, "abcd"_s, "http://www.example.com/#abcd"_s });
    checkURL("http://[A:b:c:DE:fF:0:1:aC]/"_s, { "http"_s, ""_s, ""_s, "[a:b:c:de:ff:0:1:ac]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:de:ff:0:1:ac]/"_s });
    checkURL("http:////////user:@webkit.org:99?foo"_s, { "http"_s, "user"_s, ""_s, "webkit.org"_s, 99, "/"_s, "foo"_s, ""_s, "http://user@webkit.org:99/?foo"_s });
    checkURL("http:////////user:@webkit.org:99#foo"_s, { "http"_s, "user"_s, ""_s, "webkit.org"_s, 99, "/"_s, ""_s, "foo"_s, "http://user@webkit.org:99/#foo"_s });
    checkURL("http:////\t////user:@webkit.org:99?foo"_s, { "http"_s, "user"_s, ""_s, "webkit.org"_s, 99, "/"_s, "foo"_s, ""_s, "http://user@webkit.org:99/?foo"_s });
    checkURL("http://\t//\\///user:@webkit.org:99?foo"_s, { "http"_s, "user"_s, ""_s, "webkit.org"_s, 99, "/"_s, "foo"_s, ""_s, "http://user@webkit.org:99/?foo"_s });
    checkURL("http:/\\user:@webkit.org:99?foo"_s, { "http"_s, "user"_s, ""_s, "webkit.org"_s, 99, "/"_s, "foo"_s, ""_s, "http://user@webkit.org:99/?foo"_s });
    checkURL("http://127.0.0.1"_s, { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s });
    checkURLDifferences("http://127.0.0.1."_s,
        { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s },
        { "http"_s, ""_s, ""_s, "127.0.0.1."_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1./"_s });
    checkURLDifferences("http://127.0.0.1./"_s,
        { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s },
        { "http"_s, ""_s, ""_s, "127.0.0.1."_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1./"_s });
    checkURL("http://127.0.0.1../"_s, { "http"_s, ""_s, ""_s, "127.0.0.1.."_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1../"_s });
    checkURLDifferences("http://0x100.0/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://0x100.0/"_s },
        { "http"_s, ""_s, ""_s, "0x100.0"_s, 0, "/"_s, ""_s, ""_s, "http://0x100.0/"_s });
    checkURLDifferences("http://0.0.0x100.0/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://0.0.0x100.0/"_s },
        { "http"_s, ""_s, ""_s, "0.0.0x100.0"_s, 0, "/"_s, ""_s, ""_s, "http://0.0.0x100.0/"_s });
    checkURLDifferences("http://0.0.0.0x100/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://0.0.0.0x100/"_s },
        { "http"_s, ""_s, ""_s, "0.0.0.0x100"_s, 0, "/"_s, ""_s, ""_s, "http://0.0.0.0x100/"_s });
    checkURL("http://host:123?"_s, { "http"_s, ""_s, ""_s, "host"_s, 123, "/"_s, ""_s, ""_s, "http://host:123/?"_s });
    checkURL("http://host:123?query"_s, { "http"_s, ""_s, ""_s, "host"_s, 123, "/"_s, "query"_s, ""_s, "http://host:123/?query"_s });
    checkURL("http://host:123#"_s, { "http"_s, ""_s, ""_s, "host"_s, 123, "/"_s, ""_s, ""_s, "http://host:123/#"_s });
    checkURL("http://host:123#fragment"_s, { "http"_s, ""_s, ""_s, "host"_s, 123, "/"_s, ""_s, "fragment"_s, "http://host:123/#fragment"_s });
    checkURLDifferences("foo:////"_s,
        { "foo"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "foo:////"_s },
        { "foo"_s, ""_s, ""_s, ""_s, 0, "////"_s, ""_s, ""_s, "foo:////"_s });
    checkURLDifferences("foo:///?"_s,
        { "foo"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "foo:///?"_s },
        { "foo"_s, ""_s, ""_s, ""_s, 0, "///"_s, ""_s, ""_s, "foo:///?"_s });
    checkURLDifferences("foo:///#"_s,
        { "foo"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "foo:///#"_s },
        { "foo"_s, ""_s, ""_s, ""_s, 0, "///"_s, ""_s, ""_s, "foo:///#"_s });
    checkURLDifferences("foo:///"_s,
        { "foo"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "foo:///"_s },
        { "foo"_s, ""_s, ""_s, ""_s, 0, "///"_s, ""_s, ""_s, "foo:///"_s });
    checkURLDifferences("foo://?"_s,
        { "foo"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "foo://?"_s },
        { "foo"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "foo://?"_s });
    checkURLDifferences("foo://#"_s,
        { "foo"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "foo://#"_s },
        { "foo"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "foo://#"_s });
    checkURLDifferences("foo://"_s,
        { "foo"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "foo://"_s },
        { "foo"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "foo://"_s });
    checkURL("foo:/?"_s, { "foo"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "foo:/?"_s });
    checkURL("foo:/#"_s, { "foo"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "foo:/#"_s });
    checkURL("foo:/"_s, { "foo"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "foo:/"_s });
    checkURL("foo:?"_s, { "foo"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "foo:?"_s });
    checkURL("foo:#"_s, { "foo"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "foo:#"_s });
    checkURLDifferences("A://"_s,
        { "a"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "a://"_s },
        { "a"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "a://"_s });
    checkURLDifferences("aA://"_s,
        { "aa"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "aa://"_s },
        { "aa"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "aa://"_s });
    checkURL(utf16String(u"foo://host/#ПП\u0007 a</"), { "foo"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, "%D0%9F%D0%9F%07%20a%3C/"_s, "foo://host/#%D0%9F%D0%9F%07%20a%3C/"_s });
    checkURL(utf16String(u"foo://host/#\u0007 a</"), { "foo"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, "%07%20a%3C/"_s, "foo://host/#%07%20a%3C/"_s });
    checkURL(utf16String(u"http://host?ß😍#ß😍"), { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, "%C3%9F%F0%9F%98%8D"_s, "%C3%9F%F0%9F%98%8D"_s, "http://host/?%C3%9F%F0%9F%98%8D#%C3%9F%F0%9F%98%8D"_s }, testTabsValueForSurrogatePairs);
    checkURL(utf16String(u"http://host/path#💩\t💩"), { "http"_s, ""_s, ""_s, "host"_s, 0, "/path"_s, ""_s, "%F0%9F%92%A9%F0%9F%92%A9"_s, "http://host/path#%F0%9F%92%A9%F0%9F%92%A9"_s }, testTabsValueForSurrogatePairs);
    checkURL(utf16String(u"http://host/#ПП\u0007 a</"), { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, "%D0%9F%D0%9F%07%20a%3C/"_s, "http://host/#%D0%9F%D0%9F%07%20a%3C/"_s });
    checkURL(utf16String(u"http://host/#\u0007 a</"), { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, "%07%20a%3C/"_s, "http://host/#%07%20a%3C/"_s });

    // This disagrees with the web platform test for http://:@www.example.com but agrees with Chrome and URL::parse,
    // and Firefox fails the web platform test differently. Maybe the web platform test ought to be changed.
    checkURL("http://:@host"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
}

static void testUserPassword(StringView value, StringView decoded, StringView encoded)
{
    URL userURL { makeString("http://"_s, value, "@example.com/"_s) };
    URL passURL { makeString("http://user:"_s, value, "@example.com/"_s) };
    EXPECT_EQ(encoded, userURL.encodedUser());
    EXPECT_EQ(encoded, passURL.encodedPassword());
    EXPECT_EQ(decoded, userURL.user());
    EXPECT_EQ(decoded, passURL.password());
}

static void testUserPassword(StringView value, StringView encoded)
{
    testUserPassword(value, value, encoded);
}

TEST_F(WTF_URLParser, Credentials)
{
    auto validSurrogate = utf16String<3>({0xD800, 0xDD55, '\0'});
    auto invalidSurrogate = utf16String<3>({0xD800, 'A', '\0'});
    auto replacementA = utf16String<3>({0xFFFD, 'A', '\0'});

    testUserPassword("a"_s, "a"_s);
    testUserPassword("%"_s, "%"_s);
    testUserPassword("%25"_s, "%"_s, "%25"_s);
    testUserPassword("%2525"_s, "%25"_s, "%2525"_s);
    testUserPassword("%FX"_s, "%FX"_s);
    testUserPassword("%00"_s, String::fromUTF8("\0"_s, 1), "%00"_s);
    testUserPassword("%F%25"_s, "%F%"_s, "%F%25"_s);
    testUserPassword("%X%25"_s, "%X%"_s, "%X%25"_s);
    testUserPassword("%%25"_s, "%%"_s, "%%25"_s);
    testUserPassword(StringView::fromLatin1("💩"), "%C3%B0%C2%9F%C2%92%C2%A9"_s);
    testUserPassword(StringView::fromLatin1("%💩"), "%%C3%B0%C2%9F%C2%92%C2%A9"_s);
    testUserPassword(validSurrogate, "%F0%90%85%95"_s);
    testUserPassword(replacementA, "%EF%BF%BDA"_s);
    testUserPassword(invalidSurrogate, replacementA, "%EF%BF%BDA"_s);
}

TEST_F(WTF_URLParser, ParseRelative)
{
    checkRelativeURL("/index.html"_s, "http://webkit.org/path1/path2/"_s, { "http"_s, ""_s, ""_s, "webkit.org"_s, 0, "/index.html"_s, ""_s, ""_s, "http://webkit.org/index.html"_s });
    checkRelativeURL("http://whatwg.org/index.html"_s, "http://webkit.org/path1/path2/"_s, { "http"_s, ""_s, ""_s, "whatwg.org"_s, 0, "/index.html"_s, ""_s, ""_s, "http://whatwg.org/index.html"_s });
    checkRelativeURL("index.html"_s, "http://webkit.org/path1/path2/page.html?query#fragment"_s, { "http"_s, ""_s, ""_s, "webkit.org"_s, 0, "/path1/path2/index.html"_s, ""_s, ""_s, "http://webkit.org/path1/path2/index.html"_s });
    checkRelativeURL("//whatwg.org/index.html"_s, "https://www.webkit.org/path"_s, { "https"_s, ""_s, ""_s, "whatwg.org"_s, 0, "/index.html"_s, ""_s, ""_s, "https://whatwg.org/index.html"_s });
    checkRelativeURL("http://example\t.\norg"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/"_s, ""_s, ""_s, "http://example.org/"_s });
    checkRelativeURL("test"_s, "file:///path1/path2"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path1/test"_s, ""_s, ""_s, "file:///path1/test"_s });
    checkRelativeURL(utf16String(u"http://www.foo。bar.com"), "http://other.com/"_s, { "http"_s, ""_s, ""_s, "www.foo.bar.com"_s, 0, "/"_s, ""_s, ""_s, "http://www.foo.bar.com/"_s });
    checkRelativeURLDifferences(utf16String(u"sc://ñ.test/"), "about:blank"_s,
        { "sc"_s, ""_s, ""_s, "%C3%B1.test"_s, 0, "/"_s, ""_s, ""_s, "sc://%C3%B1.test/"_s },
        { "sc"_s, ""_s, ""_s, "xn--ida.test"_s, 0, "/"_s, ""_s, ""_s, "sc://xn--ida.test/"_s });
    checkRelativeURL("#fragment"_s, "http://host/path"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/path"_s, ""_s, "fragment"_s, "http://host/path#fragment"_s });
    checkRelativeURL("#fragment"_s, "file:///path"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, "fragment"_s, "file:///path#fragment"_s });
    checkRelativeURL("#fragment"_s, "file:///path#old"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, "fragment"_s, "file:///path#fragment"_s });
    checkRelativeURL("#"_s, "file:///path#old"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path#"_s });
    checkRelativeURL("  "_s, "file:///path#old"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path"_s });
    checkRelativeURL("#"_s, "file:///path"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path#"_s });
    checkRelativeURL("#"_s, "file:///path?query"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, "query"_s, ""_s, "file:///path?query#"_s });
    checkRelativeURL("#"_s, "file:///path?query#old"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, "query"_s, ""_s, "file:///path?query#"_s });
    checkRelativeURL("?query"_s, "http://host/path"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/path"_s, "query"_s, ""_s, "http://host/path?query"_s });
    checkRelativeURL("?query#fragment"_s, "http://host/path"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/path"_s, "query"_s, "fragment"_s, "http://host/path?query#fragment"_s });
    checkRelativeURL("?new"_s, "file:///path?old#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, "new"_s, ""_s, "file:///path?new"_s });
    checkRelativeURL("?"_s, "file:///path?old#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path?"_s });
    checkRelativeURL("?"_s, "file:///path"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path?"_s });
    checkRelativeURL("?query"_s, "file:///path"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, "query"_s, ""_s, "file:///path?query"_s });
    checkRelativeURL(utf16String(u"?β"), "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, "%CE%B2"_s, ""_s, "http://example.org/foo/bar?%CE%B2"_s });
    checkRelativeURL("?"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, ""_s, "http://example.org/foo/bar?"_s });
    checkRelativeURL("#"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, ""_s, "http://example.org/foo/bar#"_s });
    checkRelativeURL("?#"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, ""_s, "http://example.org/foo/bar?#"_s });
    checkRelativeURL("#?"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, "?"_s, "http://example.org/foo/bar#?"_s });
    checkRelativeURL("/"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/"_s, ""_s, ""_s, "http://example.org/"_s });
    checkRelativeURL("http://@host"_s, "about:blank"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkRelativeURL("http://:@host"_s, "about:blank"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkRelativeURL("http://foo.com/\\@"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "foo.com"_s, 0, "//@"_s, ""_s, ""_s, "http://foo.com//@"_s });
    checkRelativeURL("\\@"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/@"_s, ""_s, ""_s, "http://example.org/@"_s });
    checkRelativeURL("/path3"_s, "http://user@example.org/path1/path2"_s, { "http"_s, "user"_s, ""_s, "example.org"_s, 0, "/path3"_s, ""_s, ""_s, "http://user@example.org/path3"_s });
    checkRelativeURL(""_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, ""_s, "http://example.org/foo/bar"_s });
    checkRelativeURL("\t"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, ""_s, "http://example.org/foo/bar"_s });
    checkRelativeURL(" "_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, ""_s, "http://example.org/foo/bar"_s });
    checkRelativeURL("  \a  \t\n"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, ""_s, "http://example.org/foo/bar"_s });
    checkRelativeURL(":foo.com\\"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/:foo.com/"_s, ""_s, ""_s, "http://example.org/foo/:foo.com/"_s });
    checkRelativeURL("http:/example.com/"_s, "about:blank"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkRelativeURL("http:example.com/"_s, "about:blank"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkRelativeURL("http:\\\\foo.com\\"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "foo.com"_s, 0, "/"_s, ""_s, ""_s, "http://foo.com/"_s });
    checkRelativeURL("http:\\\\foo.com/"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "foo.com"_s, 0, "/"_s, ""_s, ""_s, "http://foo.com/"_s });
    checkRelativeURL("http:\\\\foo.com"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "foo.com"_s, 0, "/"_s, ""_s, ""_s, "http://foo.com/"_s });
    checkRelativeURL("http://ExAmPlE.CoM"_s, "http://other.com"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
    checkRelativeURL("http:"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, ""_s, "http://example.org/foo/bar"_s });
    checkRelativeURL("#x"_s, "data:,"_s, { "data"_s, ""_s, ""_s, ""_s, 0, ","_s, ""_s, "x"_s, "data:,#x"_s });
    checkRelativeURL("#x"_s, "about:blank"_s, { "about"_s, ""_s, ""_s, ""_s, 0, "blank"_s, ""_s, "x"_s, "about:blank#x"_s });
    checkRelativeURL("  foo.com  "_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/foo.com"_s, ""_s, ""_s, "http://example.org/foo/foo.com"_s });
    checkRelativeURL(" \a baz"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/baz"_s, ""_s, ""_s, "http://example.org/foo/baz"_s });
    checkRelativeURL("~"_s, "http://example.org"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/~"_s, ""_s, ""_s, "http://example.org/~"_s });
    checkRelativeURL("notspecial:"_s, "about:blank"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "notspecial:"_s });
    checkRelativeURL("notspecial:"_s, "http://host"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "notspecial:"_s });
    checkRelativeURL("http:"_s, "http://host"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkRelativeURL("i"_s, "sc:/pa/po"_s, { "sc"_s, ""_s, ""_s, ""_s, 0, "/pa/i"_s, ""_s, ""_s, "sc:/pa/i"_s });
    checkRelativeURL("i    "_s, "sc:/pa/po"_s, { "sc"_s, ""_s, ""_s, ""_s, 0, "/pa/i"_s, ""_s, ""_s, "sc:/pa/i"_s });
    checkRelativeURL("i\t\n  "_s, "sc:/pa/po"_s, { "sc"_s, ""_s, ""_s, ""_s, 0, "/pa/i"_s, ""_s, ""_s, "sc:/pa/i"_s });
    checkRelativeURL("i"_s, "sc://ho/pa"_s, { "sc"_s, ""_s, ""_s, "ho"_s, 0, "/i"_s, ""_s, ""_s, "sc://ho/i"_s });
    checkRelativeURL("!"_s, "sc://ho/pa"_s, { "sc"_s, ""_s, ""_s, "ho"_s, 0, "/!"_s, ""_s, ""_s, "sc://ho/!"_s });
    checkRelativeURL("!"_s, "sc:/ho/pa"_s, { "sc"_s, ""_s, ""_s, ""_s, 0, "/ho/!"_s, ""_s, ""_s, "sc:/ho/!"_s });
    checkRelativeURL("notspecial:/"_s, "about:blank"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "notspecial:/"_s });
    checkRelativeURL("notspecial:/"_s, "http://host"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "notspecial:/"_s });
    checkRelativeURL("foo:/"_s, "http://example.org/foo/bar"_s, { "foo"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "foo:/"_s });
    checkRelativeURL("://:0/"_s, "http://webkit.org/"_s, { "http"_s, ""_s, ""_s, "webkit.org"_s, 0, "/://:0/"_s, ""_s, ""_s, "http://webkit.org/://:0/"_s });
    checkRelativeURL(String(), "http://webkit.org/"_s, { "http"_s, ""_s, ""_s, "webkit.org"_s, 0, "/"_s, ""_s, ""_s, "http://webkit.org/"_s });
    checkRelativeURL("https://@test@test@example:800\\path@end"_s, "http://doesnotmatter/"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "https://@test@test@example:800\\path@end"_s });
    checkRelativeURL("http://f:0/c"_s, "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "f"_s, 0, "/c"_s, ""_s, ""_s, "http://f:0/c"_s });
    checkRelativeURL(String(), "http://host/#fragment"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkRelativeURL(""_s, "http://host/#fragment"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkRelativeURL("  "_s, "http://host/#fragment"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkRelativeURL("  "_s, "http://host/path?query#fra#gment"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/path"_s, "query"_s, ""_s, "http://host/path?query"_s });
    checkRelativeURL(" \a "_s, "http://host/#fragment"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkRelativeURLDifferences("foo://"_s, "http://example.org/foo/bar"_s,
        { "foo"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "foo://"_s },
        { "foo"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "foo://"_s });
    checkRelativeURL(utf16String(u"#β"), "http://example.org/foo/bar"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, ""_s, "%CE%B2"_s, "http://example.org/foo/bar#%CE%B2"_s });
    checkRelativeURL("index.html"_s, "applewebdata://Host/"_s, { "applewebdata"_s, ""_s, ""_s, "Host"_s, 0, "/index.html"_s, ""_s, ""_s, "applewebdata://Host/index.html"_s });
    checkRelativeURL("index.html"_s, "applewebdata://Host"_s, { "applewebdata"_s, ""_s, ""_s, "Host"_s, 0, "/index.html"_s, ""_s, ""_s, "applewebdata://Host/index.html"_s });
    checkRelativeURL(""_s, "applewebdata://Host"_s, { "applewebdata"_s, ""_s, ""_s, "Host"_s, 0, ""_s, ""_s, ""_s, "applewebdata://Host"_s });
    checkRelativeURL("?query"_s, "applewebdata://Host"_s, { "applewebdata"_s, ""_s, ""_s, "Host"_s, 0, ""_s, "query"_s, ""_s, "applewebdata://Host?query"_s });
    checkRelativeURL("#fragment"_s, "applewebdata://Host"_s, { "applewebdata"_s, ""_s, ""_s, "Host"_s, 0, ""_s, ""_s, "fragment"_s, "applewebdata://Host#fragment"_s });
    checkRelativeURL("notspecial://something?"_s, "file:////var//containers//stuff/"_s, { "notspecial"_s, ""_s, ""_s, "something"_s, 0, ""_s, ""_s, ""_s, "notspecial://something?"_s }, TestTabs::No);
    checkRelativeURL("notspecial://something#"_s, "file:////var//containers//stuff/"_s, { "notspecial"_s, ""_s, ""_s, "something"_s, 0, ""_s, ""_s, ""_s, "notspecial://something#"_s }, TestTabs::No);
    checkRelativeURL("http://something?"_s, "file:////var//containers//stuff/"_s, { "http"_s, ""_s, ""_s, "something"_s, 0, "/"_s, ""_s, ""_s, "http://something/?"_s }, TestTabs::No);
    checkRelativeURL("http://something#"_s, "file:////var//containers//stuff/"_s, { "http"_s, ""_s, ""_s, "something"_s, 0, "/"_s, ""_s, ""_s, "http://something/#"_s }, TestTabs::No);
    checkRelativeURL("file:"_s, "file:///path?query#fragment"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, "query"_s, ""_s, "file:///path?query"_s });
    checkRelativeURL("/"_s, "file:///C:/a/b"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/"_s, ""_s, ""_s, "file:///C:/"_s });
    checkRelativeURL("/abc"_s, "file:///C:/a/b"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/abc"_s, ""_s, ""_s, "file:///C:/abc"_s });
    checkRelativeURL("/abc"_s, "file:///C:"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/abc"_s, ""_s, ""_s, "file:///C:/abc"_s });
    checkRelativeURL("/abc"_s, "file:///"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/abc"_s, ""_s, ""_s, "file:///abc"_s });
    checkRelativeURL("//d:"_s, "file:///C:/a/b"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/d:"_s, ""_s, ""_s, "file:///d:"_s }, TestTabs::No);
    checkRelativeURL("//d|"_s, "file:///C:/a/b"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/d:"_s, ""_s, ""_s, "file:///d:"_s }, TestTabs::No);
    checkRelativeURL("//A|"_s, "file:///C:/a/b"_s, { "file"_s, ""_s, ""_s, ""_s, 0, "/A:"_s, ""_s, ""_s, "file:///A:"_s }, TestTabs::No);

    // The checking of slashes in SpecialAuthoritySlashes needed to get this to pass contradicts what is in the spec,
    // but it is included in the web platform tests.
    checkRelativeURL("http:\\\\host\\foo"_s, "about:blank"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/foo"_s, ""_s, ""_s, "http://host/foo"_s });
}

// These are differences between the new URLParser and the old URL::parse which make URLParser more standards compliant.
TEST_F(WTF_URLParser, ParserDifferences)
{
    checkURLDifferences("http://127.0.1"_s,
        { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s },
        { "http"_s, ""_s, ""_s, "127.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.1/"_s });
    checkURLDifferences("http://011.11.0X11.0x011"_s,
        { "http"_s, ""_s, ""_s, "9.11.17.17"_s, 0, "/"_s, ""_s, ""_s, "http://9.11.17.17/"_s },
        { "http"_s, ""_s, ""_s, "011.11.0x11.0x011"_s, 0, "/"_s, ""_s, ""_s, "http://011.11.0x11.0x011/"_s });
    checkURLDifferences("http://[1234:0078:90AB:CdEf:0123:0007:89AB:0000]"_s,
        { "http"_s, ""_s, ""_s, "[1234:78:90ab:cdef:123:7:89ab:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[1234:78:90ab:cdef:123:7:89ab:0]/"_s },
        { "http"_s, ""_s, ""_s, "[1234:0078:90ab:cdef:0123:0007:89ab:0000]"_s, 0, "/"_s, ""_s, ""_s, "http://[1234:0078:90ab:cdef:0123:0007:89ab:0000]/"_s });
    checkURLDifferences("http://[0:f:0:0:f:f:0:0]"_s,
        { "http"_s, ""_s, ""_s, "[0:f::f:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f::f:f:0:0]/"_s },
        { "http"_s, ""_s, ""_s, "[0:f:0:0:f:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f:f:0:0]/"_s });
    checkURLDifferences("http://[0:f:0:0:f:0:0:0]"_s,
        { "http"_s, ""_s, ""_s, "[0:f:0:0:f::]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f::]/"_s },
        { "http"_s, ""_s, ""_s, "[0:f:0:0:f:0:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f:0:0:f:0:0:0]/"_s });
    checkURLDifferences("http://[0:0:f:0:0:f:0:0]"_s,
        { "http"_s, ""_s, ""_s, "[::f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::f:0:0:f:0:0]/"_s },
        { "http"_s, ""_s, ""_s, "[0:0:f:0:0:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:0:f:0:0:f:0:0]/"_s });
    checkURLDifferences("http://[a:0:0:0:b:c::d]"_s,
        { "http"_s, ""_s, ""_s, "[a::b:c:0:d]"_s, 0, "/"_s, ""_s, ""_s, "http://[a::b:c:0:d]/"_s },
        { "http"_s, ""_s, ""_s, "[a:0:0:0:b:c::d]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:0:0:0:b:c::d]/"_s });
    checkURLDifferences("http://[::7f00:0001]/"_s,
        { "http"_s, ""_s, ""_s, "[::7f00:1]"_s, 0, "/"_s, ""_s, ""_s, "http://[::7f00:1]/"_s },
        { "http"_s, ""_s, ""_s, "[::7f00:0001]"_s, 0, "/"_s, ""_s, ""_s, "http://[::7f00:0001]/"_s });
    checkURLDifferences("http://[::7f00:00]/"_s,
        { "http"_s, ""_s, ""_s, "[::7f00:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[::7f00:0]/"_s },
        { "http"_s, ""_s, ""_s, "[::7f00:00]"_s, 0, "/"_s, ""_s, ""_s, "http://[::7f00:00]/"_s });
    checkURLDifferences("http://[::0:7f00:0001]/"_s,
        { "http"_s, ""_s, ""_s, "[::7f00:1]"_s, 0, "/"_s, ""_s, ""_s, "http://[::7f00:1]/"_s },
        { "http"_s, ""_s, ""_s, "[::0:7f00:0001]"_s, 0, "/"_s, ""_s, ""_s, "http://[::0:7f00:0001]/"_s });
    checkURLDifferences("http://127.00.0.1/"_s,
        { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s },
        { "http"_s, ""_s, ""_s, "127.00.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.00.0.1/"_s });
    checkURLDifferences("http://127.0.0.01/"_s,
        { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s },
        { "http"_s, ""_s, ""_s, "127.0.0.01"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.01/"_s });
    checkURLDifferences("http://example.com/path1/.%2e"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/.%2e"_s, ""_s, ""_s, "http://example.com/path1/.%2e"_s });
    checkURLDifferences("http://example.com/path1/.%2E"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/.%2E"_s, ""_s, ""_s, "http://example.com/path1/.%2E"_s });
    checkURLDifferences("http://example.com/path1/.%2E/"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/.%2E/"_s, ""_s, ""_s, "http://example.com/path1/.%2E/"_s });
    checkURLDifferences("http://example.com/path1/%2e."_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/%2e."_s, ""_s, ""_s, "http://example.com/path1/%2e."_s });
    checkURLDifferences("http://example.com/path1/%2E%2e"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/%2E%2e"_s, ""_s, ""_s, "http://example.com/path1/%2E%2e"_s });
    checkURLDifferences("http://example.com/path1/%2e"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/"_s, ""_s, ""_s, "http://example.com/path1/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/%2e"_s, ""_s, ""_s, "http://example.com/path1/%2e"_s });
    checkURLDifferences("http://example.com/path1/%2E"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/"_s, ""_s, ""_s, "http://example.com/path1/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/%2E"_s, ""_s, ""_s, "http://example.com/path1/%2E"_s });
    checkURLDifferences("http://example.com/path1/%2E/"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/"_s, ""_s, ""_s, "http://example.com/path1/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/%2E/"_s, ""_s, ""_s, "http://example.com/path1/%2E/"_s });
    checkURLDifferences("http://example.com/path1/path2/%2e?query"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/"_s, "query"_s, ""_s, "http://example.com/path1/path2/?query"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/%2e"_s, "query"_s, ""_s, "http://example.com/path1/path2/%2e?query"_s });
    checkURLDifferences("http://example.com/path1/path2/%2e%2e?query"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/"_s, "query"_s, ""_s, "http://example.com/path1/?query"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/%2e%2e"_s, "query"_s, ""_s, "http://example.com/path1/path2/%2e%2e?query"_s });
    checkURLDifferences("http://example.com/path1/path2/%2e#fragment"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/"_s, ""_s, "fragment"_s, "http://example.com/path1/path2/#fragment"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/%2e"_s, ""_s, "fragment"_s, "http://example.com/path1/path2/%2e#fragment"_s });
    checkURLDifferences("http://example.com/path1/path2/%2e%2e#fragment"_s,
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/"_s, ""_s, "fragment"_s, "http://example.com/path1/#fragment"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/%2e%2e"_s, ""_s, "fragment"_s, "http://example.com/path1/path2/%2e%2e#fragment"_s });
    checkURL("http://example.com/path1/path2/A%2e%2e#fragment"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/path1/path2/A%2e%2e"_s, ""_s, "fragment"_s, "http://example.com/path1/path2/A%2e%2e#fragment"_s });
    checkURLDifferences("file://[0:a:0:0:b:c:0:0]/path"_s,
        { "file"_s, ""_s, ""_s, "[0:a::b:c:0:0]"_s, 0, "/path"_s, ""_s, ""_s, "file://[0:a::b:c:0:0]/path"_s },
        { "file"_s, ""_s, ""_s, "[0:a:0:0:b:c:0:0]"_s, 0, "/path"_s, ""_s, ""_s, "file://[0:a:0:0:b:c:0:0]/path"_s });
    checkURLDifferences("http://"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "http:/"_s });
    checkRelativeURLDifferences("//"_s, "https://www.webkit.org/path"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "//"_s },
        { "https"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "https:/"_s });
    checkURLDifferences("http://127.0.0.1:65536/path"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://127.0.0.1:65536/path"_s },
        { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/path"_s, ""_s, ""_s, "http://127.0.0.1:65536/path"_s });
    checkURLDifferences("http://host:65536"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://host:65536"_s },
        { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host:65536/"_s });
    checkURLDifferences("http://127.0.0.1:65536"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://127.0.0.1:65536"_s },
        { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1:65536/"_s });
    checkURLDifferences("http://[0:f::f:f:0:0]:65536"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[0:f::f:f:0:0]:65536"_s },
        { "http"_s, ""_s, ""_s, "[0:f::f:f:0:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f::f:f:0:0]:65536/"_s });
    checkRelativeURLDifferences(":foo.com\\"_s, "notspecial://example.org/foo/bar"_s,
        { "notspecial"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/:foo.com\\"_s, ""_s, ""_s, "notspecial://example.org/foo/:foo.com\\"_s },
        { "notspecial"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/:foo.com/"_s, ""_s, ""_s, "notspecial://example.org/foo/:foo.com/"_s });
    checkURL("sc://pa"_s, { "sc"_s, ""_s, ""_s, "pa"_s, 0, ""_s, ""_s, ""_s, "sc://pa"_s });
    checkRelativeURLDifferences("notspecial:\\\\foo.com\\"_s, "http://example.org/foo/bar"_s,
        { "notspecial"_s, ""_s, ""_s, ""_s, 0, "\\\\foo.com\\"_s, ""_s, ""_s, "notspecial:\\\\foo.com\\"_s },
        { "notspecial"_s, ""_s, ""_s, "foo.com"_s, 0, "/"_s, ""_s, ""_s, "notspecial://foo.com/"_s });
    checkRelativeURLDifferences("notspecial:\\\\foo.com/"_s, "http://example.org/foo/bar"_s,
        { "notspecial"_s, ""_s, ""_s, ""_s, 0, "\\\\foo.com/"_s, ""_s, ""_s, "notspecial:\\\\foo.com/"_s },
        { "notspecial"_s, ""_s, ""_s, "foo.com"_s, 0, "/"_s, ""_s, ""_s, "notspecial://foo.com/"_s });
    checkRelativeURLDifferences("notspecial:\\\\foo.com"_s, "http://example.org/foo/bar"_s,
        { "notspecial"_s, ""_s, ""_s, ""_s, 0, "\\\\foo.com"_s, ""_s, ""_s, "notspecial:\\\\foo.com"_s },
        { "notspecial"_s, ""_s, ""_s, "foo.com"_s, 0, ""_s, ""_s, ""_s, "notspecial://foo.com"_s });
    checkURLDifferences("file://notuser:notpassword@test"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "file://notuser:notpassword@test"_s },
        { "file"_s, "notuser"_s, "notpassword"_s, "test"_s, 0, "/"_s, ""_s, ""_s, "file://notuser:notpassword@test/"_s });
    checkURLDifferences("file://notuser:notpassword@test/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "file://notuser:notpassword@test/"_s },
        { "file"_s, "notuser"_s, "notpassword"_s, "test"_s, 0, "/"_s, ""_s, ""_s, "file://notuser:notpassword@test/"_s });
    checkRelativeURLDifferences("http:/"_s, "about:blank"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http:/"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "http:/"_s });
    checkRelativeURL("http:"_s, "about:blank"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http:"_s });
    checkRelativeURLDifferences("http:/"_s, "http://host"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http:/"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "http:/"_s });
    checkURLDifferences("http:/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http:/"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "http:/"_s });
    checkURL("http:"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http:"_s });
    checkRelativeURLDifferences("http:/example.com/"_s, "http://example.org/foo/bar"_s,
        { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/example.com/"_s, ""_s, ""_s, "http://example.org/example.com/"_s },
        { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });

    // This behavior matches Chrome and Firefox, but not WebKit using URL::parse.
    // The behavior of URL::parse is clearly wrong because reparsing file://path would make path the host.
    // The spec is unclear.
    checkURLDifferences("file:path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/path"_s, ""_s, ""_s, "file:///path"_s },
        { "file"_s, ""_s, ""_s, ""_s, 0, "path"_s, ""_s, ""_s, "file://path"_s });
    checkURLDifferences("file:pAtH"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/pAtH"_s, ""_s, ""_s, "file:///pAtH"_s },
        { "file"_s, ""_s, ""_s, ""_s, 0, "pAtH"_s, ""_s, ""_s, "file://pAtH"_s });
    checkURLDifferences("file:pAtH/"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/pAtH/"_s, ""_s, ""_s, "file:///pAtH/"_s },
        { "file"_s, ""_s, ""_s, ""_s, 0, "pAtH/"_s, ""_s, ""_s, "file://pAtH/"_s });
    checkURLDifferences("http://example.com%A0"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://example.com%A0"_s },
        { "http"_s, ""_s, ""_s, "example.com%a0"_s, 0, "/"_s, ""_s, ""_s, "http://example.com%a0/"_s });
    checkURLDifferences("http://%E2%98%83"_s,
        { "http"_s, ""_s, ""_s, "xn--n3h"_s, 0, "/"_s, ""_s, ""_s, "http://xn--n3h/"_s },
        { "http"_s, ""_s, ""_s, "%e2%98%83"_s, 0, "/"_s, ""_s, ""_s, "http://%e2%98%83/"_s });
    checkURLDifferences("http://host%73"_s,
        { "http"_s, ""_s, ""_s, "hosts"_s, 0, "/"_s, ""_s, ""_s, "http://hosts/"_s },
        { "http"_s, ""_s, ""_s, "host%73"_s, 0, "/"_s, ""_s, ""_s, "http://host%73/"_s });
    checkURLDifferences("http://host%53"_s,
        { "http"_s, ""_s, ""_s, "hosts"_s, 0, "/"_s, ""_s, ""_s, "http://hosts/"_s },
        { "http"_s, ""_s, ""_s, "host%53"_s, 0, "/"_s, ""_s, ""_s, "http://host%53/"_s });
    checkURLDifferences("http://%"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://%"_s },
        { "http"_s, ""_s, ""_s, "%"_s, 0, "/"_s, ""_s, ""_s, "http://%/"_s });
    checkURLDifferences("http://%7"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://%7"_s },
        { "http"_s, ""_s, ""_s, "%7"_s, 0, "/"_s, ""_s, ""_s, "http://%7/"_s });
    checkURLDifferences("http://%7s"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://%7s"_s },
        { "http"_s, ""_s, ""_s, "%7s"_s, 0, "/"_s, ""_s, ""_s, "http://%7s/"_s });
    checkURLDifferences("http://%73"_s,
        { "http"_s, ""_s, ""_s, "s"_s, 0, "/"_s, ""_s, ""_s, "http://s/"_s },
        { "http"_s, ""_s, ""_s, "%73"_s, 0, "/"_s, ""_s, ""_s, "http://%73/"_s });
    checkURLDifferences("http://abcdefg%"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://abcdefg%"_s },
        { "http"_s, ""_s, ""_s, "abcdefg%"_s, 0, "/"_s, ""_s, ""_s, "http://abcdefg%/"_s });
    checkURLDifferences("http://abcd%7Xefg"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://abcd%7Xefg"_s },
        { "http"_s, ""_s, ""_s, "abcd%7xefg"_s, 0, "/"_s, ""_s, ""_s, "http://abcd%7xefg/"_s });
    checkURL(StringView::fromLatin1("ws://äAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), { "ws"_s, ""_s, ""_s, "xn--aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-rsb254a"_s, 0, "/"_s, ""_s, ""_s, "ws://xn--aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-rsb254a/"_s }, TestTabs::No);
    checkURL(StringView::fromLatin1("ws://äAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), { "ws"_s, ""_s, ""_s, "xn--aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-stb515a"_s, 0, "/"_s, ""_s, ""_s, "ws://xn--aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-stb515a/"_s }, TestTabs::No);
    checkURL(StringView::fromLatin1("ws://&äAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), { "ws"_s, ""_s, ""_s, "xn--&aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-ssb254a"_s, 0, "/"_s, ""_s, ""_s, "ws://xn--&aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-ssb254a/"_s }, TestTabs::No);
    checkURL(StringView::fromLatin1("ws://&äAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), { "ws"_s, ""_s, ""_s, "xn--&aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-ttb515a"_s, 0, "/"_s, ""_s, ""_s, "ws://xn--&aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-ttb515a/"_s }, TestTabs::No);

    // URLParser matches Chrome and the spec, but not URL::parse or Firefox.
    checkURLDifferences(utf16String(u"http://０Ｘｃ０．０２５０．０１"),
        { "http"_s, ""_s, ""_s, "192.168.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://192.168.0.1/"_s },
        { "http"_s, ""_s, ""_s, "0xc0.0250.01"_s, 0, "/"_s, ""_s, ""_s, "http://0xc0.0250.01/"_s });

    checkURL("http://host/path%2e.%2E"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/path%2e.%2E"_s, ""_s, ""_s, "http://host/path%2e.%2E"_s });

    checkRelativeURLDifferences(utf16String(u"http://foo:💩@example.com/bar"), "http://other.com/"_s,
        { "http"_s, "foo"_s, utf16String(u"💩"), "example.com"_s, 0, "/bar"_s, ""_s, ""_s, "http://foo:%F0%9F%92%A9@example.com/bar"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, utf16String(u"http://foo:💩@example.com/bar") }, testTabsValueForSurrogatePairs);
    checkRelativeURLDifferences("http://&a:foo(b]c@d:2/"_s, "http://example.org/foo/bar"_s,
        { "http"_s, "&a"_s, "foo(b]c"_s, "d"_s, 2, "/"_s, ""_s, ""_s, "http://&a:foo(b%5Dc@d:2/"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://&a:foo(b]c@d:2/"_s });
    checkRelativeURLDifferences("http://`{}:`{}@h/`{}?`{}"_s, "http://doesnotmatter/"_s,
        { "http"_s, "`{}"_s, "`{}"_s, "h"_s, 0, "/%60%7B%7D"_s, "`{}"_s, ""_s, "http://%60%7B%7D:%60%7B%7D@h/%60%7B%7D?`{}"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://`{}:`{}@h/`{}?`{}"_s });
    checkURLDifferences("http://[0:f::f::f]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[0:f::f::f]"_s },
        { "http"_s, ""_s, ""_s, "[0:f::f::f]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:f::f::f]/"_s });
    checkURLDifferences("http://123"_s,
        { "http"_s, ""_s, ""_s, "0.0.0.123"_s, 0, "/"_s, ""_s, ""_s, "http://0.0.0.123/"_s },
        { "http"_s, ""_s, ""_s, "123"_s, 0, "/"_s, ""_s, ""_s, "http://123/"_s });
    checkURLDifferences("http://123.234/"_s,
        { "http"_s, ""_s, ""_s, "123.0.0.234"_s, 0, "/"_s, ""_s, ""_s, "http://123.0.0.234/"_s },
        { "http"_s, ""_s, ""_s, "123.234"_s, 0, "/"_s, ""_s, ""_s, "http://123.234/"_s });
    checkURLDifferences("http://123.234.012"_s,
        { "http"_s, ""_s, ""_s, "123.234.0.10"_s, 0, "/"_s, ""_s, ""_s, "http://123.234.0.10/"_s },
        { "http"_s, ""_s, ""_s, "123.234.012"_s, 0, "/"_s, ""_s, ""_s, "http://123.234.012/"_s });
    checkURLDifferences("http://123.234.12"_s,
        { "http"_s, ""_s, ""_s, "123.234.0.12"_s, 0, "/"_s, ""_s, ""_s, "http://123.234.0.12/"_s },
        { "http"_s, ""_s, ""_s, "123.234.12"_s, 0, "/"_s, ""_s, ""_s, "http://123.234.12/"_s });
    checkRelativeURLDifferences("file:c:\\foo\\bar.html"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/c:/foo/bar.html"_s, ""_s, ""_s, "file:///c:/foo/bar.html"_s },
        { "file"_s, ""_s, ""_s, ""_s, 0, "/tmp/mock/c:/foo/bar.html"_s, ""_s, ""_s, "file:///tmp/mock/c:/foo/bar.html"_s });
    checkRelativeURLDifferences("  File:c|////foo\\bar.html"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/c:////foo/bar.html"_s, ""_s, ""_s, "file:///c:////foo/bar.html"_s },
        { "file"_s, ""_s, ""_s, ""_s, 0, "/tmp/mock/c|////foo/bar.html"_s, ""_s, ""_s, "file:///tmp/mock/c|////foo/bar.html"_s });
    checkRelativeURLDifferences("  Fil\t\n\te\n\t\n:\t\n\tc\t\n\t|\n\t\n/\t\n\t/\n\t\n//foo\\bar.html"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/c:////foo/bar.html"_s, ""_s, ""_s, "file:///c:////foo/bar.html"_s },
        { "file"_s, ""_s, ""_s, ""_s, 0, "/tmp/mock/c|////foo/bar.html"_s, ""_s, ""_s, "file:///tmp/mock/c|////foo/bar.html"_s });
    checkRelativeURLDifferences("C|/foo/bar"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/foo/bar"_s, ""_s, ""_s, "file:///C:/foo/bar"_s },
        { "file"_s, ""_s, ""_s, ""_s, 0, "/tmp/mock/C|/foo/bar"_s, ""_s, ""_s, "file:///tmp/mock/C|/foo/bar"_s });
    checkRelativeURLDifferences("/C|/foo/bar"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/foo/bar"_s, ""_s, ""_s, "file:///C:/foo/bar"_s },
        { "file"_s, ""_s, ""_s, ""_s, 0, "/C|/foo/bar"_s, ""_s, ""_s, "file:///C|/foo/bar"_s });
    checkRelativeURLDifferences("https://@test@test@example:800/"_s, "http://doesnotmatter/"_s,
        { "https"_s, "@test@test"_s, ""_s, "example"_s, 800, "/"_s, ""_s, ""_s, "https://%40test%40test@example:800/"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "https://@test@test@example:800/"_s });
    checkRelativeURLDifferences("https://@test@test@example:800/path@end"_s, "http://doesnotmatter/"_s,
        { "https"_s, "@test@test"_s, ""_s, "example"_s, 800, "/path@end"_s, ""_s, ""_s, "https://%40test%40test@example:800/path@end"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "https://@test@test@example:800/path@end"_s });
    checkURLDifferences("notspecial://@test@test@example:800/path@end"_s,
        { "notspecial"_s, "@test@test"_s, ""_s, "example"_s, 800, "/path@end"_s, ""_s, ""_s, "notspecial://%40test%40test@example:800/path@end"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "notspecial://@test@test@example:800/path@end"_s });
    checkURLDifferences("notspecial://@test@test@example:800\\path@end"_s,
        { "notspecial"_s, "@test@test@example"_s, "800\\path"_s, "end"_s, 0, ""_s, ""_s, ""_s, "notspecial://%40test%40test%40example:800%5Cpath@end"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "notspecial://@test@test@example:800\\path@end"_s });
    checkURLDifferences("http://%48OsT"_s,
        { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s },
        { "http"_s, ""_s, ""_s, "%48ost"_s, 0, "/"_s, ""_s, ""_s, "http://%48ost/"_s });
    checkURLDifferences("http://h%4FsT"_s,
        { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s },
        { "http"_s, ""_s, ""_s, "h%4fst"_s, 0, "/"_s, ""_s, ""_s, "http://h%4fst/"_s });
    checkURLDifferences("http://h%4fsT"_s,
        { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s },
        { "http"_s, ""_s, ""_s, "h%4fst"_s, 0, "/"_s, ""_s, ""_s, "http://h%4fst/"_s });
    checkURLDifferences("http://h%6fsT"_s,
        { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s },
        { "http"_s, ""_s, ""_s, "h%6fst"_s, 0, "/"_s, ""_s, ""_s, "http://h%6fst/"_s });
    checkURLDifferences("http://host/`"_s,
        { "http"_s, ""_s, ""_s, "host"_s, 0, "/%60"_s, ""_s, ""_s, "http://host/%60"_s },
        { "http"_s, ""_s, ""_s, "host"_s, 0, "/`"_s, ""_s, ""_s, "http://host/`"_s });
    checkURLDifferences("http://://"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://://"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "//"_s, ""_s, ""_s, "http://://"_s });
    checkURLDifferences("http://:123?"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://:123?"_s },
        { "http"_s, ""_s, ""_s, ""_s, 123, "/"_s, ""_s, ""_s, "http://:123/?"_s });
    checkURLDifferences("http:/:"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http:/:"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "http://:/"_s });
    checkURLDifferences("asdf://:"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "asdf://:"_s },
        { "asdf"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "asdf://:"_s });
    checkURLDifferences("http://:"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://:"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, ""_s, "http://:/"_s });
    checkURLDifferences("http:##foo"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http:##foo"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "/"_s, ""_s, "#foo"_s, "http:/##foo"_s });
    checkURLDifferences("http:??bar"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http:??bar"_s },
        { "http"_s, ""_s, ""_s, ""_s, 0, "/"_s, "?bar"_s, ""_s, "http:/??bar"_s });
    checkURL("asdf:##foo"_s, { "asdf"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, "#foo"_s, "asdf:##foo"_s });
    checkURL("asdf:??bar"_s, { "asdf"_s, ""_s, ""_s, ""_s, 0, ""_s, "?bar"_s, ""_s, "asdf:??bar"_s });
    checkRelativeURLDifferences("//C|/foo/bar"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/foo/bar"_s, ""_s, ""_s, "file:///C:/foo/bar"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "//C|/foo/bar"_s });
    checkRelativeURLDifferences("//C:/foo/bar"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/foo/bar"_s, ""_s, ""_s, "file:///C:/foo/bar"_s },
        { "file"_s, ""_s, ""_s, "c"_s, 0, "/foo/bar"_s, ""_s, ""_s, "file://c/foo/bar"_s });
    checkRelativeURLDifferences("//C|?foo/bar"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/"_s, "foo/bar"_s, ""_s, "file:///C:/?foo/bar"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "//C|?foo/bar"_s });
    checkRelativeURLDifferences("//C|#foo/bar"_s, "file:///tmp/mock/path"_s,
        { "file"_s, ""_s, ""_s, ""_s, 0, "/C:/"_s, ""_s, "foo/bar"_s, "file:///C:/#foo/bar"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "//C|#foo/bar"_s });
    checkURLDifferences("http://0xFFFFFfFF/"_s,
        { "http"_s, ""_s, ""_s, "255.255.255.255"_s, 0, "/"_s, ""_s, ""_s, "http://255.255.255.255/"_s },
        { "http"_s, ""_s, ""_s, "0xffffffff"_s, 0, "/"_s, ""_s, ""_s, "http://0xffffffff/"_s });
    checkURLDifferences("http://0000000000000000037777777777/"_s,
        { "http"_s, ""_s, ""_s, "255.255.255.255"_s, 0, "/"_s, ""_s, ""_s, "http://255.255.255.255/"_s },
        { "http"_s, ""_s, ""_s, "0000000000000000037777777777"_s, 0, "/"_s, ""_s, ""_s, "http://0000000000000000037777777777/"_s });
    checkURLDifferences("http://4294967295/"_s,
        { "http"_s, ""_s, ""_s, "255.255.255.255"_s, 0, "/"_s, ""_s, ""_s, "http://255.255.255.255/"_s },
        { "http"_s, ""_s, ""_s, "4294967295"_s, 0, "/"_s, ""_s, ""_s, "http://4294967295/"_s });
    checkURLDifferences("http://256/"_s,
        { "http"_s, ""_s, ""_s, "0.0.1.0"_s, 0, "/"_s, ""_s, ""_s, "http://0.0.1.0/"_s },
        { "http"_s, ""_s, ""_s, "256"_s, 0, "/"_s, ""_s, ""_s, "http://256/"_s });
    checkURLDifferences("http://256./"_s,
        { "http"_s, ""_s, ""_s, "0.0.1.0"_s, 0, "/"_s, ""_s, ""_s, "http://0.0.1.0/"_s },
        { "http"_s, ""_s, ""_s, "256."_s, 0, "/"_s, ""_s, ""_s, "http://256./"_s });
    checkURLDifferences("http://123.256/"_s,
        { "http"_s, ""_s, ""_s, "123.0.1.0"_s, 0, "/"_s, ""_s, ""_s, "http://123.0.1.0/"_s },
        { "http"_s, ""_s, ""_s, "123.256"_s, 0, "/"_s, ""_s, ""_s, "http://123.256/"_s });
    checkURLDifferences("http://127.%.0.1/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://127.%.0.1/"_s },
        { "http"_s, ""_s, ""_s, "127.%.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.%.0.1/"_s });
    checkURLDifferences("http://[1:2:3:4:5:6:7:8:]/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[1:2:3:4:5:6:7:8:]/"_s },
        { "http"_s, ""_s, ""_s, "[1:2:3:4:5:6:7:8:]"_s, 0, "/"_s, ""_s, ""_s, "http://[1:2:3:4:5:6:7:8:]/"_s });
    checkURLDifferences("http://[:2:3:4:5:6:7:8:]/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[:2:3:4:5:6:7:8:]/"_s },
        { "http"_s, ""_s, ""_s, "[:2:3:4:5:6:7:8:]"_s, 0, "/"_s, ""_s, ""_s, "http://[:2:3:4:5:6:7:8:]/"_s });
    checkURLDifferences("http://[1:2:3:4:5:6:7::]/"_s,
        { "http"_s, ""_s, ""_s, "[1:2:3:4:5:6:7:0]"_s, 0, "/"_s, ""_s, ""_s, "http://[1:2:3:4:5:6:7:0]/"_s },
        { "http"_s, ""_s, ""_s, "[1:2:3:4:5:6:7::]"_s, 0, "/"_s, ""_s, ""_s, "http://[1:2:3:4:5:6:7::]/"_s });
    checkURLDifferences("http://[1:2:3:4:5:6:7:::]/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[1:2:3:4:5:6:7:::]/"_s },
        { "http"_s, ""_s, ""_s, "[1:2:3:4:5:6:7:::]"_s, 0, "/"_s, ""_s, ""_s, "http://[1:2:3:4:5:6:7:::]/"_s });
    checkURLDifferences("http://127.0.0.1~/"_s,
        { "http"_s, ""_s, ""_s, "127.0.0.1~"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1~/"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://127.0.0.1~/"_s });
    checkURLDifferences("http://127.0.1~/"_s,
        { "http"_s, ""_s, ""_s, "127.0.1~"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.1~/"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://127.0.1~/"_s });
    checkURLDifferences("http://127.0.1./"_s,
        { "http"_s, ""_s, ""_s, "127.0.0.1"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.0.1/"_s },
        { "http"_s, ""_s, ""_s, "127.0.1."_s, 0, "/"_s, ""_s, ""_s, "http://127.0.1./"_s });
    checkURLDifferences("http://127.0.1.~/"_s,
        { "http"_s, ""_s, ""_s, "127.0.1.~"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.1.~/"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://127.0.1.~/"_s });
    checkURLDifferences("http://127.0.1.~"_s,
        { "http"_s, ""_s, ""_s, "127.0.1.~"_s, 0, "/"_s, ""_s, ""_s, "http://127.0.1.~/"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://127.0.1.~"_s });
    checkRelativeURLDifferences("http://f:000/c"_s, "http://example.org/foo/bar"_s,
        { "http"_s, ""_s, ""_s, "f"_s, 0, "/c"_s, ""_s, ""_s, "http://f:0/c"_s },
        { "http"_s, ""_s, ""_s, "f"_s, 0, "/c"_s, ""_s, ""_s, "http://f:000/c"_s });
    checkRelativeURLDifferences("http://f:010/c"_s, "http://example.org/foo/bar"_s,
        { "http"_s, ""_s, ""_s, "f"_s, 10, "/c"_s, ""_s, ""_s, "http://f:10/c"_s },
        { "http"_s, ""_s, ""_s, "f"_s, 10, "/c"_s, ""_s, ""_s, "http://f:010/c"_s });
    checkURL("notspecial://HoSt"_s, { "notspecial"_s, ""_s, ""_s, "HoSt"_s, 0, ""_s, ""_s, ""_s, "notspecial://HoSt"_s });
    checkURL("notspecial://H%6FSt"_s, { "notspecial"_s, ""_s, ""_s, "H%6FSt"_s, 0, ""_s, ""_s, ""_s, "notspecial://H%6FSt"_s });
    checkURL("notspecial://H%4fSt"_s, { "notspecial"_s, ""_s, ""_s, "H%4fSt"_s, 0, ""_s, ""_s, ""_s, "notspecial://H%4fSt"_s });
    checkURLDifferences(utf16String(u"notspecial://H😍ßt"),
        { "notspecial"_s, ""_s, ""_s, "H%F0%9F%98%8D%C3%9Ft"_s, 0, ""_s, ""_s, ""_s, "notspecial://H%F0%9F%98%8D%C3%9Ft"_s },
        { "notspecial"_s, ""_s, ""_s, "xn--hsst-qc83c"_s, 0, ""_s, ""_s, ""_s, "notspecial://xn--hsst-qc83c"_s }, testTabsValueForSurrogatePairs);
    checkURLDifferences("http://[ffff:aaaa:cccc:eeee:bbbb:dddd:255.255.255.255]/"_s,
        { "http"_s, ""_s, ""_s, "[ffff:aaaa:cccc:eeee:bbbb:dddd:ffff:ffff]"_s, 0, "/"_s, ""_s, ""_s, "http://[ffff:aaaa:cccc:eeee:bbbb:dddd:ffff:ffff]/"_s },
        { "http"_s, ""_s, ""_s, "[ffff:aaaa:cccc:eeee:bbbb:dddd:255.255.255.255]"_s, 0, "/"_s, ""_s, ""_s, "http://[ffff:aaaa:cccc:eeee:bbbb:dddd:255.255.255.255]/"_s }, TestTabs::No);
    checkURLDifferences("http://[::123.234.12.210]/"_s,
        { "http"_s, ""_s, ""_s, "[::7bea:cd2]"_s, 0, "/"_s, ""_s, ""_s, "http://[::7bea:cd2]/"_s },
        { "http"_s, ""_s, ""_s, "[::123.234.12.210]"_s, 0, "/"_s, ""_s, ""_s, "http://[::123.234.12.210]/"_s });
    checkURLDifferences("http://[::a:255.255.255.255]/"_s,
        { "http"_s, ""_s, ""_s, "[::a:ffff:ffff]"_s, 0, "/"_s, ""_s, ""_s, "http://[::a:ffff:ffff]/"_s },
        { "http"_s, ""_s, ""_s, "[::a:255.255.255.255]"_s, 0, "/"_s, ""_s, ""_s, "http://[::a:255.255.255.255]/"_s });
    checkURLDifferences("http://[::0.00.255.255]/"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[::0.00.255.255]/"_s },
        { "http"_s, ""_s, ""_s, "[::0.00.255.255]"_s, 0, "/"_s, ""_s, ""_s, "http://[::0.00.255.255]/"_s });
    checkURLDifferences("http://[::0.0.255.255]/"_s,
        { "http"_s, ""_s, ""_s, "[::ffff]"_s, 0, "/"_s, ""_s, ""_s, "http://[::ffff]/"_s },
        { "http"_s, ""_s, ""_s, "[::0.0.255.255]"_s, 0, "/"_s, ""_s, ""_s, "http://[::0.0.255.255]/"_s });
    checkURLDifferences("http://[::0:1.0.255.255]/"_s,
        { "http"_s, ""_s, ""_s, "[::100:ffff]"_s, 0, "/"_s, ""_s, ""_s, "http://[::100:ffff]/"_s },
        { "http"_s, ""_s, ""_s, "[::0:1.0.255.255]"_s, 0, "/"_s, ""_s, ""_s, "http://[::0:1.0.255.255]/"_s });
    checkURLDifferences("http://[::A:1.0.255.255]/"_s,
        { "http"_s, ""_s, ""_s, "[::a:100:ffff]"_s, 0, "/"_s, ""_s, ""_s, "http://[::a:100:ffff]/"_s },
        { "http"_s, ""_s, ""_s, "[::a:1.0.255.255]"_s, 0, "/"_s, ""_s, ""_s, "http://[::a:1.0.255.255]/"_s });
    checkURLDifferences("http://[:127.0.0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[:127.0.0.1]"_s },
        { "http"_s, ""_s, ""_s, "[:127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[:127.0.0.1]/"_s });
    checkURLDifferences("http://[127.0.0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[127.0.0.1]"_s },
        { "http"_s, ""_s, ""_s, "[127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[127.0.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.1]"_s,
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:7f00:1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:7f00:1]/"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.101]"_s,
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:7f00:65]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:7f00:65]/"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:127.0.0.101]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.0.101]/"_s });
    checkURLDifferences("http://[::a:b:c:d:e:f:127.0.0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[::a:b:c:d:e:f:127.0.0.1]"_s },
        { "http"_s, ""_s, ""_s, "[::a:b:c:d:e:f:127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[::a:b:c:d:e:f:127.0.0.1]/"_s });
    checkURLDifferences("http://[a:b::c:d:e:f:127.0.0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b::c:d:e:f:127.0.0.1]"_s },
        { "http"_s, ""_s, ""_s, "[a:b::c:d:e:f:127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b::c:d:e:f:127.0.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:127.0.0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:127.0.0.1]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:127.0.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.0.0.1]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:127.0.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.0.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.1]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:127.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.011]"_s, // Chrome treats this as octal, Firefox and the spec fail
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.0.011]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:127.0.0.011]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.0.011]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.00.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.00.1]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:127.0.00.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.00.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f:127.0.0.1.]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.0.1.]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:127.0.0.1.]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0.0.1.]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f:127.0..0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0..0.1]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f:127.0..0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f:127.0..0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f::127.0.0.1]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f::127.0.0.1]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f::127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f::127.0.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e::127.0.0.1]"_s,
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:0:7f00:1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:0:7f00:1]/"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e::127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e::127.0.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d::e:127.0.0.1]"_s,
        { "http"_s, ""_s, ""_s, "[a:b:c:d:0:e:7f00:1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:0:e:7f00:1]/"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d::e:127.0.0.1]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d::e:127.0.0.1]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f::127.0.0.]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f::127.0.0.]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f::127.0.0.]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f::127.0.0.]/"_s });
    checkURLDifferences("http://[a:b:c:d:e:f::127.0.0.256]"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://[a:b:c:d:e:f::127.0.0.256]"_s },
        { "http"_s, ""_s, ""_s, "[a:b:c:d:e:f::127.0.0.256]"_s, 0, "/"_s, ""_s, ""_s, "http://[a:b:c:d:e:f::127.0.0.256]/"_s });
    checkURLDifferences("http://123456"_s, { "http"_s, ""_s, ""_s, "0.1.226.64"_s, 0, "/"_s, ""_s, ""_s, "http://0.1.226.64/"_s }, { "http"_s, ""_s, ""_s, "123456"_s, 0, "/"_s, ""_s, ""_s, "http://123456/"_s });
    checkURL("asdf://123456"_s, { "asdf"_s, ""_s, ""_s, "123456"_s, 0, ""_s, ""_s, ""_s, "asdf://123456"_s });
    checkURLDifferences("http://[0:0:0:0:a:b:c:d]"_s,
        { "http"_s, ""_s, ""_s, "[::a:b:c:d]"_s, 0, "/"_s, ""_s, ""_s, "http://[::a:b:c:d]/"_s },
        { "http"_s, ""_s, ""_s, "[0:0:0:0:a:b:c:d]"_s, 0, "/"_s, ""_s, ""_s, "http://[0:0:0:0:a:b:c:d]/"_s });
    checkURLDifferences("asdf://[0:0:0:0:a:b:c:d]"_s,
        { "asdf"_s, ""_s, ""_s, "[::a:b:c:d]"_s, 0, ""_s, ""_s, ""_s, "asdf://[::a:b:c:d]"_s },
        { "asdf"_s, ""_s, ""_s, "[0:0:0:0:a:b:c:d]"_s, 0, ""_s, ""_s, ""_s, "asdf://[0:0:0:0:a:b:c:d]"_s }, TestTabs::No);
    shouldFail("a://%:a"_s);
    checkURL("a://%:/"_s, { "a"_s, ""_s, ""_s, "%"_s, 0, "/"_s, ""_s, ""_s, "a://%/"_s });
    checkURL("a://%:"_s, { "a"_s, ""_s, ""_s, "%"_s, 0, ""_s, ""_s, ""_s, "a://%"_s });
    checkURL("a://%:1/"_s, { "a"_s, ""_s, ""_s, "%"_s, 1, "/"_s, ""_s, ""_s, "a://%:1/"_s });
    checkURLDifferences("http://%:"_s,
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "http://%:"_s },
        { "http"_s, ""_s, ""_s, "%"_s, 0, "/"_s, ""_s, ""_s, "http://%/"_s });
    checkURL("a://123456"_s, { "a"_s, ""_s, ""_s, "123456"_s, 0, ""_s, ""_s, ""_s, "a://123456"_s });
    checkURL("a://123456:7"_s, { "a"_s, ""_s, ""_s, "123456"_s, 7, ""_s, ""_s, ""_s, "a://123456:7"_s });
    checkURL("a://123456:7/"_s, { "a"_s, ""_s, ""_s, "123456"_s, 7, "/"_s, ""_s, ""_s, "a://123456:7/"_s });
    checkURL("a://A"_s, { "a"_s, ""_s, ""_s, "A"_s, 0, ""_s, ""_s, ""_s, "a://A"_s });
    checkURL("a://A:2"_s, { "a"_s, ""_s, ""_s, "A"_s, 2, ""_s, ""_s, ""_s, "a://A:2"_s });
    checkURL("a://A:2/"_s, { "a"_s, ""_s, ""_s, "A"_s, 2, "/"_s, ""_s, ""_s, "a://A:2/"_s });
    checkURLDifferences(StringView::fromLatin1(reinterpret_cast<const char*>(u8"asd://ß")),
        { "asd"_s, ""_s, ""_s, "%C3%83%C2%9F"_s, 0, ""_s, ""_s, ""_s, "asd://%C3%83%C2%9F"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "about:blank"_s }, TestTabs::No);
    checkURLDifferences(StringView::fromLatin1(reinterpret_cast<const char*>(u8"asd://ß:4")),
        { "asd"_s, ""_s, ""_s, "%C3%83%C2%9F"_s, 4, ""_s, ""_s, ""_s, "asd://%C3%83%C2%9F:4"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "about:blank"_s }, TestTabs::No);
    checkURLDifferences(StringView::fromLatin1(reinterpret_cast<const char*>(u8"asd://ß:4/")),
        { "asd"_s, ""_s, ""_s, "%C3%83%C2%9F"_s, 4, "/"_s, ""_s, ""_s, "asd://%C3%83%C2%9F:4/"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "about:blank"_s }, TestTabs::No);
    checkURLDifferences("a://[A::b]:4"_s,
        { "a"_s, ""_s, ""_s, "[a::b]"_s, 4, ""_s, ""_s, ""_s, "a://[a::b]:4"_s },
        { "a"_s, ""_s, ""_s, "[A::b]"_s, 4, ""_s, ""_s, ""_s, "a://[A::b]:4"_s });
    shouldFail("http://[~]"_s);
    shouldFail("a://[~]"_s);
    checkRelativeURLDifferences("a://b"_s, "//[aBc]"_s,
        { "a"_s, ""_s, ""_s, "b"_s, 0, ""_s, ""_s, ""_s, "a://b"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "a://b"_s });
    checkURL(utf16String(u"http://öbb.at"), { "http"_s, ""_s, ""_s, "xn--bb-eka.at"_s, 0, "/"_s, ""_s, ""_s, "http://xn--bb-eka.at/"_s });
    checkURL(utf16String(u"http://ÖBB.at"), { "http"_s, ""_s, ""_s, "xn--bb-eka.at"_s, 0, "/"_s, ""_s, ""_s, "http://xn--bb-eka.at/"_s });
    checkURL(utf16String(u"http://√.com"), { "http"_s, ""_s, ""_s, "xn--19g.com"_s, 0, "/"_s, ""_s, ""_s, "http://xn--19g.com/"_s });
    checkURLDifferences(utf16String(u"http://faß.de"),
        { "http"_s, ""_s, ""_s, "xn--fa-hia.de"_s, 0, "/"_s, ""_s, ""_s, "http://xn--fa-hia.de/"_s },
        { "http"_s, ""_s, ""_s, "fass.de"_s, 0, "/"_s, ""_s, ""_s, "http://fass.de/"_s });
    checkURL(utf16String(u"http://ԛәлп.com"), { "http"_s, ""_s, ""_s, "xn--k1ai47bhi.com"_s, 0, "/"_s, ""_s, ""_s, "http://xn--k1ai47bhi.com/"_s });
    checkURLDifferences(utf16String(u"http://Ⱥbby.com"),
        { "http"_s, ""_s, ""_s, "xn--bby-iy0b.com"_s, 0, "/"_s, ""_s, ""_s, "http://xn--bby-iy0b.com/"_s },
        { "http"_s, ""_s, ""_s, "xn--bby-spb.com"_s, 0, "/"_s, ""_s, ""_s, "http://xn--bby-spb.com/"_s });
    checkURLDifferences(utf16String(u"http://\u2132"),
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, utf16String(u"http://Ⅎ") },
        { "http"_s, ""_s, ""_s, "xn--f3g"_s, 0, "/"_s, ""_s, ""_s, "http://xn--f3g/"_s });
    checkURLDifferences(utf16String(u"http://\u05D9\u05B4\u05D5\u05D0\u05B8/"),
        { "http"_s, ""_s, ""_s, "xn--cdbi5etas"_s, 0, "/"_s, ""_s, ""_s, "http://xn--cdbi5etas/"_s },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "about:blank"_s }, TestTabs::No);
    checkURLDifferences(utf16String(u"http://bidirectional\u0786\u07AE\u0782\u07B0\u0795\u07A9\u0793\u07A6\u0783\u07AA/"),
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, utf16String(u"http://bidirectionalކޮންޕީޓަރު/") },
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "about:blank"_s }, TestTabs::No);
    checkURLDifferences(utf16String(u"http://contextj\u200D"),
        { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, utf16String(u"http://contextj\u200D") },
        { "http"_s, ""_s, ""_s, "contextj"_s, 0, "/"_s, ""_s, ""_s, "http://contextj/"_s });
    checkURL(utf16String(u"http://contexto\u30FB"), { "http"_s, ""_s, ""_s, "xn--contexto-wg5g"_s, 0, "/"_s, ""_s, ""_s, "http://xn--contexto-wg5g/"_s });
    checkURLDifferences(utf16String(u"http://\u321D\u321E/"),
        { "http"_s, ""_s, ""_s, "xn--()()-bs0sc174agx4b"_s, 0, "/"_s, ""_s, ""_s, "http://xn--()()-bs0sc174agx4b/"_s },
        { "http"_s, ""_s, ""_s, "xn--5mkc"_s, 0, "/"_s, ""_s, ""_s, "http://xn--5mkc/"_s });
}

TEST_F(WTF_URLParser, DefaultPort)
{
    checkURL("FtP://host:21/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("ftp://host:21/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("f\ttp://host:21/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("f\ttp://host\t:21/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("f\ttp://host:\t21/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("f\ttp://host:2\t1/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("f\ttp://host:21\t/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("ftp://host\t:21/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("ftp://host:\t21/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("ftp://host:2\t1/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("ftp://host:21\t/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s });
    checkURL("ftp://host:22/"_s, { "ftp"_s, ""_s, ""_s, "host"_s, 22, "/"_s, ""_s, ""_s, "ftp://host:22/"_s });
    checkURLDifferences("ftp://host:21"_s,
        { "ftp"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ftp://host/"_s },
        { "ftp"_s, ""_s, ""_s, "host"_s, 0, ""_s, ""_s, ""_s, "ftp://host"_s });
    checkURLDifferences("ftp://host:22"_s,
        { "ftp"_s, ""_s, ""_s, "host"_s, 22, "/"_s, ""_s, ""_s, "ftp://host:22/"_s },
        { "ftp"_s, ""_s, ""_s, "host"_s, 22, ""_s, ""_s, ""_s, "ftp://host:22"_s });
    
    checkURL("gOpHeR://host:70/"_s, { "gopher"_s, ""_s, ""_s, "host"_s, 70, "/"_s, ""_s, ""_s, "gopher://host:70/"_s });
    checkURL("gopher://host:70/"_s, { "gopher"_s, ""_s, ""_s, "host"_s, 70, "/"_s, ""_s, ""_s, "gopher://host:70/"_s });
    checkURL("gopher://host:71/"_s, { "gopher"_s, ""_s, ""_s, "host"_s, 71, "/"_s, ""_s, ""_s, "gopher://host:71/"_s });
    
    checkURL("hTtP://host:80"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkURL("http://host:80"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkURL("http://host:80/"_s, { "http"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "http://host/"_s });
    checkURL("http://host:81"_s, { "http"_s, ""_s, ""_s, "host"_s, 81, "/"_s, ""_s, ""_s, "http://host:81/"_s });
    checkURL("http://host:81/"_s, { "http"_s, ""_s, ""_s, "host"_s, 81, "/"_s, ""_s, ""_s, "http://host:81/"_s });
    
    checkURL("hTtPs://host:443"_s, { "https"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "https://host/"_s });
    checkURL("https://host:443"_s, { "https"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "https://host/"_s });
    checkURL("https://host:443/"_s, { "https"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "https://host/"_s });
    checkURL("https://host:444"_s, { "https"_s, ""_s, ""_s, "host"_s, 444, "/"_s, ""_s, ""_s, "https://host:444/"_s });
    checkURL("https://host:444/"_s, { "https"_s, ""_s, ""_s, "host"_s, 444, "/"_s, ""_s, ""_s, "https://host:444/"_s });
    
    checkURL("wS://host:80/"_s, { "ws"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ws://host/"_s });
    checkURL("ws://host:80/"_s, { "ws"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ws://host/"_s });
    checkURL("ws://host:81/"_s, { "ws"_s, ""_s, ""_s, "host"_s, 81, "/"_s, ""_s, ""_s, "ws://host:81/"_s });
    // URLParser matches Chrome and Firefox, but not URL::parse
    checkURLDifferences("ws://host:80"_s,
        { "ws"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "ws://host/"_s },
        { "ws"_s, ""_s, ""_s, "host"_s, 0, ""_s, ""_s, ""_s, "ws://host"_s });
    checkURLDifferences("ws://host:81"_s,
        { "ws"_s, ""_s, ""_s, "host"_s, 81, "/"_s, ""_s, ""_s, "ws://host:81/"_s },
        { "ws"_s, ""_s, ""_s, "host"_s, 81, ""_s, ""_s, ""_s, "ws://host:81"_s });
    
    checkURL("WsS://host:443/"_s, { "wss"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "wss://host/"_s });
    checkURL("wss://host:443/"_s, { "wss"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "wss://host/"_s });
    checkURL("wss://host:444/"_s, { "wss"_s, ""_s, ""_s, "host"_s, 444, "/"_s, ""_s, ""_s, "wss://host:444/"_s });
    // URLParser matches Chrome and Firefox, but not URL::parse
    checkURLDifferences("wss://host:443"_s,
        { "wss"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "wss://host/"_s },
        { "wss"_s, ""_s, ""_s, "host"_s, 0, ""_s, ""_s, ""_s, "wss://host"_s });
    checkURLDifferences("wss://host:444"_s,
        { "wss"_s, ""_s, ""_s, "host"_s, 444, "/"_s, ""_s, ""_s, "wss://host:444/"_s },
        { "wss"_s, ""_s, ""_s, "host"_s, 444, ""_s, ""_s, ""_s, "wss://host:444"_s });

    checkURL("fTpS://host:990/"_s, { "ftps"_s, ""_s, ""_s, "host"_s, 990, "/"_s, ""_s, ""_s, "ftps://host:990/"_s });
    checkURL("ftps://host:990/"_s, { "ftps"_s, ""_s, ""_s, "host"_s, 990, "/"_s, ""_s, ""_s, "ftps://host:990/"_s });
    checkURL("ftps://host:991/"_s, { "ftps"_s, ""_s, ""_s, "host"_s, 991, "/"_s, ""_s, ""_s, "ftps://host:991/"_s });
    checkURL("ftps://host:990"_s, { "ftps"_s, ""_s, ""_s, "host"_s, 990, ""_s, ""_s, ""_s, "ftps://host:990"_s });
    checkURL("ftps://host:991"_s, { "ftps"_s, ""_s, ""_s, "host"_s, 991, ""_s, ""_s, ""_s, "ftps://host:991"_s });

    checkURL("uNkNoWn://host:80/"_s, { "unknown"_s, ""_s, ""_s, "host"_s, 80, "/"_s, ""_s, ""_s, "unknown://host:80/"_s });
    checkURL("unknown://host:80/"_s, { "unknown"_s, ""_s, ""_s, "host"_s, 80, "/"_s, ""_s, ""_s, "unknown://host:80/"_s });
    checkURL("unknown://host:81/"_s, { "unknown"_s, ""_s, ""_s, "host"_s, 81, "/"_s, ""_s, ""_s, "unknown://host:81/"_s });
    checkURL("unknown://host:80"_s, { "unknown"_s, ""_s, ""_s, "host"_s, 80, ""_s, ""_s, ""_s, "unknown://host:80"_s });
    checkURL("unknown://host:81"_s, { "unknown"_s, ""_s, ""_s, "host"_s, 81, ""_s, ""_s, ""_s, "unknown://host:81"_s });

    checkURL("file://host/"_s, { "file"_s, ""_s, ""_s, "host"_s, 0, "/"_s, ""_s, ""_s, "file://host/"_s });
    checkURL("file://host:"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "file://host:"_s });
    checkURL("file://host:0"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "file://host:0"_s });
    checkURL("file://host:80"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "file://host:80"_s });
    checkURL("file://host:80/path"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "file://host:80/path"_s });
    checkURL("file://:80/path"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "file://:80/path"_s });
    checkURL("file://:0/path"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "file://:0/path"_s });
    
    checkURL("http://example.com:0000000000000077"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 77, "/"_s, ""_s, ""_s, "http://example.com:77/"_s });
    checkURL("http://example.com:0000000000000080"_s, { "http"_s, ""_s, ""_s, "example.com"_s, 0, "/"_s, ""_s, ""_s, "http://example.com/"_s });
}

TEST_F(WTF_URLParser, ParserFailures)
{
    shouldFail("    "_s);
    shouldFail("  \a  "_s);
    shouldFail(""_s);
    shouldFail(String());
    shouldFail(""_s, "about:blank"_s);
    shouldFail(String(), "about:blank"_s);
    shouldFail("http://127.0.0.1:abc"_s);
    shouldFail("http://host:abc"_s);
    shouldFail("http://:abc"_s);
    shouldFail("http://a:@"_s, "about:blank"_s);
    shouldFail("http://:b@"_s, "about:blank"_s);
    shouldFail("http://:@"_s, "about:blank"_s);
    shouldFail("http://a:@"_s);
    shouldFail("http://:b@"_s);
    shouldFail("http://@"_s);
    shouldFail("http://[0:f::f:f:0:0]:abc"_s);
    shouldFail("../i"_s, "sc:sd"_s);
    shouldFail("../i"_s, "sc:sd/sd"_s);
    shouldFail("/i"_s, "sc:sd"_s);
    shouldFail("/i"_s, "sc:sd/sd"_s);
    shouldFail("?i"_s, "sc:sd"_s);
    shouldFail("?i"_s, "sc:sd/sd"_s);
    shouldFail("http://example example.com"_s, "http://other.com/"_s);
    shouldFail("http://[www.example.com]/"_s, "about:blank"_s);
    shouldFail("http://192.168.0.1 hello"_s, "http://other.com/"_s);
    shouldFail("http://[example.com]"_s, "http://other.com/"_s);
    shouldFail("i"_s, "sc:sd"_s);
    shouldFail("i"_s, "sc:sd/sd"_s);
    shouldFail("i"_s);
    shouldFail("asdf"_s);
    shouldFail("~"_s);
    shouldFail("%"_s);
    shouldFail("//%"_s);
    shouldFail("~"_s, "about:blank"_s);
    shouldFail("~~~"_s);
    shouldFail("://:0/"_s);
    shouldFail("://:0/"_s, ""_s);
    shouldFail("://:0/"_s, "about:blank"_s);
    shouldFail("about~"_s);
    shouldFail("//C:asdf/foo/bar"_s, "file:///tmp/mock/path"_s);
    shouldFail("wss://[c::]abc/"_s);
    shouldFail("abc://[c::]:abc/"_s);
    shouldFail("abc://[c::]01"_s);
    shouldFail("http://[1234::ab#]"_s);
    shouldFail("http://[1234::ab/]"_s);
    shouldFail("http://[1234::ab?]"_s);
    shouldFail("http://[1234::ab@]"_s);
    shouldFail("http://[1234::ab~]"_s);
    shouldFail("http://[2001::1"_s);
    shouldFail(StringView::fromLatin1("http://4:b\xE1"));
    shouldFail("http://[1:2:3:4:5:6:7:8~]/"_s);
    shouldFail("http://[a:b:c:d:e:f:g:127.0.0.1]"_s);
    shouldFail("http://[a:b:c:d:e:f:g:h:127.0.0.1]"_s);
    shouldFail("http://[a:b:c:d:e:f:127.0.0.0x11]"_s); // Chrome treats this as hex, Firefox and the spec fail
    shouldFail("http://[a:b:c:d:e:f:127.0.-0.1]"_s);
    shouldFail("asdf://space In\aHost"_s);
    shouldFail("asdf://[0:0:0:0:a:b:c:d"_s);
    shouldFail("http://[::0:0.0.00000.0]/"_s);
}

// These are in the spec but not in the web platform tests.
TEST_F(WTF_URLParser, AdditionalTests)
{
    checkURL("about:\a\aabc"_s, { "about"_s, ""_s, ""_s, ""_s, 0, "%07%07abc"_s, ""_s, ""_s, "about:%07%07abc"_s });
    checkURL("notspecial:\t\t\n\t"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "notspecial:"_s });
    checkURL("notspecial\t\t\n\t:\t\t\n\t/\t\t\n\t/\t\t\n\thost"_s, { "notspecial"_s, ""_s, ""_s, "host"_s, 0, ""_s, ""_s, ""_s, "notspecial://host"_s });
    checkRelativeURL("http:"_s, "http://example.org/foo/bar?query#fragment"_s, { "http"_s, ""_s, ""_s, "example.org"_s, 0, "/foo/bar"_s, "query"_s, ""_s, "http://example.org/foo/bar?query"_s });
    checkRelativeURL("ws:"_s, "http://example.org/foo/bar"_s, { ""_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "ws:"_s });
    checkRelativeURL("notspecial:"_s, "http://example.org/foo/bar"_s, { "notspecial"_s, ""_s, ""_s, ""_s, 0, ""_s, ""_s, ""_s, "notspecial:"_s });

    const wchar_t surrogateBegin = 0xD800;
    const wchar_t validSurrogateEnd = 0xDD55;
    const wchar_t invalidSurrogateEnd = 'A';
    const wchar_t replacementCharacter = 0xFFFD;
    checkURL(utf16String<12>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', surrogateBegin, validSurrogateEnd, '\0'}),
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/%F0%90%85%95"_s, ""_s, ""_s, "http://w/%F0%90%85%95"_s }, testTabsValueForSurrogatePairs);
    shouldFail(utf16String<10>({'h', 't', 't', 'p', ':', '/', surrogateBegin, invalidSurrogateEnd, '/', '\0'}));
    shouldFail(utf16String<9>({'h', 't', 't', 'p', ':', '/', replacementCharacter, '/', '\0'}));
    
    // URLParser matches Chrome and Firefox but not URL::parse.
    checkURLDifferences(utf16String<12>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', surrogateBegin, invalidSurrogateEnd}),
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/%EF%BF%BDA"_s, ""_s, ""_s, "http://w/%EF%BF%BDA"_s },
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/%ED%A0%80A"_s, ""_s, ""_s, "http://w/%ED%A0%80A"_s });
    checkURLDifferences(utf16String<13>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', '?', surrogateBegin, invalidSurrogateEnd, '\0'}),
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/"_s, "%EF%BF%BDA"_s, ""_s, "http://w/?%EF%BF%BDA"_s },
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/"_s, "%ED%A0%80A"_s, ""_s, "http://w/?%ED%A0%80A"_s });
    checkURLDifferences(utf16String<11>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', surrogateBegin, '\0'}),
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/%EF%BF%BD"_s, ""_s, ""_s, "http://w/%EF%BF%BD"_s },
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/%ED%A0%80"_s, ""_s, ""_s, "http://w/%ED%A0%80"_s });
    checkURLDifferences(utf16String<12>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', '?', surrogateBegin, '\0'}),
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/"_s, "%EF%BF%BD"_s, ""_s, "http://w/?%EF%BF%BD"_s },
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/"_s, "%ED%A0%80"_s, ""_s, "http://w/?%ED%A0%80"_s });
    checkURLDifferences(utf16String<13>({'h', 't', 't', 'p', ':', '/', '/', 'w', '/', '?', surrogateBegin, ' ', '\0'}),
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/"_s, "%EF%BF%BD"_s, ""_s, "http://w/?%EF%BF%BD"_s },
        { "http"_s, ""_s, ""_s, "w"_s, 0, "/"_s, "%ED%A0%80"_s, ""_s, "http://w/?%ED%A0%80"_s });
}

} // namespace TestWebKitAPI
