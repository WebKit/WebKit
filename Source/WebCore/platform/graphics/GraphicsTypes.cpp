/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Rik Cabanier (cabanier@adobe.com)
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "GraphicsTypes.h"

#include "AlphaPremultiplication.h"
#include <wtf/Assertions.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static constexpr std::array compositeOperatorNames {
    "clear"_s,
    "copy"_s,
    "source-over"_s,
    "source-in"_s,
    "source-out"_s,
    "source-atop"_s,
    "destination-over"_s,
    "destination-in"_s,
    "destination-out"_s,
    "destination-atop"_s,
    "xor"_s,
    "darker"_s,
    "lighter"_s,
    "difference"_s
};

static constexpr std::array blendOperatorNames {
    "normal"_s,
    "multiply"_s,
    "screen"_s,
    "darken"_s,
    "lighten"_s,
    "overlay"_s,
    "color-dodge"_s,
    "color-burn"_s,
    "hard-light"_s,
    "soft-light"_s,
    "difference"_s,
    "exclusion"_s,
    "hue"_s,
    "saturation"_s,
    "color"_s,
    "luminosity"_s,
    "plus-darker"_s,
    "plus-lighter"_s
};
const uint8_t numCompositeOperatorNames = std::size(compositeOperatorNames);
const uint8_t numBlendOperatorNames = std::size(blendOperatorNames);

bool parseBlendMode(const String& s, BlendMode& blendMode)
{
    for (unsigned i = 0; i < numBlendOperatorNames; i++) {
        if (s == blendOperatorNames[i]) {
            blendMode = static_cast<BlendMode>(i + static_cast<unsigned>(BlendMode::Normal));
            return true;
        }
    }
    
    return false;
}

bool parseCompositeAndBlendOperator(const String& s, CompositeOperator& op, BlendMode& blendOp)
{
    for (int i = 0; i < numCompositeOperatorNames; i++) {
        if (s == compositeOperatorNames[i]) {
            op = static_cast<CompositeOperator>(i);
            blendOp = BlendMode::Normal;
            return true;
        }
    }
    
    if (parseBlendMode(s, blendOp)) {
        // For now, blending will always assume source-over. This will be fixed in the future
        op = CompositeOperator::SourceOver;
        return true;
    }
    
    return false;
}

// FIXME: when we support blend modes in combination with compositing other than source-over
// this routine needs to be updated.
String compositeOperatorName(CompositeOperator op, BlendMode blendOp)
{
    ASSERT(op >= CompositeOperator::Clear);
    ASSERT(static_cast<uint8_t>(op) < numCompositeOperatorNames);
    ASSERT(blendOp >= BlendMode::Normal);
    ASSERT(static_cast<uint8_t>(blendOp) <= numBlendOperatorNames);
    if (blendOp > BlendMode::Normal)
        return blendOperatorNames[static_cast<unsigned>(blendOp) - static_cast<unsigned>(BlendMode::Normal)];
    return compositeOperatorNames[static_cast<unsigned>(op)];
}

String blendModeName(BlendMode blendOp)
{
    ASSERT(blendOp >= BlendMode::Normal);
    ASSERT(blendOp <= BlendMode::PlusLighter);
    return blendOperatorNames[static_cast<unsigned>(blendOp) - static_cast<unsigned>(BlendMode::Normal)];
}

TextStream& operator<<(TextStream& ts, CompositeOperator op)
{
    return ts << compositeOperatorName(op, BlendMode::Normal);
}

TextStream& operator<<(TextStream& ts, BlendMode blendMode)
{
    return ts << blendModeName(blendMode);
}

TextStream& operator<<(TextStream& ts, CompositeMode compositeMode)
{
    ts.dumpProperty("composite-operation"_s, compositeMode.operation);
    ts.dumpProperty("blend-mode"_s, compositeMode.blendMode);
    return ts;
}

TextStream& operator<<(TextStream& ts, GradientSpreadMethod spreadMethod)
{
    switch (spreadMethod) {
    case GradientSpreadMethod::Pad:
        ts << "pad"_s;
        break;
    case GradientSpreadMethod::Reflect:
        ts << "reflect"_s;
        break;
    case GradientSpreadMethod::Repeat:
        ts << "repeat"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, InterpolationQuality interpolationQuality)
{
    switch (interpolationQuality) {
    case InterpolationQuality::Default:
        ts << "default"_s;
        break;
    case InterpolationQuality::DoNotInterpolate:
        ts << "do-not-interpolate"_s;
        break;
    case InterpolationQuality::Low:
        ts << "low"_s;
        break;
    case InterpolationQuality::Medium:
        ts << "medium"_s;
        break;
    case InterpolationQuality::High:
        ts << "high"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, WindRule rule)
{
    switch (rule) {
    case WindRule::NonZero:
        ts << "NON-ZERO"_s;
        break;
    case WindRule::EvenOdd:
        ts << "EVEN-ODD"_s;
        break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, LineCap capStyle)
{
    switch (capStyle) {
    case LineCap::Butt:
        ts << "BUTT"_s;
        break;
    case LineCap::Round:
        ts << "ROUND"_s;
        break;
    case LineCap::Square:
        ts << "SQUARE"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, LineJoin joinStyle)
{
    switch (joinStyle) {
    case LineJoin::Miter:
        ts << "MITER"_s;
        break;
    case LineJoin::Round:
        ts << "ROUND"_s;
        break;
    case LineJoin::Bevel:
        ts << "BEVEL"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, StrokeStyle strokeStyle)
{
    switch (strokeStyle) {
    case StrokeStyle::NoStroke:
        ts << "no-stroke"_s;
        break;
    case StrokeStyle::SolidStroke:
        ts << "solid-stroke"_s;
        break;
    case StrokeStyle::DottedStroke:
        ts << "dotted-stroke"_s;
        break;
    case StrokeStyle::DashedStroke:
        ts << "dashed-stroke"_s;
        break;
    case StrokeStyle::DoubleStroke:
        ts << "double-stroke"_s;
        break;
    case StrokeStyle::WavyStroke:
        ts << "wavy-stroke"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, TextDrawingMode textDrawingMode)
{
    switch (textDrawingMode) {
    case TextDrawingMode::Fill:
        ts << "fill"_s;
        break;
    case TextDrawingMode::Stroke:
        ts << "stroke"_s;
        break;
    }
    return ts;
}

} // namespace WebCore
