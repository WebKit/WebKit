/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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

#include "GraphicsTypes.h"
#include "SourceBrush.h"
#include "WindRule.h"
#include <wtf/OptionSet.h>

namespace WebCore {

struct GraphicsContextState {
    enum class Change : uint32_t {
        FillBrush                   = 1 << 0,
        FillRule                    = 1 << 1,

        StrokeBrush                 = 1 << 2,
        StrokeThickness             = 1 << 3,
        StrokeStyle                 = 1 << 4,

        CompositeMode               = 1 << 5,
        DropShadow                  = 1 << 6,

        Alpha                       = 1 << 7,
        TextDrawingMode             = 1 << 8,
        ImageInterpolationQuality   = 1 << 9,

        ShouldAntialias             = 1 << 10,
        ShouldSmoothFonts           = 1 << 11,
        ShouldSubpixelQuantizeFonts = 1 << 12,
        ShadowsIgnoreTransforms     = 1 << 13,
        DrawLuminanceMask           = 1 << 14,
#if HAVE(OS_DARK_MODE_SUPPORT)
        UseDarkAppearance           = 1 << 15,
#endif
    };
    using ChangeFlags = OptionSet<Change>;

    WEBCORE_EXPORT GraphicsContextState(const ChangeFlags& = { });

    ChangeFlags changes() const { return changeFlags; }
    void didApplyChanges() { changeFlags = { }; }

    void setFillBrush(const SourceBrush& brush) { setProperty(Change::FillBrush, &GraphicsContextState::fillBrush, brush); }
    void setFillColor(const Color& color) { setProperty(Change::FillBrush, &GraphicsContextState::fillBrush, { color }); }
    void setFillGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform) { fillBrush.setGradient(WTFMove(gradient), spaceTransform); changeFlags.add(Change::FillBrush); }
    void setFillPattern(Ref<Pattern>&& pattern) { fillBrush.setPattern(WTFMove(pattern)); changeFlags.add(Change::FillBrush); }

    void setFillRule(WindRule fillRule) { setProperty(Change::FillRule, &GraphicsContextState::fillRule, fillRule); }

    void setStrokeBrush(const SourceBrush& brush) { setProperty(Change::StrokeBrush, &GraphicsContextState::strokeBrush, brush); }
    void setStrokeColor(const Color& color) { setProperty(Change::StrokeBrush, &GraphicsContextState::strokeBrush, { color }); }
    void setStrokeGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform) { strokeBrush.setGradient(WTFMove(gradient), spaceTransform); changeFlags.add(Change::StrokeBrush); }
    void setStrokePattern(Ref<Pattern>&& pattern) { strokeBrush.setPattern(WTFMove(pattern)); changeFlags.add(Change::StrokeBrush); }

    void setStrokeThickness(float strokeThickness) { setProperty(Change::StrokeThickness, &GraphicsContextState::strokeThickness, strokeThickness); }
    void setStrokeStyle(StrokeStyle strokeStyle) { setProperty(Change::StrokeStyle, &GraphicsContextState::strokeStyle, strokeStyle); }

    void setCompositeMode(CompositeMode compositeMode) { setProperty(Change::CompositeMode, &GraphicsContextState::compositeMode, compositeMode); }
    void setDropShadow(const DropShadow& dropShadow) { setProperty(Change::DropShadow, &GraphicsContextState::dropShadow, dropShadow); }

    void setAlpha(float alpha) { setProperty(Change::Alpha, &GraphicsContextState::alpha, alpha); }
    void setImageInterpolationQuality(InterpolationQuality imageInterpolationQuality) { setProperty(Change::ImageInterpolationQuality, &GraphicsContextState::imageInterpolationQuality, imageInterpolationQuality); }
    void setTextDrawingMode(TextDrawingModeFlags textDrawingMode) { setProperty(Change::TextDrawingMode, &GraphicsContextState::textDrawingMode, textDrawingMode); }

    void setShouldAntialias(bool shouldAntialias) { setProperty(Change::ShouldAntialias, &GraphicsContextState::shouldAntialias, shouldAntialias); }
    void setShouldSmoothFonts(bool shouldSmoothFonts) { setProperty(Change::ShouldSmoothFonts, &GraphicsContextState::shouldSmoothFonts, shouldSmoothFonts); }
    void setShouldSubpixelQuantizeFonts(bool shouldSubpixelQuantizeFonts) { setProperty(Change::ShouldSubpixelQuantizeFonts, &GraphicsContextState::shouldSubpixelQuantizeFonts, shouldSubpixelQuantizeFonts); }
    void setShadowsIgnoreTransforms(bool shadowsIgnoreTransforms) { setProperty(Change::ShadowsIgnoreTransforms, &GraphicsContextState::shadowsIgnoreTransforms, shadowsIgnoreTransforms); }
    void setDrawLuminanceMask(bool drawLuminanceMask) { setProperty(Change::DrawLuminanceMask, &GraphicsContextState::drawLuminanceMask, drawLuminanceMask); }
