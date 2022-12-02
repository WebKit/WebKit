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

#if ENABLE(PUBLIC_SUFFIX_LIST)

#include "Test.h"
#include "WTFTestUtilities.h"
#include <WebCore/PublicSuffix.h>
#include <wtf/MainThread.h>

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
    EXPECT_TRUE(isPublicSuffix("com"_s));
    EXPECT_FALSE(isPublicSuffix("test.com"_s));
    EXPECT_FALSE(isPublicSuffix("com.com"_s));
    EXPECT_TRUE(isPublicSuffix("net"_s));
    EXPECT_TRUE(isPublicSuffix("org"_s));
    EXPECT_TRUE(isPublicSuffix("co.uk"_s));
    EXPECT_FALSE(isPublicSuffix("bl.uk"_s));
    EXPECT_FALSE(isPublicSuffix("test.co.uk"_s));
    EXPECT_TRUE(isPublicSuffix("xn--zf0ao64a.tw"_s));
    EXPECT_FALSE(isPublicSuffix("r4---asdf.test.com"_s));
    EXPECT_FALSE(isPublicSuffix(utf16String(bidirectionalDomain)));
    EXPECT_TRUE(isPublicSuffix(utf16String(u"\u6803\u6728.jp")));
    EXPECT_FALSE(isPublicSuffix(""_s));
    EXPECT_FALSE(isPublicSuffix(String::fromUTF8("åäö")));

    // UK
    EXPECT_TRUE(isPublicSuffix("uk"_s));
    EXPECT_FALSE(isPublicSuffix("webkit.uk"_s));
    EXPECT_TRUE(isPublicSuffix("co.uk"_s));
    EXPECT_FALSE(isPublicSuffix("company.co.uk"_s));

    // Note: These tests are based on the Public Domain TLD test suite
    // https://raw.githubusercontent.com/publicsuffix/list/master/tests/test_psl.txt
    //
    // That file states:
    //     Any copyright is dedicated to the Public Domain.
    //     https://creativecommons.org/publicdomain/zero/1.0/

    // null input.
    EXPECT_FALSE(isPublicSuffix(""_s));
    // Mixed case.
    EXPECT_TRUE(isPublicSuffix("COM"_s));
    EXPECT_FALSE(isPublicSuffix("example.COM"_s));
    EXPECT_FALSE(isPublicSuffix("WwW.example.COM"_s));
    // Unlisted TLD.
    // FIXME: Re-enable this subtest once webkit.org/b/234609 is resolved.
    // EXPECT_FALSE(isPublicSuffix("example"_s));
    EXPECT_FALSE(isPublicSuffix("example.example"_s));
    EXPECT_FALSE(isPublicSuffix("b.example.example"_s));
    EXPECT_FALSE(isPublicSuffix("a.b.example.example"_s));
    // TLD with only 1 rule.
    EXPECT_TRUE(isPublicSuffix("biz"_s));
    EXPECT_FALSE(isPublicSuffix("domain.biz"_s));
    EXPECT_FALSE(isPublicSuffix("b.domain.biz"_s));
    EXPECT_FALSE(isPublicSuffix("a.b.domain.biz"_s));
    // TLD with some 2-level rules.
    EXPECT_FALSE(isPublicSuffix("example.com"_s));
    EXPECT_FALSE(isPublicSuffix("b.example.com"_s));
    EXPECT_FALSE(isPublicSuffix("a.b.example.com"_s));
    EXPECT_TRUE(isPublicSuffix("uk.com"_s));
    EXPECT_FALSE(isPublicSuffix("example.uk.com"_s));
    EXPECT_FALSE(isPublicSuffix("b.example.uk.com"_s));
    EXPECT_FALSE(isPublicSuffix("a.b.example.uk.com"_s));
    EXPECT_FALSE(isPublicSuffix("test.ac"_s));
    // TLD with only 1 (wildcard) rule.
    EXPECT_TRUE(isPublicSuffix("mm"_s));
    EXPECT_TRUE(isPublicSuffix("c.mm"_s));
    EXPECT_FALSE(isPublicSuffix("b.c.mm"_s));
    EXPECT_FALSE(isPublicSuffix("a.b.c.mm"_s));
    // More complex TLD.
    EXPECT_TRUE(isPublicSuffix("jp"_s));
    EXPECT_FALSE(isPublicSuffix("test.jp"_s));
    EXPECT_FALSE(isPublicSuffix("www.test.jp"_s));
    EXPECT_TRUE(isPublicSuffix("ac.jp"_s));
    EXPECT_FALSE(isPublicSuffix("test.ac.jp"_s));
    EXPECT_FALSE(isPublicSuffix("www.test.ac.jp"_s));
    EXPECT_TRUE(isPublicSuffix("kyoto.jp"_s));
    EXPECT_FALSE(isPublicSuffix("test.kyoto.jp"_s));
    EXPECT_TRUE(isPublicSuffix("ide.kyoto.jp"_s));
    EXPECT_FALSE(isPublicSuffix("b.ide.kyoto.jp"_s));
    EXPECT_FALSE(isPublicSuffix("a.b.ide.kyoto.jp"_s));
    EXPECT_TRUE(isPublicSuffix("c.kobe.jp"_s));
    EXPECT_FALSE(isPublicSuffix("b.c.kobe.jp"_s));
    EXPECT_FALSE(isPublicSuffix("a.b.c.kobe.jp"_s));
    EXPECT_FALSE(isPublicSuffix("city.kobe.jp"_s));
    EXPECT_FALSE(isPublicSuffix("www.city.kobe.jp"_s));
    // TLD with a wildcard rule and exceptions.
    EXPECT_TRUE(isPublicSuffix("ck"_s));
    EXPECT_TRUE(isPublicSuffix("test.ck"_s));
    EXPECT_FALSE(isPublicSuffix("b.test.ck"_s));
    EXPECT_FALSE(isPublicSuffix("a.b.test.ck"_s));
    EXPECT_FALSE(isPublicSuffix("www.ck"_s));
    EXPECT_FALSE(isPublicSuffix("www.www.ck"_s));
    // US K12.
    EXPECT_TRUE(isPublicSuffix("us"_s));
    EXPECT_FALSE(isPublicSuffix("test.us"_s));
    EXPECT_FALSE(isPublicSuffix("www.test.us"_s));
    EXPECT_TRUE(isPublicSuffix("ak.us"_s));
    EXPECT_FALSE(isPublicSuffix("test.ak.us"_s));
    EXPECT_FALSE(isPublicSuffix("www.test.ak.us"_s));
    EXPECT_TRUE(isPublicSuffix("k12.ak.us"_s));
    EXPECT_FALSE(isPublicSuffix("test.k12.ak.us"_s));
    EXPECT_FALSE(isPublicSuffix("www.test.k12.ak.us"_s));
    // IDN labels (punycoded)
    EXPECT_FALSE(isPublicSuffix("xn--85x722f.com.cn"_s));
    EXPECT_FALSE(isPublicSuffix("xn--85x722f.xn--55qx5d.cn"_s));
    EXPECT_FALSE(isPublicSuffix("www.xn--85x722f.xn--55qx5d.cn"_s));
    EXPECT_FALSE(isPublicSuffix("shishi.xn--55qx5d.cn"_s));
    EXPECT_TRUE(isPublicSuffix("xn--55qx5d.cn"_s));
    EXPECT_FALSE(isPublicSuffix("xn--85x722f.xn--fiqs8s"_s));
    EXPECT_FALSE(isPublicSuffix("www.xn--85x722f.xn--fiqs8s"_s));
    EXPECT_FALSE(isPublicSuffix("shishi.xn--fiqs8s"_s));
    EXPECT_TRUE(isPublicSuffix("xn--fiqs8s"_s));
}

