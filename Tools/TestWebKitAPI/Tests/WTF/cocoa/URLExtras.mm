/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"

#import "Test.h"
#import "WTFTestUtilities.h"
#import <wtf/URL.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

static NSData *literalAsData(const char* literal)
{
    return [NSData dataWithBytes:literal length:strlen(literal)];
}

static const char* dataAsString(NSData *data)
{
    static char buffer[1000];
    if ([data length] > sizeof(buffer) - 1)
        return "ERROR";
    if (memchr([data bytes], 0, [data length]))
        return "ERROR";
    memcpy(buffer, [data bytes], [data length]);
    buffer[[data length]] = '\0';
    return buffer;
}

static const char* originalDataAsString(NSURL *URL)
{
    return dataAsString(WTF::originalURLData(URL));
}

static const char* userVisibleString(NSURL *URL)
{
    return [WTF::userVisibleString(URL) UTF8String];
}

static NSURL *literalURL(const char* literal)
{
    return WTF::URLWithData(literalAsData(literal), nil);
}

TEST(WTF_URLExtras, URLExtras)
{
    EXPECT_STREQ("http://site.com", originalDataAsString(literalURL("http://site.com")));
    EXPECT_STREQ("http://%77ebsite.com", originalDataAsString(literalURL("http://%77ebsite.com")));

    EXPECT_STREQ("http://site.com", userVisibleString(literalURL("http://site.com")));
    EXPECT_STREQ("http://%77ebsite.com", userVisibleString(literalURL("http://%77ebsite.com")));

    EXPECT_STREQ("-.example.com", [WTF::decodeHostName(@"-.example.com") UTF8String]);
    EXPECT_STREQ("-a.example.com", [WTF::decodeHostName(@"-a.example.com") UTF8String]);
    EXPECT_STREQ("a-.example.com", [WTF::decodeHostName(@"a-.example.com") UTF8String]);
    EXPECT_STREQ("ab--cd.example.com", [WTF::decodeHostName(@"ab--cd.example.com") UTF8String]);
#if HAVE(NSURL_EMPTY_PUNYCODE_CHECK)
    EXPECT_NULL([WTF::decodeHostName(@"xn--.example.com") UTF8String]);
#else
    EXPECT_STREQ(".example.com", [WTF::decodeHostName(@"xn--.example.com") UTF8String]);
#endif
    EXPECT_STREQ("a..example.com", [WTF::decodeHostName(@"a..example.com") UTF8String]);
}
    
