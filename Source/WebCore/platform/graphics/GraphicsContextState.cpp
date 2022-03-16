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

#include "config.h"
#include "GraphicsContextState.h"

#include "Gradient.h"
#include "GraphicsContext.h"
#include "Pattern.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

#define CHECK_FOR_CHANGED_PROPERTY(flag, property) \
    if (m_changeFlags.contains(GraphicsContextState::flag) && (m_state.property != state.property)) \
        changeFlags.add(GraphicsContextState::flag);

GraphicsContextState::GraphicsContextState()
    : shouldAntialias(true)
    , shouldSmoothFonts(true)
    , shouldSubpixelQuantizeFonts(true)
    , shadowsIgnoreTransforms(false)
    , drawLuminanceMask(false)
{
}

GraphicsContextState::~GraphicsContextState() = default;
GraphicsContextState::GraphicsContextState(const GraphicsContextState&) = default;
GraphicsContextState::GraphicsContextState(GraphicsContextState&&) = default;
GraphicsContextState& GraphicsContextState::operator=(const GraphicsContextState&) = default;
GraphicsContextState& GraphicsContextState::operator=(GraphicsContextState&&) = default;

void GraphicsContextState::mergeChanges(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    auto strokeFlags = { GraphicsContextState::StrokeColorChange, GraphicsContextState::StrokeGradientChange, GraphicsContextState::StrokePatternChange };
    if (flags.containsAny(strokeFlags)) {
        strokeColor = state.strokeColor;
        strokeGradient = state.strokeGradient;
        strokePattern = state.strokePattern;
    }

    auto fillFlags = { GraphicsContextState::FillColorChange, GraphicsContextState::FillGradientChange, GraphicsContextState::FillPatternChange };
    if (flags.containsAny(fillFlags)) {
        fillColor = state.fillColor;
        fillGradient = state.fillGradient;
        fillPattern = state.fillPattern;
    }

    if (flags.contains(GraphicsContextState::ShadowChange)) {
        shadowOffset = state.shadowOffset;
        shadowBlur = state.shadowBlur;
        shadowColor = state.shadowColor;
        shadowRadiusMode = state.shadowRadiusMode;
    }

    if (flags.contains(GraphicsContextState::StrokeThicknessChange))
        strokeThickness = state.strokeThickness;

    if (flags.contains(GraphicsContextState::TextDrawingModeChange))
        textDrawingMode = state.textDrawingMode;

    if (flags.contains(GraphicsContextState::StrokeStyleChange))
        strokeStyle = state.strokeStyle;

    if (flags.contains(GraphicsContextState::FillRuleChange))
        fillRule = state.fillRule;

    if (flags.contains(GraphicsContextState::AlphaChange))
        alpha = state.alpha;

    if (flags.containsAny({ GraphicsContextState::CompositeOperationChange, GraphicsContextState::BlendModeChange })) {
        compositeOperator = state.compositeOperator;
        blendMode = state.blendMode;
    }

    if (flags.contains(GraphicsContextState::ShouldAntialiasChange))
        shouldAntialias = state.shouldAntialias;

    if (flags.contains(GraphicsContextState::ShouldSmoothFontsChange))
        shouldSmoothFonts = state.shouldSmoothFonts;

    if (flags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange))
        shouldSubpixelQuantizeFonts = state.shouldSubpixelQuantizeFonts;

    if (flags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange))
        shadowsIgnoreTransforms = state.shadowsIgnoreTransforms;

    if (flags.contains(GraphicsContextState::DrawLuminanceMaskChange))
        drawLuminanceMask = state.drawLuminanceMask;

    if (flags.contains(GraphicsContextState::ImageInterpolationQualityChange))
        imageInterpolationQuality = state.imageInterpolationQuality;

#if HAVE(OS_DARK_MODE_SUPPORT)
    if (flags.contains(GraphicsContextState::UseDarkAppearanceChange))
        useDarkAppearance = state.useDarkAppearance;
#endif
}

