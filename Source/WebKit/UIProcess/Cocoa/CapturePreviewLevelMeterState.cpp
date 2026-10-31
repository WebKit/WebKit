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
#include "CapturePreviewLevelMeterState.h"

#include <algorithm>
#include <cmath>

namespace WebKit {

CapturePreviewLevelMeterState::Segment CapturePreviewLevelMeterState::segment(unsigned index) const
{
    if (index + 1 == peakSegments)
        return Segment::Peak;
    return index < litSegments ? Segment::Lit : Segment::Unlit;
}

CGFloat CapturePreviewLevelMeterState::segmentX(unsigned index, CGFloat width)
{
    return std::round(index * (width - segmentWidth) / (segmentCount - 1));
}

bool CapturePreviewLevelMeterState::update(float level, MonotonicTime now)
{
    unsigned previousLit = litSegments;
    unsigned previousPeak = peakSegments;

    litSegments = std::isfinite(level) ? static_cast<unsigned>(std::lround(std::clamp(level, 0.f, 1.f) * segmentCount)) : 0;
    if (litSegments >= peakSegments || now >= peakExpiry) {
        peakSegments = litSegments;
        peakExpiry = now + peakHoldDuration;
    }

    return litSegments != previousLit || peakSegments != previousPeak;
}

bool CapturePreviewLevelMeterState::reset()
{
    bool changed = litSegments || peakSegments;
    litSegments = 0;
    peakSegments = 0;
    return changed;
}

} // namespace WebKit
