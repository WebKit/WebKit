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

TEST(WTF, TestUUIDParsing)
{
    auto created  = UUID::create();
    auto value = UUID::parse(created.toString());
    EXPECT_TRUE(!!value);
    EXPECT_EQ(created, *value);

    // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    value = UUID::parse("12345678-9abc-5de0-89AB-0123456789ab");
    EXPECT_FALSE(!!value);

    value = UUID::parse("12345678-9abc-4dea-79AB-0123456789ab");
    EXPECT_FALSE(!!value);

    value = UUID::parse("12345678-9abc-4de0-89ab-0123456789ab");
    EXPECT_TRUE(!!value);
    EXPECT_TRUE(value->toString() == "12345678-9abc-4de0-89ab-0123456789ab");

    value = UUID::parse("6ef944c1-5cb8-48aa-Ad12-C5f823f005c3");
    EXPECT_TRUE(!!value);
    EXPECT_TRUE(value->toString() == "6ef944c1-5cb8-48aa-ad12-c5f823f005c3");
}