GraphicsContextState::StateChangeFlags GraphicsContextStateChange::changesFromState(const GraphicsContextState& state) const
{
    GraphicsContextState::StateChangeFlags changeFlags;

    CHECK_FOR_CHANGED_PROPERTY(StrokeGradientChange, strokeGradient);
    CHECK_FOR_CHANGED_PROPERTY(StrokePatternChange, strokePattern);
    CHECK_FOR_CHANGED_PROPERTY(FillGradientChange, fillGradient);
    CHECK_FOR_CHANGED_PROPERTY(FillPatternChange, fillPattern);

    if (m_changeFlags.contains(GraphicsContextState::ShadowChange)
        && (m_state.shadowOffset != state.shadowOffset
            || m_state.shadowBlur != state.shadowBlur
            || m_state.shadowColor != state.shadowColor))
        changeFlags.add(GraphicsContextState::ShadowChange);

    CHECK_FOR_CHANGED_PROPERTY(StrokeThicknessChange, strokeThickness);
    CHECK_FOR_CHANGED_PROPERTY(TextDrawingModeChange, textDrawingMode);
    CHECK_FOR_CHANGED_PROPERTY(StrokeColorChange, strokeColor);
    CHECK_FOR_CHANGED_PROPERTY(FillColorChange, fillColor);
    CHECK_FOR_CHANGED_PROPERTY(StrokeStyleChange, strokeStyle);
    CHECK_FOR_CHANGED_PROPERTY(FillRuleChange, fillRule);
    CHECK_FOR_CHANGED_PROPERTY(AlphaChange, alpha);

    if (m_changeFlags.containsAny({ GraphicsContextState::CompositeOperationChange, GraphicsContextState::BlendModeChange })
        && (m_state.compositeOperator != state.compositeOperator || m_state.blendMode != state.blendMode)) {
        changeFlags.add(GraphicsContextState::CompositeOperationChange);
        changeFlags.add(GraphicsContextState::BlendModeChange);
    }

    CHECK_FOR_CHANGED_PROPERTY(ShouldAntialiasChange, shouldAntialias);
    CHECK_FOR_CHANGED_PROPERTY(ShouldSmoothFontsChange, shouldSmoothFonts);
    CHECK_FOR_CHANGED_PROPERTY(ShouldSubpixelQuantizeFontsChange, shouldSubpixelQuantizeFonts);
    CHECK_FOR_CHANGED_PROPERTY(ShadowsIgnoreTransformsChange, shadowsIgnoreTransforms);
    CHECK_FOR_CHANGED_PROPERTY(DrawLuminanceMaskChange, drawLuminanceMask);
    CHECK_FOR_CHANGED_PROPERTY(ImageInterpolationQualityChange, imageInterpolationQuality);

#if HAVE(OS_DARK_MODE_SUPPORT)
    CHECK_FOR_CHANGED_PROPERTY(UseDarkAppearanceChange, useDarkAppearance);
#endif

    return changeFlags;
}

void GraphicsContextStateChange::accumulate(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    m_state.mergeChanges(state, flags);

    auto strokeFlags = { GraphicsContextState::StrokeColorChange, GraphicsContextState::StrokeGradientChange, GraphicsContextState::StrokePatternChange };
    if (flags.containsAny(strokeFlags))
        m_changeFlags.remove(strokeFlags);

    auto fillFlags = { GraphicsContextState::FillColorChange, GraphicsContextState::FillGradientChange, GraphicsContextState::FillPatternChange };
    if (flags.containsAny(fillFlags))
        m_changeFlags.remove(fillFlags);

    auto compositeOperatorFlags = { GraphicsContextState::CompositeOperationChange, GraphicsContextState::BlendModeChange };
    if (flags.containsAny(compositeOperatorFlags))
        m_changeFlags.remove(compositeOperatorFlags);

    m_changeFlags.add(flags);
}

void GraphicsContextStateChange::apply(GraphicsContext& context) const
{
    if (m_changeFlags.contains(GraphicsContextState::StrokeGradientChange))
        context.setStrokeGradient(*m_state.strokeGradient, m_state.strokeGradientSpaceTransform);

    if (m_changeFlags.contains(GraphicsContextState::StrokePatternChange))
        context.setStrokePattern(*m_state.strokePattern);

    if (m_changeFlags.contains(GraphicsContextState::FillGradientChange))
        context.setFillGradient(*m_state.fillGradient, m_state.fillGradientSpaceTransform);

    if (m_changeFlags.contains(GraphicsContextState::FillPatternChange))
        context.setFillPattern(*m_state.fillPattern);

    if (m_changeFlags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange))
        context.setShadowsIgnoreTransforms(m_state.shadowsIgnoreTransforms);

    if (m_changeFlags.contains(GraphicsContextState::ShadowChange))
        context.setShadow(m_state.shadowOffset, m_state.shadowBlur, m_state.shadowColor, m_state.shadowRadiusMode);

    if (m_changeFlags.contains(GraphicsContextState::StrokeThicknessChange))
        context.setStrokeThickness(m_state.strokeThickness);

    if (m_changeFlags.contains(GraphicsContextState::TextDrawingModeChange))
        context.setTextDrawingMode(m_state.textDrawingMode);

    if (m_changeFlags.contains(GraphicsContextState::StrokeColorChange))
        context.setStrokeColor(m_state.strokeColor);

    if (m_changeFlags.contains(GraphicsContextState::FillColorChange))
        context.setFillColor(m_state.fillColor);

    if (m_changeFlags.contains(GraphicsContextState::StrokeStyleChange))
        context.setStrokeStyle(m_state.strokeStyle);

    if (m_changeFlags.contains(GraphicsContextState::FillRuleChange))
        context.setFillRule(m_state.fillRule);

    if (m_changeFlags.contains(GraphicsContextState::AlphaChange))
        context.setAlpha(m_state.alpha);

    if (m_changeFlags.containsAny({ GraphicsContextState::CompositeOperationChange, GraphicsContextState::BlendModeChange }))
        context.setCompositeOperation(m_state.compositeOperator, m_state.blendMode);

    if (m_changeFlags.contains(GraphicsContextState::ShouldAntialiasChange))
        context.setShouldAntialias(m_state.shouldAntialias);

    if (m_changeFlags.contains(GraphicsContextState::ShouldSmoothFontsChange))
        context.setShouldSmoothFonts(m_state.shouldSmoothFonts);

    if (m_changeFlags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange))
        context.setShouldSubpixelQuantizeFonts(m_state.shouldSubpixelQuantizeFonts);

    if (m_changeFlags.contains(GraphicsContextState::DrawLuminanceMaskChange))
        context.setDrawLuminanceMask(m_state.drawLuminanceMask);

    if (m_changeFlags.contains(GraphicsContextState::ImageInterpolationQualityChange))
        context.setImageInterpolationQuality(m_state.imageInterpolationQuality);

