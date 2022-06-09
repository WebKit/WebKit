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
    URL kurl({ }, "http://username:password@www.example.com:8080/index.html?var=val#fragment");

    EXPECT_FALSE(kurl.isEmpty());
    EXPECT_FALSE(kurl.isNull());
    EXPECT_TRUE(kurl.isValid());

    EXPECT_EQ(kurl.protocol() == "http", true);
    EXPECT_EQ(String("www.example.com"), kurl.host().toString());
    EXPECT_TRUE(!!kurl.port());
    EXPECT_EQ(8080, kurl.port().value());
    EXPECT_EQ(String("username"), kurl.user());
    EXPECT_EQ(String("password"), kurl.password());
    EXPECT_EQ(String("/index.html"), kurl.path());
    EXPECT_EQ(String("index.html"), kurl.lastPathComponent());
    EXPECT_EQ(String("var=val"), kurl.query());
    EXPECT_TRUE(kurl.hasFragmentIdentifier());
    EXPECT_EQ(String("fragment"), kurl.fragmentIdentifier());
}

static URL createURL(const char* urlAsString)
{
    return URL({ }, urlAsString);
};

TEST_F(WTF_URL, URLProtocolHostAndPort)
{
    auto url = createURL("http://username:password@www.example.com:8080/index.html?var=val#fragment");
    EXPECT_EQ(String("http://www.example.com:8080"), url.protocolHostAndPort());

    url = createURL("http://username:@www.example.com:8080/index.html?var=val#fragment");
    EXPECT_EQ(String("http://www.example.com:8080"), url.protocolHostAndPort());

    url = createURL("http://:password@www.example.com:8080/index.html?var=val#fragment");
    EXPECT_EQ(String("http://www.example.com:8080"), url.protocolHostAndPort());

    url = createURL("http://username@www.example.com:8080/index.html?var=val#fragment");
    EXPECT_EQ(String("http://www.example.com:8080"), url.protocolHostAndPort());

    url = createURL("http://www.example.com:8080/index.html?var=val#fragment");
    EXPECT_EQ(String("http://www.example.com:8080"), url.protocolHostAndPort());

    url = createURL("http://www.example.com:/index.html?var=val#fragment");
    EXPECT_EQ(String("http://www.example.com"), url.protocolHostAndPort());

    url = createURL("http://www.example.com/index.html?var=val#fragment");
    EXPECT_EQ(String("http://www.example.com"), url.protocolHostAndPort());

    url = createURL("file:///a/b/c");
    EXPECT_EQ(String("file://"), url.protocolHostAndPort());

    url = createURL("file:///a/b");
    EXPECT_EQ(String("file://"), url.protocolHostAndPort());

    url = createURL("file:///a");
    EXPECT_EQ(String("file://"), url.protocolHostAndPort());

    url = createURL("file:///a");
    EXPECT_EQ(String("file://"), url.protocolHostAndPort());

    url = createURL("asdf://username:password@www.example.com:8080/index.html?var=val#fragment");
    EXPECT_EQ(String("asdf://www.example.com:8080"), url.protocolHostAndPort());

    url = createURL("asdf:///a/b/c");
    EXPECT_EQ(String("asdf://"), url.protocolHostAndPort());
}

TEST_F(WTF_URL, URLDataURIStringSharing)
{
    URL baseURL({ }, "http://www.webkit.org/");
    String threeApples = "data:text/plain;charset=utf-8;base64,76O/76O/76O/";

    URL url(baseURL, threeApples);
    EXPECT_EQ(threeApples.impl(), url.string().impl());
}

