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

#include "WTFStringUtilities.h"
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
    EXPECT_TRUE(isPublicSuffix("com"));
    EXPECT_FALSE(isPublicSuffix("test.com"));
    EXPECT_FALSE(isPublicSuffix("com.com"));
    EXPECT_TRUE(isPublicSuffix("net"));
    EXPECT_TRUE(isPublicSuffix("org"));
    EXPECT_TRUE(isPublicSuffix("co.uk"));
    EXPECT_FALSE(isPublicSuffix("bl.uk"));
    EXPECT_FALSE(isPublicSuffix("test.co.uk"));
    EXPECT_TRUE(isPublicSuffix("xn--zf0ao64a.tw"));
    EXPECT_FALSE(isPublicSuffix("r4---asdf.test.com"));
    EXPECT_FALSE(isPublicSuffix(utf16String(bidirectionalDomain)));
    EXPECT_TRUE(isPublicSuffix(utf16String(u"\u6803\u6728.jp")));
    EXPECT_FALSE(isPublicSuffix(""));
    EXPECT_FALSE(isPublicSuffix("åäö"));

    // UK
    EXPECT_TRUE(isPublicSuffix("uk"));
    EXPECT_FALSE(isPublicSuffix("webkit.uk"));
    EXPECT_TRUE(isPublicSuffix("co.uk"));
    EXPECT_FALSE(isPublicSuffix("company.co.uk"));

    // Note: These tests are based on the Public Domain TLD test suite
    // https://raw.githubusercontent.com/publicsuffix/list/master/tests/test_psl.txt
    //
    // That file states:
    //     Any copyright is dedicated to the Public Domain.
    //     https://creativecommons.org/publicdomain/zero/1.0/

    // null input.
    EXPECT_FALSE(isPublicSuffix(""));
    // Mixed case.
    EXPECT_TRUE(isPublicSuffix("COM"));
    EXPECT_FALSE(isPublicSuffix("example.COM"));
    EXPECT_FALSE(isPublicSuffix("WwW.example.COM"));
    // Unlisted TLD.
    EXPECT_FALSE(isPublicSuffix("example"));
    EXPECT_FALSE(isPublicSuffix("example.example"));
    EXPECT_FALSE(isPublicSuffix("b.example.example"));
    EXPECT_FALSE(isPublicSuffix("a.b.example.example"));
    // TLD with only 1 rule.
    EXPECT_TRUE(isPublicSuffix("biz"));
    EXPECT_FALSE(isPublicSuffix("domain.biz"));
    EXPECT_FALSE(isPublicSuffix("b.domain.biz"));
    EXPECT_FALSE(isPublicSuffix("a.b.domain.biz"));
    // TLD with some 2-level rules.
    EXPECT_FALSE(isPublicSuffix("example.com"));
    EXPECT_FALSE(isPublicSuffix("b.example.com"));
    EXPECT_FALSE(isPublicSuffix("a.b.example.com"));
    EXPECT_TRUE(isPublicSuffix("uk.com"));
    EXPECT_FALSE(isPublicSuffix("example.uk.com"));
    EXPECT_FALSE(isPublicSuffix("b.example.uk.com"));
    EXPECT_FALSE(isPublicSuffix("a.b.example.uk.com"));
    EXPECT_FALSE(isPublicSuffix("test.ac"));
    // TLD with only 1 (wildcard) rule.
    EXPECT_TRUE(isPublicSuffix("mm"));
    EXPECT_TRUE(isPublicSuffix("c.mm"));
    EXPECT_FALSE(isPublicSuffix("b.c.mm"));
    EXPECT_FALSE(isPublicSuffix("a.b.c.mm"));
    // More complex TLD.
    EXPECT_TRUE(isPublicSuffix("jp"));
    EXPECT_FALSE(isPublicSuffix("test.jp"));
    EXPECT_FALSE(isPublicSuffix("www.test.jp"));
    EXPECT_TRUE(isPublicSuffix("ac.jp"));
    EXPECT_FALSE(isPublicSuffix("test.ac.jp"));
    EXPECT_FALSE(isPublicSuffix("www.test.ac.jp"));
    EXPECT_TRUE(isPublicSuffix("kyoto.jp"));
    EXPECT_FALSE(isPublicSuffix("test.kyoto.jp"));
    EXPECT_TRUE(isPublicSuffix("ide.kyoto.jp"));
    EXPECT_FALSE(isPublicSuffix("b.ide.kyoto.jp"));
    EXPECT_FALSE(isPublicSuffix("a.b.ide.kyoto.jp"));
    EXPECT_TRUE(isPublicSuffix("c.kobe.jp"));
    EXPECT_FALSE(isPublicSuffix("b.c.kobe.jp"));
    EXPECT_FALSE(isPublicSuffix("a.b.c.kobe.jp"));
    EXPECT_FALSE(isPublicSuffix("city.kobe.jp"));
    EXPECT_FALSE(isPublicSuffix("www.city.kobe.jp"));
    // TLD with a wildcard rule and exceptions.
    EXPECT_TRUE(isPublicSuffix("ck"));
    EXPECT_TRUE(isPublicSuffix("test.ck"));
    EXPECT_FALSE(isPublicSuffix("b.test.ck"));
    EXPECT_FALSE(isPublicSuffix("a.b.test.ck"));
    EXPECT_FALSE(isPublicSuffix("www.ck"));
    EXPECT_FALSE(isPublicSuffix("www.www.ck"));
    // US K12.
    EXPECT_TRUE(isPublicSuffix("us"));
    EXPECT_FALSE(isPublicSuffix("test.us"));
    EXPECT_FALSE(isPublicSuffix("www.test.us"));
    EXPECT_TRUE(isPublicSuffix("ak.us"));
    EXPECT_FALSE(isPublicSuffix("test.ak.us"));
    EXPECT_FALSE(isPublicSuffix("www.test.ak.us"));
    EXPECT_TRUE(isPublicSuffix("k12.ak.us"));
    EXPECT_FALSE(isPublicSuffix("test.k12.ak.us"));
    EXPECT_FALSE(isPublicSuffix("www.test.k12.ak.us"));
    // IDN labels (punycoded)
    EXPECT_FALSE(isPublicSuffix("xn--85x722f.com.cn"));
    EXPECT_FALSE(isPublicSuffix("xn--85x722f.xn--55qx5d.cn"));
    EXPECT_FALSE(isPublicSuffix("www.xn--85x722f.xn--55qx5d.cn"));
    EXPECT_FALSE(isPublicSuffix("shishi.xn--55qx5d.cn"));
    EXPECT_TRUE(isPublicSuffix("xn--55qx5d.cn"));
    EXPECT_FALSE(isPublicSuffix("xn--85x722f.xn--fiqs8s"));
    EXPECT_FALSE(isPublicSuffix("www.xn--85x722f.xn--fiqs8s"));
    EXPECT_FALSE(isPublicSuffix("shishi.xn--fiqs8s"));
    EXPECT_TRUE(isPublicSuffix("xn--fiqs8s"));
}

