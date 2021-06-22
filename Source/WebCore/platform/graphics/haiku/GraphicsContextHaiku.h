/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2015 Julian Harnath <julian.harnath@rwth-aachen.de>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(HAIKU)

#include "GraphicsContext.h"

#include <View.h>

#include <stack>

namespace WebCore {

class GraphicsContextHaiku: public GraphicsContext {
public:
    GraphicsContextHaiku(BView* view);
    ~GraphicsContextHaiku();

    bool hasPlatformContext() const { return m_view != nullptr; }
    PlatformGraphicsContext* platformContext() const { return m_view; }

    void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags) override;
    FloatRect roundToDevicePixels(const FloatRect&, RoundingMode = RoundAllSides) override;
    void drawRect(const FloatRect&, float borderThickness = 1) override;
    void drawLine(const FloatPoint&, const FloatPoint&) override;
    void drawEllipse(const FloatRect&) override;
    void fillPath(const Path&) override;
    void strokePath(const Path&) override;
    void fillRect(const FloatRect&) override;
    void fillRect(const FloatRect&, const Color&) override;
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) override;
    void clearRect(const FloatRect&) override;
    void strokeRect(const FloatRect&, float lineWidth) override;
    void setLineCap(LineCap) override;
    void setLineDash(const DashArray&, float dashOffset) override;
    void setLineJoin(LineJoin) override;
    void setMiterLimit(float) override;
    void drawNativeImage(NativeImage&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& = { }) override;
    void drawPattern(NativeImage&, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { }) override;
    void clip(const FloatRect&) override;
    void clipOut(const FloatRect&) override;
    void clipOut(const Path&) override;
    void clipPath(const Path&, WindRule = WindRule::EvenOdd) override;
    void drawLinesForText(const FloatPoint&, float thickness, const DashArray& widths, bool printing, bool doubleLines = false, StrokeStyle = SolidStroke) override;
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) override;
    void drawFocusRing(const Vector<FloatRect>&, float width, float offset, const Color&) override;
    void drawFocusRing(const Path&, float width, float offset, const Color&) override;
    void scale(const FloatSize&) override;
    void rotate(float angleInRadians) override;
    void translate(float x, float y) override;
    void concatCTM(const AffineTransform&) override;
    void setCTM(const AffineTransform&) override;
    AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const override;

    void beginTransparencyLayer(float opacity) override;
    void endTransparencyLayer() override;
    IntRect clipBounds() const override;

    void save() override;
    void restore() override;

    BView* m_view;
    pattern m_strokeStyle;
};

};

#endif