TEST_F(WTF_URL, URLSetQuery)
{
    URL url = createURL("http://www.webkit.org/?test");
    URL url1 = createURL("http://www.webkit.org/");
    URL url2 = createURL("http://www.webkit.org/?");
    URL url3 = createURL("http://www.webkit.org/?test");
    URL url4 = createURL("http://www.webkit.org/?test1");

    url1.setQuery("test");
    url2.setQuery("test");
    url3.setQuery("test");
    url4.setQuery("test");

    EXPECT_EQ(url.string(), url1.string());
    EXPECT_EQ(url.string(), url2.string());
    EXPECT_EQ(url.string(), url3.string());
    EXPECT_EQ(url.string(), url4.string());

    URL urlWithFragmentIdentifier = createURL("http://www.webkit.org/?test%C3%83%C2%A5#newFragment");
    URL urlWithFragmentIdentifier1 = createURL("http://www.webkit.org/#newFragment");
    URL urlWithFragmentIdentifier2 = createURL("http://www.webkit.org/?#newFragment");
    URL urlWithFragmentIdentifier3 = createURL("http://www.webkit.org/?test1#newFragment");

    urlWithFragmentIdentifier1.setQuery("test\xc3\xa5");
    urlWithFragmentIdentifier2.setQuery("test\xc3\xa5");
    urlWithFragmentIdentifier3.setQuery("test\xc3\xa5");

    EXPECT_EQ(urlWithFragmentIdentifier.string(), urlWithFragmentIdentifier1.string());
    EXPECT_EQ(urlWithFragmentIdentifier.string(), urlWithFragmentIdentifier2.string());
    EXPECT_EQ(urlWithFragmentIdentifier.string(), urlWithFragmentIdentifier3.string());
}

TEST_F(WTF_URL, URLSetFragmentIdentifier)
{
    URL url = createURL("http://www.webkit.org/#newFragment%C3%83%C2%A5");
    URL url1 = createURL("http://www.webkit.org/");
    URL url2 = createURL("http://www.webkit.org/#test2");
    URL url3 = createURL("http://www.webkit.org/#");

    url1.setFragmentIdentifier("newFragment\xc3\xa5");
    url2.setFragmentIdentifier("newFragment\xc3\xa5");
    url3.setFragmentIdentifier("newFragment\xc3\xa5");

    EXPECT_EQ(url.string(), url1.string());
    EXPECT_EQ(url.string(), url2.string());
    EXPECT_EQ(url.string(), url3.string());

    URL urlWithQuery = createURL("http://www.webkit.org/?test1#newFragment");
    URL urlWithQuery1 = createURL("http://www.webkit.org/?test1");
    URL urlWithQuery2 = createURL("http://www.webkit.org/?test1#");
    URL urlWithQuery3 = createURL("http://www.webkit.org/?test1#test2");

    urlWithQuery1.setFragmentIdentifier("newFragment");
    urlWithQuery2.setFragmentIdentifier("newFragment");
    urlWithQuery3.setFragmentIdentifier("newFragment");

    EXPECT_EQ(urlWithQuery.string(), urlWithQuery1.string());
    EXPECT_EQ(urlWithQuery.string(), urlWithQuery2.string());
    EXPECT_EQ(urlWithQuery.string(), urlWithQuery3.string());
}

