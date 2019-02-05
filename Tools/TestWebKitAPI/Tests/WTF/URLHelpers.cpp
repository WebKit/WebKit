/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Igalia S.L.
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
#include <wtf/URLHelpers.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using namespace WTF;
using namespace WTF::URLHelpers;

TEST(WTF, UserVisibleURLUnaffected)
{
    const char* const items[] = {
        "http://site.com",
        "http://%77ebsite.com",
        "http://xn--cfa45g/", // U+0131, U+0307
        "http://xn--tma03b/", // U+0237, U+0307
        "http://xn--tma03bxga/", // U+0237, U+034F, U+034F, U+0307
        "http://xn--tma03bl01e/", // U+0237, U+200B, U+0307
        "http://xn--a-egb/", // a, U+034F
        "http://xn--a-qgn/", // a, U+200B
        "http://xn--a-mgn/", // a, U+2009
        "http://xn--u7f/", // U+1D04
        "http://xn--57f/", // U+1D0F
        "http://xn--i38a/", // U+A731
        "http://xn--j8f/", // U+1D1C
        "http://xn--n8f/", // U+1D20
        "http://xn--o8f/", // U+1D21
        "http://xn--p8f/", // U+1D22
        "http://xn--0na/", // U+0261
        "http://xn--cn-ded/", // U+054D
        "http://xn--ews-nfe.org/", // U+054D
        "http://xn--yotube-qkh/", // U+0578
        "http://xn--cla-7fe.edu/", // U+0578
        "http://xn--rsa94l/", // U+05D5 U+0307
        "http://xn--hdb9c/", // U+05D5 U+05B9
        "http://xn--idb7c/", // U+05D5 U+05BA
        "http://xn--pdb3b/", // U+05D5 U+05C1
        "http://xn--qdb1b/", // U+05D5 U+05C2
        "http://xn--sdb7a/", // U+05D5 U+05C4
        "http://xn--2-zic/", // U+0032 U+05E1

        // Valid mixtures of Armenian and other scripts
        "https://en.wikipedia.org/wiki/.\u0570\u0561\u0575",
        "https://\u0573\u0574\u0578.\u0570\u0561\u0575",
        "https://\u0573-1-\u0574\u0578.\u0570\u0561\u0575",
        "https://2\u0573_\u0574\u0578.\u0570\u0561\u0575",
        "https://\u0573_\u0574\u05783.\u0570\u0561\u0575",
        "https://got\u0551\u0535\u0543.com",
        "https://\u0551\u0535\u0543fans.net",
        "https://\u0551\u0535or\u0575\u0543.biz",
        "https://\u0551\u0535and!$^&*()-~+={}or<>,.?\u0575\u0543.biz",
    };

    for (auto& item : items) {
        CString input(item);
        String result = userVisibleURL(input);
        EXPECT_EQ(result.utf8(), item);
    }
}

TEST(WTF, UserVisibleURLAffected)
{
    struct {
        const char* input;
        const char* output;
    } const items[] = {
        // Division sign: an allowed non-ASCII character, since it's not a
        // lookalike character.
        { "http://site.com\xC3\xB7othersite.org", "http://site.com\xC3\xB7othersite.org" },
        { "http://site.com%C3%B7othersite.org", "http://site.com\xC3\xB7othersite.org" },

        // Full width solidus: disallowed because it looks like the solidus,
        // which is the character that indicates the end of the host name.
        { "http://site.com\xEF\xBC\x8Fothersite.org", "http://site.com%EF%BC%8Fothersite.org" },
        { "http://site.com%EF%BC%8Fothersite.org", "http://site.com%EF%BC%8Fothersite.org" },

        // Ideographic space: disallowed because it looks like the ASCII space.
        { "http://site.com\xE3\x80\x80othersite.org", "http://site.com%E3%80%80othersite.org" },
        { "http://site.com%E3%80%80othersite.org", "http://site.com%E3%80%80othersite.org" },
    };

    for (auto& item : items) {
        CString input(item.input);
        String result = userVisibleURL(input);
        EXPECT_EQ(result.utf8(), item.output);
    }
}

} // namespace TestWebKitAPI
