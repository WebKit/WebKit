/*
 * Copyright (C) 2006, 2007, 2010, 2020 Apple Inc. All rights reserved.
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

#include "ColorSpaceCG.h"
#include "GraphicsContext.h"

namespace WebCore {

CGAffineTransform getUserToBaseCTM(CGContextRef);

class CGContextStateSaver {
public:
    CGContextStateSaver(CGContextRef context, bool saveAndRestore = true)
        : m_context(context)
        , m_saveAndRestore(saveAndRestore)
    {
        if (m_saveAndRestore)
            CGContextSaveGState(m_context);
    }
    
    ~CGContextStateSaver()
    {
        if (m_saveAndRestore)
            CGContextRestoreGState(m_context);
    }
    
    void save()
    {
        ASSERT(!m_saveAndRestore);
        CGContextSaveGState(m_context);
        m_saveAndRestore = true;
    }

    void restore()
    {
        ASSERT(m_saveAndRestore);
        CGContextRestoreGState(m_context);
        m_saveAndRestore = false;
    }
    
    bool didSave() const
    {
        return m_saveAndRestore;
    }
    
private:
    CGContextRef m_context;
    bool m_saveAndRestore;
};

class WEBCORE_EXPORT GraphicsContextCG : public GraphicsContext {
public:
    GraphicsContextCG(CGContextRef);

#if PLATFORM(WIN)
    GraphicsContextCG(HDC, bool hasAlpha = false); // FIXME: To be removed.
#endif

    ~GraphicsContextCG();

    bool hasPlatformContext() const;
    CGContextRef platformContext() const final;

    void save() final;
    void restore() final;

    void drawRect(const FloatRect&, float borderThickness = 1) final;
    void drawLine(const FloatPoint&, const FloatPoint&) final;
    void drawEllipse(const FloatRect&) final;

    void applyStrokePattern() final;
    void applyFillPattern() final;
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

    void setIsCALayerContext(bool) final;
    bool isCALayerContext() const final;

    void setIsAcceleratedContext(bool) final;
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

    void drawLinesForText(const FloatPoint&, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle) final;

    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;

    void setURLForRect(const URL&, const FloatRect&) final;

    void setDestinationForRect(const String& name, const FloatRect&) final;
    void addDestinationAtPoint(const String& name, const FloatPoint&) final;

    bool supportsInternalLinks() const final;

    void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags) final;

#if OS(WINDOWS)
    GraphicsContextPlatformPrivate* deprecatedPrivateContext() const final;
#endif

private:
    GraphicsContextPlatformPrivate* m_data { nullptr };
};

}