TEST_F(WTF_URL, URLRemoveQueryAndFragmentIdentifier)
{
    URL url = createURL("http://www.webkit.org/");
    URL url1 = createURL("http://www.webkit.org/?");
    URL url2 = createURL("http://www.webkit.org/?test1");
    URL url3 = createURL("http://www.webkit.org/?test1#test2");
    URL url4 = createURL("http://www.webkit.org/#test2");
    URL url5 = createURL("http://www.webkit.org/#");

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
        const char* url1;
        const char* url2;
        bool expected;
    } cases[] = {
        {"http://example.com/", "http://example.com/", true},
        {"http://example.com/#hash", "http://example.com/", true},
        {"http://example.com/path", "http://example.com/", false},
        {"http://example.com/path", "http://example.com/path", true},
        {"http://example.com/path#hash", "http://example.com/path", true},
        {"http://example.com/path?query", "http://example.com/path", false},
        {"http://example.com/path?query#hash", "http://example.com/path", false},
        {"http://example.com/otherpath", "http://example.com/path", false},
        {"http://example.com:80/", "http://example.com/", true},
        {"http://example.com:80/#hash", "http://example.com/", true},
        {"http://example.com:80/path", "http://example.com/", false},
        {"http://example.com:80/path#hash", "http://example.com/path", true},
        {"http://example.com:80/path?query", "http://example.com/path", false},
        {"http://example.com:80/path?query#hash", "http://example.com/path", false},
        {"http://example.com:80/otherpath", "http://example.com/path", false},
        {"http://not-example.com:80/", "http://example.com/", false},
        {"http://example.com:81/", "http://example.com/", false},
        {"http://example.com:81/#hash", "http://example.com:81/", true},
        {"http://example.com:81/path", "http://example.com:81", false},
        {"http://example.com:81/path#hash", "http://example.com:81/path", true},
        {"http://example.com:81/path?query", "http://example.com:81/path", false},
        {"http://example.com:81/path?query#hash", "http://example.com:81/path", false},
        {"http://example.com:81/otherpath", "http://example.com:81/path", false},
        {"file:///path/to/file.html", "file:///path/to/file.html", true},
        {"file:///path/to/file.html#hash", "file:///path/to/file.html", true},
        {"file:///path/to/file.html?query", "file:///path/to/file.html", false},
        {"file:///path/to/file.html?query#hash", "file:///path/to/file.html", false},
        {"file:///path/to/other_file.html", "file:///path/to/file.html", false},
        {"file:///path/to/other/file.html", "file:///path/to/file.html", false},
        {"data:text/plain;charset=utf-8;base64,76O/76O/76O/", "data:text/plain;charset=utf-8;base64,760/760/760/", false},
        {"http://example.com", "file://example.com", false},
        {"http://example.com/#hash", "file://example.com", false},
        {"http://example.com/?query", "file://example.com/", false},
        {"http://example.com/?query#hash", "file://example.com/", false},
    };

    for (const auto& test : cases) {
        URL url1 = createURL(test.url1);
        URL url2 = createURL(test.url2);
        EXPECT_EQ(test.expected, equalIgnoringFragmentIdentifier(url1, url2))
            << "Test failed for " << test.url1 << " vs. " << test.url2;
    }
}

TEST_F(WTF_URL, ProtocolIsInHTTPFamily)
{
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily({ }));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily(""));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("a"));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("ab"));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abc"));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abcd"));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abcde"));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abcdef"));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("abcdefg"));
    EXPECT_TRUE(WTF::protocolIsInHTTPFamily("http:"));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("http"));
    EXPECT_TRUE(WTF::protocolIsInHTTPFamily("https:"));
    EXPECT_FALSE(WTF::protocolIsInHTTPFamily("https"));
    EXPECT_TRUE(WTF::protocolIsInHTTPFamily("https://!@#$%^&*()"));
}

TEST_F(WTF_URL, HostIsIPAddress)
{
    EXPECT_FALSE(URL::hostIsIPAddress({ }));
    EXPECT_FALSE(URL::hostIsIPAddress(""));
    EXPECT_FALSE(URL::hostIsIPAddress("localhost"));
    EXPECT_FALSE(URL::hostIsIPAddress("127.localhost"));
    EXPECT_FALSE(URL::hostIsIPAddress("localhost.127"));
    EXPECT_FALSE(URL::hostIsIPAddress("127.0.0"));
    EXPECT_FALSE(URL::hostIsIPAddress("127.0 .0.1"));
    EXPECT_FALSE(URL::hostIsIPAddress(" 127.0.0.1"));
    EXPECT_FALSE(URL::hostIsIPAddress("127..0.0.1"));
    EXPECT_FALSE(URL::hostIsIPAddress("127.0.0."));
    EXPECT_FALSE(URL::hostIsIPAddress("256.0.0.1"));
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98"));
    EXPECT_FALSE(URL::hostIsIPAddress("012x:4567:89AB:cdef:3210:7654:ba98:FeDc"));
#if !PLATFORM(COCOA)
    // FIXME: This fails in Mac.
    EXPECT_FALSE(URL::hostIsIPAddress("127.0.0.01"));
    EXPECT_FALSE(URL::hostIsIPAddress("00123:4567:89AB:cdef:3210:7654:ba98:FeDc"));
#endif
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:123.45.67.89"));
    EXPECT_FALSE(URL::hostIsIPAddress(":::"));
    EXPECT_FALSE(URL::hostIsIPAddress("0123::89AB:cdef:3210:7654::FeDc"));
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98:"));
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98:FeDc:"));
    EXPECT_FALSE(URL::hostIsIPAddress(":4567:89AB:cdef:3210:7654:ba98:FeDc"));
    EXPECT_FALSE(URL::hostIsIPAddress(":0123:4567:89AB:cdef:3210:7654:ba98:FeDc"));

    EXPECT_TRUE(URL::hostIsIPAddress("127.0.0.1"));
    EXPECT_TRUE(URL::hostIsIPAddress("255.1.10.100"));
    EXPECT_TRUE(URL::hostIsIPAddress("0.0.0.0"));
    EXPECT_TRUE(URL::hostIsIPAddress("::1"));
    EXPECT_TRUE(URL::hostIsIPAddress("::"));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98:FeDc"));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98::"));
    EXPECT_TRUE(URL::hostIsIPAddress("::4567:89AB:cdef:3210:7654:ba98:FeDc"));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:123.45.67.89"));
    EXPECT_TRUE(URL::hostIsIPAddress("::123.45.67.89"));
}