#if HAVE(OS_DARK_MODE_SUPPORT)
    void setUseDarkAppearance(bool useDarkAppearance) { setProperty(Change::UseDarkAppearance, &GraphicsContextState::useDarkAppearance, useDarkAppearance); }
#endif
    
    bool containsOnlyInlineChanges() const;
    void mergeChanges(const GraphicsContextState&, const std::optional<GraphicsContextState>& lastDrawingState = std::nullopt);

    void didBeginTransparencyLayer();
    void didEndTransparencyLayer(float originalOpacity);

    WTF::TextStream& dump(WTF::TextStream&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GraphicsContextState> decode(Decoder&);

    ChangeFlags changeFlags;

    SourceBrush fillBrush { Color::black };
    WindRule fillRule { WindRule::NonZero };
    
    SourceBrush strokeBrush { Color::black };
    float strokeThickness { 0 };
    StrokeStyle strokeStyle { SolidStroke };
    
    CompositeMode compositeMode { CompositeOperator::SourceOver, BlendMode::Normal };
    DropShadow dropShadow;
    
    float alpha { 1 };
    InterpolationQuality imageInterpolationQuality { InterpolationQuality::Default };
    TextDrawingModeFlags textDrawingMode { TextDrawingMode::Fill };

    bool shouldAntialias { true };
    bool shouldSmoothFonts { true };
    bool shouldSubpixelQuantizeFonts { true };
    bool shadowsIgnoreTransforms { false };
    bool drawLuminanceMask { false };
#if HAVE(OS_DARK_MODE_SUPPORT)
    bool useDarkAppearance { false };
#endif
    
private:
    template<typename T>
    void setProperty(Change change, T GraphicsContextState::*property, const T& value)
    {
#if !USE(CAIRO)
        if (this->*property == value)
            return;
#endif
        this->*property = value;
        changeFlags.add(change);
    }

    template<typename T>
    void setProperty(Change change, T GraphicsContextState::*property, T&& value)
    {
#if !USE(CAIRO)
        if (this->*property == value)
            return;
#endif
        this->*property = WTFMove(value);
        changeFlags.add(change);
    }
};

template<class Encoder>
void GraphicsContextState::encode(Encoder& encoder) const
{
    auto encode = [&](Change change, auto GraphicsContextState::*property) {
        if (changeFlags.contains(change))
            encoder << this->*property;
    };

    encoder << changeFlags;

    encode(Change::FillBrush,                       &GraphicsContextState::fillBrush);
    encode(Change::FillRule,                        &GraphicsContextState::fillRule);

    encode(Change::StrokeBrush,                     &GraphicsContextState::strokeBrush);
    encode(Change::StrokeThickness,                 &GraphicsContextState::strokeThickness);
    encode(Change::StrokeStyle,                     &GraphicsContextState::strokeStyle);

    encode(Change::CompositeMode,                   &GraphicsContextState::compositeMode);
    encode(Change::DropShadow,                      &GraphicsContextState::dropShadow);

    encode(Change::Alpha,                           &GraphicsContextState::alpha);
    encode(Change::ImageInterpolationQuality,       &GraphicsContextState::imageInterpolationQuality);
    encode(Change::TextDrawingMode,                 &GraphicsContextState::textDrawingMode);

    encode(Change::ShouldAntialias,                 &GraphicsContextState::shouldAntialias);
    encode(Change::ShouldSmoothFonts,               &GraphicsContextState::shouldSmoothFonts);
    encode(Change::ShouldSubpixelQuantizeFonts,     &GraphicsContextState::shouldSubpixelQuantizeFonts);
    encode(Change::ShadowsIgnoreTransforms,         &GraphicsContextState::shadowsIgnoreTransforms);
    encode(Change::DrawLuminanceMask,               &GraphicsContextState::drawLuminanceMask);
#if HAVE(OS_DARK_MODE_SUPPORT)
    encode(Change::UseDarkAppearance,               &GraphicsContextState::useDarkAppearance);
#endif
}

