/*
 * Copyright (C) 2017 Igalia, S.L. All rights reserved.
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

#include <WebCore/GridPosition.h>

namespace TestWebKitAPI {

TEST(GridPositionTest, GridPositionLimits)
{

    WebCore::GridPosition gridPosition;

    gridPosition.setExplicitPosition(999999, "");
    EXPECT_EQ(gridPosition.integerPosition(), 999999);
    gridPosition.setExplicitPosition(1000000, "");
    EXPECT_EQ(gridPosition.integerPosition(), 1000000);
    gridPosition.setExplicitPosition(1000001, "");
    EXPECT_EQ(gridPosition.integerPosition(), 1000000);
    gridPosition.setExplicitPosition(INT_MAX, "");
    EXPECT_EQ(gridPosition.integerPosition(), 1000000);
    gridPosition.setExplicitPosition(-999999, "");
    EXPECT_EQ(gridPosition.integerPosition(), -999999);
    gridPosition.setExplicitPosition(-1000000, "");
    EXPECT_EQ(gridPosition.integerPosition(), -1000000);
    gridPosition.setExplicitPosition(-1000001, "");
    EXPECT_EQ(gridPosition.integerPosition(), -1000000);
    gridPosition.setExplicitPosition(INT_MIN, "");
    EXPECT_EQ(gridPosition.integerPosition(), -1000000);

    gridPosition.setSpanPosition(999999, "");
    EXPECT_EQ(gridPosition.spanPosition(), 999999);
    gridPosition.setSpanPosition(1000000, "");
    EXPECT_EQ(gridPosition.spanPosition(), 1000000);
    gridPosition.setSpanPosition(1000001, "");
    EXPECT_EQ(gridPosition.spanPosition(), 1000000);
    gridPosition.setSpanPosition(INT_MAX, "");
    EXPECT_EQ(gridPosition.spanPosition(), 1000000);
    gridPosition.setSpanPosition(-999999, "");
    EXPECT_EQ(gridPosition.spanPosition(), -999999);
    gridPosition.setSpanPosition(-1000000, "");
    EXPECT_EQ(gridPosition.spanPosition(), -1000000);
    gridPosition.setSpanPosition(-1000001, "");
    EXPECT_EQ(gridPosition.spanPosition(), -1000000);
    gridPosition.setSpanPosition(INT_MIN, "");
    EXPECT_EQ(gridPosition.spanPosition(), -1000000);

}

} // namespace TestWebKitAPI