TEST_F(WTF_URL, HostIsMatchingDomain)
{
    URL url = createURL("http://www.webkit.org");

    EXPECT_TRUE(url.isMatchingDomain(String { }));
    EXPECT_TRUE(url.isMatchingDomain(emptyString()));
    EXPECT_TRUE(url.isMatchingDomain("org"));
    EXPECT_TRUE(url.isMatchingDomain("webkit.org"));
    EXPECT_TRUE(url.isMatchingDomain("www.webkit.org"));

    EXPECT_FALSE(url.isMatchingDomain("rg"));
    EXPECT_FALSE(url.isMatchingDomain(".org"));
    EXPECT_FALSE(url.isMatchingDomain("ww.webkit.org"));
    EXPECT_FALSE(url.isMatchingDomain("http://www.webkit.org"));

    url = createURL("file:///www.webkit.org");

    EXPECT_TRUE(url.isMatchingDomain(String { }));
    EXPECT_TRUE(url.isMatchingDomain(emptyString()));
    EXPECT_FALSE(url.isMatchingDomain("org"));
    EXPECT_FALSE(url.isMatchingDomain("webkit.org"));
    EXPECT_FALSE(url.isMatchingDomain("www.webkit.org"));

    URL emptyURL;
    EXPECT_FALSE(emptyURL.isMatchingDomain(String { }));
    EXPECT_FALSE(emptyURL.isMatchingDomain(emptyString()));
}

TEST_F(WTF_URL, PrintStream)
{
    String urlString("http://www.webkit.org/");
    URL url({ }, urlString);
    StringPrintStream out;
    out.print(url);
    EXPECT_EQ(out.toString(), urlString);
}

