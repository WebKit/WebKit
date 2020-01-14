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
class Path;
class PlatformContextCairo;

struct GraphicsContextState;

namespace Cairo {

namespace State {

void setStrokeThickness(PlatformContextCairo&, float);
void setStrokeStyle(PlatformContextCairo&, StrokeStyle);

void setCompositeOperation(PlatformContextCairo&, CompositeOperator, BlendMode);
void setShouldAntialias(PlatformContextCairo&, bool);

void setCTM(PlatformContextCairo&, const AffineTransform&);
AffineTransform getCTM(PlatformContextCairo&);

IntRect getClipBounds(PlatformContextCairo&);
FloatRect roundToDevicePixels(PlatformContextCairo&, const FloatRect&);

bool isAcceleratedContext(PlatformContextCairo&);

} // namespace State

struct FillSource {
    FillSource() = default;
    explicit FillSource(const GraphicsContextState&);

    float globalAlpha { 0 };
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

    WindRule fillRule { WindRule::NonZero };
};

struct StrokeSource {
    StrokeSource() = default;
    explicit StrokeSource(const GraphicsContextState&);

    float globalAlpha { 0 };
    RefPtr<cairo_pattern_t> pattern;
    struct {
        RefPtr<cairo_pattern_t> base;
        RefPtr<cairo_pattern_t> alphaAdjusted;
    } gradient;
    Color color;
};

struct ShadowState {
    ShadowState() = default;
    WEBCORE_EXPORT explicit ShadowState(const GraphicsContextState&);

    bool isVisible() const;
    bool isRequired(PlatformContextCairo&) const;

    FloatSize offset;
    float blur { 0 };
    Color color;
    bool ignoreTransforms { false };

    float globalAlpha { 1.0 };
    CompositeOperator globalCompositeOperator { CompositeOperator::SourceOver };
};

void setLineCap(PlatformContextCairo&, LineCap);
void setLineDash(PlatformContextCairo&, const DashArray&, float);
void setLineJoin(PlatformContextCairo&, LineJoin);
void setMiterLimit(PlatformContextCairo&, float);

void fillRect(PlatformContextCairo&, const FloatRect&, const FillSource&, const ShadowState&);
void fillRect(PlatformContextCairo&, const FloatRect&, const Color&, const ShadowState&);
void fillRect(PlatformContextCairo&, const FloatRect&, cairo_pattern_t*);
void fillRoundedRect(PlatformContextCairo&, const FloatRoundedRect&, const Color&, const ShadowState&);
void fillRectWithRoundedHole(PlatformContextCairo&, const FloatRect&, const FloatRoundedRect&, const FillSource&, const ShadowState&);
void fillPath(PlatformContextCairo&, const Path&, const FillSource&, const ShadowState&);
void strokeRect(PlatformContextCairo&, const FloatRect&, float, const StrokeSource&, const ShadowState&);
void strokePath(PlatformContextCairo&, const Path&, const StrokeSource&, const ShadowState&);
void clearRect(PlatformContextCairo&, const FloatRect&);

void drawGlyphs(PlatformContextCairo&, const FillSource&, const StrokeSource&, const ShadowState&, const FloatPoint&, cairo_scaled_font_t*, double, const Vector<cairo_glyph_t>&, float, TextDrawingModeFlags, float, const FloatSize&, const Color&, FontSmoothingMode);

void drawNativeImage(PlatformContextCairo&, cairo_surface_t*, const FloatRect&, const FloatRect&, const ImagePaintingOptions&, float, const ShadowState&);
void drawPattern(PlatformContextCairo&, cairo_surface_t*, const IntSize&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const ImagePaintingOptions&);
WEBCORE_EXPORT void drawSurface(PlatformContextCairo&, cairo_surface_t*, const FloatRect&, const FloatRect&, InterpolationQuality, float, const ShadowState&);

void drawRect(PlatformContextCairo&, const FloatRect&, float, const Color&, StrokeStyle, const Color&);
void drawLine(PlatformContextCairo&, const FloatPoint&, const FloatPoint&, StrokeStyle, const Color&, float, bool);
void drawLinesForText(PlatformContextCairo&, const FloatPoint&, float thickness, const DashArray&, bool, bool, const Color&);
void drawDotsForDocumentMarker(PlatformContextCairo&, const FloatRect&, DocumentMarkerLineStyle);
void drawEllipse(PlatformContextCairo&, const FloatRect&, const Color&, StrokeStyle, const Color&, float);

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
