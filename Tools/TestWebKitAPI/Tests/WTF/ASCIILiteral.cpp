/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <wtf/text/ASCIILiteral.h>

namespace TestWebKitAPI {

TEST(WTF, ASCIILIteralComparison)
{
    ASCIILiteral nullLiteral;
    ASCIILiteral nullLiteral2;
    auto emptyLiteral = ""_s;
    auto emptyLiteral2 = ""_s;
    auto aLiteral = "a"_s;
    auto bLiteral = "b"_s;
    auto aLiteral2 = "a"_s;
    EXPECT_FALSE(nullLiteral == emptyLiteral);
    EXPECT_TRUE(nullLiteral < emptyLiteral);
    EXPECT_TRUE(nullLiteral <= emptyLiteral);
    EXPECT_FALSE(nullLiteral > emptyLiteral);
    EXPECT_FALSE(nullLiteral >= emptyLiteral);
    EXPECT_TRUE(nullLiteral == nullLiteral);
    EXPECT_TRUE(nullLiteral == nullLiteral2);
    EXPECT_TRUE(emptyLiteral == emptyLiteral);
    EXPECT_TRUE(emptyLiteral == emptyLiteral2);
    EXPECT_TRUE(nullLiteral < aLiteral);
    EXPECT_TRUE(nullLiteral < bLiteral);
    EXPECT_TRUE(emptyLiteral < aLiteral);
    EXPECT_TRUE(emptyLiteral < bLiteral);
    EXPECT_FALSE(nullLiteral >= aLiteral);
    EXPECT_FALSE(nullLiteral >= bLiteral);
    EXPECT_FALSE(emptyLiteral >= aLiteral);
    EXPECT_FALSE(emptyLiteral >= bLiteral);
    EXPECT_TRUE(aLiteral < bLiteral);
    EXPECT_TRUE(aLiteral <= bLiteral);
    EXPECT_TRUE(bLiteral > aLiteral);
    EXPECT_TRUE(bLiteral >= aLiteral);
    EXPECT_FALSE(bLiteral < aLiteral);
    EXPECT_FALSE(aLiteral < aLiteral2);
    EXPECT_TRUE(aLiteral <= aLiteral2);
    EXPECT_TRUE(aLiteral >= aLiteral2);
}

} // namespace TestWebKitAPI
