/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/MediaTime.h>

using namespace std;

namespace WTF {

std::ostream& operator<<(std::ostream& out, const MediaTime& val)
{
    out << "{ ";
    if (val.isInvalid())
        out << "invalid";
    else if (val.isPositiveInfinite())
        out << "+infinite";
    else if (val.isNegativeInfinite())
        out << "-infinite";
    else if (val.hasDoubleValue())
        out << "double: " << val.toDouble();
    else
        out << "value: " << val.timeValue() << ", scale: " << val.timeScale();
    return out << " }";
}

}

namespace TestWebKitAPI {

TEST(WTF, MediaTime)
{
    // Comparison Operators
    EXPECT_EQ(MediaTime::positiveInfiniteTime() > MediaTime::negativeInfiniteTime(), true);
    EXPECT_EQ(MediaTime::negativeInfiniteTime() < MediaTime::positiveInfiniteTime(), true);
    EXPECT_EQ(MediaTime::negativeInfiniteTime() == MediaTime::negativeInfiniteTime(), true);
    EXPECT_EQ(MediaTime::positiveInfiniteTime() == MediaTime::positiveInfiniteTime(), true);
    EXPECT_EQ(MediaTime::positiveInfiniteTime() != MediaTime::negativeInfiniteTime(), true);
    EXPECT_EQ(MediaTime::invalidTime() == MediaTime::invalidTime(), true);
    EXPECT_EQ(MediaTime::invalidTime() != MediaTime::invalidTime(), false);
    EXPECT_EQ(MediaTime::invalidTime() != MediaTime::zeroTime(), true);
    EXPECT_EQ(MediaTime::invalidTime() > MediaTime::negativeInfiniteTime(), true);
    EXPECT_EQ(MediaTime::invalidTime() > MediaTime::positiveInfiniteTime(), true);
    EXPECT_EQ(MediaTime::negativeInfiniteTime() < MediaTime::invalidTime(), true);
    EXPECT_EQ(MediaTime::positiveInfiniteTime() < MediaTime::invalidTime(), true);
    EXPECT_EQ(MediaTime::indefiniteTime() == MediaTime::indefiniteTime(), true);
    EXPECT_EQ(MediaTime::indefiniteTime() != MediaTime::indefiniteTime(), false);
    EXPECT_EQ(MediaTime::indefiniteTime() != MediaTime::zeroTime(), true);
    EXPECT_EQ(MediaTime::indefiniteTime() > MediaTime::negativeInfiniteTime(), true);
    EXPECT_EQ(MediaTime::indefiniteTime() < MediaTime::positiveInfiniteTime(), true);
    EXPECT_EQ(MediaTime::negativeInfiniteTime() < MediaTime::indefiniteTime(), true);
    EXPECT_EQ(MediaTime::positiveInfiniteTime() > MediaTime::indefiniteTime(), true);
    EXPECT_EQ(MediaTime(1, 1) < MediaTime::indefiniteTime(), true);
    EXPECT_EQ(MediaTime::indefiniteTime() > MediaTime(1, 1), true);
    EXPECT_EQ(MediaTime(1, 1) < MediaTime(2, 1), true);
    EXPECT_EQ(MediaTime(2, 1) > MediaTime(1, 1), true);
    EXPECT_EQ(MediaTime(1, 1) != MediaTime(2, 1), true);
    EXPECT_EQ(MediaTime(2, 1) == MediaTime(2, 1), true);
    EXPECT_EQ(MediaTime(2, 1) == MediaTime(4, 2), true);
    EXPECT_EQ(MediaTime(-2, 1) < MediaTime(-1, 1), true);
    EXPECT_EQ(MediaTime(-2, 1) <= MediaTime(-1, 1), true);
    EXPECT_EQ(MediaTime(-1, 1) < MediaTime(-2, 1), false);
    EXPECT_EQ(MediaTime(-1, 1) < MediaTime(2, 1), true);
    EXPECT_EQ(MediaTime(1, 1) > MediaTime(-2, 1), true);
    EXPECT_EQ(MediaTime(-8, 10) >= MediaTime(-1, 1), true);
    EXPECT_TRUE((bool)MediaTime(1, 1));
    EXPECT_TRUE(!MediaTime(0, 1));
    EXPECT_TRUE(!MediaTime::invalidTime());
    EXPECT_TRUE(!MediaTime::createWithDouble(0.0, 1));
    EXPECT_FALSE(!MediaTime(1, 1));
    EXPECT_FALSE((bool)MediaTime::invalidTime());

    // Addition Operators
    EXPECT_EQ(MediaTime::positiveInfiniteTime() + MediaTime::positiveInfiniteTime(), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::negativeInfiniteTime() + MediaTime::negativeInfiniteTime(), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::positiveInfiniteTime() + MediaTime::negativeInfiniteTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::negativeInfiniteTime() + MediaTime::positiveInfiniteTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::positiveInfiniteTime() + MediaTime(1, 1), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime(1, 1) + MediaTime::positiveInfiniteTime(), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::negativeInfiniteTime() + MediaTime(1, 1), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime(1, 1) + MediaTime::negativeInfiniteTime(), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::invalidTime() + MediaTime::positiveInfiniteTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::invalidTime() + MediaTime::negativeInfiniteTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::invalidTime() + MediaTime::invalidTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::invalidTime() + MediaTime(1, 1), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::indefiniteTime() + MediaTime::positiveInfiniteTime(), MediaTime::indefiniteTime());
    EXPECT_EQ(MediaTime::indefiniteTime() + MediaTime::negativeInfiniteTime(), MediaTime::indefiniteTime());
    EXPECT_EQ(MediaTime::indefiniteTime() + MediaTime::indefiniteTime(), MediaTime::indefiniteTime());
    EXPECT_EQ(MediaTime::indefiniteTime() + MediaTime(1, 1), MediaTime::indefiniteTime());
    EXPECT_EQ(MediaTime(1, 1) + MediaTime(1, 1), MediaTime(2, 1));
    EXPECT_EQ(MediaTime(1, 2) + MediaTime(1, 3), MediaTime(5, 6));
    EXPECT_EQ(MediaTime(1, MediaTime::MaximumTimeScale - 1) + MediaTime(1, MediaTime::MaximumTimeScale - 2), MediaTime(2, MediaTime::MaximumTimeScale));

    // Subtraction Operators
    EXPECT_EQ(MediaTime::positiveInfiniteTime() - MediaTime::positiveInfiniteTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::negativeInfiniteTime() - MediaTime::negativeInfiniteTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::positiveInfiniteTime() - MediaTime::negativeInfiniteTime(), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::negativeInfiniteTime() - MediaTime::positiveInfiniteTime(), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::positiveInfiniteTime() - MediaTime(1, 1), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime(1, 1) - MediaTime::positiveInfiniteTime(), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::negativeInfiniteTime() - MediaTime(1, 1), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime(1, 1) - MediaTime::negativeInfiniteTime(), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::invalidTime() - MediaTime::positiveInfiniteTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::invalidTime() - MediaTime::negativeInfiniteTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::invalidTime() - MediaTime::invalidTime(), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::invalidTime() - MediaTime(1, 1), MediaTime::invalidTime());
    EXPECT_EQ(MediaTime::indefiniteTime() - MediaTime::positiveInfiniteTime(), MediaTime::indefiniteTime());
    EXPECT_EQ(MediaTime::indefiniteTime() - MediaTime::negativeInfiniteTime(), MediaTime::indefiniteTime());
    EXPECT_EQ(MediaTime::indefiniteTime() - MediaTime::indefiniteTime(), MediaTime::indefiniteTime());
    EXPECT_EQ(MediaTime::indefiniteTime() - MediaTime(1, 1), MediaTime::indefiniteTime());
    EXPECT_EQ(MediaTime(3, 1) - MediaTime(2, 1), MediaTime(1, 1));
    EXPECT_EQ(MediaTime(1, 2) - MediaTime(1, 3), MediaTime(1, 6));
    EXPECT_EQ(MediaTime(2, MediaTime::MaximumTimeScale - 1) - MediaTime(1, MediaTime::MaximumTimeScale - 2), MediaTime(1, MediaTime::MaximumTimeScale));

    // Multiplication Operators
    EXPECT_EQ(MediaTime::positiveInfiniteTime(), MediaTime::positiveInfiniteTime() * 2);
    EXPECT_EQ(MediaTime::negativeInfiniteTime(), MediaTime::negativeInfiniteTime() * 2);
    EXPECT_EQ(MediaTime::negativeInfiniteTime(), MediaTime::positiveInfiniteTime() * -2);
    EXPECT_EQ(MediaTime::positiveInfiniteTime(), MediaTime::negativeInfiniteTime() * -2);
    EXPECT_EQ(MediaTime::invalidTime(), MediaTime::invalidTime() * 2);
    EXPECT_EQ(MediaTime::invalidTime(), MediaTime::invalidTime() * -2);
    EXPECT_EQ(MediaTime::indefiniteTime(), MediaTime::indefiniteTime() * 2);
    EXPECT_EQ(MediaTime::indefiniteTime(), MediaTime::indefiniteTime() * -2);
    EXPECT_EQ(MediaTime(6, 1), MediaTime(3, 1) * 2);
    EXPECT_EQ(MediaTime(0, 1), MediaTime(0, 1) * 2);
    EXPECT_EQ(MediaTime(int64_t(1) << 60, 1), MediaTime(int64_t(1) << 60, 2) * 2);
    EXPECT_EQ(MediaTime::positiveInfiniteTime(), MediaTime(numeric_limits<int64_t>::max(), 1) * 2);
    EXPECT_EQ(MediaTime::positiveInfiniteTime(), MediaTime(numeric_limits<int64_t>::min(), 1) * -2);
    EXPECT_EQ(MediaTime::negativeInfiniteTime(), MediaTime(numeric_limits<int64_t>::max(), 1) * -2);
    EXPECT_EQ(MediaTime::negativeInfiniteTime(), MediaTime(numeric_limits<int64_t>::min(), 1) * 2);

    // Constants
    EXPECT_EQ(MediaTime::zeroTime(), MediaTime(0, 1));
    EXPECT_EQ(MediaTime::invalidTime(), MediaTime(-1, 1, 0));
    EXPECT_EQ(MediaTime::positiveInfiniteTime(), MediaTime(0, 1, MediaTime::PositiveInfinite));
    EXPECT_EQ(MediaTime::negativeInfiniteTime(), MediaTime(0, 1, MediaTime::NegativeInfinite));
    EXPECT_EQ(MediaTime::indefiniteTime(), MediaTime(0, 1, MediaTime::Indefinite));

    // Absolute Functions
    EXPECT_EQ(abs(MediaTime::positiveInfiniteTime()), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(abs(MediaTime::negativeInfiniteTime()), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(abs(MediaTime::invalidTime()), MediaTime::invalidTime());
    EXPECT_EQ(abs(MediaTime(1, 1)), MediaTime(1, 1));
    EXPECT_EQ(abs(MediaTime(-1, 1)), MediaTime(1, 1));

    // Floating Point Functions
    EXPECT_EQ(MediaTime::createWithFloat(1.0f), MediaTime(1, 1));
    EXPECT_EQ(MediaTime::createWithFloat(1.5f), MediaTime(3, 2));
    EXPECT_EQ(MediaTime::createWithDouble(1.0), MediaTime(1, 1));
    EXPECT_EQ(MediaTime::createWithDouble(1.5), MediaTime(3, 2));
    EXPECT_EQ(MediaTime(1, 1).toFloat(), 1.0f);
    EXPECT_EQ(MediaTime(3, 2).toFloat(), 1.5f);
    EXPECT_EQ(MediaTime(1, 1).toDouble(), 1.0);
    EXPECT_EQ(MediaTime(3, 2).toDouble(), 1.5);
    EXPECT_EQ(MediaTime(1, 1 << 16).toFloat(), 1 / pow(2.0f, 16.0f));
    EXPECT_EQ(MediaTime(1, 1 << 30).toDouble(), 1 / pow(2.0, 30.0));
    EXPECT_EQ(MediaTime::createWithDouble(piDouble, 1 << 30), MediaTime(3373259426U, 1 << 30));

    EXPECT_EQ(MediaTime::createWithFloat(std::numeric_limits<float>::infinity()), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithFloat(-std::numeric_limits<float>::infinity()), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithFloat(std::numeric_limits<float>::quiet_NaN()), MediaTime::invalidTime());

    EXPECT_EQ(MediaTime::createWithDouble(std::numeric_limits<double>::infinity()), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(-std::numeric_limits<double>::infinity()), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(std::numeric_limits<double>::quiet_NaN()), MediaTime::invalidTime());

    // Floating Point Round Trip
    EXPECT_EQ(10.0123456789f, MediaTime::createWithFloat(10.0123456789f).toFloat());
    EXPECT_EQ(10.0123456789, MediaTime::createWithDouble(10.0123456789).toDouble());
    EXPECT_EQ(MediaTime(1, 3), MediaTime::createWithDouble(MediaTime(1, 3).toDouble()));

    // Floating Point Math
    EXPECT_EQ(1.5 + 3.3, (MediaTime::createWithDouble(1.5) + MediaTime::createWithDouble(3.3)).toDouble());
    EXPECT_EQ(1.5 - 3.3, (MediaTime::createWithDouble(1.5) - MediaTime::createWithDouble(3.3)).toDouble());
    EXPECT_EQ(-3.3, (-MediaTime::createWithDouble(3.3)).toDouble());
    EXPECT_EQ(3.3 * 2, (MediaTime::createWithDouble(3.3) * 2).toDouble());

    EXPECT_EQ((MediaTime::createWithDouble(0.5)+MediaTime::createWithDouble(1e303)).toDouble(), 0.5f+1e303);
    EXPECT_EQ((MediaTime::createWithDouble(0.5, 1U)+MediaTime::createWithDouble(1e303)).toDouble(), 0.5f+1e303);
    EXPECT_EQ((MediaTime(1, 2)+MediaTime::createWithDouble(1e303)).toDouble(), 0.5f+1e303);
    EXPECT_EQ((2*MediaTime::createWithDouble(1e303)).toDouble(), 2*1e303);

    // Floating Point and non-Floating Point math
    EXPECT_EQ(2.0, (MediaTime::createWithDouble(1.5) + MediaTime(1, 2)).toDouble());
    EXPECT_EQ(1.0, (MediaTime::createWithDouble(1.5) - MediaTime(1, 2)).toDouble());

    // Overflow Behavior
    EXPECT_EQ(MediaTime::createWithFloat(pow(2.0f, 64.0f), 1U), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithFloat(-pow(2.0f, 64.0f), 1U), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithFloat(pow(2.0f, 64.0f)).toFloat(), pow(2.0f, 64.0f));
    EXPECT_EQ(MediaTime::createWithFloat(-pow(2.0f, 64.0f)).toFloat(), -pow(2.0f, 64.0f));
    EXPECT_EQ(MediaTime::createWithFloat(pow(2.0f, 63.0f), 2).timeScale(), 1U);
    EXPECT_EQ(MediaTime::createWithFloat(pow(2.0f, 63.0f), 3).timeScale(), 1U);
    EXPECT_EQ(MediaTime::createWithDouble(pow(2.0, 64.0), 1U), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(-pow(2.0, 64.0), 1U), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(pow(2.0f, 64.0f)).toDouble(), pow(2.0f, 64.0f));
    EXPECT_EQ(MediaTime::createWithDouble(-pow(2.0f, 64.0f)).toDouble(), -pow(2.0f, 64.0f));
    EXPECT_EQ(MediaTime::createWithDouble(pow(2.0, 63.0), 2).timeScale(), 1U);
    EXPECT_EQ(MediaTime::createWithDouble(pow(2.0, 63.0), 3).timeScale(), 1U);
    EXPECT_EQ((MediaTime(numeric_limits<int64_t>::max(), 2) + MediaTime(numeric_limits<int64_t>::max(), 2)).timeScale(), 1U);
    EXPECT_EQ((MediaTime(numeric_limits<int64_t>::min(), 2) - MediaTime(numeric_limits<int64_t>::max(), 2)).timeScale(), 1U);
    EXPECT_EQ(MediaTime(numeric_limits<int64_t>::max(), 1) + MediaTime(numeric_limits<int64_t>::max(), 1), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime(numeric_limits<int64_t>::min(), 1) + MediaTime(numeric_limits<int64_t>::min(), 1), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime(numeric_limits<int64_t>::min(), 1) - MediaTime(numeric_limits<int64_t>::max(), 1), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime(numeric_limits<int64_t>::max(), 1) - MediaTime(numeric_limits<int64_t>::min(), 1), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(numeric_limits<double>::max()) + MediaTime::createWithDouble(numeric_limits<double>::max()), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(numeric_limits<double>::lowest()) + MediaTime::createWithDouble(numeric_limits<double>::lowest()), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(numeric_limits<double>::lowest()) - MediaTime::createWithDouble(numeric_limits<double>::max()), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(numeric_limits<double>::max()) - MediaTime::createWithDouble(numeric_limits<double>::lowest()), MediaTime::positiveInfiniteTime());

    // Rounding
    EXPECT_EQ(MediaTime(1, 1).toTimeScale(2).timeValue(), 2);
    EXPECT_EQ(MediaTime(1, 3).toTimeScale(6).timeValue(), 2);
    EXPECT_EQ(MediaTime(2, 2).toTimeScale(1).timeValue(), 1);
    EXPECT_EQ(MediaTime(2, 6).toTimeScale(3).timeValue(), 1);
    EXPECT_EQ(MediaTime(1, 2).toTimeScale(1).hasBeenRounded(), true);
    EXPECT_EQ(MediaTime(1, 2).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), 1);
    EXPECT_EQ(MediaTime(51, 100).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), 1);
    EXPECT_EQ(MediaTime(49, 100).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), 0);
    EXPECT_EQ(MediaTime(3, 2).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), 2);
    EXPECT_EQ(MediaTime(151, 100).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), 2);
    EXPECT_EQ(MediaTime(149, 100).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), 1);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(-51, 100).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(-49, 100).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), 0);
    EXPECT_EQ(MediaTime(-3, 2).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), -2);
    EXPECT_EQ(MediaTime(-151, 100).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), -2);
    EXPECT_EQ(MediaTime(-149, 100).toTimeScale(1, MediaTime::RoundingFlags::HalfAwayFromZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), 0);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), 0);
    EXPECT_EQ(MediaTime(3, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), 1);
    EXPECT_EQ(MediaTime(151, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), 1);
    EXPECT_EQ(MediaTime(149, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), 1);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), 0);
    EXPECT_EQ(MediaTime(-51, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), 0);
    EXPECT_EQ(MediaTime(-49, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), 0);
    EXPECT_EQ(MediaTime(-3, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(-151, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(-149, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(1, 2).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), 1);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(3, 2).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), 2);
    EXPECT_EQ(MediaTime(151, 100).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), 2);
    EXPECT_EQ(MediaTime(149, 100).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), 2);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(-51, 100).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(-49, 100).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), -1);
    EXPECT_EQ(MediaTime(-3, 2).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), -2);
    EXPECT_EQ(MediaTime(-151, 100).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), -2);
    EXPECT_EQ(MediaTime(-149, 100).toTimeScale(1, MediaTime::RoundingFlags::AwayFromZero).timeValue(), -2);
    EXPECT_EQ(MediaTime(1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), 1);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), 0);
    EXPECT_EQ(MediaTime(3, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), 2);
    EXPECT_EQ(MediaTime(151, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), 2);
    EXPECT_EQ(MediaTime(149, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), 2);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), 0);
    EXPECT_EQ(MediaTime(-51, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), 0);
    EXPECT_EQ(MediaTime(-49, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), 0);
    EXPECT_EQ(MediaTime(-3, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), -1);
    EXPECT_EQ(MediaTime(-151, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), -1);
    EXPECT_EQ(MediaTime(-149, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardPositiveInfinity).timeValue(), -1);
    EXPECT_EQ(MediaTime(1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), 0);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), -1);
    EXPECT_EQ(MediaTime(3, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), 1);
    EXPECT_EQ(MediaTime(151, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), 1);
    EXPECT_EQ(MediaTime(149, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), 1);
    EXPECT_EQ(MediaTime(-1, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), -1);
    EXPECT_EQ(MediaTime(-51, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), -1);
    EXPECT_EQ(MediaTime(-49, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), -1);
    EXPECT_EQ(MediaTime(-3, 2).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), -2);
    EXPECT_EQ(MediaTime(-151, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), -2);
    EXPECT_EQ(MediaTime(-149, 100).toTimeScale(1, MediaTime::RoundingFlags::TowardNegativeInfinity).timeValue(), -2);
    EXPECT_EQ(MediaTime(numeric_limits<int64_t>::max(), 1).toTimeScale(2), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime(numeric_limits<int64_t>::min(), 1).toTimeScale(2), MediaTime::negativeInfiniteTime());
    int64_t maxInt32 = numeric_limits<int32_t>::max();
    EXPECT_EQ(MediaTime(maxInt32 * 2, 1).toTimeScale(2).timeValue(), maxInt32 * 4);
    int64_t bigInt = 1LL << 62;
    EXPECT_EQ(MediaTime(bigInt, 1U << 31).toTimeScale(1U << 29).timeValue(), bigInt / 4);
    EXPECT_EQ(MediaTime(bigInt + 1, 1U << 31).toTimeScale(1U << 29, MediaTime::RoundingFlags::TowardZero).timeValue(), bigInt / 4);
    EXPECT_EQ(MediaTime(bigInt + 1, 1U << 31).toTimeScale(1U << 29).hasBeenRounded(), true);
    EXPECT_EQ(MediaTime(bigInt - 2, MediaTime::MaximumTimeScale).toTimeScale(MediaTime::MaximumTimeScale - 1).hasBeenRounded(), true);
    EXPECT_EQ(MediaTime(bigInt, 1).toTimeScale(MediaTime::MaximumTimeScale), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime(-bigInt, 1).toTimeScale(MediaTime::MaximumTimeScale), MediaTime::negativeInfiniteTime());

    // Non-zero timescale
    EXPECT_EQ(MediaTime(102, 0), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime(-102, 0), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(99, 0), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(-99, 0), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(99).toTimeScale(0), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithDouble(-99).toTimeScale(0), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithFloat(909, 0), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithFloat(-909, 0), MediaTime::negativeInfiniteTime());
    EXPECT_EQ(MediaTime::createWithFloat(999).toTimeScale(0), MediaTime::positiveInfiniteTime());
    EXPECT_EQ(MediaTime::createWithFloat(-999).toTimeScale(0), MediaTime::negativeInfiniteTime());
}

}

