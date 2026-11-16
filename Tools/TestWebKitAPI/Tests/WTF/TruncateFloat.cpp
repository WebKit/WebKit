/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <cmath>
#include <limits>
#include <wtf/MathExtras.h>

namespace TestWebKitAPI {

// Prevent constant folding so we exercise the inline-asm path.
template<typename T>
NEVER_INLINE static T opaque(T value) { return value; }

// ---- truncateDoubleToInt32 ----

TEST(WTF_MathExtras, TruncateDoubleToInt32_InRange)
{
    EXPECT_EQ(truncateDoubleToInt32(0.0), 0);
    EXPECT_EQ(truncateDoubleToInt32(-0.0), 0);
    EXPECT_EQ(truncateDoubleToInt32(1.0), 1);
    EXPECT_EQ(truncateDoubleToInt32(-1.0), -1);
    EXPECT_EQ(truncateDoubleToInt32(0.5), 0);
    EXPECT_EQ(truncateDoubleToInt32(-0.5), 0);
    EXPECT_EQ(truncateDoubleToInt32(0.9), 0);
    EXPECT_EQ(truncateDoubleToInt32(-0.9), 0);
    EXPECT_EQ(truncateDoubleToInt32(100.7), 100);
    EXPECT_EQ(truncateDoubleToInt32(-100.7), -100);
    EXPECT_EQ(truncateDoubleToInt32(2147483647.0), INT32_MAX);
    EXPECT_EQ(truncateDoubleToInt32(-2147483648.0), INT32_MIN);
    EXPECT_EQ(truncateDoubleToInt32(std::numeric_limits<double>::epsilon()), 0);
    EXPECT_EQ(truncateDoubleToInt32(std::numeric_limits<double>::denorm_min()), 0);
    EXPECT_EQ(truncateDoubleToInt32(-std::numeric_limits<double>::denorm_min()), 0);
}

TEST(WTF_MathExtras, TruncateDoubleToInt32_InRange_Opaque)
{
    EXPECT_EQ(truncateDoubleToInt32(opaque(0.0)), 0);
    EXPECT_EQ(truncateDoubleToInt32(opaque(-0.0)), 0);
    EXPECT_EQ(truncateDoubleToInt32(opaque(1.0)), 1);
    EXPECT_EQ(truncateDoubleToInt32(opaque(-1.0)), -1);
    EXPECT_EQ(truncateDoubleToInt32(opaque(0.5)), 0);
    EXPECT_EQ(truncateDoubleToInt32(opaque(-0.5)), 0);
    EXPECT_EQ(truncateDoubleToInt32(opaque(0.9)), 0);
    EXPECT_EQ(truncateDoubleToInt32(opaque(-0.9)), 0);
    EXPECT_EQ(truncateDoubleToInt32(opaque(100.7)), 100);
    EXPECT_EQ(truncateDoubleToInt32(opaque(-100.7)), -100);
    EXPECT_EQ(truncateDoubleToInt32(opaque(2147483647.0)), INT32_MAX);
    EXPECT_EQ(truncateDoubleToInt32(opaque(-2147483648.0)), INT32_MIN);
    EXPECT_EQ(truncateDoubleToInt32(opaque(std::numeric_limits<double>::epsilon())), 0);
    EXPECT_EQ(truncateDoubleToInt32(opaque(std::numeric_limits<double>::denorm_min())), 0);
    EXPECT_EQ(truncateDoubleToInt32(opaque(-std::numeric_limits<double>::denorm_min())), 0);
}

TEST(WTF_MathExtras, TruncateDoubleToInt32_OutOfRange)
{
    (void)truncateDoubleToInt32(opaque(2147483648.0));
    (void)truncateDoubleToInt32(opaque(-2147483649.0));
    (void)truncateDoubleToInt32(opaque(std::numeric_limits<double>::quiet_NaN()));
    (void)truncateDoubleToInt32(opaque(std::numeric_limits<double>::signaling_NaN()));
    (void)truncateDoubleToInt32(opaque(-std::numeric_limits<double>::quiet_NaN()));
    (void)truncateDoubleToInt32(opaque(std::numeric_limits<double>::infinity()));
    (void)truncateDoubleToInt32(opaque(-std::numeric_limits<double>::infinity()));
    (void)truncateDoubleToInt32(opaque(std::numeric_limits<double>::max()));
    (void)truncateDoubleToInt32(opaque(std::numeric_limits<double>::lowest()));
    (void)truncateDoubleToInt32(opaque(4294967296.0));
    (void)truncateDoubleToInt32(opaque(static_cast<double>(1ULL << 52)));
    (void)truncateDoubleToInt32(opaque(static_cast<double>(1ULL << 53)));
}

// ---- truncateDoubleToUint32 ----

TEST(WTF_MathExtras, TruncateDoubleToUint32_InRange)
{
    EXPECT_EQ(truncateDoubleToUint32(0.0), 0u);
    EXPECT_EQ(truncateDoubleToUint32(-0.0), 0u);
    EXPECT_EQ(truncateDoubleToUint32(1.0), 1u);
    EXPECT_EQ(truncateDoubleToUint32(0.5), 0u);
    EXPECT_EQ(truncateDoubleToUint32(0.9), 0u);
    EXPECT_EQ(truncateDoubleToUint32(100.7), 100u);
    EXPECT_EQ(truncateDoubleToUint32(4294967295.0), UINT32_MAX);
    EXPECT_EQ(truncateDoubleToUint32(2147483647.0), 2147483647u);
    EXPECT_EQ(truncateDoubleToUint32(2147483648.0), 2147483648u);
    EXPECT_EQ(truncateDoubleToUint32(std::numeric_limits<double>::epsilon()), 0u);
    EXPECT_EQ(truncateDoubleToUint32(std::numeric_limits<double>::denorm_min()), 0u);
}

TEST(WTF_MathExtras, TruncateDoubleToUint32_InRange_Opaque)
{
    EXPECT_EQ(truncateDoubleToUint32(opaque(0.0)), 0u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(-0.0)), 0u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(1.0)), 1u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(0.5)), 0u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(0.9)), 0u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(100.7)), 100u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(4294967295.0)), UINT32_MAX);
    EXPECT_EQ(truncateDoubleToUint32(opaque(2147483647.0)), 2147483647u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(2147483648.0)), 2147483648u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(std::numeric_limits<double>::epsilon())), 0u);
    EXPECT_EQ(truncateDoubleToUint32(opaque(std::numeric_limits<double>::denorm_min())), 0u);
}