TEST_F(PublicSuffix, TopPrivatelyControlledDomain)
{
    EXPECT_EQ(String(utf16String(u"example.\u6803\u6728.jp")), topPrivatelyControlledDomain(utf16String(u"example.\u6803\u6728.jp")));
    EXPECT_EQ(String(), topPrivatelyControlledDomain(String()));
    EXPECT_EQ(String(), topPrivatelyControlledDomain(""));
    EXPECT_EQ(String("test.com"), topPrivatelyControlledDomain("test.com"));
    EXPECT_EQ(String("test.com"), topPrivatelyControlledDomain("com.test.com"));
    EXPECT_EQ(String("test.com"), topPrivatelyControlledDomain("subdomain.test.com"));
    EXPECT_EQ(String("com.com"), topPrivatelyControlledDomain("www.com.com"));
    EXPECT_EQ(String("test.co.uk"), topPrivatelyControlledDomain("test.co.uk"));
    EXPECT_EQ(String("test.co.uk"), topPrivatelyControlledDomain("subdomain.test.co.uk"));
    EXPECT_EQ(String("bl.uk"), topPrivatelyControlledDomain("bl.uk"));
    EXPECT_EQ(String("bl.uk"), topPrivatelyControlledDomain("subdomain.bl.uk"));
    EXPECT_EQ(String("test.xn--zf0ao64a.tw"), topPrivatelyControlledDomain("test.xn--zf0ao64a.tw"));
    EXPECT_EQ(String("test.xn--zf0ao64a.tw"), topPrivatelyControlledDomain("www.test.xn--zf0ao64a.tw"));
    EXPECT_EQ(String("127.0.0.1"), topPrivatelyControlledDomain("127.0.0.1"));
    EXPECT_EQ(String(), topPrivatelyControlledDomain("1"));
    EXPECT_EQ(String(), topPrivatelyControlledDomain("com"));
    EXPECT_EQ(String("test.com"), topPrivatelyControlledDomain("r4---asdf.test.com"));
    EXPECT_EQ(String("r4---asdf.com"), topPrivatelyControlledDomain("r4---asdf.com"));
    EXPECT_EQ(String(), topPrivatelyControlledDomain("r4---asdf"));
    EXPECT_EQ(utf16String(bidirectionalDomain), utf16String(bidirectionalDomain));
    EXPECT_EQ(String("example.com"), topPrivatelyControlledDomain("ExamPle.com"));
    EXPECT_EQ(String("example.com"), topPrivatelyControlledDomain("SUB.dOmain.ExamPle.com"));
    EXPECT_EQ(String("localhost"), topPrivatelyControlledDomain("localhost"));
    EXPECT_EQ(String("localhost"), topPrivatelyControlledDomain("LocalHost"));
    EXPECT_EQ(String("åäö"), topPrivatelyControlledDomain("åäö"));
    EXPECT_EQ(String("ÅÄÖ"), topPrivatelyControlledDomain("ÅÄÖ"));
}

}

#endif // ENABLE(PUBLIC_SUFFIX_LIST)
