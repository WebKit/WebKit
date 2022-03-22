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

#include <wtf/text/TextStream.h>

namespace WebCore {

GraphicsContextState::GraphicsContextState(const ChangeFlags& changeFlags)
    : changeFlags(changeFlags)
{
}

bool GraphicsContextState::containsOnlyInlineChanges() const
{
    static constexpr ChangeFlags inlineChangeFlags { Change::StrokeThickness, Change::StrokeBrush, Change::FillBrush };

    if (changeFlags != (changeFlags & inlineChangeFlags))
        return false;

    if (changeFlags.contains(Change::StrokeBrush) && !strokeBrush.isInlineColor())
        return false;

    if (changeFlags.contains(Change::FillBrush) && !fillBrush.isInlineColor())
        return false;

    return true;
}

void GraphicsContextState::mergeChanges(const GraphicsContextState& state, const std::optional<GraphicsContextState>& lastDrawingState)
{
    auto mergeChange = [&](Change change, auto GraphicsContextState::*property) {
        if (!state.changeFlags.contains(change) || this->*property == state.*property)
            return;
        this->*property = state.*property;
        changeFlags.set(change, !lastDrawingState || (*lastDrawingState).*property != this->*property);
    };

    mergeChange(Change::FillBrush,                      &GraphicsContextState::fillBrush);
    mergeChange(Change::FillRule,                       &GraphicsContextState::fillRule);

    mergeChange(Change::StrokeBrush,                    &GraphicsContextState::strokeBrush);
    mergeChange(Change::StrokeThickness,                &GraphicsContextState::strokeThickness);
    mergeChange(Change::StrokeStyle,                    &GraphicsContextState::strokeStyle);

    mergeChange(Change::CompositeMode,                  &GraphicsContextState::compositeMode);
    mergeChange(Change::DropShadow,                     &GraphicsContextState::dropShadow);

    mergeChange(Change::Alpha,                          &GraphicsContextState::alpha);
    mergeChange(Change::ImageInterpolationQuality,      &GraphicsContextState::imageInterpolationQuality);
    mergeChange(Change::TextDrawingMode,                &GraphicsContextState::textDrawingMode);

    mergeChange(Change::ShouldAntialias,                &GraphicsContextState::shouldAntialias);
    mergeChange(Change::ShouldSmoothFonts,              &GraphicsContextState::shouldSmoothFonts);
    mergeChange(Change::ShouldSubpixelQuantizeFonts,    &GraphicsContextState::shouldSubpixelQuantizeFonts);
    mergeChange(Change::ShadowsIgnoreTransforms,        &GraphicsContextState::shadowsIgnoreTransforms);
    mergeChange(Change::DrawLuminanceMask,              &GraphicsContextState::drawLuminanceMask);
#if HAVE(OS_DARK_MODE_SUPPORT)
    mergeChange(Change::UseDarkAppearance,              &GraphicsContextState::useDarkAppearance);
#endif
}

void GraphicsContextState::didBeginTransparencyLayer()
{
#if USE(CG)
    // CGContextBeginTransparencyLayer() sets the CG global alpha to 1. Keep our alpha in sync.
    alpha = 1;
#endif
}

void GraphicsContextState::didEndTransparencyLayer(float originalOpacity)
{
#if USE(CG)
    // CGContextBeginTransparencyLayer() sets the CG global alpha to 1. Resore our alpha now.
    alpha = originalOpacity;
#else
    UNUSED_PARAM(originalOpacity);
#endif
}

static const char* stateChangeName(GraphicsContextState::Change change)
{
    switch (change) {
    case GraphicsContextState::Change::FillBrush:
        return "fill-brush";

    case GraphicsContextState::Change::FillRule:
        return "fill-rule";

    case GraphicsContextState::Change::StrokeBrush:
        return "stroke-brush";

    case GraphicsContextState::Change::StrokeThickness:
        return "stroke-thickness";

    case GraphicsContextState::Change::StrokeStyle:
        return "stroke-style";

    case GraphicsContextState::Change::CompositeMode:
        return "composite-mode";

    case GraphicsContextState::Change::DropShadow:
        return "drop-shadow";

    case GraphicsContextState::Change::Alpha:
        return "alpha";

    case GraphicsContextState::Change::ImageInterpolationQuality:
        return "image-interpolation-quality";

    case GraphicsContextState::Change::TextDrawingMode:
        return "text-drawing-mode";

    case GraphicsContextState::Change::ShouldAntialias:
        return "should-antialias";

    case GraphicsContextState::Change::ShouldSmoothFonts:
        return "should-smooth-fonts";

    case GraphicsContextState::Change::ShouldSubpixelQuantizeFonts:
        return "should-subpixel-quantize-fonts";

    case GraphicsContextState::Change::ShadowsIgnoreTransforms:
        return "shadows-ignore-transforms";

    case GraphicsContextState::Change::DrawLuminanceMask:
        return "draw-luminance-mask";

#if HAVE(OS_DARK_MODE_SUPPORT)
    case GraphicsContextState::Change::UseDarkAppearance:
        return "use-dark-appearance";
#endif
    }
}

TextStream& GraphicsContextState::dump(TextStream& ts) const
{
    auto dump = [&](Change change, auto GraphicsContextState::*property) {
        if (changeFlags.contains(change))
            ts.dumpProperty(stateChangeName(change), this->*property);
    };

    ts.dumpProperty("change-flags", changeFlags);

    dump(Change::FillBrush,                     &GraphicsContextState::fillBrush);
    dump(Change::FillRule,                      &GraphicsContextState::fillRule);

    dump(Change::StrokeBrush,                   &GraphicsContextState::strokeBrush);
    dump(Change::StrokeThickness,               &GraphicsContextState::strokeThickness);
    dump(Change::StrokeStyle,                   &GraphicsContextState::strokeStyle);

    dump(Change::CompositeMode,                 &GraphicsContextState::compositeMode);
    dump(Change::DropShadow,                    &GraphicsContextState::dropShadow);

    dump(Change::Alpha,                         &GraphicsContextState::alpha);
    dump(Change::ImageInterpolationQuality,     &GraphicsContextState::imageInterpolationQuality);
    dump(Change::TextDrawingMode,               &GraphicsContextState::textDrawingMode);

    dump(Change::ShouldAntialias,               &GraphicsContextState::shouldAntialias);
    dump(Change::ShouldSmoothFonts,             &GraphicsContextState::shouldSmoothFonts);
    dump(Change::ShouldSubpixelQuantizeFonts,   &GraphicsContextState::shouldSubpixelQuantizeFonts);
    dump(Change::ShadowsIgnoreTransforms,       &GraphicsContextState::shadowsIgnoreTransforms);
    dump(Change::DrawLuminanceMask,             &GraphicsContextState::drawLuminanceMask);
#if HAVE(OS_DARK_MODE_SUPPORT)
    dump(Change::UseDarkAppearance,             &GraphicsContextState::useDarkAppearance);
#endif
    return ts;
}

TextStream& operator<<(TextStream& ts, GraphicsContextState::Change change)
{
    ts << stateChangeName(change);
    return ts;
}

TextStream& operator<<(TextStream& ts, const GraphicsContextState& state)
{
    return state.dump(ts);
}

} // namespace WebCore