TEST(WTF_MathExtras, TruncateDoubleToUint32_OutOfRange)
{
    (void)truncateDoubleToUint32(opaque(-1.0));
    (void)truncateDoubleToUint32(opaque(-0.5));
    (void)truncateDoubleToUint32(opaque(4294967296.0));
    (void)truncateDoubleToUint32(opaque(std::numeric_limits<double>::quiet_NaN()));
    (void)truncateDoubleToUint32(opaque(std::numeric_limits<double>::signaling_NaN()));
    (void)truncateDoubleToUint32(opaque(-std::numeric_limits<double>::quiet_NaN()));
    (void)truncateDoubleToUint32(opaque(std::numeric_limits<double>::infinity()));
    (void)truncateDoubleToUint32(opaque(-std::numeric_limits<double>::infinity()));
    (void)truncateDoubleToUint32(opaque(std::numeric_limits<double>::max()));
    (void)truncateDoubleToUint32(opaque(std::numeric_limits<double>::lowest()));
}

// ---- truncateDoubleToInt64 ----

TEST(WTF_MathExtras, TruncateDoubleToInt64_InRange)
{
    EXPECT_EQ(truncateDoubleToInt64(0.0), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(-0.0), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(1.0), 1LL);
    EXPECT_EQ(truncateDoubleToInt64(-1.0), -1LL);
    EXPECT_EQ(truncateDoubleToInt64(0.5), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(-0.5), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(0.9), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(-0.9), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(2147483647.0), 2147483647LL);
    EXPECT_EQ(truncateDoubleToInt64(-2147483648.0), -2147483648LL);
    EXPECT_EQ(truncateDoubleToInt64(2147483648.0), 2147483648LL);
    EXPECT_EQ(truncateDoubleToInt64(-2147483649.0), -2147483649LL);
    EXPECT_EQ(truncateDoubleToInt64(4294967295.0), 4294967295LL);
    EXPECT_EQ(truncateDoubleToInt64(4294967296.0), 4294967296LL);
    EXPECT_EQ(truncateDoubleToInt64(static_cast<double>(1LL << 52)), (1LL << 52));
    EXPECT_EQ(truncateDoubleToInt64(std::numeric_limits<double>::epsilon()), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(std::numeric_limits<double>::denorm_min()), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(-std::numeric_limits<double>::denorm_min()), 0LL);
}

