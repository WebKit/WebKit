/*
 * Copyright (C) 2006-2019 Apple Inc.  All rights reserved.
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

#if USE(DIRECT2D)

#include "DashArray.h"
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <d2d1.h>

namespace WebCore {

class AffineTransform;
class Color;
class FloatRect;
class FloatRoundedRect;
class FloatSize;
class Path;
class PlatformContextCairo;

struct GraphicsContextState;

namespace Direct2D {

namespace State {

void setStrokeThickness(PlatformContextDirect2D&, float);
void setStrokeStyle(PlatformContextDirect2D&, StrokeStyle);

void setCompositeOperation(PlatformContextDirect2D&, CompositeOperator, BlendMode);
void setShouldAntialias(PlatformContextDirect2D&, bool);

void setCTM(PlatformContextDirect2D&, const AffineTransform&);
AffineTransform getCTM(PlatformContextDirect2D&);

IntRect getClipBounds(PlatformContextDirect2D&);
FloatRect roundToDevicePixels(PlatformContextDirect2D&, const FloatRect&);

bool isAcceleratedContext(PlatformContextDirect2D&);

} // namespace State

struct FillSource {
    FillSource() = default;
    explicit FillSource(const GraphicsContextState&, PlatformContextDirect2D&);

    float globalAlpha { 0 };
    COMPtr<ID2D1Brush> brush;
    Color color;

    WindRule fillRule { WindRule::NonZero };
};

struct StrokeSource {
    StrokeSource() = default;
    explicit StrokeSource(const GraphicsContextState&, PlatformContextDirect2D&);

    float globalAlpha { 0 };
    float thickness { 0 };
    COMPtr<ID2D1Brush> brush;
    Color color;
};

struct ShadowState {
    ShadowState() = default;
    WEBCORE_EXPORT explicit ShadowState(const GraphicsContextState&);

    bool isVisible() const;
    bool isRequired(PlatformContextDirect2D&) const;

    FloatSize offset;
    float blur { 0 };
    Color color;
    bool ignoreTransforms { false };

    float globalAlpha { 1.0 };
    CompositeOperator globalCompositeOperator { CompositeSourceOver };
};

void setLineCap(PlatformContextDirect2D&, LineCap);
void setLineDash(PlatformContextDirect2D&, const DashArray&, float);
void setLineJoin(PlatformContextDirect2D&, LineJoin);
void setMiterLimit(PlatformContextDirect2D&, float);

void fillRect(PlatformContextDirect2D&, const FloatRect&, const FillSource&, const ShadowState&);
void fillRect(PlatformContextDirect2D&, const FloatRect&, const Color&, const ShadowState&);
void fillRectWithGradient(PlatformContextDirect2D&, const FloatRect&, ID2D1Brush*);
void fillRoundedRect(PlatformContextDirect2D&, const FloatRoundedRect&, const Color&, const ShadowState&);
void fillRectWithRoundedHole(PlatformContextDirect2D&, const FloatRect&, const FloatRoundedRect&, const FillSource&, const ShadowState&);
void fillPath(PlatformContextDirect2D&, const Path&, const FillSource&, const ShadowState&);
void fillPath(PlatformContextDirect2D&, const Path&, const Color&, const ShadowState&);

void strokeRect(PlatformContextDirect2D&, const FloatRect&, float, const StrokeSource&, const ShadowState&);
void strokePath(PlatformContextDirect2D&, const Path&, const StrokeSource&, const ShadowState&);
void clearRect(PlatformContextDirect2D&, const FloatRect&);

void drawGlyphs(PlatformContextDirect2D&, const FillSource&, const StrokeSource&, const ShadowState&, const FloatPoint&, const Font&, double, const Vector<unsigned short>& glyphs, const Vector<float>& horizontalAdvances, const Vector<DWRITE_GLYPH_OFFSET>&, float, TextDrawingModeFlags, float, const FloatSize&, const Color&);

void drawNativeImage(PlatformContextDirect2D&, IWICBitmap*, const FloatSize& imageSize, const FloatRect&, const FloatRect&, CompositeOperator, BlendMode, ImageOrientation, InterpolationQuality, float, const ShadowState&);
void drawNativeImage(PlatformContextDirect2D&, ID2D1Bitmap*, const FloatSize& imageSize, const FloatRect&, const FloatRect&, CompositeOperator, BlendMode, ImageOrientation, InterpolationQuality, float, const ShadowState&);
void drawPath(PlatformContextDirect2D&, const Path&, const StrokeSource&, const ShadowState&);
void drawPattern(PlatformContextDirect2D&, COMPtr<ID2D1Bitmap>&&, const IntSize&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, CompositeOperator, BlendMode);

void drawWithoutShadow(PlatformContextDirect2D&, const FloatRect& boundingRect, const WTF::Function<void(ID2D1RenderTarget*)>& drawCommands);
void drawWithShadow(PlatformContextDirect2D&, const FloatRect& boundingRect, const ShadowState&, const WTF::Function<void(ID2D1RenderTarget*)>& drawCommands);
void drawWithShadowHelper(ID2D1RenderTarget* context, ID2D1Bitmap*, const Color& shadowColor, const FloatSize& shadowOffset, float shadowBlur);

void drawRect(PlatformContextDirect2D&, const FloatRect&, float, const Color&, StrokeStyle, const Color&);
void drawLine(PlatformContextDirect2D&, const FloatPoint&, const FloatPoint&, StrokeStyle, const Color&, float, bool);
void drawLinesForText(PlatformContextDirect2D&, const FloatPoint&, float thickness, const DashArray&, bool, bool, const Color&);
void drawDotsForDocumentMarker(PlatformContextDirect2D&, const FloatRect&, DocumentMarkerLineStyle);
void drawEllipse(PlatformContextDirect2D&, const FloatRect&, StrokeStyle, const Color&, float);
void fillEllipse(PlatformContextDirect2D&, const FloatRect&, const Color&, StrokeStyle, const Color&, float);

void drawFocusRing(PlatformContextDirect2D&, const Path&, float, const Color&);
void drawFocusRing(PlatformContextDirect2D&, const Vector<FloatRect>&, float, const Color&);

void flush(PlatformContextDirect2D&);
void save(PlatformContextDirect2D&);
void restore(PlatformContextDirect2D&);

void translate(PlatformContextDirect2D&, float, float);
void rotate(PlatformContextDirect2D&, float);
void scale(PlatformContextDirect2D&, const FloatSize&);
void concatCTM(PlatformContextDirect2D&, const AffineTransform&);

void beginTransparencyLayer(PlatformContextDirect2D&, float);
void endTransparencyLayer(PlatformContextDirect2D&);

void clip(PlatformContextDirect2D&, const FloatRect&);

void clipOut(PlatformContextDirect2D&, const FloatRect&);
void clipOut(PlatformContextDirect2D&, const Path&);
void clipPath(PlatformContextDirect2D&, const Path&, WindRule);
void clipPath(PlatformContextDirect2D&, ID2D1Geometry* path);

void clipToImageBuffer(PlatformContextDirect2D&, ID2D1RenderTarget*, const FloatRect&);

} // namespace Direct2D
} // namespace WebCore

#endif // USE(DIRECT2D)
