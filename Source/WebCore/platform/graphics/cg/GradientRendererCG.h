/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/ColorInterpolationMethod.h>
#include <WebCore/DestinationColorSpace.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class GradientColorStops;
struct GradientColorStop;
using GradientColorStopVector = Vector<GradientColorStop, 2>;

class GradientRendererCG {
public:
    GradientRendererCG(ColorInterpolationMethod, const GradientColorStops&, std::optional<DestinationColorSpace> = { });

    void drawLinearGradient(CGContextRef, CGPoint startPoint, CGPoint endPoint, CGGradientDrawingOptions);
    void drawRadialGradient(CGContextRef, CGPoint startCenter, CGFloat startRadius, CGPoint endCenter, CGFloat endRadius, CGGradientDrawingOptions);
    void drawConicGradient(CGContextRef, CGPoint center, CGFloat angle);

    const std::optional<DestinationColorSpace>& colorSpace() const { return m_colorSpace; }

    static RetainPtr<CGGradientRef> createGradientBySampling(ColorInterpolationMethod, const GradientColorStopVector&, const std::optional<DestinationColorSpace>& = { });

private:
    using Gradient = RetainPtr<CGGradientRef>;

    Gradient makeGradient(ColorInterpolationMethod, const GradientColorStops&) const;
    Gradient makeGradientBySampling(ColorInterpolationMethod, const GradientColorStops&) const;

    std::optional<DestinationColorSpace> m_colorSpace;
    Gradient m_gradient;
};

}
