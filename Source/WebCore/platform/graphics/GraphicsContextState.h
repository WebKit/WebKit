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

#include "GraphicsStyle.h"
#include "GraphicsTypes.h"
#include "SourceBrush.h"
#include "WindRule.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class GraphicsContextState {
    friend class GraphicsContextCairo;
public:
    enum class Change : uint32_t {
        FillBrush                   = 1 << 0,
        FillRule                    = 1 << 1,

        StrokeBrush                 = 1 << 2,
        StrokeThickness             = 1 << 3,
        StrokeStyle                 = 1 << 4,

        CompositeMode               = 1 << 5,
        DropShadow                  = 1 << 6,
        Style                       = 1 << 7,

        Alpha                       = 1 << 8,
        TextDrawingMode             = 1 << 9,
        ImageInterpolationQuality   = 1 << 10,

        ShouldAntialias             = 1 << 11,
        ShouldSmoothFonts           = 1 << 12,
        ShouldSubpixelQuantizeFonts = 1 << 13,
        ShadowsIgnoreTransforms     = 1 << 14,
        DrawLuminanceMask           = 1 << 15,
#if HAVE(OS_DARK_MODE_SUPPORT)
        UseDarkAppearance           = 1 << 16,
#endif
    };
    using ChangeFlags = OptionSet<Change>;

    static constexpr ChangeFlags basicChangeFlags { Change::StrokeThickness, Change::StrokeBrush, Change::FillBrush };

    WEBCORE_EXPORT GraphicsContextState(const ChangeFlags& = { }, InterpolationQuality = InterpolationQuality::Default);

    ChangeFlags changes() const { return m_changeFlags; }
    void didApplyChanges() { m_changeFlags = { }; }

    GraphicsContextState cloneForRecording() const;

    const SourceBrush& fillBrush() const { return m_fillBrush; }
    void setFillBrush(const SourceBrush& brush) { setProperty(Change::FillBrush, &GraphicsContextState::m_fillBrush, brush); }
    void setFillColor(const Color& color) { setProperty(Change::FillBrush, &GraphicsContextState::m_fillBrush, { color }); }
    void setFillGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform) { m_fillBrush.setGradient(WTFMove(gradient), spaceTransform); m_changeFlags.add(Change::FillBrush); }
    void setFillPattern(Ref<Pattern>&& pattern) { m_fillBrush.setPattern(WTFMove(pattern)); m_changeFlags.add(Change::FillBrush); }

    WindRule fillRule() const { return m_fillRule; }
    void setFillRule(WindRule fillRule) { setProperty(Change::FillRule, &GraphicsContextState::m_fillRule, fillRule); }

    const SourceBrush& strokeBrush() const { return m_strokeBrush; }
    void setStrokeBrush(const SourceBrush& brush) { setProperty(Change::StrokeBrush, &GraphicsContextState::m_strokeBrush, brush); }
    void setStrokeColor(const Color& color) { setProperty(Change::StrokeBrush, &GraphicsContextState::m_strokeBrush, { color }); }
    void setStrokeGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform) { m_strokeBrush.setGradient(WTFMove(gradient), spaceTransform); m_changeFlags.add(Change::StrokeBrush); }
    void setStrokePattern(Ref<Pattern>&& pattern) { m_strokeBrush.setPattern(WTFMove(pattern)); m_changeFlags.add(Change::StrokeBrush); }

    float strokeThickness() const { return m_strokeThickness; }
    void setStrokeThickness(float strokeThickness) { setProperty(Change::StrokeThickness, &GraphicsContextState::m_strokeThickness, strokeThickness); }

    StrokeStyle strokeStyle() const { return m_strokeStyle; }
    void setStrokeStyle(StrokeStyle strokeStyle) { setProperty(Change::StrokeStyle, &GraphicsContextState::m_strokeStyle, strokeStyle); }

    const CompositeMode& compositeMode() const { return m_compositeMode; }
    void setCompositeMode(CompositeMode compositeMode) { setProperty(Change::CompositeMode, &GraphicsContextState::m_compositeMode, compositeMode); }

    const DropShadow& dropShadow() const { return m_dropShadow; }
    void setDropShadow(const DropShadow& dropShadow) { setProperty(Change::DropShadow, &GraphicsContextState::m_dropShadow, dropShadow); }

    const std::optional<GraphicsStyle>& style() const { return m_style; }
    void setStyle(const std::optional<GraphicsStyle>& style) { setProperty(Change::Style, &GraphicsContextState::m_style, style); }

    float alpha() const { return m_alpha; }
    void setAlpha(float alpha) { setProperty(Change::Alpha, &GraphicsContextState::m_alpha, alpha); }

    InterpolationQuality imageInterpolationQuality() const { return m_imageInterpolationQuality; }
    void setImageInterpolationQuality(InterpolationQuality imageInterpolationQuality) { setProperty(Change::ImageInterpolationQuality, &GraphicsContextState::m_imageInterpolationQuality, imageInterpolationQuality); }

    TextDrawingModeFlags textDrawingMode() const { return m_textDrawingMode; }
    void setTextDrawingMode(TextDrawingModeFlags textDrawingMode) { setProperty(Change::TextDrawingMode, &GraphicsContextState::m_textDrawingMode, textDrawingMode); }

    bool shouldAntialias() const { return m_shouldAntialias; }
    void setShouldAntialias(bool shouldAntialias) { setProperty(Change::ShouldAntialias, &GraphicsContextState::m_shouldAntialias, shouldAntialias); }

    bool shouldSmoothFonts() const { return m_shouldSmoothFonts; }
    void setShouldSmoothFonts(bool shouldSmoothFonts) { setProperty(Change::ShouldSmoothFonts, &GraphicsContextState::m_shouldSmoothFonts, shouldSmoothFonts); }

    bool shouldSubpixelQuantizeFonts() const { return m_shouldSubpixelQuantizeFonts; }
    void setShouldSubpixelQuantizeFonts(bool shouldSubpixelQuantizeFonts) { setProperty(Change::ShouldSubpixelQuantizeFonts, &GraphicsContextState::m_shouldSubpixelQuantizeFonts, shouldSubpixelQuantizeFonts); }

    bool shadowsIgnoreTransforms() const { return m_shadowsIgnoreTransforms; }
    void setShadowsIgnoreTransforms(bool shadowsIgnoreTransforms) { setProperty(Change::ShadowsIgnoreTransforms, &GraphicsContextState::m_shadowsIgnoreTransforms, shadowsIgnoreTransforms); }

    bool drawLuminanceMask() const { return m_drawLuminanceMask; }
    void setDrawLuminanceMask(bool drawLuminanceMask) { setProperty(Change::DrawLuminanceMask, &GraphicsContextState::m_drawLuminanceMask, drawLuminanceMask); }

