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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleSnapLengthAsBorderWidth.h"

#include <cmath>

namespace WebCore {
namespace Style {

float snapLengthAsBorderWidth(float length, float deviceScaleFactor)
{
    // https://drafts.csswg.org/css-values-4/#snap-a-length-as-a-border-width

    // 1. Assert: `length` is non-negative.
    // NOTE: Not asserted, but checked in step 3.

    // 2. If `length` is an integer number of device pixels, do nothing.
    // NOTE: Handled by step 4 without explicitly checking here.

    // 3. If `length` is greater than zero, but less than 1 device pixel, round `length` up to 1 device pixel.
    if (auto singleDevicePixelLength = 1.0f / deviceScaleFactor; length > 0.0f && length < singleDevicePixelLength)
        return singleDevicePixelLength;

    // 4. If `length` is greater than 1 device pixel, round it down to the nearest integer number of device pixels.
    return std::floor(length * deviceScaleFactor) / deviceScaleFactor;
}

Length<CSS::Nonnegative> snapLengthAsBorderWidth(Length<CSS::Nonnegative> length, float deviceScaleFactor)
{
    return Length<CSS::Nonnegative> { snapLengthAsBorderWidth(length.unresolvedValue(), deviceScaleFactor) };
}

float snapSignedLengthAsBorderWidth(float length, float deviceScaleFactor)
{
    if (length >= 0.0f)
        return snapLengthAsBorderWidth(length, deviceScaleFactor);
    return -snapLengthAsBorderWidth(-length, deviceScaleFactor);
}

Length<> snapSignedLengthAsBorderWidth(Length<> length, float deviceScaleFactor)
{
    return Length<> { snapSignedLengthAsBorderWidth(length.unresolvedValue(), deviceScaleFactor) };
}

} // namespace Style
} // namespace WebCore
