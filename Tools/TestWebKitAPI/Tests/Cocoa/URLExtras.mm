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

#import <WebCore/WebCoreNSURLExtras.h>

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
    return dataAsString(WebCore::originalURLData(URL));
}

static const char* userVisibleString(NSURL *URL)
{
    return [WebCore::userVisibleString(URL) UTF8String];
}

static NSURL *literalURL(const char* literal)
{
    return WebCore::URLWithData(literalAsData(literal), nil);
}

TEST(WebCore, URLExtras)
{
    EXPECT_STREQ("http://site.com", originalDataAsString(literalURL("http://site.com")));
    EXPECT_STREQ("http://%77ebsite.com", originalDataAsString(literalURL("http://%77ebsite.com")));

    EXPECT_STREQ("http://site.com", userVisibleString(literalURL("http://site.com")));
    EXPECT_STREQ("http://%77ebsite.com", userVisibleString(literalURL("http://%77ebsite.com")));
}

TEST(WebCore, URLExtras_DivisionSign)
{
    // Selected the division sign as an example of a non-ASCII character that is allowed in host names, since it's a lookalike character.

    // Code path similar to the one used when typing in a URL.
    EXPECT_STREQ("http://site.xn--comothersite-kjb.org", originalDataAsString(WebCore::URLWithUserTypedString(@"http://site.com\xC3\xB7othersite.org", nil)));
    EXPECT_STREQ("http://site.com\xC3\xB7othersite.org", userVisibleString(WebCore::URLWithUserTypedString(@"http://site.com\xC3\xB7othersite.org", nil)));

    // Code paths similar to the ones used for URLs found in webpages or HTTP responses.
    EXPECT_STREQ("http://site.com\xC3\xB7othersite.org", originalDataAsString(literalURL("http://site.com\xC3\xB7othersite.org")));
    EXPECT_STREQ("http://site.com\xC3\xB7othersite.org", userVisibleString(literalURL("http://site.com\xC3\xB7othersite.org")));
    EXPECT_STREQ("http://site.com%C3%B7othersite.org", originalDataAsString(literalURL("http://site.com%C3%B7othersite.org")));
    EXPECT_STREQ("http://site.com\xC3\xB7othersite.org", userVisibleString(literalURL("http://site.com%C3%B7othersite.org")));

    // Separate functions that deal with just a host name on its own.
    EXPECT_STREQ("site.xn--comothersite-kjb.org", [WebCore::encodeHostName(@"site.com\xC3\xB7othersite.org") UTF8String]);
    EXPECT_STREQ("site.com\xC3\xB7othersite.org", [WebCore::decodeHostName(@"site.com\xC3\xB7othersite.org") UTF8String]);
}

TEST(WebCore, URLExtras_Solidus)
{
    // Selected full width solidus, which looks like the solidus, which is the character that indicates the end of the host name.

    // Code path similar to the one used when typing in a URL.
    EXPECT_STREQ("http://site.com/othersite.org", originalDataAsString(WebCore::URLWithUserTypedString(@"http://site.com\xEF\xBC\x8Fothersite.org", nil)));
    EXPECT_STREQ("http://site.com/othersite.org", userVisibleString(WebCore::URLWithUserTypedString(@"http://site.com\xEF\xBC\x8Fothersite.org", nil)));

    // Code paths similar to the ones used for URLs found in webpages or HTTP responses.
    EXPECT_STREQ("http://site.com\xEF\xBC\x8Fothersite.org", originalDataAsString(literalURL("http://site.com\xEF\xBC\x8Fothersite.org")));
    EXPECT_STREQ("http://site.com%EF%BC%8Fothersite.org", userVisibleString(literalURL("http://site.com\xEF\xBC\x8Fothersite.org")));
    EXPECT_STREQ("http://site.com%EF%BC%8Fothersite.org", originalDataAsString(literalURL("http://site.com%EF%BC%8Fothersite.org")));
    EXPECT_STREQ("http://site.com%EF%BC%8Fothersite.org", userVisibleString(literalURL("http://site.com%EF%BC%8Fothersite.org")));

    // Separate functions that deal with just a host name on its own.
    EXPECT_STREQ("site.com/othersite.org", [WebCore::encodeHostName(@"site.com\xEF\xBC\x8Fothersite.org") UTF8String]);
    EXPECT_STREQ("site.com\xEF\xBC\x8Fothersite.org", [WebCore::decodeHostName(@"site.com\xEF\xBC\x8Fothersite.org") UTF8String]);
}

TEST(WebCore, URLExtras_Space)
{
    // Selected ideographic space, which looks like the ASCII space, which is not allowed unescaped.

    // Code path similar to the one used when typing in a URL.
    EXPECT_STREQ("http://site.com%20othersite.org", originalDataAsString(WebCore::URLWithUserTypedString(@"http://site.com\xE3\x80\x80othersite.org", nil)));
    EXPECT_STREQ("http://site.com%20othersite.org", userVisibleString(WebCore::URLWithUserTypedString(@"http://site.com\xE3\x80\x80othersite.org", nil)));

    // Code paths similar to the ones used for URLs found in webpages or HTTP responses.
    EXPECT_STREQ("http://site.com\xE3\x80\x80othersite.org", originalDataAsString(literalURL("http://site.com\xE3\x80\x80othersite.org")));
    EXPECT_STREQ("http://site.com%E3%80%80othersite.org", userVisibleString(literalURL("http://site.com\xE3\x80\x80othersite.org")));
    EXPECT_STREQ("http://site.com%E3%80%80othersite.org", originalDataAsString(literalURL("http://site.com%E3%80%80othersite.org")));
    EXPECT_STREQ("http://site.com%E3%80%80othersite.org", userVisibleString(literalURL("http://site.com%E3%80%80othersite.org")));

    // Separate functions that deal with just a host name on its own.
    EXPECT_STREQ("site.com othersite.org", [WebCore::encodeHostName(@"site.com\xE3\x80\x80othersite.org") UTF8String]);
    EXPECT_STREQ("site.com\xE3\x80\x80othersite.org", [WebCore::decodeHostName(@"site.com\xE3\x80\x80othersite.org") UTF8String]);
}

} // namespace TestWebKitAPI