TEST(WTF_URLExtras, URLExtras_Spoof)
{
    Vector<ASCIILiteral> punycodedSpoofHosts = {
        "xn--cfa45g"_s, // U+0131, U+0307
        "xn--tma03b"_s, // U+0237, U+0307
        "xn--tma03bxga"_s, // U+0237, U+034F, U+034F, U+0307
        "xn--tma03bl01e"_s, // U+0237, U+200B, U+0307
        "xn--a-egb"_s, // a, U+034F
        "xn--a-qgn"_s, // a, U+200B
        "xn--a-mgn"_s, // a, U+2009
        "xn--u7f"_s, // U+1D04
        "xn--57f"_s, // U+1D0F
        "xn--i38a"_s, // U+A731
        "xn--j8f"_s, // U+1D1C
        "xn--n8f"_s, // U+1D20
        "xn--o8f"_s, // U+1D21
        "xn--p8f"_s, // U+1D22
        "xn--0na"_s, // U+0261
        "xn--cn-ded"_s, // U+054D
        "xn--ews-nfe.org"_s, // U+054D
        "xn--yotube-qkh"_s, // U+0578
        "xn--cla-7fe.edu"_s, // U+0578
        "xn--rsa94l"_s, // U+05D5 U+0307
        "xn--hdb9c"_s, // U+05D5 U+05B9
        "xn--idb7c"_s, // U+05D5 U+05BA
        "xn--pdb3b"_s, // U+05D5 U+05C1
        "xn--qdb1b"_s, // U+05D5 U+05C2
        "xn--sdb7a"_s, // U+05D5 U+05C4
        "xn--2-zic"_s, // U+0032 U+05E1
        "xn--uoa"_s, // U+027E
        "xn--fja"_s, // U+01C0
        "xn--jna"_s, // U+0250
        "xn--koa"_s, // U+0274
        "xn--spa"_s, // U+029F
        "xn--tma"_s, // U+0237
        "xn--8pa"_s, // U+02AF
        "xn--o-pdc"_s, // U+0585 'o'
        "xn--o-qdc"_s, // 'o' U+0585
        "xn--g-hdc"_s, // U+0581 'g'
        "xn--g-idc"_s, // 'g' U+0581
        "xn--o-00e"_s, // U+0BE6 'o'
        "xn--o-10e"_s, // 'o' U+0BE6
        "xn--a-53i"_s, // U+15AF 'a'
        "xn--a-63i"_s, // 'a' U+15AF
        "xn--a-g4i"_s, // U+15B4 'a'
        "xn--a-h4i"_s, // 'a' U+15B4
        "xn--a-80i"_s, // U+157C 'a'
        "xn--a-90i"_s, // 'a' U+157C
        "xn--a-0fj"_s, // U+166D 'a'
        "xn--a-1fj"_s, // 'a' U+166D
        "xn--a-2fj"_s, // U+166E 'a'
        "xn--a-3fj"_s, // 'a' U+166E
        "xn--a-rli"_s, // U+146D 'a'
        "xn--a-sli"_s, // 'a' U+146D
        "xn--a-vli"_s, // U+146F 'a'
        "xn--a-wli"_s, // 'a' U+146F
        "xn--a-1li"_s, // U+1472 'a'
        "xn--a-2li"_s, // 'a' U+1472
        "xn--a-8oi"_s, // U+14AA 'a'
        "xn--a-9oi"_s, // 'a' U+14AA
        "xn--a-v1i"_s, // U+1587 'a'
        "xn--a-w1i"_s, // 'a' U+1587
        "xn--a-f5i"_s, // U+15C5 'a'
        "xn--a-g5i"_s, // 'a' U+15C5
        "xn--a-u6i"_s, // U+15DE 'a'
        "xn--a-v6i"_s, // 'a' U+15DE
        "xn--a-h7i"_s, // U+15E9 'a'
        "xn--a-i7i"_s, // 'a' U+15E9
        "xn--a-x7i"_s, // U+15F1 'a'
        "xn--a-y7i"_s, // 'a' U+15F1
        "xn--a-37i"_s, // U+15F4 'a'
        "xn--a-47i"_s, // 'a' U+15F4
        "xn--n-twf"_s, // U+0E01 'n'
        "xn--n-uwf"_s, // 'n' U+0E01
        "xn--3hb112n"_s, // U+065B
        "xn--a-ypc062v"_s, // 'a' U+065B
    };
    for (auto& host : punycodedSpoofHosts) {
        auto url = makeString("http://", host, "/").utf8();
        EXPECT_STREQ(url.data(), userVisibleString(literalURL(url.data())));
    }
}

