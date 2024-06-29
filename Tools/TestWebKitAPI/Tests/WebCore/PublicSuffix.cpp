/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include <WebCore/PublicSuffixStore.h>
#include <wtf/MainThread.h>
#include <wtf/URL.h>

using namespace WebCore;

namespace TestWebKitAPI {

class PublicSuffix: public testing::Test {
public:
    virtual void SetUp()
    {
        WTF::initializeMainThread();
    }
};

const char16_t bidirectionalDomain[28] = u"bidirectional\u0786\u07AE\u0782\u07B0\u0795\u07A9\u0793\u07A6\u0783\u07AA.com";

TEST_F(PublicSuffix, IsPublicSuffix)
{
    auto& publicSuffixStore = PublicSuffixStore::singleton();
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("com.com"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("net"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("org"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("co.uk"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("bl.uk"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.co.uk"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("xn--zf0ao64a.tw"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("r4---asdf.test.com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix(utf16String(bidirectionalDomain)));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix(utf16String(u"\u6803\u6728.jp")));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix(""_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix(String::fromUTF8("åäö")));

    // UK
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("uk"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("webkit.uk"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("co.uk"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("company.co.uk"_s));

    // Note: These tests are based on the Public Domain TLD test suite
    // https://raw.githubusercontent.com/publicsuffix/list/master/tests/test_psl.txt
    //
    // That file states:
    //     Any copyright is dedicated to the Public Domain.
    //     https://creativecommons.org/publicdomain/zero/1.0/

    // null input.
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix(""_s));
    // Mixed case.
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("COM"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("example.COM"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("WwW.example.COM"_s));
    // Unlisted TLD.
    // FIXME: Re-enable this subtest once webkit.org/b/234609 is resolved.
    // EXPECT_FALSE(publicSuffixStore.isPublicSuffix("example"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("example.example"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("b.example.example"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("a.b.example.example"_s));
    // TLD with only 1 rule.
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("biz"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("domain.biz"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("b.domain.biz"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("a.b.domain.biz"_s));
    // TLD with some 2-level rules.
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("example.com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("b.example.com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("a.b.example.com"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("uk.com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("example.uk.com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("b.example.uk.com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("a.b.example.uk.com"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.ac"_s));
    // TLD with only 1 (wildcard) rule.
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("mm"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("c.mm"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("b.c.mm"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("a.b.c.mm"_s));
    // More complex TLD.
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.test.jp"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("ac.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.ac.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.test.ac.jp"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("kyoto.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.kyoto.jp"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("ide.kyoto.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("b.ide.kyoto.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("a.b.ide.kyoto.jp"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("c.kobe.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("b.c.kobe.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("a.b.c.kobe.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("city.kobe.jp"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.city.kobe.jp"_s));
    // TLD with a wildcard rule and exceptions.
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("ck"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("test.ck"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("b.test.ck"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("a.b.test.ck"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.ck"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.www.ck"_s));
    // US K12.
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("us"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.us"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.test.us"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("ak.us"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.ak.us"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.test.ak.us"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("k12.ak.us"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("test.k12.ak.us"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.test.k12.ak.us"_s));
    // IDN labels (punycoded)
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("xn--85x722f.com.cn"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("xn--85x722f.xn--55qx5d.cn"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.xn--85x722f.xn--55qx5d.cn"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("shishi.xn--55qx5d.cn"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("xn--55qx5d.cn"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("xn--85x722f.xn--fiqs8s"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("www.xn--85x722f.xn--fiqs8s"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("shishi.xn--fiqs8s"_s));
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("xn--fiqs8s"_s));
}

TEST_F(PublicSuffix, TopPrivatelyControlledDomain)
{
    auto& publicSuffixStore = PublicSuffixStore::singleton();
    EXPECT_EQ(String(utf16String(u"example.\u6803\u6728.jp")), publicSuffixStore.topPrivatelyControlledDomain(utf16String(u"example.\u6803\u6728.jp")));
    EXPECT_EQ(String(), publicSuffixStore.topPrivatelyControlledDomain(String()));
    EXPECT_EQ(String(), publicSuffixStore.topPrivatelyControlledDomain(""_s));
    EXPECT_EQ(String("test.com"_s), publicSuffixStore.topPrivatelyControlledDomain("test.com"_s));
    EXPECT_EQ(String("test.com"_s), publicSuffixStore.topPrivatelyControlledDomain("com.test.com"_s));
    EXPECT_EQ(String("test.com"_s), publicSuffixStore.topPrivatelyControlledDomain("subdomain.test.com"_s));
    EXPECT_EQ(String("com.com"_s), publicSuffixStore.topPrivatelyControlledDomain("www.com.com"_s));
    EXPECT_EQ(String("test.co.uk"_s), publicSuffixStore.topPrivatelyControlledDomain("test.co.uk"_s));
    EXPECT_EQ(String("test.co.uk"_s), publicSuffixStore.topPrivatelyControlledDomain("subdomain.test.co.uk"_s));
    EXPECT_EQ(String("bl.uk"_s), publicSuffixStore.topPrivatelyControlledDomain("bl.uk"_s));
    EXPECT_EQ(String("bl.uk"_s), publicSuffixStore.topPrivatelyControlledDomain("subdomain.bl.uk"_s));
    EXPECT_EQ(String("test.xn--zf0ao64a.tw"_s), publicSuffixStore.topPrivatelyControlledDomain("test.xn--zf0ao64a.tw"_s));
    EXPECT_EQ(String("test.xn--zf0ao64a.tw"_s), publicSuffixStore.topPrivatelyControlledDomain("www.test.xn--zf0ao64a.tw"_s));
    EXPECT_EQ(String("127.0.0.1"_s), publicSuffixStore.topPrivatelyControlledDomain("127.0.0.1"_s));
    EXPECT_EQ(String(), publicSuffixStore.topPrivatelyControlledDomain("1"_s));
    EXPECT_EQ(String(), publicSuffixStore.topPrivatelyControlledDomain("com"_s));
    EXPECT_EQ(String("test.com"_s), publicSuffixStore.topPrivatelyControlledDomain("r4---asdf.test.com"_s));
    EXPECT_EQ(String("r4---asdf.com"_s), publicSuffixStore.topPrivatelyControlledDomain("r4---asdf.com"_s));
    EXPECT_EQ(String(), publicSuffixStore.topPrivatelyControlledDomain("r4---asdf"_s));
    EXPECT_EQ(utf16String(bidirectionalDomain), utf16String(bidirectionalDomain));
    EXPECT_EQ(String("example.com"_s), publicSuffixStore.topPrivatelyControlledDomain("ExamPle.com"_s));
    EXPECT_EQ(String("example.com"_s), publicSuffixStore.topPrivatelyControlledDomain("SUB.dOmain.ExamPle.com"_s));
    EXPECT_EQ(String("localhost"_s), publicSuffixStore.topPrivatelyControlledDomain("localhost"_s));
    EXPECT_EQ(String("localhost"_s), publicSuffixStore.topPrivatelyControlledDomain("LocalHost"_s));
    EXPECT_EQ(String::fromUTF8("åäö"), publicSuffixStore.topPrivatelyControlledDomain(String::fromUTF8("åäö")));
    EXPECT_EQ(String::fromUTF8("ÅÄÖ"), publicSuffixStore.topPrivatelyControlledDomain(String::fromUTF8("ÅÄÖ")));
    EXPECT_EQ(String("test.com"_s), publicSuffixStore.topPrivatelyControlledDomain(".test.com"_s));
    EXPECT_EQ(String(), publicSuffixStore.topPrivatelyControlledDomain("...."_s));
}

#if PLATFORM(COCOA)
TEST_F(PublicSuffix, PublicSuffixCache)
{
    WTF::URL abExampleURL { "http://a.b.example.example"_s };
    auto& publicSuffixStore = PublicSuffixStore::singleton();
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix("example.example"_s));
    EXPECT_EQ(String("example"_s), publicSuffixStore.publicSuffix(abExampleURL).string());
    // Non-cocoa platforms currently do not use public suffix cache for topPrivatelyControlledDomain().
    EXPECT_EQ(String("example.example"_s), publicSuffixStore.topPrivatelyControlledDomain("a.b.example.example"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix(""_s));

    publicSuffixStore.enablePublicSuffixCache();
    publicSuffixStore.clearHostTopPrivatelyControlledDomainCache();
    publicSuffixStore.addPublicSuffix(WebCore::PublicSuffix::fromRawString("example.example"_s));
    publicSuffixStore.addPublicSuffix(WebCore::PublicSuffix { });
    EXPECT_TRUE(publicSuffixStore.isPublicSuffix("example.example"_s));
    EXPECT_EQ(String("example.example"_s), publicSuffixStore.publicSuffix(abExampleURL).string());
    EXPECT_EQ(String("b.example.example"_s), publicSuffixStore.topPrivatelyControlledDomain("a.b.example.example"_s));
    EXPECT_FALSE(publicSuffixStore.isPublicSuffix(""_s));
}
#endif

}
