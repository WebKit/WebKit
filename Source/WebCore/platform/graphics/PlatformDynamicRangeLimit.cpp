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
#include "PlatformDynamicRangeLimit.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

PlatformDynamicRangeLimit::ValueType PlatformDynamicRangeLimit::normalizedAverage(float standardPercent, float constrainedPercent, float noLimitPercent)
{
    if (!std::isfinite(standardPercent) || standardPercent < 0 || standardPercent > 100 || !std::isfinite(constrainedPercent) || constrainedPercent < 0 || constrainedPercent > 100 || !std::isfinite(noLimitPercent) || noLimitPercent < 0 || noLimitPercent > 100) {
        ASSERT(std::isfinite(standardPercent));
        ASSERT(standardPercent >= 0);
        ASSERT(standardPercent <= 100);
        ASSERT(std::isfinite(constrainedPercent));
        ASSERT(constrainedPercent >= 0);
        ASSERT(constrainedPercent <= 100);
        ASSERT(std::isfinite(noLimitPercent));
        ASSERT(noLimitPercent >= 0);
        ASSERT(noLimitPercent <= 100);
        // Unexpected value -> Clamp HDR down to prevent misuses.
        return standardValue;
    }

    float sum = standardPercent + constrainedPercent + noLimitPercent;
    if (!sum) {
        ASSERT(sum);
        return standardValue;
    }

    float weightedSum = standardPercent * standardValue + constrainedPercent * constrainedValue + noLimitPercent * noLimitValue;
    float weightedAverage = WTF::safeFPDivision(weightedSum, sum);
    return std::clamp(weightedAverage, 0.0f, 1.0f);
}

TextStream& operator<<(TextStream& ts, PlatformDynamicRangeLimit dynamicRangeLimit)
{
    ts << dynamicRangeLimit.value();
    return ts;
}

} // namespace WebCore
