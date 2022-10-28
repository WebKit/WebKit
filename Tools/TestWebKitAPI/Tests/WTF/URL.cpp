/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringHash.h>

namespace TestWebKitAPI {

class WTF_URL : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }
};

TEST_F(WTF_URL, URLConstructorDefault)
{
    URL kurl;

    EXPECT_TRUE(kurl.isEmpty());
    EXPECT_TRUE(kurl.isNull());
    EXPECT_FALSE(kurl.isValid());
}

TEST_F(WTF_URL, URLConstructorConstChar)
{
    URL kurl("http://username:password@www.example.com:8080/index.html?var=val#fragment"_s);

    EXPECT_FALSE(kurl.isEmpty());
    EXPECT_FALSE(kurl.isNull());
    EXPECT_TRUE(kurl.isValid());

    EXPECT_TRUE(kurl.protocol() == "http"_s);
    EXPECT_EQ(String("www.example.com"_s), kurl.host().toString());
    EXPECT_TRUE(!!kurl.port());
    EXPECT_EQ(8080, kurl.port().value());
    EXPECT_EQ(String("username"_s), kurl.user());
    EXPECT_EQ(String("password"_s), kurl.password());
    EXPECT_EQ(String("/index.html"_s), kurl.path());
    EXPECT_EQ(String("index.html"_s), kurl.lastPathComponent());
    EXPECT_EQ(String("var=val"_s), kurl.query());
    EXPECT_TRUE(kurl.hasFragmentIdentifier());
    EXPECT_EQ(String("fragment"_s), kurl.fragmentIdentifier());
}

static URL createURL(ASCIILiteral urlAsString)
{
    return URL(urlAsString);
};

TEST_F(WTF_URL, URLProtocolHostAndPort)
{
    auto url = createURL("http://username:password@www.example.com:8080/index.html?var=val#fragment"_s);
    EXPECT_EQ(String("http://www.example.com:8080"_s), url.protocolHostAndPort());

    url = createURL("http://username:@www.example.com:8080/index.html?var=val#fragment"_s);
    EXPECT_EQ(String("http://www.example.com:8080"_s), url.protocolHostAndPort());

    url = createURL("http://:password@www.example.com:8080/index.html?var=val#fragment"_s);
    EXPECT_EQ(String("http://www.example.com:8080"_s), url.protocolHostAndPort());

    url = createURL("http://username@www.example.com:8080/index.html?var=val#fragment"_s);
    EXPECT_EQ(String("http://www.example.com:8080"_s), url.protocolHostAndPort());

    url = createURL("http://www.example.com:8080/index.html?var=val#fragment"_s);
    EXPECT_EQ(String("http://www.example.com:8080"_s), url.protocolHostAndPort());

    url = createURL("http://www.example.com:/index.html?var=val#fragment"_s);
    EXPECT_EQ(String("http://www.example.com"_s), url.protocolHostAndPort());

    url = createURL("http://www.example.com/index.html?var=val#fragment"_s);
    EXPECT_EQ(String("http://www.example.com"_s), url.protocolHostAndPort());

    url = createURL("file:///a/b/c"_s);
    EXPECT_EQ(String("file://"_s), url.protocolHostAndPort());

    url = createURL("file:///a/b"_s);
    EXPECT_EQ(String("file://"_s), url.protocolHostAndPort());

    url = createURL("file:///a"_s);
    EXPECT_EQ(String("file://"_s), url.protocolHostAndPort());

    url = createURL("file:///a"_s);
    EXPECT_EQ(String("file://"_s), url.protocolHostAndPort());

    url = createURL("asdf://username:password@www.example.com:8080/index.html?var=val#fragment"_s);
    EXPECT_EQ(String("asdf://www.example.com:8080"_s), url.protocolHostAndPort());

    url = createURL("asdf:///a/b/c"_s);
    EXPECT_EQ(String("asdf://"_s), url.protocolHostAndPort());
}

TEST_F(WTF_URL, URLDataURIStringSharing)
{
    URL baseURL("http://www.webkit.org/"_s);
    String threeApples = "data:text/plain;charset=utf-8;base64,76O/76O/76O/"_s;

    URL url(baseURL, threeApples);
    EXPECT_EQ(threeApples.impl(), url.string().impl());
}