TEST(WTF_MathExtras, TruncateDoubleToInt64_InRange_Opaque)
{
    EXPECT_EQ(truncateDoubleToInt64(opaque(0.0)), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(-0.0)), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(1.0)), 1LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(-1.0)), -1LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(0.5)), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(-0.5)), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(0.9)), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(-0.9)), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(2147483647.0)), 2147483647LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(-2147483648.0)), -2147483648LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(2147483648.0)), 2147483648LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(-2147483649.0)), -2147483649LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(4294967295.0)), 4294967295LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(4294967296.0)), 4294967296LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(static_cast<double>(1LL << 52))), (1LL << 52));
    EXPECT_EQ(truncateDoubleToInt64(opaque(std::numeric_limits<double>::epsilon())), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(std::numeric_limits<double>::denorm_min())), 0LL);
    EXPECT_EQ(truncateDoubleToInt64(opaque(-std::numeric_limits<double>::denorm_min())), 0LL);
}

TEST(WTF_MathExtras, TruncateDoubleToInt64_OutOfRange)
{
    (void)truncateDoubleToInt64(opaque(static_cast<double>(INT64_MAX)));
    (void)truncateDoubleToInt64(opaque(static_cast<double>(INT64_MIN)));
    (void)truncateDoubleToInt64(opaque(std::numeric_limits<double>::quiet_NaN()));
    (void)truncateDoubleToInt64(opaque(std::numeric_limits<double>::signaling_NaN()));
    (void)truncateDoubleToInt64(opaque(-std::numeric_limits<double>::quiet_NaN()));
    (void)truncateDoubleToInt64(opaque(std::numeric_limits<double>::infinity()));
    (void)truncateDoubleToInt64(opaque(-std::numeric_limits<double>::infinity()));
    (void)truncateDoubleToInt64(opaque(std::numeric_limits<double>::max()));
    (void)truncateDoubleToInt64(opaque(std::numeric_limits<double>::lowest()));
    (void)truncateDoubleToInt64(opaque(static_cast<double>(1ULL << 53)));
}

// ---- truncateDoubleToUint64 ----

TEST(WTF_MathExtras, TruncateDoubleToUint64_InRange)
{
    EXPECT_EQ(truncateDoubleToUint64(0.0), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(-0.0), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(1.0), 1ULL);
    EXPECT_EQ(truncateDoubleToUint64(0.5), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(0.9), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(4294967295.0), 4294967295ULL);
    EXPECT_EQ(truncateDoubleToUint64(4294967296.0), 4294967296ULL);
    EXPECT_EQ(truncateDoubleToUint64(static_cast<double>(1ULL << 52)), (1ULL << 52));
    EXPECT_EQ(truncateDoubleToUint64(std::numeric_limits<double>::epsilon()), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(std::numeric_limits<double>::denorm_min()), 0ULL);
}

TEST(WTF_MathExtras, TruncateDoubleToUint64_InRange_Opaque)
{
    EXPECT_EQ(truncateDoubleToUint64(opaque(0.0)), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(opaque(-0.0)), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(opaque(1.0)), 1ULL);
    EXPECT_EQ(truncateDoubleToUint64(opaque(0.5)), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(opaque(0.9)), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(opaque(4294967295.0)), 4294967295ULL);
    EXPECT_EQ(truncateDoubleToUint64(opaque(4294967296.0)), 4294967296ULL);
    EXPECT_EQ(truncateDoubleToUint64(opaque(static_cast<double>(1ULL << 52))), (1ULL << 52));
    EXPECT_EQ(truncateDoubleToUint64(opaque(std::numeric_limits<double>::epsilon())), 0ULL);
    EXPECT_EQ(truncateDoubleToUint64(opaque(std::numeric_limits<double>::denorm_min())), 0ULL);
}

