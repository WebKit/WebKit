/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "GraphicsContext.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

// BifurcatedGraphicsContext allows you to duplicate painting between two given GraphicsContexts;
// for example, for painting into a bitmap-backed and a display-list-backed context simultaneously.
// Any state that is returned by GraphicsContext methods will be retrieved from the primary context.

class WEBCORE_EXPORT BifurcatedGraphicsContext : public GraphicsContext {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    BifurcatedGraphicsContext(GraphicsContext& primaryContext, GraphicsContext& secondaryContext);
    ~BifurcatedGraphicsContext();

    bool hasPlatformContext() const;
    PlatformGraphicsContext* platformContext() const final;

    void save() final;
    void restore() final;

    void drawRect(const FloatRect&, float borderThickness = 1) final;
    void drawLine(const FloatPoint&, const FloatPoint&) final;
    void drawEllipse(const FloatRect&) final;

#if USE(CG) || USE(DIRECT2D)
    void applyStrokePattern() final;
    void applyFillPattern() final;
#endif
    void drawPath(const Path&) final;
    void fillPath(const Path&) final;
    void strokePath(const Path&) final;

    void beginTransparencyLayer(float opacity) final;
    void endTransparencyLayer() final;

    void applyDeviceScaleFactor(float factor) final;

    using GraphicsContext::fillRect;
    void fillRect(const FloatRect&) final;
    void fillRect(const FloatRect&, const Color&) final;
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&) final;
    void clearRect(const FloatRect&) final;
    void strokeRect(const FloatRect&, float lineWidth) final;

    void fillEllipse(const FloatRect& ellipse) final;
    void strokeEllipse(const FloatRect& ellipse) final;

#if USE(CG) || USE(DIRECT2D)
    void setIsCALayerContext(bool) final;
    bool isCALayerContext() const final;

    void setIsAcceleratedContext(bool) final;
#endif

    RenderingMode renderingMode() const final;

    void clip(const FloatRect&) final;
    void clipOut(const FloatRect&) final;

    void clipOut(const Path&) final;

    void clipPath(const Path&, WindRule = WindRule::EvenOdd) final;

    IntRect clipBounds() const final;

    void setLineCap(LineCap) final;
    void setLineDash(const DashArray&, float dashOffset) final;
    void setLineJoin(LineJoin) final;
    void setMiterLimit(float) final;

    void drawNativeImage(NativeImage&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& = { }) final;
    void drawPattern(NativeImage&, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { }) final;

#if ENABLE(VIDEO)
    void paintFrameForMedia(MediaPlayer&, const FloatRect& destination) final;
#endif

    using GraphicsContext::scale;
    void scale(const FloatSize&) final;
    void rotate(float angleInRadians) final;
    void translate(float x, float y) final;

    void concatCTM(const AffineTransform&) final;
    void setCTM(const AffineTransform&) final;

    AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const final;

    FloatRect roundToDevicePixels(const FloatRect&, RoundingMode = RoundAllSides) final;

    void drawFocusRing(const Vector<FloatRect>&, float, float, const Color&) final;
    void drawFocusRing(const Path&, float, float, const Color&) final;
#if PLATFORM(MAC)
    void drawFocusRing(const Path&, double, bool&, const Color&) final;
    void drawFocusRing(const Vector<FloatRect>&, double, bool&, const Color&) final;
#endif

    FloatSize drawText(const FontCascade&, const TextRun&, const FloatPoint&, unsigned from = 0, std::optional<unsigned> to = std::nullopt) final;
    void drawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned numGlyphs, const FloatPoint&, FontSmoothingMode) final;
    void drawEmphasisMarks(const FontCascade&, const TextRun&, const AtomString& mark, const FloatPoint&, unsigned from = 0, std::optional<unsigned> to = std::nullopt) final;
    void drawBidiText(const FontCascade&, const TextRun&, const FloatPoint&, FontCascade::CustomFontNotReadyAction = FontCascade::DoNotPaintIfFontNotReady) final;

    void drawLinesForText(const FloatPoint&, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle) final;

    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;

    void setURLForRect(const URL&, const FloatRect&) final;

    void setDestinationForRect(const String& name, const FloatRect&) final;
    void addDestinationAtPoint(const String& name, const FloatPoint&) final;

    bool supportsInternalLinks() const final;

    void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags) final;

#if OS(WINDOWS) && !USE(CAIRO)
    GraphicsContextPlatformPrivate* deprecatedPrivateContext() const final;
#endif

private:
    GraphicsContext& m_primaryContext;
    GraphicsContext& m_secondaryContext;
};

}