TEST_F(WTF_URL, URLSetQuery)
{
    URL url = createURL("http://www.webkit.org/?test"_s);
    URL url1 = createURL("http://www.webkit.org/"_s);
    URL url2 = createURL("http://www.webkit.org/?"_s);
    URL url3 = createURL("http://www.webkit.org/?test"_s);
    URL url4 = createURL("http://www.webkit.org/?test1"_s);

    url1.setQuery("test"_s);
    url2.setQuery("test"_s);
    url3.setQuery("test"_s);
    url4.setQuery("test"_s);

    EXPECT_EQ(url.string(), url1.string());
    EXPECT_EQ(url.string(), url2.string());
    EXPECT_EQ(url.string(), url3.string());
    EXPECT_EQ(url.string(), url4.string());

    URL urlWithFragmentIdentifier = createURL("http://www.webkit.org/?test%C3%83%C2%A5#newFragment"_s);
    URL urlWithFragmentIdentifier1 = createURL("http://www.webkit.org/#newFragment"_s);
    URL urlWithFragmentIdentifier2 = createURL("http://www.webkit.org/?#newFragment"_s);
    URL urlWithFragmentIdentifier3 = createURL("http://www.webkit.org/?test1#newFragment"_s);

    urlWithFragmentIdentifier1.setQuery(StringView::fromLatin1("test\xc3\xa5"));
    urlWithFragmentIdentifier2.setQuery(StringView::fromLatin1("test\xc3\xa5"));
    urlWithFragmentIdentifier3.setQuery(StringView::fromLatin1("test\xc3\xa5"));

    EXPECT_EQ(urlWithFragmentIdentifier.string(), urlWithFragmentIdentifier1.string());
    EXPECT_EQ(urlWithFragmentIdentifier.string(), urlWithFragmentIdentifier2.string());
    EXPECT_EQ(urlWithFragmentIdentifier.string(), urlWithFragmentIdentifier3.string());
}

TEST_F(WTF_URL, URLSetFragmentIdentifier)
{
    URL url = createURL("http://www.webkit.org/#newFragment%C3%83%C2%A5"_s);
    URL url1 = createURL("http://www.webkit.org/"_s);
    URL url2 = createURL("http://www.webkit.org/#test2"_s);
    URL url3 = createURL("http://www.webkit.org/#"_s);

    url1.setFragmentIdentifier(StringView::fromLatin1("newFragment\xc3\xa5"));
    url2.setFragmentIdentifier(StringView::fromLatin1("newFragment\xc3\xa5"));
    url3.setFragmentIdentifier(StringView::fromLatin1("newFragment\xc3\xa5"));

    EXPECT_EQ(url.string(), url1.string());
    EXPECT_EQ(url.string(), url2.string());
    EXPECT_EQ(url.string(), url3.string());

    URL urlWithQuery = createURL("http://www.webkit.org/?test1#newFragment"_s);
    URL urlWithQuery1 = createURL("http://www.webkit.org/?test1"_s);
    URL urlWithQuery2 = createURL("http://www.webkit.org/?test1#"_s);
    URL urlWithQuery3 = createURL("http://www.webkit.org/?test1#test2"_s);

    urlWithQuery1.setFragmentIdentifier("newFragment"_s);
    urlWithQuery2.setFragmentIdentifier("newFragment"_s);
    urlWithQuery3.setFragmentIdentifier("newFragment"_s);

    EXPECT_EQ(urlWithQuery.string(), urlWithQuery1.string());
    EXPECT_EQ(urlWithQuery.string(), urlWithQuery2.string());
    EXPECT_EQ(urlWithQuery.string(), urlWithQuery3.string());
}

