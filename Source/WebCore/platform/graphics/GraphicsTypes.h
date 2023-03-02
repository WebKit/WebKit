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

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<CompositeMode> decode(Decoder&);
};

inline bool operator==(const CompositeMode& a, const CompositeMode& b)
{
    return a.operation == b.operation && a.blendMode == b.blendMode;
}

inline bool operator!=(const CompositeMode& a, const CompositeMode& b)
{
    return !(a == b);
}

template<class Encoder>
void CompositeMode::encode(Encoder& encoder) const
{
    encoder << operation;
    encoder << blendMode;
}

template<class Decoder>
std::optional<CompositeMode> CompositeMode::decode(Decoder& decoder)
{
    std::optional<CompositeOperator> operation;
    decoder >> operation;
    if (!operation)
        return std::nullopt;

    std::optional<BlendMode> blendMode;
    decoder >> blendMode;
    if (!blendMode)
        return std::nullopt;

    return { { *operation, *blendMode } };
}

struct DocumentMarkerLineStyle {
    enum class Mode : uint8_t {
        TextCheckingDictationPhraseWithAlternatives,
        Spelling,
        Grammar,
        AutocorrectionReplacement,
        DictationAlternatives
    } mode;
    bool shouldUseDarkAppearance { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DocumentMarkerLineStyle> decode(Decoder&);
};

template<class Encoder>
void DocumentMarkerLineStyle::encode(Encoder& encoder) const
{
    encoder << mode;
    encoder << shouldUseDarkAppearance;
}

template<class Decoder>
std::optional<DocumentMarkerLineStyle> DocumentMarkerLineStyle::decode(Decoder& decoder)
{
    std::optional<Mode> mode;
    decoder >> mode;
    if (!mode)
        return std::nullopt;

    std::optional<bool> shouldUseDarkAppearance;
    decoder >> shouldUseDarkAppearance;
    if (!shouldUseDarkAppearance)
        return std::nullopt;

    return { { *mode, *shouldUseDarkAppearance } };
}

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

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DropShadow> decode(Decoder&);
};

inline bool operator==(const DropShadow& a, const DropShadow& b)
{
    return a.offset == b.offset && a.blurRadius == b.blurRadius && a.color == b.color && a.radiusMode == b.radiusMode;
}

inline bool operator!=(const DropShadow& a, const DropShadow& b)
{
    return !(a == b);
}

template<class Encoder>
void DropShadow::encode(Encoder& encoder) const
{
    encoder << offset;
    encoder << blurRadius;
    encoder << color;
    encoder << radiusMode;
}

template<class Decoder>
std::optional<DropShadow> DropShadow::decode(Decoder& decoder)
{
    std::optional<FloatSize> offset;
    decoder >> offset;
    if (!offset)
        return std::nullopt;

    std::optional<float> blurRadius;
    decoder >> blurRadius;
    if (!blurRadius)
        return std::nullopt;

    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    std::optional<ShadowRadiusMode> radiusMode;
    decoder >> radiusMode;
    if (!radiusMode)
        return std::nullopt;

    return { { *offset, *blurRadius, *color, *radiusMode } };
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

enum StrokeStyle : uint8_t {
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

namespace WTF {

template<> struct EnumTraits<WebCore::CompositeOperator> {
    using values = EnumValues<
    WebCore::CompositeOperator,
    WebCore::CompositeOperator::Clear,
    WebCore::CompositeOperator::Copy,
    WebCore::CompositeOperator::SourceOver,
    WebCore::CompositeOperator::SourceIn,
    WebCore::CompositeOperator::SourceOut,
    WebCore::CompositeOperator::SourceAtop,
    WebCore::CompositeOperator::DestinationOver,
    WebCore::CompositeOperator::DestinationIn,
    WebCore::CompositeOperator::DestinationOut,
    WebCore::CompositeOperator::DestinationAtop,
    WebCore::CompositeOperator::XOR,
    WebCore::CompositeOperator::PlusDarker,
    WebCore::CompositeOperator::PlusLighter,
    WebCore::CompositeOperator::Difference
    >;
};

template<> struct EnumTraits<WebCore::BlendMode> {
    using values = EnumValues<
    WebCore::BlendMode,
    WebCore::BlendMode::Normal,
    WebCore::BlendMode::Multiply,
    WebCore::BlendMode::Screen,
    WebCore::BlendMode::Darken,
    WebCore::BlendMode::Lighten,
    WebCore::BlendMode::Overlay,
    WebCore::BlendMode::ColorDodge,
    WebCore::BlendMode::ColorBurn,
    WebCore::BlendMode::HardLight,
    WebCore::BlendMode::SoftLight,
    WebCore::BlendMode::Difference,
    WebCore::BlendMode::Exclusion,
    WebCore::BlendMode::Hue,
    WebCore::BlendMode::Saturation,
    WebCore::BlendMode::Color,
    WebCore::BlendMode::Luminosity,
    WebCore::BlendMode::PlusDarker,
    WebCore::BlendMode::PlusLighter
    >;
};

template<> struct EnumTraits<WebCore::GradientSpreadMethod> {
    using values = EnumValues<
    WebCore::GradientSpreadMethod,
    WebCore::GradientSpreadMethod::Pad,
    WebCore::GradientSpreadMethod::Reflect,
    WebCore::GradientSpreadMethod::Repeat
    >;
};

template<> struct EnumTraits<WebCore::InterpolationQuality> {
    using values = EnumValues<
    WebCore::InterpolationQuality,
    WebCore::InterpolationQuality::Default,
    WebCore::InterpolationQuality::DoNotInterpolate,
    WebCore::InterpolationQuality::Low,
    WebCore::InterpolationQuality::Medium,
    WebCore::InterpolationQuality::High
    >;
};

template<> struct EnumTraits<WebCore::LineCap> {
    using values = EnumValues<
    WebCore::LineCap,
    WebCore::LineCap::Butt,
    WebCore::LineCap::Round,
    WebCore::LineCap::Square
    >;
};

template<> struct EnumTraits<WebCore::LineJoin> {
    using values = EnumValues<
    WebCore::LineJoin,
    WebCore::LineJoin::Miter,
    WebCore::LineJoin::Round,
    WebCore::LineJoin::Bevel
    >;
};

template<> struct EnumTraits<WebCore::DocumentMarkerLineStyle::Mode> {
    using values = EnumValues<
    WebCore::DocumentMarkerLineStyle::Mode,
    WebCore::DocumentMarkerLineStyle::Mode::TextCheckingDictationPhraseWithAlternatives,
    WebCore::DocumentMarkerLineStyle::Mode::Spelling,
    WebCore::DocumentMarkerLineStyle::Mode::Grammar,
    WebCore::DocumentMarkerLineStyle::Mode::AutocorrectionReplacement,
    WebCore::DocumentMarkerLineStyle::Mode::DictationAlternatives
    >;
};

template<> struct EnumTraits<WebCore::StrokeStyle> {
    using values = EnumValues<
    WebCore::StrokeStyle,
    WebCore::StrokeStyle::NoStroke,
    WebCore::StrokeStyle::SolidStroke,
    WebCore::StrokeStyle::DottedStroke,
    WebCore::StrokeStyle::DashedStroke,
    WebCore::StrokeStyle::DoubleStroke,
    WebCore::StrokeStyle::WavyStroke
    >;
};

template<> struct EnumTraits<WebCore::TextDrawingMode> {
    using values = EnumValues<
        WebCore::TextDrawingMode,
        WebCore::TextDrawingMode::Fill,
        WebCore::TextDrawingMode::Stroke
    >;
};

} // namespace WTF
