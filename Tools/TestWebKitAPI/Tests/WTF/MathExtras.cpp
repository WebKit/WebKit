/*
 * Copyright (C) 2012 Intel Corporation
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

#include <wtf/MathExtras.h>

namespace TestWebKitAPI {

TEST(WTF, Lrint)
{
    EXPECT_EQ(lrint(-7.5), -8);
    EXPECT_EQ(lrint(-8.5), -8);
    EXPECT_EQ(lrint(-0.5), 0);
    EXPECT_EQ(lrint(0.5), 0);
    EXPECT_EQ(lrint(-0.5), 0);
    EXPECT_EQ(lrint(1.3), 1);
    EXPECT_EQ(lrint(1.7), 2);
    EXPECT_EQ(lrint(0), 0);
    EXPECT_EQ(lrint(-0), 0);
    // Largest double number with 0.5 precision and one halfway rounding case below.
    EXPECT_EQ(lrint(pow(2.0, 52) - 0.5), pow(2.0, 52));
    EXPECT_EQ(lrint(pow(2.0, 52) - 1.5), pow(2.0, 52) - 2);
    // Smallest double number with 0.5 precision and one halfway rounding case above.
    EXPECT_EQ(lrint(-pow(2.0, 52) + 0.5), -pow(2.0, 52));
    EXPECT_EQ(lrint(-pow(2.0, 52) + 1.5), -pow(2.0, 52) + 2);
}

} // namespace TestWebKitAPI