TEST_F(WTF_URL, URLRemoveQueryAndFragmentIdentifier)
{
    URL url = createURL("http://www.webkit.org/"_s);
    URL url1 = createURL("http://www.webkit.org/?"_s);
    URL url2 = createURL("http://www.webkit.org/?test1"_s);
    URL url3 = createURL("http://www.webkit.org/?test1#test2"_s);
    URL url4 = createURL("http://www.webkit.org/#test2"_s);
    URL url5 = createURL("http://www.webkit.org/#"_s);

    url.removeQueryAndFragmentIdentifier();
    url1.removeQueryAndFragmentIdentifier();
    url2.removeQueryAndFragmentIdentifier();
    url3.removeQueryAndFragmentIdentifier();
    url4.removeQueryAndFragmentIdentifier();
    url5.removeQueryAndFragmentIdentifier();

    EXPECT_EQ(url.string(), url.string());
    EXPECT_EQ(url.string(), url1.string());
    EXPECT_EQ(url.string(), url2.string());
    EXPECT_EQ(url.string(), url3.string());
    EXPECT_EQ(url.string(), url4.string());
    EXPECT_EQ(url.string(), url5.string());
}

TEST_F(WTF_URL, EqualIgnoringFragmentIdentifier)
{
    struct TestCase {
        ASCIILiteral url1;
        ASCIILiteral url2;
        bool expected;
    } cases[] = {
        {"http://example.com/"_s, "http://example.com/"_s, true},
        {"http://example.com/#hash"_s, "http://example.com/"_s, true},
        {"http://example.com/path"_s, "http://example.com/"_s, false},
        {"http://example.com/path"_s, "http://example.com/path"_s, true},
        {"http://example.com/path#hash"_s, "http://example.com/path"_s, true},
        {"http://example.com/path?query"_s, "http://example.com/path"_s, false},
        {"http://example.com/path?query#hash"_s, "http://example.com/path"_s, false},
        {"http://example.com/otherpath"_s, "http://example.com/path"_s, false},
        {"http://example.com:80/"_s, "http://example.com/"_s, true},
        {"http://example.com:80/#hash"_s, "http://example.com/"_s, true},
        {"http://example.com:80/path"_s, "http://example.com/"_s, false},
        {"http://example.com:80/path#hash"_s, "http://example.com/path"_s, true},
        {"http://example.com:80/path?query"_s, "http://example.com/path"_s, false},
        {"http://example.com:80/path?query#hash"_s, "http://example.com/path"_s, false},
        {"http://example.com:80/otherpath"_s, "http://example.com/path"_s, false},
        {"http://not-example.com:80/"_s, "http://example.com/"_s, false},
        {"http://example.com:81/"_s, "http://example.com/"_s, false},
        {"http://example.com:81/#hash"_s, "http://example.com:81/"_s, true},
        {"http://example.com:81/path"_s, "http://example.com:81"_s, false},
        {"http://example.com:81/path#hash"_s, "http://example.com:81/path"_s, true},
        {"http://example.com:81/path?query"_s, "http://example.com:81/path"_s, false},
        {"http://example.com:81/path?query#hash"_s, "http://example.com:81/path"_s, false},
        {"http://example.com:81/otherpath"_s, "http://example.com:81/path"_s, false},
        {"file:///path/to/file.html"_s, "file:///path/to/file.html"_s, true},
        {"file:///path/to/file.html#hash"_s, "file:///path/to/file.html"_s, true},
        {"file:///path/to/file.html?query"_s, "file:///path/to/file.html"_s, false},
        {"file:///path/to/file.html?query#hash"_s, "file:///path/to/file.html"_s, false},
        {"file:///path/to/other_file.html"_s, "file:///path/to/file.html"_s, false},
        {"file:///path/to/other/file.html"_s, "file:///path/to/file.html"_s, false},
        {"data:text/plain;charset=utf-8;base64,76O/76O/76O/"_s, "data:text/plain;charset=utf-8;base64,760/760/760/"_s, false},
        {"http://example.com"_s, "file://example.com"_s, false},
        {"http://example.com/#hash"_s, "file://example.com"_s, false},
        {"http://example.com/?query"_s, "file://example.com/"_s, false},
        {"http://example.com/?query#hash"_s, "file://example.com/"_s, false},
    };

    for (const auto& test : cases) {
        URL url1 = createURL(test.url1);
        URL url2 = createURL(test.url2);
        EXPECT_EQ(test.expected, equalIgnoringFragmentIdentifier(url1, url2))
            << "Test failed for " << test.url1.characters() << " vs. " << test.url2.characters();
    }
}

