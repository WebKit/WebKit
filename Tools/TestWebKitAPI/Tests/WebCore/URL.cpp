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
#include <WebCore/URL.h>
#include <WebCore/URLParser.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace TestWebKitAPI {

class URLTest : public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }
};

TEST_F(URLTest, URLConstructorDefault)
{
    URL kurl;

    EXPECT_TRUE(kurl.isEmpty());
    EXPECT_TRUE(kurl.isNull());
    EXPECT_FALSE(kurl.isValid());
}

TEST_F(URLTest, URLConstructorConstChar)
{
    URL kurl(ParsedURLString, "http://username:password@www.example.com:8080/index.html?var=val#fragment");

    EXPECT_FALSE(kurl.isEmpty());
    EXPECT_FALSE(kurl.isNull());
    EXPECT_TRUE(kurl.isValid());

    EXPECT_EQ(kurl.protocol() == "http", true);
    EXPECT_EQ(String("www.example.com"), kurl.host());
    EXPECT_TRUE(!!kurl.port());
    EXPECT_EQ(8080, kurl.port().value());
    EXPECT_EQ(String("username"), kurl.user());
    EXPECT_EQ(String("password"), kurl.pass());
    EXPECT_EQ(String("/index.html"), kurl.path());
    EXPECT_EQ(String("index.html"), kurl.lastPathComponent());
    EXPECT_EQ(String("var=val"), kurl.query());
    EXPECT_TRUE(kurl.hasFragmentIdentifier());
    EXPECT_EQ(String("fragment"), kurl.fragmentIdentifier());
}

static URL createURL(const char* urlAsString)
{
    URLParser parser(urlAsString);
    return parser.result();
};

TEST_F(URLTest, URLProtocolHostAndPort)
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

TEST_F(URLTest, URLDataURIStringSharing)
{
    URL baseURL(ParsedURLString, "http://www.webkit.org/");
    String threeApples = "data:text/plain;charset=utf-8;base64,76O/76O/76O/";

    URL url(baseURL, threeApples);
    EXPECT_EQ(threeApples.impl(), url.string().impl());
}

TEST_F(URLTest, URLSetQuery)
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

TEST_F(URLTest, URLSetFragmentIdentifier)
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

TEST_F(URLTest, URLRemoveQueryAndFragmentIdentifier)
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

TEST_F(URLTest, ProtocolIsInHTTPFamily)
{
    EXPECT_FALSE(protocolIsInHTTPFamily({}));
    EXPECT_FALSE(protocolIsInHTTPFamily(""));
    EXPECT_FALSE(protocolIsInHTTPFamily("a"));
    EXPECT_FALSE(protocolIsInHTTPFamily("ab"));
    EXPECT_FALSE(protocolIsInHTTPFamily("abc"));
    EXPECT_FALSE(protocolIsInHTTPFamily("abcd"));
    EXPECT_FALSE(protocolIsInHTTPFamily("abcde"));
    EXPECT_FALSE(protocolIsInHTTPFamily("abcdef"));
    EXPECT_FALSE(protocolIsInHTTPFamily("abcdefg"));
    EXPECT_TRUE(protocolIsInHTTPFamily("http:"));
    EXPECT_FALSE(protocolIsInHTTPFamily("http"));
    EXPECT_TRUE(protocolIsInHTTPFamily("https:"));
    EXPECT_FALSE(protocolIsInHTTPFamily("https"));
    EXPECT_TRUE(protocolIsInHTTPFamily("https://!@#$%^&*()"));
}

TEST_F(URLTest, HostIsIPAddress)
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
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98"));
    EXPECT_FALSE(URL::hostIsIPAddress("012x:4567:89AB:cdef:3210:7654:ba98:FeDc"));
    EXPECT_FALSE(URL::hostIsIPAddress("00123:4567:89AB:cdef:3210:7654:ba98:FeDc"));
    EXPECT_FALSE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:123.45.67.89"));
    EXPECT_FALSE(URL::hostIsIPAddress(":::"));

    EXPECT_TRUE(URL::hostIsIPAddress("127.0.0.1"));
    EXPECT_TRUE(URL::hostIsIPAddress("123.45.67.89"));
    EXPECT_TRUE(URL::hostIsIPAddress("0.0.0.0"));
    EXPECT_TRUE(URL::hostIsIPAddress("::1"));
    EXPECT_TRUE(URL::hostIsIPAddress("::"));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98:FeDc"));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:ba98::"));
    EXPECT_TRUE(URL::hostIsIPAddress("::4567:89AB:cdef:3210:7654:ba98:FeDc"));
    EXPECT_TRUE(URL::hostIsIPAddress("0123:4567:89AB:cdef:3210:7654:123.45.67.89"));
    EXPECT_TRUE(URL::hostIsIPAddress("::123.45.67.89"));
}

} // namespace TestWebKitAPI
