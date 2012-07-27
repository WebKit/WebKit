/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "MemoryInfo.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(MemoryInfo, quantizeMemorySize)
{
    EXPECT_EQ(10000000u, quantizeMemorySize(1024));
    EXPECT_EQ(10000000u, quantizeMemorySize(1024 * 1024));
    EXPECT_EQ(410000000u, quantizeMemorySize(389472983));
    EXPECT_EQ(39600000u, quantizeMemorySize(38947298));
    EXPECT_EQ(29400000u, quantizeMemorySize(28947298));
    EXPECT_EQ(19300000u, quantizeMemorySize(18947298));
    EXPECT_EQ(14300000u, quantizeMemorySize(13947298));
    EXPECT_EQ(10000000u, quantizeMemorySize(3894729));
    EXPECT_EQ(10000000u, quantizeMemorySize(389472));
    EXPECT_EQ(10000000u, quantizeMemorySize(38947));
    EXPECT_EQ(10000000u, quantizeMemorySize(3894));
    EXPECT_EQ(10000000u, quantizeMemorySize(389));
    EXPECT_EQ(10000000u, quantizeMemorySize(38));
    EXPECT_EQ(10000000u, quantizeMemorySize(3));
    EXPECT_EQ(10000000u, quantizeMemorySize(1));
    EXPECT_EQ(10000000u, quantizeMemorySize(0));
}

} // namespace