TEST_F(WTF_URL, ProtocolIsInHTTPFamily)
{
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily({ }));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily(""_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("a"_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("ab"_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abc"_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abcd"_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abcde"_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abcdef"_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abcdefg"_s));
    EXPECT_TRUE(WTF::protocolIsInHTTPFamily("http:"_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("http"_s));
    EXPECT_TRUE(WTF::protocolIsInHTTPFamily("https:"_s));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("https"_s));
    EXPECT_TRUE(WTF::protocolIsInHTTPFamily("https://!@#$%^&*()"_s));
}

TEST_F(WTF_URL, HostIsIPAddress)
{
    EXPECT_FALSE(URL::hostIsIPAddress({ }));
    EXPECT_FALSE(URL::hostIsIPAddress(""_s));
    EXPECT_FALSE(URL::hostIsIPAddress("localhost"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("127.localhost"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("localhost.127"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("127.0.0"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("127.0 .0.1"_s));
    EXPECT_FALSE(URL::hostIsIPAddress(" 127.0.0.1"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("127..0.0.1"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("127.0.0."_s));
    EXPECT_FALSE(URL::hostIsIPAddress("256.0.0.1"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("012x:4567:89AB:cdef:3210:7654:ba98:FeDc"_s));
#if !PLATFORM(COCOA)
    // FIXME: This fails in Mac.
    EXPECT_FALSE(URL::hostIsIPAddress("127.0.0.01"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("00123:4567:89AB:cdef:3210:7654:ba98:FeDc"_s));
#endif
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:123.45.67.89"_s));
    EXPECT_FALSE(URL::hostIsIPAddress(":::"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("0123::89AB:cdef:3210:7654::FeDc"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98:"_s));
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98:FeDc:"_s));
    EXPECT_FALSE(URL::hostIsIPAddress(":4567:89AB:cdef:3210:7654:ba98:FeDc"_s));
    EXPECT_FALSE(URL::hostIsIPAddress(":0123:4567:89AB:cdef:3210:7654:ba98:FeDc"_s));

    EXPECT_TRUE(URL::hostIsIPAddress("127.0.0.1"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("255.1.10.100"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("0.0.0.0"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("::1"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("::"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98:FeDc"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98::"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("::4567:89AB:cdef:3210:7654:ba98:FeDc"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:123.45.67.89"_s));
    EXPECT_TRUE(URL::hostIsIPAddress("::123.45.67.89"_s));
}

TEST_F(WTF_URL, HostIsMatchingDomain)
{
    URL url = createURL("http://www.webkit.org"_s);

    EXPECT_TRUE(url.isMatchingDomain(String { }));
    EXPECT_TRUE(url.isMatchingDomain(emptyString()));
    EXPECT_TRUE(url.isMatchingDomain("org"_s));
    EXPECT_TRUE(url.isMatchingDomain("webkit.org"_s));
    EXPECT_TRUE(url.isMatchingDomain("www.webkit.org"_s));

    EXPECT_FALSE(url.isMatchingDomain("rg"_s));
    EXPECT_FALSE(url.isMatchingDomain(".org"_s));
    EXPECT_FALSE(url.isMatchingDomain("ww.webkit.org"_s));
    EXPECT_FALSE(url.isMatchingDomain("http://www.webkit.org"_s));

    url = createURL("file:///www.webkit.org"_s);

    EXPECT_TRUE(url.isMatchingDomain(String { }));
    EXPECT_TRUE(url.isMatchingDomain(emptyString()));
    EXPECT_FALSE(url.isMatchingDomain("org"_s));
    EXPECT_FALSE(url.isMatchingDomain("webkit.org"_s));
    EXPECT_FALSE(url.isMatchingDomain("www.webkit.org"_s));

    URL emptyURL;
    EXPECT_FALSE(emptyURL.isMatchingDomain(String { }));
    EXPECT_FALSE(emptyURL.isMatchingDomain(emptyString()));
}

TEST_F(WTF_URL, PrintStream)
{
    String urlString("http://www.webkit.org/"_s);
    URL url(urlString);
    StringPrintStream out;
    out.print(url);
    EXPECT_EQ(out.toString(), urlString);
}

TEST_F(WTF_URL, URLDifferingQueryParameters)
{
    URL url1 = createURL("www.webkit.org/?"_s);
    URL url2 = createURL("http://www.webkit.org/?key1=val1"_s);
    Vector<KeyValuePair<String, String>> testVector12 { { "key1"_s, "val1"_s } };
    EXPECT_EQ(differingQueryParameters(url1, url2), testVector12);
    
    URL url33 = createURL("http://www.webkit.org/?key1=val1"_s);
    URL url34 = createURL("webkit.org/?"_s);
    EXPECT_EQ(differingQueryParameters(url33, url34), testVector12);
    
    URL url35 = createURL(".org/path/?key1=val1"_s);
    URL url36 = createURL("/path/?key1=val1"_s);
    Vector<KeyValuePair<String, String>> testVector { };
    EXPECT_EQ(differingQueryParameters(url35, url36), testVector);
    
    URL url = createURL("http://www.webkit.org/?key1=val1"_s);
    EXPECT_EQ(differingQueryParameters(url, url), testVector);
    
    URL url9 = createURL("http://www.webkit.org/?key1=val1"_s);
    URL url10 = createURL("http://www.webkit.org/?key1=val1"_s);
    EXPECT_EQ(differingQueryParameters(url9, url10) , testVector);
    
    URL url7 = createURL("http://www.webkit.org/?"_s);
    URL url8 = createURL("http://www.webkit.org/?"_s);
    EXPECT_EQ(differingQueryParameters(url7, url8), testVector);
    
    URL url3 = createURL("http://www.webkit.org/?"_s);
    URL url4 = createURL("http://www.webkit.org/?key1=val1"_s);
    Vector<KeyValuePair<String, String>> testVector34 { { "key1"_s, "val1"_s } };
    EXPECT_EQ(differingQueryParameters(url3, url4), testVector34);
    
    URL url5 = createURL("http://www.webkit.org/?key1=val1"_s);
    URL url6 = createURL("http://www.webkit.org/?"_s);
    Vector<KeyValuePair<String, String>> testVector56 { { "key1"_s, "val1"_s } };
    EXPECT_EQ(differingQueryParameters(url5, url6), testVector56);
    
    URL url13 = createURL("http://www.webkit.org/?key1=val1&key2=val2"_s);
    URL url14 = createURL("http://www.webkit.org/?key1=val1&key1=val1"_s);
    Vector<KeyValuePair<String, String>> testVector1314 { { "key1"_s, "val1"_s }, { "key2"_s, "val2"_s } };
    EXPECT_EQ(differingQueryParameters(url13, url14), testVector1314);
    
    URL url15 = createURL("http://www.webkit.org/?key1=val1"_s);
    URL url16 = createURL("http://www.webkit.org/?key2=val2"_s);
    Vector<KeyValuePair<String, String>> testVector1516 { { "key1"_s, "val1"_s }, { "key2"_s, "val2"_s } };
    EXPECT_EQ(differingQueryParameters(url15, url16), testVector1516);
    
    URL url11 = createURL("http://www.webkit.org/?key2=val2&key1=val1"_s);
    URL url12 = createURL("http://www.webkit.org/?key3=val3&key1=val1"_s);
    Vector<KeyValuePair<String, String>> testVector1112 { { "key2"_s, "val2"_s }, { "key3"_s, "val3"_s } };
    EXPECT_EQ(differingQueryParameters(url11, url12), testVector1112);
    
    URL url17 = createURL("http://www.webkit.org/?key1=val1&key2=val2"_s);
    URL url18 = createURL("http://www.webkit.org/?key1&key3=val3"_s);
    Vector<KeyValuePair<String, String>> testVector1718 { {"key1"_s, ""_s }, { "key1"_s, "val1"_s }, { "key2"_s, "val2"_s }, { "key3"_s, "val3"_s } };
    EXPECT_EQ(differingQueryParameters(url17, url18), testVector1718);
    
    URL url19 = createURL("http://www.webkit.org/?key2=val2&key1=val1&key2=val2"_s);
    URL url20 = createURL("http://www.webkit.org/?key3=val3&key1"_s);
    Vector<KeyValuePair<String, String>> testVector1920 { { "key1"_s, ""_s }, { "key1"_s, "val1"_s }, { "key2"_s, "val2"_s }, { "key2"_s, "val2"_s }, { "key3"_s, "val3"_s } };
    EXPECT_EQ(differingQueryParameters(url19, url20), testVector1920);
    
    URL url21 = createURL("http://www.webkit.org/??"_s);
    URL url22 = createURL("http://www.webkit.org/?/?test=test"_s);
    Vector<KeyValuePair<String, String>> testVector2122 { { "/?test"_s, "test"_s }, { "?"_s, ""_s } };
    EXPECT_EQ(differingQueryParameters(url21, url22), testVector2122);
    
    URL url23 = createURL("http://www.webkit.org/?=test"_s);
    URL url24 = createURL("http://www.webkit.org/?=="_s);
    Vector<KeyValuePair<String, String>> testVector2324 { { ""_s, "="_s }, { ""_s, "test"_s } };
    EXPECT_EQ(differingQueryParameters(url23, url24), testVector2324);
    
    URL url27 = createURL("http://www.webkit.org??"_s);
    URL url28 = createURL("http://www.webkit.org?/?test=test"_s);
    Vector<KeyValuePair<String, String>> testVector2728 { { "/?test"_s, "test"_s }, { "?"_s, ""_s } };
    EXPECT_EQ(differingQueryParameters(url27, url28), testVector2728);
    
    URL url29 = createURL("http://www.webkit.org?=test"_s);
    URL url30 = createURL("http://www.webkit.org?=="_s);
    Vector<KeyValuePair<String, String>> testVector2930 { { ""_s, "="_s }, { ""_s, "test"_s } };
    EXPECT_EQ(differingQueryParameters(url29, url30), testVector2930);
    
    URL url31 = createURL("http://www.webkit.org?=?"_s);
    URL url32 = createURL("http://www.webkit.org=?"_s);
    Vector<KeyValuePair<String, String>> testVector3132 { { ""_s, "?"_s } };
    EXPECT_EQ(differingQueryParameters(url31, url32), testVector3132);
    
    URL url25 = createURL("http://www.webkit.org/?=?"_s);
    URL url26 = createURL("http://www.webkit.org/=?"_s);
    Vector<KeyValuePair<String, String>> testVector2526 { { ""_s, "?"_s } };
    EXPECT_EQ(differingQueryParameters(url25, url26), testVector2526);
}

TEST_F(WTF_URL, URLIsEqualIgnoringQueryAndFragments)
{
    URL url1 = createURL("www.webkit.org/?"_s);
    URL url2 = createURL("http://www.webkit.org/?key1=val1"_s);
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url1, url2));
    
    URL url13 = createURL("webkit.org/?=?"_s);
    URL url14 = createURL("webkit.org/?=?"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url13, url14));
    
    URL url15 = createURL("http://www.webkit.org/"_s);
    URL url16 = createURL("webkit.org/"_s);
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url15, url16));
    
    URL url17 = createURL("webkit.org/?=?"_s);
    URL url18 = createURL("kit.org/?=?"_s);
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url17, url18));

    URL url = createURL("http://www.webkit.org/?key1=val1"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url, url));
    
    URL url3 = createURL("http://www.webkit.org/?"_s);
    URL url4 = createURL("http://www.webkit.org/?key1=val1"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url3, url4));
    
    URL url7 = createURL("http://www.webkit.org?"_s);
    URL url8 = createURL("http://www.webkit.org/?key1=val1"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url7, url8));
    
    URL url5 = createURL("http://www.example.org/path?"_s);
    URL url6 = createURL("http://www.webkit.org/?key1=val1"_s);
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url5, url6));
    
    URL url9 = createURL("http://example.com?a=b"_s);
    URL url10 = createURL("http://example.com/?a=b"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url9, url10));
    
    URL url11 = createURL("http://www.webkit.org/?"_s);
    URL url12 = createURL("http://www.webkit.org?"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url11, url12));
    
    URL url21 = createURL("http://www.webkit.org/??"_s);
    URL url22 = createURL("http://www.webkit.org/?/?"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url21, url22));
    
    URL url23 = createURL("http://www.webkit.org/?=&"_s);
    URL url24 = createURL("http://www.webkit.org/?==&"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url23, url24));
    
    URL url27 = createURL("http://www.webkit.org?&?"_s);
    URL url28 = createURL("http://www.webkit.org??&"_s);
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url27, url28));
    
    URL url31 = createURL("http://www.webkit.org?=?"_s);
    URL url32 = createURL("http://www.webkit.org=?"_s);
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url31, url32));
    
    URL url25 = createURL("http://www.webkit.org/?=?"_s);
    URL url26 = createURL("http://www.webkit.org/=?"_s);
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url25, url26));
}

