/*
 * Copyright (C) 2026 Taishi Eguchi
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

#pragma once

#include <wtf/Hasher.h>
#include <wtf/Markable.h>

namespace WebCore {

// Computed values of the @font-face ascent-override, descent-override, and
// line-gap-override descriptors, as fractions of the used font size.
// https://www.w3.org/TR/css-fonts-4/#font-metrics-override-desc
struct FontMetricsOverride {
    bool isNormal() const { return !value; }

    friend bool operator==(const FontMetricsOverride&, const FontMetricsOverride&) = default;

    Markable<float> value { };
};

struct FontMetricsOverrides {
    bool isNormal() const { return ascentOverride.isNormal() && descentOverride.isNormal() && lineGapOverride.isNormal(); }

    friend bool operator==(const FontMetricsOverrides&, const FontMetricsOverrides&) = default;

    FontMetricsOverride ascentOverride;
    FontMetricsOverride descentOverride;
    FontMetricsOverride lineGapOverride;
};

inline void add(Hasher& hasher, const FontMetricsOverride& metricsOverride)
{
    if (!metricsOverride.value) {
        add(hasher, false);
        return;
    }

    // operator== treats +0.0f and -0.0f as equal, so they must hash identically.
    float value = *metricsOverride.value;
    if (!value)
        value = 0.0f;

    add(hasher, true, value);
}

inline void add(Hasher& hasher, const FontMetricsOverrides& metricsOverrides)
{
    add(hasher, metricsOverrides.ascentOverride, metricsOverrides.descentOverride, metricsOverrides.lineGapOverride);
}

} // namespace WebCore
