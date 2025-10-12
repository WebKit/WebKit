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
#include <WebCore/ColorComponents.h>
#include <WebCore/ColorInterpolationMethod.h>
#include <WebCore/DestinationColorSpace.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class GradientColorStops;

struct ColorConvertedToInterpolationColorSpaceStop {
    float offset;
    ColorComponents<float, 4> colorComponents;
};

class GradientRendererCG {
public:
    GradientRendererCG(ColorInterpolationMethod, const GradientColorStops&, std::optional<DestinationColorSpace>);

    void drawLinearGradient(CGContextRef, CGPoint startPoint, CGPoint endPoint, CGGradientDrawingOptions);
    void drawRadialGradient(CGContextRef, CGPoint startCenter, CGFloat startRadius, CGPoint endCenter, CGFloat endRadius, CGGradientDrawingOptions);
    void drawConicGradient(CGContextRef, CGPoint center, CGFloat angle);

    std::optional<DestinationColorSpace> colorSpace() const;

private:
    struct Gradient {
        RetainPtr<CGGradientRef> gradient;
        std::optional<DestinationColorSpace> colorSpace;
    };

    struct Shading {
        template<typename InterpolationSpace, AlphaPremultiplication> static void shadingFunction(void*, const CGFloat*, CGFloat*);

        class Data : public ThreadSafeRefCounted<Data> {
        public:
            static Ref<Data> create(ColorInterpolationMethod colorInterpolationMethod, Vector<ColorConvertedToInterpolationColorSpaceStop> stops, bool firstStopIsSynthetic, bool lastStopIsSynthetic)
            {
                return adoptRef(*new Data(colorInterpolationMethod, WTFMove(stops), firstStopIsSynthetic, lastStopIsSynthetic));
            }

            ColorInterpolationMethod colorInterpolationMethod() const { return m_colorInterpolationMethod; }
            const Vector<ColorConvertedToInterpolationColorSpaceStop>& stops() const { return m_stops; }

            bool firstStopIsSynthetic() const { return m_firstStopIsSynthetic; }
            bool lastStopIsSynthetic() const { return m_lastStopIsSynthetic; }

        private:
            Data(ColorInterpolationMethod colorInterpolationMethod, Vector<ColorConvertedToInterpolationColorSpaceStop> stops, bool firstStopIsSynthetic, bool lastStopIsSynthetic)
                : m_colorInterpolationMethod { colorInterpolationMethod }
                , m_firstStopIsSynthetic(firstStopIsSynthetic)
                , m_lastStopIsSynthetic(lastStopIsSynthetic)
                , m_stops { WTFMove(stops) }
            {
            }

            ColorInterpolationMethod m_colorInterpolationMethod;
            bool m_firstStopIsSynthetic { false };
            bool m_lastStopIsSynthetic { false };
            Vector<ColorConvertedToInterpolationColorSpaceStop> m_stops;
        };

        Ref<Data> data;
        RetainPtr<CGFunctionRef> function;
        RetainPtr<CGColorSpaceRef> colorSpace;
    };

    using Strategy = Variant<Gradient, Shading>;

    Strategy pickStrategy(ColorInterpolationMethod, const GradientColorStops&, std::optional<DestinationColorSpace>) const;
    Strategy makeGradient(ColorInterpolationMethod, const GradientColorStops&, std::optional<DestinationColorSpace>) const;
    Strategy makeShading(ColorInterpolationMethod, const GradientColorStops&) const;

    Strategy m_strategy;
};

}