TEST_F(WTF_URL, URLRemoveQueryParameters)
{
    const auto url = createURL("http://www.webkit.org/?key=val"_s);
    auto url1 = createURL("http://www.webkit.org/?key=val&key1=val1"_s);
    auto url2 = createURL("http://www.webkit.org/?"_s);
    auto url3 = createURL("http://www.webkit.org/?key=val#fragment"_s);
    auto url4 = createURL("http://www.webkit.org/?key=val&key=val#fragment"_s);
    auto url5 = createURL("http://www.webkit.org/?key&key=#fragment"_s);
    auto url6 = createURL("http://www.webkit.org/#fragment"_s);
    auto url7 = createURL("http://www.webkit.org/?key=val#fragment"_s);
    const auto url8 = createURL("http://www.webkit.org/"_s);
    const auto url9 = createURL("http://www.webkit.org/#fragment"_s);
    const auto url10 = createURL("http://www.webkit.org/?key=val#fragment"_s);
    auto url11 = createURL("http://www.webkit.org/?key=val&key1=val1#fragment"_s);
    auto url12 = createURL("http://www.webkit.org/?key=val&key1=val1#fragment"_s);
    auto url13 = createURL("http://www.webkit.org"_s);
    auto url14 = createURL("http://www.webkit.org/?u+v=x+%20y&key1=foo"_s);
    auto url15 = createURL("http://www.webkit.org/?u+v=x+%20y"_s);

    HashSet<String> keyRemovalSet1 { "key"_s };
    HashSet<String> keyRemovalSet2 { "key1"_s };
    HashSet<String> keyRemovalSet3 { "key2"_s };
    HashSet<String> keyRemovalSet4 { "key"_s, "key1"_s };

    removeQueryParameters(url1, keyRemovalSet2);
    EXPECT_EQ(url1.string(), url.string());

    const auto originalURL2 = url2;
    removeQueryParameters(url2, keyRemovalSet1);
    EXPECT_EQ(url2.string(), originalURL2.string());

    removeQueryParameters(url3, keyRemovalSet1);
    EXPECT_EQ(url3.string(), url6.string());

    removeQueryParameters(url4, keyRemovalSet1);
    EXPECT_EQ(url4.string(), url6.string());

    removeQueryParameters(url5, keyRemovalSet1);
    EXPECT_EQ(url5.string(), url6.string());

    removeQueryParameters(url6, keyRemovalSet1);
    EXPECT_EQ(url6.string(), url9.string());

    removeQueryParameters(url7, keyRemovalSet2);
    EXPECT_EQ(url7.string(), url10.string());

    removeQueryParameters(url11, keyRemovalSet3);
    EXPECT_EQ(url11.string(), url12.string());

    removeQueryParameters(url12, keyRemovalSet4);
    EXPECT_EQ(url12.string(), url9.string());

    removeQueryParameters(url13, keyRemovalSet1);
    EXPECT_EQ(url13.string(), url8.string());

    removeQueryParameters(url14, keyRemovalSet4);
    EXPECT_EQ(url14.string(), url15.string());
}

TEST_F(WTF_URL, IsolatedCopy)
{
    URL url1 { "http://www.apple.com"_str };
    auto* originalStringImpl = url1.string().impl();
    URL url1Copy = url1.isolatedCopy();
    EXPECT_EQ(url1Copy, url1);
    EXPECT_NE(url1Copy.string().impl(), originalStringImpl); // Should have done a deep copy of the String.

    // Tests optimization for URL::isolatedCopy() on a r-value reference.
    URL url2 { "https://www.webkit.org"_str };
    originalStringImpl = url2.string().impl();
    URL url2Copy = WTFMove(url2).isolatedCopy();
    EXPECT_EQ(url2Copy, URL { "https://www.webkit.org"_str });
    EXPECT_EQ(url2Copy.string().impl(), originalStringImpl); // Should have adopted the StringImpl of url2.
}

} // namespace TestWebKitAPI
