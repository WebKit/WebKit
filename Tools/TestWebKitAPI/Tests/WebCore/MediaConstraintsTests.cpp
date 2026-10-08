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

#if ENABLE(MEDIA_STREAM)

#include <WebCore/MediaConstraints.h>

namespace TestWebKitAPI {

TEST(MediaConstraintsTest, ValueForCapabilityRangeMinMaxPreservesValidCurrent)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(640);
    constraint.setMax(1920);

    // Current value already satisfies [min, max] and there is no ideal, so it
    // should be left unchanged rather than jumping to max.
    EXPECT_EQ(constraint.valueForCapabilityRange(1280, 0, 4096), 1280);
}

TEST(MediaConstraintsTest, ValueForCapabilityRangeMinMaxClampsAboveMax)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(640);
    constraint.setMax(1920);

    EXPECT_EQ(constraint.valueForCapabilityRange(2000, 0, 4096), 1920);
}

TEST(MediaConstraintsTest, ValueForCapabilityRangeMinMaxClampsBelowMin)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(640);
    constraint.setMax(1920);

    EXPECT_EQ(constraint.valueForCapabilityRange(100, 0, 4096), 640);
}

TEST(MediaConstraintsTest, ValueForCapabilityRangeMaxOnlyClampsAboveMax)
{
    WebCore::IntConstraint constraint;
    constraint.setMax(1920);

    EXPECT_EQ(constraint.valueForCapabilityRange(2000, 0, 4096), 1920);
}

TEST(MediaConstraintsTest, ValueForCapabilityRangeMaxOnlyPreservesValidCurrent)
{
    WebCore::IntConstraint constraint;
    constraint.setMax(1920);

    // Current value already satisfies max and there is no ideal, so it
    // should be left unchanged rather than jumping to max.
    EXPECT_EQ(constraint.valueForCapabilityRange(1280, 0, 4096), 1280);
}

TEST(MediaConstraintsTest, ValueForCapabilityRangeMaxOnlyIgnoresCurrentBelowCapabilityMin)
{
    WebCore::IntConstraint constraint;
    constraint.setMax(1920);

    // Current value is below the capability's own minimum (e.g. an unset/default
    // value before the source has produced any frames), so it is not a valid current
    // value and must not be preserved; the constraint should still clamp to max.
    EXPECT_EQ(constraint.valueForCapabilityRange(0, 1, 4096), 1920);
}

TEST(MediaConstraintsTest, ValueForCapabilityRangeMinOnlyPreservesValidCurrent)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(640);

    // Current value already satisfies min and there is no ideal, so it
    // should be left unchanged rather than jumping to min.
    EXPECT_EQ(constraint.valueForCapabilityRange(1280, 0, 4096), 1280);
}

TEST(MediaConstraintsTest, ValueForCapabilityRangeMinMaxIdealIsUnaffected)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(640);
    constraint.setMax(1920);
    constraint.setIdeal(1024);

    EXPECT_EQ(constraint.valueForCapabilityRange(1280, 0, 4096), 1024);
}

// The discrete sample rates supported by CoreAudioCaptureSource, the only capability
// that currently resolves through valueForDiscreteCapabilityValues().
static Vector<int> discreteSampleRates()
{
    return { 8000, 16000, 32000, 44100, 48000, 96000 };
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesExact)
{
    WebCore::IntConstraint constraint;
    constraint.setExact(32000);

    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(48000, discreteSampleRates()), 32000);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesMinOnlySelectsSmallestSupportedValue)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(32000);

    // Current value doesn't satisfy min, so the smallest supported value that does is used.
    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(8000, discreteSampleRates()), 32000);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesMinOnlyPreservesValidCurrent)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(16000);

    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(48000, discreteSampleRates()), 48000);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesMaxOnlyClampsToLargestSupportedValue)
{
    WebCore::IntConstraint constraint;
    constraint.setMax(44100);

    // Current value doesn't satisfy max, so the largest supported value that does is used.
    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(96000, discreteSampleRates()), 44100);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesMaxOnlyPreservesValidCurrent)
{
    WebCore::IntConstraint constraint;
    constraint.setMax(48000);

    // Current value already satisfies max and there is no ideal, so it should be
    // left unchanged rather than jumping to the largest allowed value.
    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(44100, discreteSampleRates()), 44100);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesMaxOnlyIgnoresUnsupportedCurrent)
{
    WebCore::IntConstraint constraint;
    constraint.setMax(48000);

    // Current value is not one of the supported values, so it is not a valid current
    // value and must not be preserved.
    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(22050, discreteSampleRates()), 48000);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesMinMaxPreservesValidCurrent)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(16000);
    constraint.setMax(48000);

    // Current value already satisfies [min, max] and there is no ideal, so it
    // should be left unchanged rather than jumping to max.
    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(44100, discreteSampleRates()), 44100);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesMinMaxClampsAboveMax)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(16000);
    constraint.setMax(44100);

    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(96000, discreteSampleRates()), 44100);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesMinMaxClampsBelowMin)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(32000);
    constraint.setMax(48000);

    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(8000, discreteSampleRates()), 32000);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesIdealIsClampedToMax)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(16000);
    constraint.setMax(44100);
    constraint.setIdeal(96000);

    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(32000, discreteSampleRates()), 44100);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesIdealIsUsedWhenSupported)
{
    WebCore::IntConstraint constraint;
    constraint.setIdeal(32000);

    EXPECT_EQ(constraint.valueForDiscreteCapabilityValues(48000, discreteSampleRates()), 32000);
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesUnsatisfiableMaxLeavesSettingAlone)
{
    WebCore::IntConstraint constraint;
    constraint.setMax(4000);

    EXPECT_FALSE(constraint.valueForDiscreteCapabilityValues(48000, discreteSampleRates()));
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesUnsatisfiableMinLeavesSettingAlone)
{
    WebCore::IntConstraint constraint;
    constraint.setMin(192000);

    EXPECT_FALSE(constraint.valueForDiscreteCapabilityValues(48000, discreteSampleRates()));
}

TEST(MediaConstraintsTest, ValueForDiscreteCapabilityValuesEmptyCapabilityValuesLeavesSettingAlone)
{
    WebCore::IntConstraint constraint;
    constraint.setMax(48000);

    EXPECT_FALSE(constraint.valueForDiscreteCapabilityValues(48000, { }));
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)
