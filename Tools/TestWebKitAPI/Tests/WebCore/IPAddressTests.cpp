/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <WebCore/DNS.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

#if OS(UNIX)

TEST(IPAddressTests, MatchingNetMaskLength)
{
    auto address1 = WebCore::IPAddress::fromString("17.100.120.255"_s);
    auto address2 = WebCore::IPAddress::fromString("17.100.100.255"_s);
    auto address3 = WebCore::IPAddress::fromString("2001:db8::1234:5678"_s);
    auto address4 = WebCore::IPAddress::fromString("2001:db8::1111:0000"_s);
    auto address5 = WebCore::IPAddress::fromString("::1234:5678"_s);
    auto address6 = WebCore::IPAddress::fromString("::"_s);

    EXPECT_EQ(address1->matchingNetMaskLength(*address2), 19U);
    EXPECT_EQ(address2->matchingNetMaskLength(*address1), 19U);
    EXPECT_EQ(address1->matchingNetMaskLength(*address3), 0U);
    EXPECT_EQ(address3->matchingNetMaskLength(*address1), 0U);
    EXPECT_EQ(address3->matchingNetMaskLength(*address4), 102U);
    EXPECT_EQ(address4->matchingNetMaskLength(*address3), 102U);
    EXPECT_EQ(address3->matchingNetMaskLength(*address5), 2U);
    EXPECT_EQ(address5->matchingNetMaskLength(*address3), 2U);
    EXPECT_EQ(address5->matchingNetMaskLength(*address6), 99U);
    EXPECT_EQ(address6->matchingNetMaskLength(*address5), 99U);
}

TEST(IPAddressTests, InvalidAddresses)
{
    EXPECT_EQ(WebCore::IPAddress::fromString(""_s), std::nullopt);
    EXPECT_EQ(WebCore::IPAddress::fromString("foo"_s), std::nullopt);
    EXPECT_EQ(WebCore::IPAddress::fromString("2001:88888::"_s), std::nullopt);
    EXPECT_EQ(WebCore::IPAddress::fromString("192.168.255.256"_s), std::nullopt);
}

TEST(IPAddressTests, CompareIPAddresses)
{
    auto address1 = *WebCore::IPAddress::fromString("17.100.100.255"_s);
    auto address2 = *WebCore::IPAddress::fromString("18.101.100.0"_s);
    auto address3 = *WebCore::IPAddress::fromString("2001:db8::1234:1000"_s);
    auto address4 = *WebCore::IPAddress::fromString("2001:db9::1234:0000"_s);

    EXPECT_TRUE(address1 < address2);
    EXPECT_TRUE(address2 > address1);
    EXPECT_TRUE(address3 < address4);
    EXPECT_TRUE(address4 > address3);
    EXPECT_TRUE(address1 == WebCore::IPAddress::fromString("17.100.100.255"_s));
    EXPECT_EQ(address1.compare(address3), WebCore::IPAddress::ComparisonResult::CannotCompare);
    EXPECT_EQ(address4.compare(address2), WebCore::IPAddress::ComparisonResult::CannotCompare);
}

#endif // OS(UNIX)

} // namespace TestWebKitAPI

