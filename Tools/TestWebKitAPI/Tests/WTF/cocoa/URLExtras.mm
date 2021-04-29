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

#import "WTFStringUtilities.h"
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
    EXPECT_STREQ(".example.com", [WTF::decodeHostName(@"xn--.example.com") UTF8String]);
    EXPECT_STREQ("a..example.com", [WTF::decodeHostName(@"a..example.com") UTF8String]);
}
    
TEST(WTF_URLExtras, URLExtras_Spoof)
{
    Vector<String> punycodedSpoofHosts = {
        "xn--cfa45g", // U+0131, U+0307
        "xn--tma03b", // U+0237, U+0307
        "xn--tma03bxga", // U+0237, U+034F, U+034F, U+0307
        "xn--tma03bl01e", // U+0237, U+200B, U+0307
        "xn--a-egb", // a, U+034F
        "xn--a-qgn", // a, U+200B
        "xn--a-mgn", // a, U+2009
        "xn--u7f", // U+1D04
        "xn--57f", // U+1D0F
        "xn--i38a", // U+A731
        "xn--j8f", // U+1D1C
        "xn--n8f", // U+1D20
        "xn--o8f", // U+1D21
        "xn--p8f", // U+1D22
        "xn--0na", // U+0261
        "xn--cn-ded", // U+054D
        "xn--ews-nfe.org", // U+054D
        "xn--yotube-qkh", // U+0578
        "xn--cla-7fe.edu", // U+0578
        "xn--rsa94l", // U+05D5 U+0307
        "xn--hdb9c", // U+05D5 U+05B9
        "xn--idb7c", // U+05D5 U+05BA
        "xn--pdb3b", // U+05D5 U+05C1
        "xn--qdb1b", // U+05D5 U+05C2
        "xn--sdb7a", // U+05D5 U+05C4
        "xn--2-zic", // U+0032 U+05E1
        "xn--uoa", // U+027E
        "xn--fja", // U+01C0
        "xn--koa", // U+0274
        "xn--tma", // U+0237
        "xn--o-pdc", // U+0585 'o'
        "xn--o-qdc", // 'o' U+0585
        "xn--g-hdc", // U+0581 'g'
        "xn--g-idc", // 'g' U+0581
    };
    for (const String& host : punycodedSpoofHosts) {
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

    WTF::URL url2(URL(), utf16String(u"http://\u2267\u222E\uFE63"));
    EXPECT_STREQ([[url2 absoluteString] UTF8String], "http://%E2%89%A7%E2%88%AE%EF%B9%A3");

    std::array<UChar, 3> utf16 { 0xC2, 0xB6, 0x00 };
    WTF::URL url3(URL(), String(utf16.data()));
    EXPECT_FALSE(url3.string().is8Bit());
    EXPECT_FALSE(url3.isValid());
    EXPECT_STREQ([[url3 absoluteString] UTF8String], "%C3%82%C2%B6");
    
    std::array<LChar, 3> latin1 { 0xC2, 0xB6, 0x00 };
    WTF::URL url4(URL(), String(latin1.data()));
    EXPECT_FALSE(url4.isValid());
    EXPECT_TRUE(url4.string().is8Bit());
    EXPECT_STREQ([[url4 absoluteString] UTF8String], "%C3%82%C2%B6");

    char buffer[100];
    memset(buffer, 0, sizeof(buffer));
    WTF::URL url5(URL(), "file:///A%C3%A7%C3%A3o.html"_s);
    CFURLGetFileSystemRepresentation(url5.createCFURL().get(), false, reinterpret_cast<UInt8*>(buffer), sizeof(buffer));
    EXPECT_STREQ(buffer, "/Ação.html");
}

TEST(WTF_URLExtras, URLExtras_Nil)
{
    NSURL *url1 = WTF::URLWithUserTypedString(nil, nil);
    EXPECT_TRUE(url1 == nil);

    NSURL *url2 = WTF::URLWithUserTypedStringDeprecated(nil, nil);
    EXPECT_TRUE(url2 == nil);
}

TEST(WTF_URLExtras, CreateNSArray)
{
    Vector<URL> urls { URL(URL(), "https://webkit.org/") };
    auto array = createNSArray(urls);
    EXPECT_TRUE([array.get()[0] isKindOfClass:NSURL.class]);
}

} // namespace TestWebKitAPI