TEST(WTF_MathExtras, TruncateDoubleToUint64_OutOfRange)
{
    (void)truncateDoubleToUint64(opaque(-1.0));
    (void)truncateDoubleToUint64(opaque(-0.5));
    (void)truncateDoubleToUint64(opaque(std::numeric_limits<double>::quiet_NaN()));
    (void)truncateDoubleToUint64(opaque(std::numeric_limits<double>::signaling_NaN()));
    (void)truncateDoubleToUint64(opaque(-std::numeric_limits<double>::quiet_NaN()));
    (void)truncateDoubleToUint64(opaque(std::numeric_limits<double>::infinity()));
    (void)truncateDoubleToUint64(opaque(-std::numeric_limits<double>::infinity()));
    (void)truncateDoubleToUint64(opaque(std::numeric_limits<double>::max()));
    (void)truncateDoubleToUint64(opaque(std::numeric_limits<double>::lowest()));
}

// ---- truncateFloatToInt32 ----

TEST(WTF_MathExtras, TruncateFloatToInt32_InRange)
{
    EXPECT_EQ(truncateFloatToInt32(0.0f), 0);
    EXPECT_EQ(truncateFloatToInt32(-0.0f), 0);
    EXPECT_EQ(truncateFloatToInt32(1.0f), 1);
    EXPECT_EQ(truncateFloatToInt32(-1.0f), -1);
    EXPECT_EQ(truncateFloatToInt32(0.5f), 0);
    EXPECT_EQ(truncateFloatToInt32(-0.5f), 0);
    EXPECT_EQ(truncateFloatToInt32(0.9f), 0);
    EXPECT_EQ(truncateFloatToInt32(-0.9f), 0);
    EXPECT_EQ(truncateFloatToInt32(100.7f), 100);
    EXPECT_EQ(truncateFloatToInt32(-100.7f), -100);
    EXPECT_EQ(truncateFloatToInt32(std::numeric_limits<float>::epsilon()), 0);
    EXPECT_EQ(truncateFloatToInt32(std::numeric_limits<float>::denorm_min()), 0);
    EXPECT_EQ(truncateFloatToInt32(-std::numeric_limits<float>::denorm_min()), 0);
}

TEST(WTF_MathExtras, TruncateFloatToInt32_InRange_Opaque)
{
    EXPECT_EQ(truncateFloatToInt32(opaque(0.0f)), 0);
    EXPECT_EQ(truncateFloatToInt32(opaque(-0.0f)), 0);
    EXPECT_EQ(truncateFloatToInt32(opaque(1.0f)), 1);
    EXPECT_EQ(truncateFloatToInt32(opaque(-1.0f)), -1);
    EXPECT_EQ(truncateFloatToInt32(opaque(0.5f)), 0);
    EXPECT_EQ(truncateFloatToInt32(opaque(-0.5f)), 0);
    EXPECT_EQ(truncateFloatToInt32(opaque(0.9f)), 0);
    EXPECT_EQ(truncateFloatToInt32(opaque(-0.9f)), 0);
    EXPECT_EQ(truncateFloatToInt32(opaque(100.7f)), 100);
    EXPECT_EQ(truncateFloatToInt32(opaque(-100.7f)), -100);
    EXPECT_EQ(truncateFloatToInt32(opaque(std::numeric_limits<float>::epsilon())), 0);
    EXPECT_EQ(truncateFloatToInt32(opaque(std::numeric_limits<float>::denorm_min())), 0);
    EXPECT_EQ(truncateFloatToInt32(opaque(-std::numeric_limits<float>::denorm_min())), 0);
}

TEST(WTF_MathExtras, TruncateFloatToInt32_OutOfRange)
{
    (void)truncateFloatToInt32(opaque(static_cast<float>(INT32_MAX) + 1.0f));
    (void)truncateFloatToInt32(opaque(static_cast<float>(INT32_MIN) - 1.0f));
    (void)truncateFloatToInt32(opaque(std::numeric_limits<float>::quiet_NaN()));
    (void)truncateFloatToInt32(opaque(std::numeric_limits<float>::signaling_NaN()));
    (void)truncateFloatToInt32(opaque(-std::numeric_limits<float>::quiet_NaN()));
    (void)truncateFloatToInt32(opaque(std::numeric_limits<float>::infinity()));
    (void)truncateFloatToInt32(opaque(-std::numeric_limits<float>::infinity()));
    (void)truncateFloatToInt32(opaque(std::numeric_limits<float>::max()));
    (void)truncateFloatToInt32(opaque(std::numeric_limits<float>::lowest()));
}