TEST(WTF_URLExtras, URLExtras_NotSpoofed)
{
    // Valid mixtures of Armenian and other scripts
    EXPECT_STREQ("https://en.wikipedia.org/wiki/.\u0570\u0561\u0575", userVisibleString(literalURL("https://en.wikipedia.org/wiki/.\u0570\u0561\u0575")));
    EXPECT_STREQ("https://\u0573\u0574\u0578.\u0570\u0561\u0575", userVisibleString(literalURL("https://\u0573\u0574\u0578.\u0570\u0561\u0575")));
    EXPECT_STREQ("https://\u0573-1-\u0574\u0578.\u0570\u0561\u0575", userVisibleString(literalURL("https://\u0573-1-\u0574\u0578.\u0570\u0561\u0575")));
    EXPECT_STREQ("https://2\u0573_\u0574\u0578.\u0570\u0561\u0575", userVisibleString(literalURL("https://2\u0573_\u0574\u0578.\u0570\u0561\u0575")));
    EXPECT_STREQ("https://\u0573_\u0574\u05783.\u0570\u0561\u0575", userVisibleString(literalURL("https://\u0573_\u0574\u05783.\u0570\u0561\u0575")));
    EXPECT_STREQ("https://got%D5%91\u0535\u0543.com", userVisibleString(literalURL("https://got\u0551\u0535\u0543.com")));
    EXPECT_STREQ("https://\u0551\u0535\u0543fans.net", userVisibleString(literalURL("https://\u0551\u0535\u0543fans.net")));
    EXPECT_STREQ("https://\u0551\u0535or\u0575\u0543.biz", userVisibleString(literalURL("https://\u0551\u0535or\u0575\u0543.biz")));
    EXPECT_STREQ("https://\u0551\u0535and!$^&*()-~+={}or<>,.?\u0575\u0543.biz", userVisibleString(literalURL("https://\u0551\u0535and!$^&*()-~+={}or<>,.?\u0575\u0543.biz")));
    EXPECT_STREQ("https://\u0551%67/", userVisibleString(literalURL("https://\u0551g/")));
    EXPECT_STREQ("https://\u0581%67/", userVisibleString(literalURL("https://\u0581g/")));
    EXPECT_STREQ("https://o%D5%95%2F", userVisibleString(literalURL("https://o\u0555/")));
    EXPECT_STREQ("https://o%D6%85%2F", userVisibleString(literalURL("https://o\u0585/")));

    // Tamil
    EXPECT_STREQ("https://\u0BE6\u0BE7\u0BE8\u0BE9count/", userVisibleString(literalURL("https://\u0BE6\u0BE7\u0BE8\u0BE9count/")));

    // Canadian aboriginal
    EXPECT_STREQ("https://\u146D\u1401abc/", userVisibleString(literalURL("https://\u146D\u1401abc/")));
    EXPECT_STREQ("https://\u146F\u1401abc/", userVisibleString(literalURL("https://\u146F\u1401abc/")));
    EXPECT_STREQ("https://\u1472\u1401abc/", userVisibleString(literalURL("https://\u1472\u1401abc/")));
    EXPECT_STREQ("https://\u14AA\u1401abc/", userVisibleString(literalURL("https://\u14AA\u1401abc/")));
    EXPECT_STREQ("https://\u157C\u1401abc/", userVisibleString(literalURL("https://\u157C\u1401abc/")));
    EXPECT_STREQ("https://\u1587\u1401abc/", userVisibleString(literalURL("https://\u1587\u1401abc/")));
    EXPECT_STREQ("https://\u15AF\u1401abc/", userVisibleString(literalURL("https://\u15AF\u1401abc/")));
    EXPECT_STREQ("https://\u15B4\u1401abc/", userVisibleString(literalURL("https://\u15B4\u1401abc/")));
    EXPECT_STREQ("https://\u15C5\u1401abc/", userVisibleString(literalURL("https://\u15C5\u1401abc/")));
    EXPECT_STREQ("https://\u15DE\u1401abc/", userVisibleString(literalURL("https://\u15DE\u1401abc/")));
    EXPECT_STREQ("https://\u15E9\u1401abc/", userVisibleString(literalURL("https://\u15E9\u1401abc/")));
    EXPECT_STREQ("https://\u15F1\u1401abc/", userVisibleString(literalURL("https://\u15F1\u1401abc/")));
    EXPECT_STREQ("https://\u15F4\u1401abc/", userVisibleString(literalURL("https://\u15F4\u1401abc/")));
    EXPECT_STREQ("https://\u166D\u1401abc/", userVisibleString(literalURL("https://\u166D\u1401abc/")));
    EXPECT_STREQ("https://\u166E\u1401abc/", userVisibleString(literalURL("https://\u166E\u1401abc/")));

    // Thai
    EXPECT_STREQ("https://\u0E01\u0E02abc/", userVisibleString(literalURL("https://\u0E01\u0E02abc/")));

    // Arabic
    EXPECT_STREQ("https://\u0620\u065Babc/", userVisibleString(literalURL("https://\u0620\u065Babc/")));
}