TEST_F(PublicSuffix, TopPrivatelyControlledDomain)
{
    EXPECT_EQ(String(utf16String(u"example.\u6803\u6728.jp")), topPrivatelyControlledDomain(utf16String(u"example.\u6803\u6728.jp")));
    EXPECT_EQ(String(), topPrivatelyControlledDomain(String()));
    EXPECT_EQ(String(), topPrivatelyControlledDomain(""_s));
    EXPECT_EQ(String("test.com"_s), topPrivatelyControlledDomain("test.com"_s));
    EXPECT_EQ(String("test.com"_s), topPrivatelyControlledDomain("com.test.com"_s));
    EXPECT_EQ(String("test.com"_s), topPrivatelyControlledDomain("subdomain.test.com"_s));
    EXPECT_EQ(String("com.com"_s), topPrivatelyControlledDomain("www.com.com"_s));
    EXPECT_EQ(String("test.co.uk"_s), topPrivatelyControlledDomain("test.co.uk"_s));
    EXPECT_EQ(String("test.co.uk"_s), topPrivatelyControlledDomain("subdomain.test.co.uk"_s));
    EXPECT_EQ(String("bl.uk"_s), topPrivatelyControlledDomain("bl.uk"_s));
    EXPECT_EQ(String("bl.uk"_s), topPrivatelyControlledDomain("subdomain.bl.uk"_s));
    EXPECT_EQ(String("test.xn--zf0ao64a.tw"_s), topPrivatelyControlledDomain("test.xn--zf0ao64a.tw"_s));
    EXPECT_EQ(String("test.xn--zf0ao64a.tw"_s), topPrivatelyControlledDomain("www.test.xn--zf0ao64a.tw"_s));
    EXPECT_EQ(String("127.0.0.1"_s), topPrivatelyControlledDomain("127.0.0.1"_s));
    EXPECT_EQ(String(), topPrivatelyControlledDomain("1"_s));
    EXPECT_EQ(String(), topPrivatelyControlledDomain("com"_s));
    EXPECT_EQ(String("test.com"_s), topPrivatelyControlledDomain("r4---asdf.test.com"_s));
    EXPECT_EQ(String("r4---asdf.com"_s), topPrivatelyControlledDomain("r4---asdf.com"_s));
    EXPECT_EQ(String(), topPrivatelyControlledDomain("r4---asdf"_s));
    EXPECT_EQ(utf16String(bidirectionalDomain), utf16String(bidirectionalDomain));
    EXPECT_EQ(String("example.com"_s), topPrivatelyControlledDomain("ExamPle.com"_s));
    EXPECT_EQ(String("example.com"_s), topPrivatelyControlledDomain("SUB.dOmain.ExamPle.com"_s));
    EXPECT_EQ(String("localhost"_s), topPrivatelyControlledDomain("localhost"_s));
    EXPECT_EQ(String("localhost"_s), topPrivatelyControlledDomain("LocalHost"_s));
    EXPECT_EQ(String::fromUTF8("åäö"), topPrivatelyControlledDomain(String::fromUTF8("åäö")));
    EXPECT_EQ(String::fromUTF8("ÅÄÖ"), topPrivatelyControlledDomain(String::fromUTF8("ÅÄÖ")));
    EXPECT_EQ(String("test.com"_s), topPrivatelyControlledDomain(".test.com"_s));
    EXPECT_EQ(String(), topPrivatelyControlledDomain("...."_s));
}

}

#endif // ENABLE(PUBLIC_SUFFIX_LIST)