// ---- truncateFloatToUint32 ----

TEST(WTF_MathExtras, TruncateFloatToUint32_InRange)
{
    EXPECT_EQ(truncateFloatToUint32(0.0f), 0u);
    EXPECT_EQ(truncateFloatToUint32(-0.0f), 0u);
    EXPECT_EQ(truncateFloatToUint32(1.0f), 1u);
    EXPECT_EQ(truncateFloatToUint32(0.5f), 0u);
    EXPECT_EQ(truncateFloatToUint32(0.9f), 0u);
    EXPECT_EQ(truncateFloatToUint32(100.7f), 100u);
    EXPECT_EQ(truncateFloatToUint32(std::numeric_limits<float>::epsilon()), 0u);
    EXPECT_EQ(truncateFloatToUint32(std::numeric_limits<float>::denorm_min()), 0u);
}

TEST(WTF_MathExtras, TruncateFloatToUint32_InRange_Opaque)
{
    EXPECT_EQ(truncateFloatToUint32(opaque(0.0f)), 0u);
    EXPECT_EQ(truncateFloatToUint32(opaque(-0.0f)), 0u);
    EXPECT_EQ(truncateFloatToUint32(opaque(1.0f)), 1u);
    EXPECT_EQ(truncateFloatToUint32(opaque(0.5f)), 0u);
    EXPECT_EQ(truncateFloatToUint32(opaque(0.9f)), 0u);
    EXPECT_EQ(truncateFloatToUint32(opaque(100.7f)), 100u);
    EXPECT_EQ(truncateFloatToUint32(opaque(std::numeric_limits<float>::epsilon())), 0u);
    EXPECT_EQ(truncateFloatToUint32(opaque(std::numeric_limits<float>::denorm_min())), 0u);
}

TEST(WTF_MathExtras, TruncateFloatToUint32_OutOfRange)
{
    (void)truncateFloatToUint32(opaque(-1.0f));
    (void)truncateFloatToUint32(opaque(-0.5f));
    (void)truncateFloatToUint32(opaque(std::numeric_limits<float>::quiet_NaN()));
    (void)truncateFloatToUint32(opaque(std::numeric_limits<float>::signaling_NaN()));
    (void)truncateFloatToUint32(opaque(-std::numeric_limits<float>::quiet_NaN()));
    (void)truncateFloatToUint32(opaque(std::numeric_limits<float>::infinity()));
    (void)truncateFloatToUint32(opaque(-std::numeric_limits<float>::infinity()));
    (void)truncateFloatToUint32(opaque(std::numeric_limits<float>::max()));
    (void)truncateFloatToUint32(opaque(std::numeric_limits<float>::lowest()));
}

// ---- truncateFloatToInt64 ----

TEST(WTF_MathExtras, TruncateFloatToInt64_InRange)
{
    EXPECT_EQ(truncateFloatToInt64(0.0f), 0LL);
    EXPECT_EQ(truncateFloatToInt64(-0.0f), 0LL);
    EXPECT_EQ(truncateFloatToInt64(1.0f), 1LL);
    EXPECT_EQ(truncateFloatToInt64(-1.0f), -1LL);
    EXPECT_EQ(truncateFloatToInt64(0.5f), 0LL);
    EXPECT_EQ(truncateFloatToInt64(-0.5f), 0LL);
    EXPECT_EQ(truncateFloatToInt64(100.7f), 100LL);
    EXPECT_EQ(truncateFloatToInt64(-100.7f), -100LL);
    EXPECT_EQ(truncateFloatToInt64(std::numeric_limits<float>::epsilon()), 0LL);
    EXPECT_EQ(truncateFloatToInt64(std::numeric_limits<float>::denorm_min()), 0LL);
    EXPECT_EQ(truncateFloatToInt64(-std::numeric_limits<float>::denorm_min()), 0LL);
}

TEST(WTF_MathExtras, TruncateFloatToInt64_InRange_Opaque)
{
    EXPECT_EQ(truncateFloatToInt64(opaque(0.0f)), 0LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(-0.0f)), 0LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(1.0f)), 1LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(-1.0f)), -1LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(0.5f)), 0LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(-0.5f)), 0LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(100.7f)), 100LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(-100.7f)), -100LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(std::numeric_limits<float>::epsilon())), 0LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(std::numeric_limits<float>::denorm_min())), 0LL);
    EXPECT_EQ(truncateFloatToInt64(opaque(-std::numeric_limits<float>::denorm_min())), 0LL);
}

