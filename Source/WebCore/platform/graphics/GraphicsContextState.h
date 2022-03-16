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

#include "AffineTransform.h"
#include "Color.h"
#include "FloatSize.h"
#include "GraphicsTypes.h"
#include "WindRule.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class GraphicsContext;
class Gradient;
class Pattern;

struct GraphicsContextState {
    WEBCORE_EXPORT GraphicsContextState();
    WEBCORE_EXPORT ~GraphicsContextState();

    GraphicsContextState(const GraphicsContextState&);
    WEBCORE_EXPORT GraphicsContextState(GraphicsContextState&&);

    GraphicsContextState& operator=(const GraphicsContextState&);
    WEBCORE_EXPORT GraphicsContextState& operator=(GraphicsContextState&&);

    enum Change : uint32_t {
        StrokeGradientChange                    = 1 << 0,
        StrokePatternChange                     = 1 << 1,
        FillGradientChange                      = 1 << 2,
        FillPatternChange                       = 1 << 3,
        StrokeThicknessChange                   = 1 << 4,
        StrokeColorChange                       = 1 << 5,
        StrokeStyleChange                       = 1 << 6,
        FillColorChange                         = 1 << 7,
        FillRuleChange                          = 1 << 8,
        ShadowChange                            = 1 << 9,
        ShadowsIgnoreTransformsChange           = 1 << 10,
        AlphaChange                             = 1 << 11,
        CompositeOperationChange                = 1 << 12,
        BlendModeChange                         = 1 << 13,
        TextDrawingModeChange                   = 1 << 14,
        ShouldAntialiasChange                   = 1 << 15,
        ShouldSmoothFontsChange                 = 1 << 16,
        ShouldSubpixelQuantizeFontsChange       = 1 << 17,
        DrawLuminanceMaskChange                 = 1 << 18,
        ImageInterpolationQualityChange         = 1 << 19,
#if HAVE(OS_DARK_MODE_SUPPORT)
        UseDarkAppearanceChange                 = 1 << 20,
#endif
    };
    using StateChangeFlags = OptionSet<Change>;

    void mergeChanges(const GraphicsContextState&, GraphicsContextState::StateChangeFlags);

    RefPtr<Gradient> strokeGradient;
    RefPtr<Pattern> strokePattern;
    
    RefPtr<Gradient> fillGradient;
    RefPtr<Pattern> fillPattern;

    FloatSize shadowOffset;

    Color strokeColor { Color::black };
    Color fillColor { Color::black };
    Color shadowColor;

    AffineTransform strokeGradientSpaceTransform;
    AffineTransform fillGradientSpaceTransform;

    float strokeThickness { 0 };
    float shadowBlur { 0 };
    float alpha { 1 };

    StrokeStyle strokeStyle { SolidStroke };
    WindRule fillRule { WindRule::NonZero };

    TextDrawingModeFlags textDrawingMode { TextDrawingMode::Fill };
    CompositeOperator compositeOperator { CompositeOperator::SourceOver };
    BlendMode blendMode { BlendMode::Normal };
    InterpolationQuality imageInterpolationQuality { InterpolationQuality::Default };
    ShadowRadiusMode shadowRadiusMode { ShadowRadiusMode::Default };

    bool shouldAntialias : 1;
    bool shouldSmoothFonts : 1;
    bool shouldSubpixelQuantizeFonts : 1;
    bool shadowsIgnoreTransforms : 1;
    bool drawLuminanceMask : 1;
#if HAVE(OS_DARK_MODE_SUPPORT)
    bool useDarkAppearance : 1;
#endif
};

struct GraphicsContextStateChange {
    GraphicsContextStateChange() = default;
    GraphicsContextStateChange(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
        : m_state(state)
        , m_changeFlags(flags)
    {
    }

    GraphicsContextState::StateChangeFlags changesFromState(const GraphicsContextState&) const;

    void accumulate(const GraphicsContextState&, GraphicsContextState::StateChangeFlags);
    void apply(GraphicsContext&) const;
    
    void dump(WTF::TextStream&) const;

    GraphicsContextState m_state;
    GraphicsContextState::StateChangeFlags m_changeFlags;
};

WTF::TextStream& operator<<(WTF::TextStream&, const GraphicsContextStateChange&);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::GraphicsContextState::Change> {
    using values = EnumValues<
        WebCore::GraphicsContextState::Change,
        WebCore::GraphicsContextState::Change::StrokeGradientChange,
        WebCore::GraphicsContextState::Change::StrokePatternChange,
        WebCore::GraphicsContextState::Change::FillGradientChange,
        WebCore::GraphicsContextState::Change::FillPatternChange,
        WebCore::GraphicsContextState::Change::StrokeThicknessChange,
        WebCore::GraphicsContextState::Change::StrokeColorChange,
        WebCore::GraphicsContextState::Change::StrokeStyleChange,
        WebCore::GraphicsContextState::Change::FillColorChange,
        WebCore::GraphicsContextState::Change::FillRuleChange,
        WebCore::GraphicsContextState::Change::ShadowChange,
        WebCore::GraphicsContextState::Change::ShadowsIgnoreTransformsChange,
        WebCore::GraphicsContextState::Change::AlphaChange,
        WebCore::GraphicsContextState::Change::CompositeOperationChange,
        WebCore::GraphicsContextState::Change::BlendModeChange,
        WebCore::GraphicsContextState::Change::TextDrawingModeChange,
        WebCore::GraphicsContextState::Change::ShouldAntialiasChange,
        WebCore::GraphicsContextState::Change::ShouldSmoothFontsChange,
        WebCore::GraphicsContextState::Change::ShouldSubpixelQuantizeFontsChange,
        WebCore::GraphicsContextState::Change::DrawLuminanceMaskChange,
        WebCore::GraphicsContextState::Change::ImageInterpolationQualityChange
#if HAVE(OS_DARK_MODE_SUPPORT)
        , WebCore::GraphicsContextState::Change::UseDarkAppearanceChange
#endif
    >;
};

} // namespace WTF
