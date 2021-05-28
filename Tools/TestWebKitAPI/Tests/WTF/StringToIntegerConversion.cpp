/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <wtf/text/StringToIntegerConversion.h>

namespace TestWebKitAPI {

TEST(WTF, ParseInteger)
{
    EXPECT_EQ(std::nullopt, parseInteger<int>(static_cast<const char*>(nullptr)));
    EXPECT_EQ(std::nullopt, parseInteger<int>(""));
    EXPECT_EQ(0, parseInteger<int>("0"));
    EXPECT_EQ(1, parseInteger<int>("1"));
    EXPECT_EQ(3, parseInteger<int>("3"));
    EXPECT_EQ(-3, parseInteger<int>("-3"));
    EXPECT_EQ(12345, parseInteger<int>("12345"));
    EXPECT_EQ(-12345, parseInteger<int>("-12345"));
    EXPECT_EQ(2147483647, parseInteger<int>("2147483647"));
    EXPECT_EQ(std::nullopt, parseInteger<int>("2147483648"));
    EXPECT_EQ(-2147483647 - 1, parseInteger<int>("-2147483648"));
    EXPECT_EQ(std::nullopt, parseInteger<int>("-2147483649"));
    EXPECT_EQ(std::nullopt, parseInteger<int>("x1"));
    EXPECT_EQ(1, parseInteger<int>(" 1"));
    EXPECT_EQ(std::nullopt, parseInteger<int>("1x"));

    EXPECT_EQ(0U, 0U + *parseInteger<uint16_t>("0"));
    EXPECT_EQ(3U, 0U + *parseInteger<uint16_t>("3"));
    EXPECT_EQ(12345U, 0U + *parseInteger<uint16_t>("12345"));
    EXPECT_EQ(65535U, 0U + *parseInteger<uint16_t>("65535"));
    EXPECT_EQ(std::nullopt, parseInteger<uint16_t>("-1"));
    EXPECT_EQ(std::nullopt, parseInteger<uint16_t>("-3"));
    EXPECT_EQ(std::nullopt, parseInteger<uint16_t>("65536"));
}

TEST(WTF, ParseIntegerAllowingTrailingJunk)
{
    EXPECT_EQ(std::nullopt, parseIntegerAllowingTrailingJunk<int>(static_cast<const char*>(nullptr)));
    EXPECT_EQ(std::nullopt, parseIntegerAllowingTrailingJunk<int>(""));
    EXPECT_EQ(0, parseIntegerAllowingTrailingJunk<int>("0"));
    EXPECT_EQ(1, parseIntegerAllowingTrailingJunk<int>("1"));
    EXPECT_EQ(3, parseIntegerAllowingTrailingJunk<int>("3"));
    EXPECT_EQ(-3, parseIntegerAllowingTrailingJunk<int>("-3"));
    EXPECT_EQ(12345, parseIntegerAllowingTrailingJunk<int>("12345"));
    EXPECT_EQ(-12345, parseIntegerAllowingTrailingJunk<int>("-12345"));
    EXPECT_EQ(2147483647, parseIntegerAllowingTrailingJunk<int>("2147483647"));
    EXPECT_EQ(std::nullopt, parseIntegerAllowingTrailingJunk<int>("2147483648"));
    EXPECT_EQ(-2147483647 - 1, parseIntegerAllowingTrailingJunk<int>("-2147483648"));
    EXPECT_EQ(std::nullopt, parseIntegerAllowingTrailingJunk<int>("-2147483649"));
    EXPECT_EQ(std::nullopt, parseIntegerAllowingTrailingJunk<int>("x1"));
    EXPECT_EQ(1, parseIntegerAllowingTrailingJunk<int>(" 1"));
    EXPECT_EQ(1, parseIntegerAllowingTrailingJunk<int>("1x"));

    EXPECT_EQ(0U, 0U + *parseIntegerAllowingTrailingJunk<uint16_t>("0"));
    EXPECT_EQ(3U, 0U + *parseIntegerAllowingTrailingJunk<uint16_t>("3"));
    EXPECT_EQ(12345U, 0U + *parseIntegerAllowingTrailingJunk<uint16_t>("12345"));
    EXPECT_EQ(65535U, 0U + *parseIntegerAllowingTrailingJunk<uint16_t>("65535"));
    EXPECT_EQ(std::nullopt, parseIntegerAllowingTrailingJunk<uint16_t>("-1"));
    EXPECT_EQ(std::nullopt, parseIntegerAllowingTrailingJunk<uint16_t>("-3"));
    EXPECT_EQ(std::nullopt, parseIntegerAllowingTrailingJunk<uint16_t>("65536"));
}

} // namespace TestWebKitAPI
