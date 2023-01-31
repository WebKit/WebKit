/*
 * Copyright (C) 2004, 2005, 2006 Apple Inc.  All rights reserved.
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

#pragma once

#include "Color.h"
#include "FloatSize.h"
#include "WindRule.h"
#include <optional>
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

enum class CompositeOperator : uint8_t {
    Clear,
    Copy,
    SourceOver,
    SourceIn,
    SourceOut,
    SourceAtop,
    DestinationOver,
    DestinationIn,
    DestinationOut,
    DestinationAtop,
    XOR,
    PlusDarker,
    PlusLighter,
    Difference
};

enum class BlendMode : uint8_t {
    Normal = 1, // Start with 1 to match SVG's blendmode enumeration.
    Multiply,
    Screen,
    Darken,
    Lighten,
    Overlay,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity,
    PlusDarker,
    PlusLighter
};

struct CompositeMode {
    CompositeOperator operation;
    BlendMode blendMode;
};

inline bool operator==(const CompositeMode& a, const CompositeMode& b)
{
    return a.operation == b.operation && a.blendMode == b.blendMode;
}

inline bool operator!=(const CompositeMode& a, const CompositeMode& b)
{
    return !(a == b);
}

enum class DocumentMarkerLineStyleMode : uint8_t {
    TextCheckingDictationPhraseWithAlternatives,
    Spelling,
    Grammar,
    AutocorrectionReplacement,
    DictationAlternatives
};

struct DocumentMarkerLineStyle {
    DocumentMarkerLineStyleMode mode;
    bool shouldUseDarkAppearance { false };
};

// Legacy shadow blur radius is used for canvas, and -webkit-box-shadow.
// It has different treatment of radii > 8px.
enum class ShadowRadiusMode : bool {
    Default,
    Legacy
};

struct DropShadow {
    FloatSize offset;
    float blurRadius { 0 };
    Color color;
    ShadowRadiusMode radiusMode { ShadowRadiusMode::Default };

    bool isVisible() const { return color.isVisible(); }
    bool isBlurred() const { return isVisible() && blurRadius; }
    bool hasOutsets() const { return isBlurred() || (isVisible() && !offset.isZero()); }
};

inline bool operator==(const DropShadow& a, const DropShadow& b)
{
    return a.offset == b.offset && a.blurRadius == b.blurRadius && a.color == b.color && a.radiusMode == b.radiusMode;
}

inline bool operator!=(const DropShadow& a, const DropShadow& b)
{
    return !(a == b);
}

enum class GradientSpreadMethod : uint8_t {
    Pad,
    Reflect,
    Repeat
};

enum class InterpolationQuality : uint8_t {
    Default,
    DoNotInterpolate,
    Low,
    Medium,
    High
};

enum class LineCap : uint8_t {
    Butt,
    Round,
    Square
};

enum class LineJoin : uint8_t {
    Miter,
    Round,
    Bevel
};

enum HorizontalAlignment {
    AlignLeft,
    AlignRight,
    AlignHCenter
};

enum class StrokeStyle : uint8_t {
    NoStroke,
    SolidStroke,
    DottedStroke,
    DashedStroke,
    DoubleStroke,
    WavyStroke,
};

enum class TextDrawingMode : uint8_t {
    Fill = 1 << 0,
    Stroke = 1 << 1,
};
using TextDrawingModeFlags = OptionSet<TextDrawingMode>;

enum TextBaseline {
    AlphabeticTextBaseline,
    TopTextBaseline,
    MiddleTextBaseline,
    BottomTextBaseline,
    IdeographicTextBaseline,
    HangingTextBaseline
};

enum TextAlign {
    StartTextAlign,
    EndTextAlign,
    LeftTextAlign,
    CenterTextAlign,
    RightTextAlign
};

String compositeOperatorName(WebCore::CompositeOperator, WebCore::BlendMode);
String blendModeName(WebCore::BlendMode);
bool parseBlendMode(const String&, WebCore::BlendMode&);
bool parseCompositeAndBlendOperator(const String&, WebCore::CompositeOperator&, WebCore::BlendMode&);

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WebCore::BlendMode);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WebCore::CompositeOperator);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, CompositeMode);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const DropShadow&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, GradientSpreadMethod);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, InterpolationQuality);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, LineCap);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, LineJoin);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, StrokeStyle);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, TextDrawingMode);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WindRule);

} // namespace WebCore
