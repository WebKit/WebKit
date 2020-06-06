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

#include "WindRule.h"
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

enum LineCap {
    ButtCap,
    RoundCap,
    SquareCap
};

enum LineJoin {
    MiterJoin,
    RoundJoin,
    BevelJoin
};

enum HorizontalAlignment {
    AlignLeft,
    AlignRight,
    AlignHCenter
};

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
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WindRule);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, LineCap);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, LineJoin);

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

template<> struct EnumTraits<WebCore::LineCap> {
    using values = EnumValues<
    WebCore::LineCap,
    WebCore::LineCap::ButtCap,
    WebCore::LineCap::RoundCap,
    WebCore::LineCap::SquareCap
    >;
};

template<> struct EnumTraits<WebCore::LineJoin> {
    using values = EnumValues<
    WebCore::LineJoin,
    WebCore::LineJoin::MiterJoin,
    WebCore::LineJoin::RoundJoin,
    WebCore::LineJoin::BevelJoin
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

} // namespace WTF