#if HAVE(OS_DARK_MODE_SUPPORT)
    if (m_changeFlags.contains(GraphicsContextState::UseDarkAppearanceChange))
        context.setUseDarkAppearance(m_state.useDarkAppearance);
#endif
}

void GraphicsContextStateChange::dump(TextStream& ts) const
{
    ts.dumpProperty("change-flags", m_changeFlags.toRaw());

    if (m_changeFlags.contains(GraphicsContextState::StrokeGradientChange))
        ts.dumpProperty("stroke-gradient", m_state.strokeGradient.get());

    if (m_changeFlags.contains(GraphicsContextState::StrokePatternChange))
        ts.dumpProperty("stroke-pattern", m_state.strokePattern.get());

    if (m_changeFlags.contains(GraphicsContextState::FillGradientChange))
        ts.dumpProperty("fill-gradient", m_state.fillGradient.get());

    if (m_changeFlags.contains(GraphicsContextState::FillPatternChange))
        ts.dumpProperty("fill-pattern", m_state.fillPattern.get());

    if (m_changeFlags.contains(GraphicsContextState::ShadowChange)) {
        ts.dumpProperty("shadow-blur", m_state.shadowBlur);
        ts.dumpProperty("shadow-offset", m_state.shadowOffset);
        ts.dumpProperty("shadow-color", m_state.shadowColor);
        ts.dumpProperty("shadows-use-legacy-radius", m_state.shadowRadiusMode == ShadowRadiusMode::Legacy);
    }

    if (m_changeFlags.contains(GraphicsContextState::StrokeThicknessChange))
        ts.dumpProperty("stroke-thickness", m_state.strokeThickness);

    if (m_changeFlags.contains(GraphicsContextState::TextDrawingModeChange))
        ts.dumpProperty("text-drawing-mode", m_state.textDrawingMode.toRaw());

    if (m_changeFlags.contains(GraphicsContextState::StrokeColorChange))
        ts.dumpProperty("stroke-color", m_state.strokeColor);

    if (m_changeFlags.contains(GraphicsContextState::FillColorChange))
        ts.dumpProperty("fill-color", m_state.fillColor);

    if (m_changeFlags.contains(GraphicsContextState::StrokeStyleChange))
        ts.dumpProperty("stroke-style", m_state.strokeStyle);

    if (m_changeFlags.contains(GraphicsContextState::FillRuleChange))
        ts.dumpProperty("fill-rule", m_state.fillRule);

    if (m_changeFlags.contains(GraphicsContextState::AlphaChange))
        ts.dumpProperty("alpha", m_state.alpha);

    if (m_changeFlags.contains(GraphicsContextState::CompositeOperationChange))
        ts.dumpProperty("composite-operator", m_state.compositeOperator);

    if (m_changeFlags.contains(GraphicsContextState::BlendModeChange))
        ts.dumpProperty("blend-mode", m_state.blendMode);

    if (m_changeFlags.contains(GraphicsContextState::ShouldAntialiasChange))
        ts.dumpProperty("should-antialias", m_state.shouldAntialias);

    if (m_changeFlags.contains(GraphicsContextState::ShouldSmoothFontsChange))
        ts.dumpProperty("should-smooth-fonts", m_state.shouldSmoothFonts);

    if (m_changeFlags.contains(GraphicsContextState::ShouldSubpixelQuantizeFontsChange))
        ts.dumpProperty("should-subpixel-quantize-fonts", m_state.shouldSubpixelQuantizeFonts);

    if (m_changeFlags.contains(GraphicsContextState::ShadowsIgnoreTransformsChange))
        ts.dumpProperty("shadows-ignore-transforms", m_state.shadowsIgnoreTransforms);

    if (m_changeFlags.contains(GraphicsContextState::DrawLuminanceMaskChange))
        ts.dumpProperty("draw-luminance-mask", m_state.drawLuminanceMask);

#if HAVE(OS_DARK_MODE_SUPPORT)
    if (m_changeFlags.contains(GraphicsContextState::UseDarkAppearanceChange))
        ts.dumpProperty("use-dark-appearance", m_state.useDarkAppearance);
#endif
}

TextStream& operator<<(TextStream& ts, const GraphicsContextStateChange& stateChange)
{
    stateChange.dump(ts);
    return ts;
}

} // namespace WebCore
