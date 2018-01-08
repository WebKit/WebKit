/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012, Intel Corporation
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

#if USE(CAIRO)

#include "DashArray.h"
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include <cairo.h>

namespace WebCore {

class AffineTransform;
class Color;
class FloatRect;
class FloatRoundedRect;
class FloatSize;
class GraphicsContext;
class Path;
class PlatformContextCairo;

struct GraphicsContextState;

namespace Cairo {

namespace State {

void setStrokeThickness(PlatformContextCairo&, float);
void setStrokeStyle(PlatformContextCairo&, StrokeStyle);

void setGlobalAlpha(PlatformContextCairo&, float);
void setCompositeOperation(PlatformContextCairo&, CompositeOperator, BlendMode);
void setShouldAntialias(PlatformContextCairo&, bool);
void setImageInterpolationQuality(PlatformContextCairo&, InterpolationQuality);

void setCTM(PlatformContextCairo&, const AffineTransform&);
AffineTransform getCTM(PlatformContextCairo&);

void setShadowValues(PlatformContextCairo&, const FloatSize&, const FloatSize&, const Color&, bool);
void clearShadow(PlatformContextCairo&);

IntRect getClipBounds(PlatformContextCairo&);
FloatRect roundToDevicePixels(PlatformContextCairo&, const FloatRect&);

bool isAcceleratedContext(PlatformContextCairo&);

} // namespace State

struct FillSource {
    FillSource() = default;
    explicit FillSource(const GraphicsContextState&);

    struct {
        RefPtr<cairo_pattern_t> object;
        FloatSize size;
        AffineTransform transform;
        bool repeatX { false };
        bool repeatY { false };
    } pattern;
    struct {
        RefPtr<cairo_pattern_t> base;
        RefPtr<cairo_pattern_t> alphaAdjusted;
    } gradient;
    Color color;

    WindRule fillRule;
};

struct StrokeSource {
    StrokeSource() = default;
    explicit StrokeSource(const GraphicsContextState&);

    RefPtr<cairo_pattern_t> pattern;
    struct {
        RefPtr<cairo_pattern_t> base;
        RefPtr<cairo_pattern_t> alphaAdjusted;
    } gradient;
    Color color;
};

struct ShadowBlurUsage {
    explicit ShadowBlurUsage(const GraphicsContextState&);

    bool required(PlatformContextCairo&) const;

    Color shadowColor;
    float shadowBlur { 0 };
    bool shadowsIgnoreTransforms { false };
};

void setLineCap(PlatformContextCairo&, LineCap);
void setLineDash(PlatformContextCairo&, const DashArray&, float);
void setLineJoin(PlatformContextCairo&, LineJoin);
void setMiterLimit(PlatformContextCairo&, float);

void fillRect(PlatformContextCairo&, const FloatRect&, const FillSource&, GraphicsContext&);
void fillRect(PlatformContextCairo&, const FloatRect&, const Color&, bool, GraphicsContext&);
void fillRect(PlatformContextCairo&, const FloatRect&, cairo_pattern_t*);
void fillRoundedRect(PlatformContextCairo&, const FloatRoundedRect&, const Color&, bool, GraphicsContext&);
void fillRectWithRoundedHole(PlatformContextCairo&, const FloatRect&, const FloatRoundedRect&, const FillSource&, const ShadowBlurUsage&, GraphicsContext&);
void fillPath(PlatformContextCairo&, const Path&, const FillSource&, GraphicsContext&);
void strokeRect(PlatformContextCairo&, const FloatRect&, float, const StrokeSource&, GraphicsContext&);
void strokePath(PlatformContextCairo&, const Path&, const StrokeSource&, GraphicsContext&);
void clearRect(PlatformContextCairo&, const FloatRect&);

void drawGlyphs(PlatformContextCairo&, const GraphicsContextState&, const FillSource&, const StrokeSource&, const ShadowBlurUsage&, const FloatPoint&, cairo_scaled_font_t*, double, const Vector<cairo_glyph_t>&, float, GraphicsContext&);

void drawNativeImage(PlatformContextCairo&, cairo_surface_t*, const FloatRect&, const FloatRect&, CompositeOperator, BlendMode, ImageOrientation, GraphicsContext&);
void drawPattern(PlatformContextCairo&, cairo_surface_t*, const IntSize&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, CompositeOperator, BlendMode);

void drawRect(PlatformContextCairo&, const FloatRect&, float, const GraphicsContextState&);
void drawLine(PlatformContextCairo&, const FloatPoint&, const FloatPoint&, const GraphicsContextState&);
void drawLinesForText(PlatformContextCairo&, const FloatPoint&, const DashArray&, bool, bool, const Color&, float);
void drawLineForDocumentMarker(PlatformContextCairo&, const FloatPoint&, float, GraphicsContext::DocumentMarkerLineStyle);
void drawEllipse(PlatformContextCairo&, const FloatRect&, const GraphicsContextState&);

void drawFocusRing(PlatformContextCairo&, const Path&, float, const Color&);
void drawFocusRing(PlatformContextCairo&, const Vector<FloatRect>&, float, const Color&);

void save(PlatformContextCairo&);
void restore(PlatformContextCairo&);

void translate(PlatformContextCairo&, float, float);
void rotate(PlatformContextCairo&, float);
void scale(PlatformContextCairo&, const FloatSize&);
void concatCTM(PlatformContextCairo&, const AffineTransform&);

void beginTransparencyLayer(PlatformContextCairo&, float);
void endTransparencyLayer(PlatformContextCairo&);

void clip(PlatformContextCairo&, const FloatRect&);
void clipOut(PlatformContextCairo&, const FloatRect&);
void clipOut(PlatformContextCairo&, const Path&);
void clipPath(PlatformContextCairo&, const Path&, WindRule);

void clipToImageBuffer(PlatformContextCairo&, cairo_surface_t*, const FloatRect&);

} // namespace Cairo
} // namespace WebCore

#endif // USE(CAIRO)
