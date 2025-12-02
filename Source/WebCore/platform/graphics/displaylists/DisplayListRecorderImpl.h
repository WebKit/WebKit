/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

#include <WebCore/DisplayListRecorder.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

namespace DisplayList {

class RecorderImpl : public Recorder {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(RecorderImpl, WEBCORE_EXPORT);
    WTF_MAKE_NONCOPYABLE(RecorderImpl);
public:
    WEBCORE_EXPORT RecorderImpl(const GraphicsContextState&, const FloatRect& initialClip, const AffineTransform&, const DestinationColorSpace& = DestinationColorSpace::SRGB(), DrawGlyphsMode = DrawGlyphsMode::Normal);
    RecorderImpl(FloatSize initialClipSize)
        : RecorderImpl({ }, { { }, initialClipSize }, { }, DestinationColorSpace::SRGB(), DrawGlyphsMode::Normal)
    {
    }
    WEBCORE_EXPORT virtual ~RecorderImpl();

    WEBCORE_EXPORT Ref<const DisplayList> takeDisplayList();
    // This function is deprecated and sign that caller is doing something incorrect. This will be
    // removed once all clients are fixed.
    WEBCORE_EXPORT Ref<const DisplayList> copyDisplayList();

    void save(GraphicsContextState::Purpose) final;
    void restore(GraphicsContextState::Purpose) final;
    void translate(float x, float y) final;
    void rotate(float angle) final;
    void scale(const FloatSize&) final;
    void setCTM(const AffineTransform&) final;
    void concatCTM(const AffineTransform&) final;
    void setLineCap(LineCap) final;
    void setLineDash(const DashArray&, float dashOffset) final;
    void setLineJoin(LineJoin) final;
    void setMiterLimit(float) final;
    void resetClip() final;
    void clip(const FloatRect&) final;
    void clipRoundedRect(const FloatRoundedRect&) final;
    void clipOut(const FloatRect&) final;
    void clipOut(const Path&) final;
    void clipOutRoundedRect(const FloatRoundedRect&) final;
    void clipPath(const Path&, WindRule) final;
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) final;
    void beginTransparencyLayer(float) final;
    void beginTransparencyLayer(CompositeOperator, BlendMode) final;
    void endTransparencyLayer() final;
    void drawFilteredImageBuffer(ImageBuffer*, const FloatRect&, Filter&, FilterResults&) final;
    void drawImageBuffer(ImageBuffer&, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions) final;
    void drawNativeImage(NativeImage&, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions) final;
    void drawSystemImage(SystemImage&, const FloatRect&) final;
    void drawRect(const FloatRect&, float) final;
    void drawLine(const FloatPoint& point1, const FloatPoint& point2) final;
    void drawLinesForText(const FloatPoint&, float thickness, std::span<const FloatSegment>, bool isPrinting, bool doubleLines, StrokeStyle) final;
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;
    void drawEllipse(const FloatRect&) final;
    void drawPath(const Path&) final;
    void drawFocusRing(const Path&, float outlineWidth, const Color&) final;
    void drawFocusRing(const Vector<FloatRect>&, float outlineOffset, float outlineWidth, const Color&) final;
    void drawPattern(NativeImage&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions) final;
    void drawPattern(ImageBuffer&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions) final;
    void fillEllipse(const FloatRect&) final;
    void fillPath(const Path&) final;
    void fillRect(const FloatRect&, RequiresClipToRect) final;
    void fillRect(const FloatRect&, const Color&) final;
    void fillRect(const FloatRect&, Gradient&) final;
    void fillRect(const FloatRect&, Gradient&, const AffineTransform&, RequiresClipToRect) final;
    void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode) final;
    void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode) final;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&) final;
    void drawGlyphs(const Font&, std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, const FloatPoint& localAnchor, FontSmoothingMode) final;
    void drawGlyphsImmediate(const Font&, std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, const FloatPoint& localAnchor, FontSmoothingMode) final;
    void drawDisplayList(const DisplayList&, ControlFactory&) final;
#if ENABLE(VIDEO)
    void drawVideoFrame(const VideoFrame&, const FloatRect& destination, ImageOrientation, bool shouldDiscardAlpha) final;
#endif
    void strokeRect(const FloatRect&, float) final;
    void strokeEllipse(const FloatRect&) final;
    void strokePath(const Path&) final;
    void clearRect(const FloatRect&) final;
    void drawControlPart(ControlPart&, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle&) final;
#if USE(CG)
    void applyStrokePattern() final;
    void applyFillPattern() final;
#endif
    void applyDeviceScaleFactor(float) final;

    void beginPage(const FloatRect&) final;
    void endPage() final;

    void setURLForRect(const URL&, const FloatRect&) final;

    // Appends a deferred placeholder command.
    // The function is called during the display list playback, drawDisplayList().
    // The function may be called multiple times.
    // The function may be called from an arbitrary thread.
    // The function should always produce the same GraphicsContext calls.
    WEBCORE_EXPORT void drawPlaceholder(Function<void(GraphicsContext&)>&&);

private:
    void appendStateChangeItemIfNecessary() final;

    Vector<Item> m_items;
};

}
}