TEST(WTF_MathExtras, TruncateFloatToInt64_OutOfRange)
{
    (void)truncateFloatToInt64(opaque(static_cast<float>(INT64_MAX)));
    (void)truncateFloatToInt64(opaque(static_cast<float>(INT64_MIN)));
    (void)truncateFloatToInt64(opaque(std::numeric_limits<float>::quiet_NaN()));
    (void)truncateFloatToInt64(opaque(std::numeric_limits<float>::signaling_NaN()));
    (void)truncateFloatToInt64(opaque(-std::numeric_limits<float>::quiet_NaN()));
    (void)truncateFloatToInt64(opaque(std::numeric_limits<float>::infinity()));
    (void)truncateFloatToInt64(opaque(-std::numeric_limits<float>::infinity()));
    (void)truncateFloatToInt64(opaque(std::numeric_limits<float>::max()));
    (void)truncateFloatToInt64(opaque(std::numeric_limits<float>::lowest()));
}

// ---- truncateFloatToUint64 ----

TEST(WTF_MathExtras, TruncateFloatToUint64_InRange)
{
    EXPECT_EQ(truncateFloatToUint64(0.0f), 0ULL);
    EXPECT_EQ(truncateFloatToUint64(-0.0f), 0ULL);
    EXPECT_EQ(truncateFloatToUint64(1.0f), 1ULL);
    EXPECT_EQ(truncateFloatToUint64(0.5f), 0ULL);
    EXPECT_EQ(truncateFloatToUint64(100.7f), 100ULL);
    EXPECT_EQ(truncateFloatToUint64(std::numeric_limits<float>::epsilon()), 0ULL);
    EXPECT_EQ(truncateFloatToUint64(std::numeric_limits<float>::denorm_min()), 0ULL);
}

TEST(WTF_MathExtras, TruncateFloatToUint64_InRange_Opaque)
{
    EXPECT_EQ(truncateFloatToUint64(opaque(0.0f)), 0ULL);
    EXPECT_EQ(truncateFloatToUint64(opaque(-0.0f)), 0ULL);
    EXPECT_EQ(truncateFloatToUint64(opaque(1.0f)), 1ULL);
    EXPECT_EQ(truncateFloatToUint64(opaque(0.5f)), 0ULL);
    EXPECT_EQ(truncateFloatToUint64(opaque(100.7f)), 100ULL);
    EXPECT_EQ(truncateFloatToUint64(opaque(std::numeric_limits<float>::epsilon())), 0ULL);
    EXPECT_EQ(truncateFloatToUint64(opaque(std::numeric_limits<float>::denorm_min())), 0ULL);
}

TEST(WTF_MathExtras, TruncateFloatToUint64_OutOfRange)
{
    (void)truncateFloatToUint64(opaque(-1.0f));
    (void)truncateFloatToUint64(opaque(-0.5f));
    (void)truncateFloatToUint64(opaque(std::numeric_limits<float>::quiet_NaN()));
    (void)truncateFloatToUint64(opaque(std::numeric_limits<float>::signaling_NaN()));
    (void)truncateFloatToUint64(opaque(-std::numeric_limits<float>::quiet_NaN()));
    (void)truncateFloatToUint64(opaque(std::numeric_limits<float>::infinity()));
    (void)truncateFloatToUint64(opaque(-std::numeric_limits<float>::infinity()));
    (void)truncateFloatToUint64(opaque(std::numeric_limits<float>::max()));
    (void)truncateFloatToUint64(opaque(std::numeric_limits<float>::lowest()));
}

// ---- tryConvertToStrictInt32 ----
// This must produce identical results across all architectures.