TEST(WTF_URLExtras, URLExtras_DivisionSign)
{
    // Selected the division sign as an example of a non-ASCII character that is allowed in host names, since it's a lookalike character.

    // Code path similar to the one used when typing in a URL.
    EXPECT_STREQ("http://site.xn--comothersite-kjb.org", originalDataAsString(WTF::URLWithUserTypedString(@"http://site.com\xC3\xB7othersite.org", nil)));
    EXPECT_STREQ("http://site.com\xC3\xB7othersite.org", userVisibleString(WTF::URLWithUserTypedString(@"http://site.com\xC3\xB7othersite.org", nil)));

    // Code paths similar to the ones used for URLs found in webpages or HTTP responses.
    EXPECT_STREQ("http://site.com\xC3\xB7othersite.org", originalDataAsString(literalURL("http://site.com\xC3\xB7othersite.org")));
    EXPECT_STREQ("http://site.com\xC3\xB7othersite.org", userVisibleString(literalURL("http://site.com\xC3\xB7othersite.org")));
    EXPECT_STREQ("http://site.com%C3%B7othersite.org", originalDataAsString(literalURL("http://site.com%C3%B7othersite.org")));
    EXPECT_STREQ("http://site.com\xC3\xB7othersite.org", userVisibleString(literalURL("http://site.com%C3%B7othersite.org")));

    // Separate functions that deal with just a host name on its own.
    EXPECT_STREQ("site.xn--comothersite-kjb.org", [WTF::encodeHostName(@"site.com\xC3\xB7othersite.org") UTF8String]);
    EXPECT_STREQ("site.com\xC3\xB7othersite.org", [WTF::decodeHostName(@"site.com\xC3\xB7othersite.org") UTF8String]);
}

TEST(WTF, URLExtras_Solidus)
{
    // Selected full width solidus, which looks like the solidus, which is the character that indicates the end of the host name.

    // Code path similar to the one used when typing in a URL.
    EXPECT_STREQ("http://site.com/othersite.org", originalDataAsString(WTF::URLWithUserTypedString(@"http://site.com\xEF\xBC\x8Fothersite.org", nil)));
    EXPECT_STREQ("http://site.com/othersite.org", userVisibleString(WTF::URLWithUserTypedString(@"http://site.com\xEF\xBC\x8Fothersite.org", nil)));

    // Code paths similar to the ones used for URLs found in webpages or HTTP responses.
    EXPECT_STREQ("http://site.com\xEF\xBC\x8Fothersite.org", originalDataAsString(literalURL("http://site.com\xEF\xBC\x8Fothersite.org")));
    EXPECT_STREQ("http://site.com%EF%BC%8Fothersite.org", userVisibleString(literalURL("http://site.com\xEF\xBC\x8Fothersite.org")));
    EXPECT_STREQ("http://site.com%EF%BC%8Fothersite.org", originalDataAsString(literalURL("http://site.com%EF%BC%8Fothersite.org")));
    EXPECT_STREQ("http://site.com%EF%BC%8Fothersite.org", userVisibleString(literalURL("http://site.com%EF%BC%8Fothersite.org")));

    // Separate functions that deal with just a host name on its own.
    EXPECT_STREQ("site.com/othersite.org", [WTF::encodeHostName(@"site.com\xEF\xBC\x8Fothersite.org") UTF8String]);
    EXPECT_STREQ("site.com/othersite.org", [WTF::decodeHostName(@"site.com\xEF\xBC\x8Fothersite.org") UTF8String]);
}