TEST_F(WTF_URL, URLDifferingQueryParameters)
{
    URL url1 = createURL("www.webkit.org/?");
    URL url2 = createURL("http://www.webkit.org/?key1=val1");
    Vector<KeyValuePair<String, String>> testVector12 {{"key1", "val1"}};
    EXPECT_EQ(differingQueryParameters(url1, url2), testVector12);
    
    URL url33 = createURL("http://www.webkit.org/?key1=val1");
    URL url34 = createURL("webkit.org/?");
    EXPECT_EQ(differingQueryParameters(url33, url34), testVector12);
    
    URL url35 = createURL(".org/path/?key1=val1");
    URL url36 = createURL("/path/?key1=val1");
    Vector<KeyValuePair<String, String>> testVector { };
    EXPECT_EQ(differingQueryParameters(url35, url36), testVector);
    
    URL url = createURL("http://www.webkit.org/?key1=val1");
    EXPECT_EQ(differingQueryParameters(url, url), testVector);
    
    URL url9 = createURL("http://www.webkit.org/?key1=val1");
    URL url10 = createURL("http://www.webkit.org/?key1=val1");
    EXPECT_EQ(differingQueryParameters(url9, url10) , testVector);
    
    URL url7 = createURL("http://www.webkit.org/?");
    URL url8 = createURL("http://www.webkit.org/?");
    EXPECT_EQ(differingQueryParameters(url7, url8), testVector);
    
    URL url3 = createURL("http://www.webkit.org/?");
    URL url4 = createURL("http://www.webkit.org/?key1=val1");
    Vector<KeyValuePair<String, String>> testVector34 {{"key1", "val1"}};
    EXPECT_EQ(differingQueryParameters(url3, url4), testVector34);
    
    URL url5 = createURL("http://www.webkit.org/?key1=val1");
    URL url6 = createURL("http://www.webkit.org/?");
    Vector<KeyValuePair<String, String>> testVector56 {{"key1", "val1"}};
    EXPECT_EQ(differingQueryParameters(url5, url6), testVector56);
    
    URL url13 = createURL("http://www.webkit.org/?key1=val1&key2=val2");
    URL url14 = createURL("http://www.webkit.org/?key1=val1&key1=val1");
    Vector<KeyValuePair<String, String>> testVector1314 {{"key1", "val1"}, {"key2", "val2"}};
    EXPECT_EQ(differingQueryParameters(url13, url14), testVector1314);
    
    URL url15 = createURL("http://www.webkit.org/?key1=val1");
    URL url16 = createURL("http://www.webkit.org/?key2=val2");
    Vector<KeyValuePair<String, String>> testVector1516 {{"key1", "val1"}, {"key2", "val2"}};
    EXPECT_EQ(differingQueryParameters(url15, url16), testVector1516);
    
    URL url11 = createURL("http://www.webkit.org/?key2=val2&key1=val1");
    URL url12 = createURL("http://www.webkit.org/?key3=val3&key1=val1");
    Vector<KeyValuePair<String, String>> testVector1112 {{"key2", "val2"}, {"key3", "val3"}};
    EXPECT_EQ(differingQueryParameters(url11, url12), testVector1112);
    
    URL url17 = createURL("http://www.webkit.org/?key1=val1&key2=val2");
    URL url18 = createURL("http://www.webkit.org/?key1&key3=val3");
    Vector<KeyValuePair<String, String>> testVector1718 {{"key1", ""}, {"key1", "val1"}, {"key2", "val2"}, {"key3", "val3"}};
    EXPECT_EQ(differingQueryParameters(url17, url18), testVector1718);
    
    URL url19 = createURL("http://www.webkit.org/?key2=val2&key1=val1&key2=val2");
    URL url20 = createURL("http://www.webkit.org/?key3=val3&key1");
    Vector<KeyValuePair<String, String>> testVector1920 {{"key1", ""}, {"key1", "val1"}, {"key2", "val2"}, {"key2", "val2"}, {"key3", "val3"}};
    EXPECT_EQ(differingQueryParameters(url19, url20), testVector1920);
    
    URL url21 = createURL("http://www.webkit.org/??");
    URL url22 = createURL("http://www.webkit.org/?/?test=test");
    Vector<KeyValuePair<String, String>> testVector2122 {{"/?test", "test"}, {"?", ""}};
    EXPECT_EQ(differingQueryParameters(url21, url22), testVector2122);
    
    URL url23 = createURL("http://www.webkit.org/?=test");
    URL url24 = createURL("http://www.webkit.org/?==");
    Vector<KeyValuePair<String, String>> testVector2324 {{"", "="}, {"", "test"}};
    EXPECT_EQ(differingQueryParameters(url23, url24), testVector2324);
    
    URL url27 = createURL("http://www.webkit.org??");
    URL url28 = createURL("http://www.webkit.org?/?test=test");
    Vector<KeyValuePair<String, String>> testVector2728 {{"/?test", "test"}, {"?", ""}};
    EXPECT_EQ(differingQueryParameters(url27, url28), testVector2728);
    
    URL url29 = createURL("http://www.webkit.org?=test");
    URL url30 = createURL("http://www.webkit.org?==");
    Vector<KeyValuePair<String, String>> testVector2930 {{"", "="}, {"", "test"}};
    EXPECT_EQ(differingQueryParameters(url29, url30), testVector2930);
    
    URL url31 = createURL("http://www.webkit.org?=?");
    URL url32 = createURL("http://www.webkit.org=?");
    Vector<KeyValuePair<String, String>> testVector3132 {{"", "?"}};
    EXPECT_EQ(differingQueryParameters(url31, url32), testVector3132);
    
    URL url25 = createURL("http://www.webkit.org/?=?");
    URL url26 = createURL("http://www.webkit.org/=?");
    Vector<KeyValuePair<String, String>> testVector2526 {{"", "?"}};
    EXPECT_EQ(differingQueryParameters(url25, url26), testVector2526);
}

