/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <wtf/UUID.h>

TEST(WTF, BootSessionUUIDIdentity)
{
    EXPECT_EQ(bootSessionUUIDString(), bootSessionUUIDString());
}

static String parseAndStringifyUUID(const String& value)
{
    auto uuid = UUID::parseVersion4(value);
    if (!uuid)
        return { };
    return uuid->toString();
}

TEST(WTF, TestUUIDVersion4Parsing)
{
    // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx

    EXPECT_FALSE(!!UUID::parseVersion4("12345678-9abc-5de0-89AB-0123456789ab"));
    EXPECT_FALSE(!!UUID::parseVersion4("12345678-9abc-4dea-79AB-0123456789ab"));
    EXPECT_FALSE(!!UUID::parseVersion4("12345678-9abc-4de0-7fff-0123456789ab"));
    EXPECT_FALSE(!!UUID::parseVersion4("12345678-9abc-4de0-c0000-0123456789ab"));

    EXPECT_FALSE(!!UUID::parseVersion4("+ef944c1-5cb8-48aa-Ad12-C5f823f005c3"));
    EXPECT_FALSE(!!UUID::parseVersion4("6ef944c1-+cb8-48aa-Ad12-C5f823f005c3"));
    EXPECT_FALSE(!!UUID::parseVersion4("6ef944c1-5cb8-+8aa-Ad12-C5f823f005c3"));
    EXPECT_FALSE(!!UUID::parseVersion4("6ef944c1-5cb8-48aa-+d12-C5f823f005c3"));
    EXPECT_FALSE(!!UUID::parseVersion4("6ef944c1-5cb8-48aa-Ad12-+5f823f005c3"));

    EXPECT_FALSE(!!UUID::parseVersion4("00000000-0000-0000-0000-000000000000"));
    EXPECT_FALSE(!!UUID::parseVersion4("00000000-0000-0000-0000-000000000001"));
    EXPECT_TRUE(!!UUID::parseVersion4("00000000-0000-4000-8000-000000000000"));
    EXPECT_TRUE(!!UUID::parseVersion4("00000000-0000-4000-8000-000000000001"));

    for (size_t cptr = 0; cptr < 10; ++cptr) {
        auto createdUUID = UUID::createVersion4();
        auto createdString = createdUUID.toString();
        EXPECT_EQ(createdString.length(), 36u);
        EXPECT_EQ(createdString[14], '4');
        EXPECT_TRUE(createdString[19] == '8' || createdString[19] == '9' || createdString[19] == 'a' || createdString[19] == 'b');

        auto uuid = UUID::parseVersion4(createdString);
        EXPECT_TRUE(!!uuid);
        EXPECT_EQ(*uuid, createdUUID);
    }

    String testNormal = "12345678-9abc-4de0-89ab-0123456789ab"_s;
    EXPECT_EQ(parseAndStringifyUUID(testNormal), testNormal);

    String test8000 = "12345678-9abc-4de0-8000-0123456789ab"_s;
    EXPECT_EQ(parseAndStringifyUUID(test8000), test8000);

    String testBfff = "12345678-9abc-4de0-Bfff-0123456789ab"_s;
    EXPECT_EQ(parseAndStringifyUUID(testBfff), testBfff.convertToASCIILowercase());

    String testAd12 = "6ef944c1-5cb8-48aa-Ad12-C5f823f005c3"_s;
    EXPECT_EQ(parseAndStringifyUUID(testAd12), testAd12.convertToASCIILowercase());
}
