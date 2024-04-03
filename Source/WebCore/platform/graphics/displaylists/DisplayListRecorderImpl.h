/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "DisplayListRecorder.h"

namespace WebCore {

namespace DisplayList {

class RecorderImpl : public Recorder {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RecorderImpl);
public:
    WEBCORE_EXPORT RecorderImpl(DisplayList&, const GraphicsContextState&, const FloatRect& initialClip, const AffineTransform&, const DestinationColorSpace& = DestinationColorSpace::SRGB(), DrawGlyphsMode = DrawGlyphsMode::Normal);
    WEBCORE_EXPORT virtual ~RecorderImpl();

    bool isEmpty() const { return m_displayList.isEmpty(); }

private:
    void recordSave() final;
    void recordRestore() final;
    void recordTranslate(float x, float y) final;
    void recordRotate(float angle) final;
    void recordScale(const FloatSize&) final;
    void recordSetCTM(const AffineTransform&) final;
    void recordConcatenateCTM(const AffineTransform&) final;
    void recordSetInlineFillColor(PackedColor::RGBA) final;
    void recordSetInlineStroke(SetInlineStroke&&) final;
    void recordSetState(const GraphicsContextState&) final;
    void recordSetLineCap(LineCap) final;
    void recordSetLineDash(const DashArray&, float dashOffset) final;
    void recordSetLineJoin(LineJoin) final;
    void recordSetMiterLimit(float) final;
    void recordClearDropShadow() final;
    void recordResetClip() final;
    void recordClip(const FloatRect&) final;
    void recordClipRoundedRect(const FloatRoundedRect&) final;
    void recordClipOut(const FloatRect&) final;
    void recordClipOutRoundedRect(const FloatRoundedRect&) final;
    void recordClipToImageBuffer(ImageBuffer&, const FloatRect& destinationRect) final;
    void recordClipOutToPath(const Path&) final;
    void recordClipPath(const Path&, WindRule) final;
    void recordDrawFilteredImageBuffer(ImageBuffer*, const FloatRect& sourceImageRect, Filter&) final;
    void recordDrawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode) final;
    void recordDrawDecomposedGlyphs(const Font&, const DecomposedGlyphs&) final;
    void recordDrawImageBuffer(ImageBuffer&, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions) final;
    void recordDrawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions) final;
    void recordDrawSystemImage(SystemImage&, const FloatRect&) final;
    void recordDrawPattern(RenderingResourceIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions = { }) final;
    void recordBeginTransparencyLayer(float) final;
    void recordEndTransparencyLayer() final;
    void recordDrawRect(const FloatRect&, float) final;
    void recordDrawLine(const FloatPoint& point1, const FloatPoint& point2) final;
    void recordDrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle) final;
    void recordDrawDotsForDocumentMarker(const FloatRect&, const DocumentMarkerLineStyle&) final;
    void recordDrawEllipse(const FloatRect&) final;
    void recordDrawPath(const Path&) final;
    void recordDrawFocusRingPath(const Path&, float outlineWidth, const Color&) final;
    void recordDrawFocusRingRects(const Vector<FloatRect>&, float outlineOffset, float outlineWidth, const Color&) final;
    void recordFillRect(const FloatRect&) final;
    void recordFillRectWithColor(const FloatRect&, const Color&) final;
    void recordFillRectWithGradient(const FloatRect&, Gradient&) final;
    void recordFillRectWithGradientAndSpaceTransform(const FloatRect&, Gradient&, const AffineTransform&) final;
    void recordFillCompositedRect(const FloatRect&, const Color&, CompositeOperator, BlendMode) final;
    void recordFillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode) final;
    void recordFillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&) final;
#if ENABLE(INLINE_PATH_DATA)
    void recordFillLine(const PathDataLine&) final;
    void recordFillArc(const PathArc&) final;
    void recordFillClosedArc(const PathClosedArc&) final;
    void recordFillQuadCurve(const PathDataQuadCurve&) final;
    void recordFillBezierCurve(const PathDataBezierCurve&) final;
#endif
    void recordFillPathSegment(const PathSegment&) final;
    void recordFillPath(const Path&) final;
    void recordFillEllipse(const FloatRect&) final;
#if ENABLE(VIDEO)
    void recordPaintFrameForMedia(MediaPlayer&, const FloatRect& destination) final;
    void recordPaintVideoFrame(VideoFrame&, const FloatRect& destination, bool shouldDiscardAlpha) final;
#endif
    void recordStrokeRect(const FloatRect&, float) final;
#if ENABLE(INLINE_PATH_DATA)
    void recordStrokeLine(const PathDataLine&) final;
    void recordStrokeLineWithColorAndThickness(const PathDataLine&, SetInlineStroke&&) final;
    void recordStrokeArc(const PathArc&) final;
    void recordStrokeClosedArc(const PathClosedArc&) final;
    void recordStrokeQuadCurve(const PathDataQuadCurve&) final;
    void recordStrokeBezierCurve(const PathDataBezierCurve&) final;
#endif
    void recordStrokePathSegment(const PathSegment&) final;
    void recordStrokePath(const Path&) final;
    void recordStrokeEllipse(const FloatRect&) final;
    void recordClearRect(const FloatRect&) final;
    void recordDrawControlPart(ControlPart&, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle&) final;
    void recordDrawDisplayListItems(const Vector<Item>&, const FloatPoint& destination) final;
#if USE(CG)
    void recordApplyStrokePattern() final;
    void recordApplyFillPattern() final;
#endif
    void recordApplyDeviceScaleFactor(float) final;

    bool recordResourceUse(NativeImage&) final;
    bool recordResourceUse(ImageBuffer&) final;
    bool recordResourceUse(const SourceImage&) final;
    bool recordResourceUse(Font&) final;
    bool recordResourceUse(DecomposedGlyphs&) final;
    bool recordResourceUse(Gradient&) final;
    bool recordResourceUse(Filter&) final;

    void append(Item&& item)
    {
        m_displayList.append(WTFMove(item));
    }

    DisplayList& m_displayList;
};

}
}