#if HAVE(OS_DARK_MODE_SUPPORT)
    bool useDarkAppearance() const { return m_useDarkAppearance; }
    void setUseDarkAppearance(bool useDarkAppearance) { setProperty(Change::UseDarkAppearance, &GraphicsContextState::m_useDarkAppearance, useDarkAppearance); }
#endif
    
    bool containsOnlyInlineChanges() const;
    void mergeLastChanges(const GraphicsContextState&, const std::optional<GraphicsContextState>& lastDrawingState = std::nullopt);
    void mergeAllChanges(const GraphicsContextState&);

    void didBeginTransparencyLayer();
    void didEndTransparencyLayer(float originalOpacity);

    WTF::TextStream& dump(WTF::TextStream&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GraphicsContextState> decode(Decoder&);

private:
    template<typename T>
    void setProperty(Change change, T GraphicsContextState::*property, const T& value)
    {
#if !USE(CAIRO)
        if (this->*property == value)
            return;
#endif
        this->*property = value;
        m_changeFlags.add(change);
    }

    template<typename T>
    void setProperty(Change change, T GraphicsContextState::*property, T&& value)
    {
#if !USE(CAIRO)
        if (this->*property == value)
            return;
#endif
        this->*property = WTFMove(value);
        m_changeFlags.add(change);
    }

    ChangeFlags m_changeFlags;

    SourceBrush m_fillBrush { Color::black };
    WindRule m_fillRule { WindRule::NonZero };

    SourceBrush m_strokeBrush { Color::black };
    float m_strokeThickness { 0 };
    StrokeStyle m_strokeStyle { SolidStroke };

    CompositeMode m_compositeMode { CompositeOperator::SourceOver, BlendMode::Normal };
    DropShadow m_dropShadow;
    std::optional<GraphicsStyle> m_style;

    float m_alpha { 1 };
    InterpolationQuality m_imageInterpolationQuality { InterpolationQuality::Default };
    TextDrawingModeFlags m_textDrawingMode { TextDrawingMode::Fill };

    bool m_shouldAntialias { true };
    bool m_shouldSmoothFonts { true };
    bool m_shouldSubpixelQuantizeFonts { true };
    bool m_shadowsIgnoreTransforms { false };
    bool m_drawLuminanceMask { false };
#if HAVE(OS_DARK_MODE_SUPPORT)
    bool m_useDarkAppearance { false };
#endif
};