TEST(WTF_URLExtras, URLExtras_Space)
{
    // Selected ideographic space, which looks like the ASCII space, which is not allowed unescaped.

    // Code path similar to the one used when typing in a URL.
    EXPECT_STREQ("http://site.com%20othersite.org", originalDataAsString(WTF::URLWithUserTypedString(@"http://site.com\xE3\x80\x80othersite.org", nil)));
    EXPECT_STREQ("http://site.com%20othersite.org", userVisibleString(WTF::URLWithUserTypedString(@"http://site.com\xE3\x80\x80othersite.org", nil)));

    // Code paths similar to the ones used for URLs found in webpages or HTTP responses.
    EXPECT_STREQ("http://site.com\xE3\x80\x80othersite.org", originalDataAsString(literalURL("http://site.com\xE3\x80\x80othersite.org")));
    EXPECT_STREQ("http://site.com%E3%80%80othersite.org", userVisibleString(literalURL("http://site.com\xE3\x80\x80othersite.org")));
    EXPECT_STREQ("http://site.com%E3%80%80othersite.org", originalDataAsString(literalURL("http://site.com%E3%80%80othersite.org")));
    EXPECT_STREQ("http://site.com%E3%80%80othersite.org", userVisibleString(literalURL("http://site.com%E3%80%80othersite.org")));

    // Separate functions that deal with just a host name on its own.
    EXPECT_STREQ("site.com othersite.org", [WTF::encodeHostName(@"site.com\xE3\x80\x80othersite.org") UTF8String]);
    EXPECT_STREQ("site.com\xE3\x80\x80othersite.org", [WTF::decodeHostName(@"site.com\xE3\x80\x80othersite.org") UTF8String]);
}

TEST(WTF_URLExtras, URLExtras_File)
{
    EXPECT_STREQ("file:///%E2%98%83", [[WTF::URLWithUserTypedString(@"file:///☃", nil) absoluteString] UTF8String]);
}

TEST(WTF_URLExtras, URLExtras_ParsingError)
{
    // Expect IDN failure.
    NSURL *url = WTF::URLWithUserTypedString(@"http://.com", nil);
    EXPECT_TRUE(url == nil);

    NSString *encodedHostName = WTF::encodeHostName(@"http://.com");
    EXPECT_TRUE(encodedHostName == nil);

    WTF::URL url2 { utf16String(u"http://\u2267\u222E\uFE63\u0661\u06F1") };
    EXPECT_STREQ([[url2 absoluteString] UTF8String], "http://%E2%89%A7%E2%88%AE%EF%B9%A3%D9%A1%DB%B1");

    std::array<UChar, 2> utf16 { 0xC2, 0xB6 };
    WTF::URL url3 { String(utf16.data(), utf16.size()) };
    EXPECT_FALSE(url3.string().is8Bit());
    EXPECT_FALSE(url3.isValid());
    EXPECT_STREQ([[url3 absoluteString] UTF8String], "%C3%82%C2%B6");
    
    std::array<LChar, 2> latin1 { 0xC2, 0xB6 };
    WTF::URL url4 { String(latin1.data(), 2) };
    EXPECT_FALSE(url4.isValid());
    EXPECT_TRUE(url4.string().is8Bit());
    EXPECT_STREQ([[url4 absoluteString] UTF8String], "%C3%82%C2%B6");

    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    WTF::URL url5 { "file:///A%C3%A7%C3%A3o.html"_str };
    CFURLGetFileSystemRepresentation(url5.createCFURL().get(), false, reinterpret_cast<UInt8*>(buffer), sizeof(buffer));
    EXPECT_STREQ(buffer, "/Ação.html");
}

TEST(WTF_URLExtras, URLExtras_Nil)
{
    NSURL *url1 = WTF::URLWithUserTypedString(nil);
    EXPECT_TRUE(url1 == nil);

    NSURL *url2 = WTF::URLWithUserTypedStringDeprecated(nil);
    EXPECT_TRUE(url2 == nil);
}

TEST(WTF_URLExtras, CreateNSArray)
{
    Vector<URL> urls { URL { "https://webkit.org/"_str } };
    auto array = createNSArray(urls);
    EXPECT_TRUE([array.get()[0] isKindOfClass:NSURL.class]);
}

} // namespace TestWebKitAPI