TEST_F(WTF_URL, URLIsEqualIgnoringQueryAndFragments)
{
    URL url1 = createURL("www.webkit.org/?");
    URL url2 = createURL("http://www.webkit.org/?key1=val1");
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url1, url2));
    
    URL url13 = createURL("webkit.org/?=?");
    URL url14 = createURL("webkit.org/?=?");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url13, url14));
    
    URL url15 = createURL("http://www.webkit.org/");
    URL url16 = createURL("webkit.org/");
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url15, url16));
    
    URL url17 = createURL("webkit.org/?=?");
    URL url18 = createURL("kit.org/?=?");
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url17, url18));

    URL url = createURL("http://www.webkit.org/?key1=val1");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url, url));
    
    URL url3 = createURL("http://www.webkit.org/?");
    URL url4 = createURL("http://www.webkit.org/?key1=val1");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url3, url4));
    
    URL url7 = createURL("http://www.webkit.org?");
    URL url8 = createURL("http://www.webkit.org/?key1=val1");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url7, url8));
    
    URL url5 = createURL("http://www.example.org/path?");
    URL url6 = createURL("http://www.webkit.org/?key1=val1");
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url5, url6));
    
    URL url9 = createURL("http://example.com?a=b");
    URL url10 = createURL("http://example.com/?a=b");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url9, url10));
    
    URL url11 = createURL("http://www.webkit.org/?");
    URL url12 = createURL("http://www.webkit.org?");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url11, url12));
    
    URL url21 = createURL("http://www.webkit.org/??");
    URL url22 = createURL("http://www.webkit.org/?/?");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url21, url22));
    
    URL url23 = createURL("http://www.webkit.org/?=&");
    URL url24 = createURL("http://www.webkit.org/?==&");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url23, url24));
    
    URL url27 = createURL("http://www.webkit.org?&?");
    URL url28 = createURL("http://www.webkit.org??&");
    EXPECT_TRUE(isEqualIgnoringQueryAndFragments(url27, url28));
    
    URL url31 = createURL("http://www.webkit.org?=?");
    URL url32 = createURL("http://www.webkit.org=?");
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url31, url32));
    
    URL url25 = createURL("http://www.webkit.org/?=?");
    URL url26 = createURL("http://www.webkit.org/=?");
    EXPECT_FALSE(isEqualIgnoringQueryAndFragments(url25, url26));
}

TEST_F(WTF_URL, URLRemoveQueryParameters)
{
    URL url = createURL("http://www.webkit.org/?key=val");
    URL url1 = createURL("http://www.webkit.org/?key=val&key1=val1");
    URL url2 = createURL("http://www.webkit.org/?");
    URL url3 = createURL("http://www.webkit.org/?key=val#fragment");
    URL url4 = createURL("http://www.webkit.org/?key=val&key=val#fragment");
    URL url5 = createURL("http://www.webkit.org/?key&key=#fragment");
    URL url6 = createURL("http://www.webkit.org/#fragment");
    URL url7 = createURL("http://www.webkit.org/?key=val#fragment");
    URL url8 = createURL("http://www.webkit.org/");
    URL url9 = createURL("http://www.webkit.org/#fragment");
    URL url10 = createURL("http://www.webkit.org/?key=val#fragment");
    URL url11 = createURL("http://www.webkit.org/?key=val&key1=val1#fragment");
    URL url12 = createURL("http://www.webkit.org/?key=val&key1=val1#fragment");
    URL url13 = createURL("http://www.webkit.org");
    
    HashSet<String> keyRemovalSet1 {"key"};
    HashSet<String> keyRemovalSet2 {"key1"};
    HashSet<String> keyRemovalSet3 {"key2"};
    HashSet<String> keyRemovalSet4 {"key", "key1"};
    
    removeQueryParameters(url1, keyRemovalSet2);
    EXPECT_EQ(url1.string(), url.string());
    
    removeQueryParameters(url2, keyRemovalSet1);
    EXPECT_EQ(url2.string(), url8.string());
    
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
}

} // namespace TestWebKitAPI