template<class Encoder>
void GraphicsContextState::encode(Encoder& encoder) const
{
    auto encode = [&](Change change, auto GraphicsContextState::*property) {
        if (m_changeFlags.contains(change))
            encoder << this->*property;
    };

    encoder << m_changeFlags;

    encode(Change::FillBrush,                       &GraphicsContextState::m_fillBrush);
    encode(Change::FillRule,                        &GraphicsContextState::m_fillRule);

    encode(Change::StrokeBrush,                     &GraphicsContextState::m_strokeBrush);
    encode(Change::StrokeThickness,                 &GraphicsContextState::m_strokeThickness);
    encode(Change::StrokeStyle,                     &GraphicsContextState::m_strokeStyle);

    encode(Change::CompositeMode,                   &GraphicsContextState::m_compositeMode);
    encode(Change::DropShadow,                      &GraphicsContextState::m_dropShadow);
    encode(Change::Style,                           &GraphicsContextState::m_style);

    encode(Change::Alpha,                           &GraphicsContextState::m_alpha);
    encode(Change::ImageInterpolationQuality,       &GraphicsContextState::m_imageInterpolationQuality);
    encode(Change::TextDrawingMode,                 &GraphicsContextState::m_textDrawingMode);

    encode(Change::ShouldAntialias,                 &GraphicsContextState::m_shouldAntialias);
    encode(Change::ShouldSmoothFonts,               &GraphicsContextState::m_shouldSmoothFonts);
    encode(Change::ShouldSubpixelQuantizeFonts,     &GraphicsContextState::m_shouldSubpixelQuantizeFonts);
    encode(Change::ShadowsIgnoreTransforms,         &GraphicsContextState::m_shadowsIgnoreTransforms);
    encode(Change::DrawLuminanceMask,               &GraphicsContextState::m_drawLuminanceMask);
#if HAVE(OS_DARK_MODE_SUPPORT)
    encode(Change::UseDarkAppearance,               &GraphicsContextState::m_useDarkAppearance);
#endif
}

template<class Decoder>
std::optional<GraphicsContextState> GraphicsContextState::decode(Decoder& decoder)
{
    auto decode = [&](GraphicsContextState& state, Change change, auto GraphicsContextState::*property) {
        if (!state.changes().contains(change))
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

    if (!decode(state, Change::FillBrush,                   &GraphicsContextState::m_fillBrush))
        return std::nullopt;
    if (!decode(state, Change::FillRule,                    &GraphicsContextState::m_fillRule))
        return std::nullopt;

    if (!decode(state, Change::StrokeBrush,                 &GraphicsContextState::m_strokeBrush))
        return std::nullopt;
    if (!decode(state, Change::StrokeThickness,             &GraphicsContextState::m_strokeThickness))
        return std::nullopt;
    if (!decode(state, Change::StrokeStyle,                 &GraphicsContextState::m_strokeStyle))
        return std::nullopt;

    if (!decode(state, Change::CompositeMode,               &GraphicsContextState::m_compositeMode))
        return std::nullopt;
    if (!decode(state, Change::DropShadow,                  &GraphicsContextState::m_dropShadow))
        return std::nullopt;
    if (!decode(state, Change::Style,                       &GraphicsContextState::m_style))
        return std::nullopt;

    if (!decode(state, Change::Alpha,                       &GraphicsContextState::m_alpha))
        return std::nullopt;
    if (!decode(state, Change::ImageInterpolationQuality,   &GraphicsContextState::m_imageInterpolationQuality))
        return std::nullopt;
    if (!decode(state, Change::TextDrawingMode,             &GraphicsContextState::m_textDrawingMode))
        return std::nullopt;

    if (!decode(state, Change::ShouldAntialias,             &GraphicsContextState::m_shouldAntialias))
        return std::nullopt;
    if (!decode(state, Change::ShouldSmoothFonts,           &GraphicsContextState::m_shouldSmoothFonts))
        return std::nullopt;
    if (!decode(state, Change::ShouldSubpixelQuantizeFonts, &GraphicsContextState::m_shouldSubpixelQuantizeFonts))
        return std::nullopt;
    if (!decode(state, Change::ShadowsIgnoreTransforms,     &GraphicsContextState::m_shadowsIgnoreTransforms))
        return std::nullopt;
    if (!decode(state, Change::DrawLuminanceMask,           &GraphicsContextState::m_drawLuminanceMask))
        return std::nullopt;
#if HAVE(OS_DARK_MODE_SUPPORT)
    if (!decode(state, Change::UseDarkAppearance,           &GraphicsContextState::m_useDarkAppearance))
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
        WebCore::GraphicsContextState::Change::Style,

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