TEST(WTF_MathExtras, TryConvertToStrictInt32_ExactIntegers)
{
    EXPECT_EQ(tryConvertToStrictInt32(0.0), std::optional<int32_t>(0));
    EXPECT_EQ(tryConvertToStrictInt32(1.0), std::optional<int32_t>(1));
    EXPECT_EQ(tryConvertToStrictInt32(-1.0), std::optional<int32_t>(-1));
    EXPECT_EQ(tryConvertToStrictInt32(42.0), std::optional<int32_t>(42));
    EXPECT_EQ(tryConvertToStrictInt32(-42.0), std::optional<int32_t>(-42));
    EXPECT_EQ(tryConvertToStrictInt32(static_cast<double>(INT32_MAX)), std::optional<int32_t>(INT32_MAX));
    EXPECT_EQ(tryConvertToStrictInt32(static_cast<double>(INT32_MIN)), std::optional<int32_t>(INT32_MIN));
}

TEST(WTF_MathExtras, TryConvertToStrictInt32_ExactIntegers_Opaque)
{
    EXPECT_EQ(tryConvertToStrictInt32(opaque(0.0)), std::optional<int32_t>(0));
    EXPECT_EQ(tryConvertToStrictInt32(opaque(1.0)), std::optional<int32_t>(1));
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-1.0)), std::optional<int32_t>(-1));
    EXPECT_EQ(tryConvertToStrictInt32(opaque(42.0)), std::optional<int32_t>(42));
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-42.0)), std::optional<int32_t>(-42));
    EXPECT_EQ(tryConvertToStrictInt32(opaque(static_cast<double>(INT32_MAX))), std::optional<int32_t>(INT32_MAX));
    EXPECT_EQ(tryConvertToStrictInt32(opaque(static_cast<double>(INT32_MIN))), std::optional<int32_t>(INT32_MIN));
}

TEST(WTF_MathExtras, TryConvertToStrictInt32_NonIntegers)
{
    EXPECT_EQ(tryConvertToStrictInt32(0.5), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(-0.5), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(1.1), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(-1.1), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(0.1), std::nullopt);
}

TEST(WTF_MathExtras, TryConvertToStrictInt32_NonIntegers_Opaque)
{
    EXPECT_EQ(tryConvertToStrictInt32(opaque(0.5)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-0.5)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(1.1)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-1.1)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(0.1)), std::nullopt);
}

TEST(WTF_MathExtras, TryConvertToStrictInt32_SpecialValues)
{
    // -0.0 is not StrictInt32.
    EXPECT_EQ(tryConvertToStrictInt32(-0.0), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(std::numeric_limits<double>::quiet_NaN()), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(std::numeric_limits<double>::signaling_NaN()), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(-std::numeric_limits<double>::quiet_NaN()), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(std::numeric_limits<double>::infinity()), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(-std::numeric_limits<double>::infinity()), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(std::numeric_limits<double>::denorm_min()), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(-std::numeric_limits<double>::denorm_min()), std::nullopt);
}

TEST(WTF_MathExtras, TryConvertToStrictInt32_SpecialValues_Opaque)
{
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-0.0)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(std::numeric_limits<double>::quiet_NaN())), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(std::numeric_limits<double>::signaling_NaN())), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-std::numeric_limits<double>::quiet_NaN())), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(std::numeric_limits<double>::infinity())), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-std::numeric_limits<double>::infinity())), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(std::numeric_limits<double>::denorm_min())), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-std::numeric_limits<double>::denorm_min())), std::nullopt);
}

TEST(WTF_MathExtras, TryConvertToStrictInt32_OutOfRange)
{
    EXPECT_EQ(tryConvertToStrictInt32(static_cast<double>(INT32_MAX) + 1.0), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(static_cast<double>(INT32_MIN) - 1.0), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(static_cast<double>(1ULL << 53)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(-static_cast<double>(1ULL << 53)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(std::numeric_limits<double>::max()), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(std::numeric_limits<double>::lowest()), std::nullopt);
}

TEST(WTF_MathExtras, TryConvertToStrictInt32_OutOfRange_Opaque)
{
    EXPECT_EQ(tryConvertToStrictInt32(opaque(static_cast<double>(INT32_MAX) + 1.0)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(static_cast<double>(INT32_MIN) - 1.0)), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(static_cast<double>(1ULL << 53))), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(-static_cast<double>(1ULL << 53))), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(std::numeric_limits<double>::max())), std::nullopt);
    EXPECT_EQ(tryConvertToStrictInt32(opaque(std::numeric_limits<double>::lowest())), std::nullopt);
}

} // namespace TestWebKitAPI