template<class Decoder>
std::optional<GraphicsContextState> GraphicsContextState::decode(Decoder& decoder)
{
    auto decode = [&](GraphicsContextState& state, Change change, auto GraphicsContextState::*property) {
        if (!state.changeFlags.contains(change))
            return true;

        using PropertyType = typename std::remove_reference<decltype(std::declval<GraphicsContextState>().*property)>::type;
        std::optional<PropertyType> value;
        decoder >> value;
        if (!value)
            return false;

        state.*property = *value;
        return true;
    };

    std::optional<ChangeFlags> changeFlags;
    decoder >> changeFlags;
    if (!changeFlags)
        return std::nullopt;

    GraphicsContextState state(*changeFlags);

    if (!decode(state, Change::FillBrush,                   &GraphicsContextState::fillBrush))
        return std::nullopt;
    if (!decode(state, Change::FillRule,                    &GraphicsContextState::fillRule))
        return std::nullopt;

    if (!decode(state, Change::StrokeBrush,                 &GraphicsContextState::strokeBrush))
        return std::nullopt;
    if (!decode(state, Change::StrokeThickness,             &GraphicsContextState::strokeThickness))
        return std::nullopt;
    if (!decode(state, Change::StrokeStyle,                 &GraphicsContextState::strokeStyle))
        return std::nullopt;

    if (!decode(state, Change::CompositeMode,               &GraphicsContextState::compositeMode))
        return std::nullopt;
    if (!decode(state, Change::DropShadow,                  &GraphicsContextState::dropShadow))
        return std::nullopt;

    if (!decode(state, Change::Alpha,                       &GraphicsContextState::alpha))
        return std::nullopt;
    if (!decode(state, Change::ImageInterpolationQuality,   &GraphicsContextState::imageInterpolationQuality))
        return std::nullopt;
    if (!decode(state, Change::TextDrawingMode,             &GraphicsContextState::textDrawingMode))
        return std::nullopt;

    if (!decode(state, Change::ShouldAntialias,             &GraphicsContextState::shouldAntialias))
        return std::nullopt;
    if (!decode(state, Change::ShouldSmoothFonts,           &GraphicsContextState::shouldSmoothFonts))
        return std::nullopt;
    if (!decode(state, Change::ShouldSubpixelQuantizeFonts, &GraphicsContextState::shouldSubpixelQuantizeFonts))
        return std::nullopt;
    if (!decode(state, Change::ShadowsIgnoreTransforms,     &GraphicsContextState::shadowsIgnoreTransforms))
        return std::nullopt;
    if (!decode(state, Change::DrawLuminanceMask,           &GraphicsContextState::drawLuminanceMask))
        return std::nullopt;
#if HAVE(OS_DARK_MODE_SUPPORT)
    if (!decode(state, Change::UseDarkAppearance,           &GraphicsContextState::useDarkAppearance))
        return std::nullopt;
#endif

    return state;
}

TextStream& operator<<(TextStream&, GraphicsContextState::Change);
TextStream& operator<<(TextStream&, const GraphicsContextState&);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::GraphicsContextState::Change> {
    using values = EnumValues<
        WebCore::GraphicsContextState::Change,
        WebCore::GraphicsContextState::Change::FillBrush,
        WebCore::GraphicsContextState::Change::FillRule,

        WebCore::GraphicsContextState::Change::StrokeBrush,
        WebCore::GraphicsContextState::Change::StrokeThickness,
        WebCore::GraphicsContextState::Change::StrokeStyle,

        WebCore::GraphicsContextState::Change::CompositeMode,
        WebCore::GraphicsContextState::Change::DropShadow,

        WebCore::GraphicsContextState::Change::Alpha,
        WebCore::GraphicsContextState::Change::TextDrawingMode,
        WebCore::GraphicsContextState::Change::ImageInterpolationQuality,

        WebCore::GraphicsContextState::Change::ShouldAntialias,
        WebCore::GraphicsContextState::Change::ShouldSmoothFonts,
        WebCore::GraphicsContextState::Change::ShouldSubpixelQuantizeFonts,
        WebCore::GraphicsContextState::Change::ShadowsIgnoreTransforms,
        WebCore::GraphicsContextState::Change::DrawLuminanceMask
#if HAVE(OS_DARK_MODE_SUPPORT)
        , WebCore::GraphicsContextState::Change::UseDarkAppearance
#endif
    >;
};

} // namespace WTF
