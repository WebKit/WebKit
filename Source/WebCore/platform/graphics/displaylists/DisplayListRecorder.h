/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "DisplayList.h"
#include "DisplayListItems.h"
#include "DrawGlyphsRecorder.h"
#include "GraphicsContext.h"
#include "Image.h" // For Image::TileRule.
#include "InlinePathData.h"
#include "TextFlags.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class FloatPoint;
class FloatRect;
class Font;
class GlyphBuffer;
class Image;
class SourceImage;
class VideoFrame;

struct ImagePaintingOptions;

namespace DisplayList {

class Recorder : public GraphicsContext {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(Recorder);
public:
    enum class DrawGlyphsMode {
        Normal,
        DeconstructUsingDrawGlyphsCommands,
        DeconstructUsingDrawDecomposedGlyphsCommands,
    };

    WEBCORE_EXPORT Recorder(const GraphicsContextState&, const FloatRect& initialClip, const AffineTransform&, DrawGlyphsMode = DrawGlyphsMode::Normal);
    WEBCORE_EXPORT virtual ~Recorder();

    virtual void convertToLuminanceMask() = 0;
    virtual void transformToColorSpace(const DestinationColorSpace&) = 0;

protected:
    virtual void recordSave() = 0;
    virtual void recordRestore() = 0;
    virtual void recordTranslate(float x, float y) = 0;
    virtual void recordRotate(float angle) = 0;
    virtual void recordScale(const FloatSize&) = 0;
    virtual void recordSetCTM(const AffineTransform&) = 0;
    virtual void recordConcatenateCTM(const AffineTransform&) = 0;
    virtual void recordSetInlineFillColor(SRGBA<uint8_t>) = 0;
    virtual void recordSetInlineStrokeColor(SRGBA<uint8_t>) = 0;
    virtual void recordSetStrokeThickness(float) = 0;
    virtual void recordSetState(const GraphicsContextState&) = 0;
    virtual void recordSetLineCap(LineCap) = 0;
    virtual void recordSetLineDash(const DashArray&, float dashOffset) = 0;
    virtual void recordSetLineJoin(LineJoin) = 0;
    virtual void recordSetMiterLimit(float) = 0;
    virtual void recordClearShadow() = 0;
    virtual void recordClip(const FloatRect&) = 0;
    virtual void recordClipOut(const FloatRect&) = 0;
    virtual void recordClipToImageBuffer(ImageBuffer&, const FloatRect& destinationRect) = 0;
    virtual void recordClipOutToPath(const Path&) = 0;
    virtual void recordClipPath(const Path&, WindRule) = 0;
    virtual void recordDrawFilteredImageBuffer(ImageBuffer*, const FloatRect& sourceImageRect, Filter&) = 0;
    virtual void recordDrawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode) = 0;
    virtual void recordDrawDecomposedGlyphs(const Font&, const DecomposedGlyphs&) = 0;
    virtual void recordDrawImageBuffer(ImageBuffer&, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) = 0;
    virtual void recordDrawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) = 0;
    virtual void recordDrawSystemImage(SystemImage&, const FloatRect&) = 0;
    virtual void recordDrawPattern(RenderingResourceIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { }) = 0;
    virtual void recordBeginTransparencyLayer(float) = 0;
    virtual void recordEndTransparencyLayer() = 0;
    virtual void recordDrawRect(const FloatRect&, float) = 0;
    virtual void recordDrawLine(const FloatPoint& point1, const FloatPoint& point2) = 0;
    virtual void recordDrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle) = 0;
    virtual void recordDrawDotsForDocumentMarker(const FloatRect&, const DocumentMarkerLineStyle&) = 0;
    virtual void recordDrawEllipse(const FloatRect&) = 0;
    virtual void recordDrawPath(const Path&) = 0;
    virtual void recordDrawFocusRingPath(const Path&, float width, float offset, const Color&) = 0;
    virtual void recordDrawFocusRingRects(const Vector<FloatRect>&, float width, float offset, const Color&) = 0;
    virtual void recordFillRect(const FloatRect&) = 0;
    virtual void recordFillRectWithColor(const FloatRect&, const Color&) = 0;
    virtual void recordFillRectWithGradient(const FloatRect&, Gradient&) = 0;
    virtual void recordFillCompositedRect(const FloatRect&, const Color&, CompositeOperator, BlendMode) = 0;
    virtual void recordFillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode) = 0;
    virtual void recordFillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&) = 0;
#if ENABLE(INLINE_PATH_DATA)
    virtual void recordFillLine(const LineData&) = 0;
    virtual void recordFillArc(const ArcData&) = 0;
    virtual void recordFillQuadCurve(const QuadCurveData&) = 0;
    virtual void recordFillBezierCurve(const BezierCurveData&) = 0;
#endif
    virtual void recordFillPath(const Path&) = 0;
    virtual void recordFillEllipse(const FloatRect&) = 0;
#if ENABLE(VIDEO)
    virtual void recordPaintFrameForMedia(MediaPlayer&, const FloatRect& destination) = 0;
#endif
    virtual void recordStrokeRect(const FloatRect&, float) = 0;
#if ENABLE(INLINE_PATH_DATA)
    virtual void recordStrokeLine(const LineData&) = 0;
    virtual void recordStrokeLineWithColorAndThickness(SRGBA<uint8_t>, float, const LineData&) = 0;
    virtual void recordStrokeArc(const ArcData&) = 0;
    virtual void recordStrokeQuadCurve(const QuadCurveData&) = 0;
    virtual void recordStrokeBezierCurve(const BezierCurveData&) = 0;
#endif
    virtual void recordStrokePath(const Path&) = 0;
    virtual void recordStrokeEllipse(const FloatRect&) = 0;
    virtual void recordClearRect(const FloatRect&) = 0;
#if USE(CG)
    virtual void recordApplyStrokePattern() = 0;
    virtual void recordApplyFillPattern() = 0;
#endif
    virtual void recordApplyDeviceScaleFactor(float) = 0;

    virtual bool recordResourceUse(NativeImage&) = 0;
    virtual bool recordResourceUse(ImageBuffer&) = 0;
    virtual bool recordResourceUse(const SourceImage&) = 0;
    virtual bool recordResourceUse(Font&) = 0;
    virtual bool recordResourceUse(DecomposedGlyphs&) = 0;

    struct ContextState {
        GraphicsContextState state;
        std::optional<GraphicsContextState> lastDrawingState;
        AffineTransform ctm;
        FloatRect clipBounds;

        ContextState(const GraphicsContextState& state, const AffineTransform& ctm, const FloatRect& clipBounds)
            : state(state)
            , ctm(ctm)
            , clipBounds(clipBounds)
        {
        }

        ContextState cloneForTransparencyLayer() const
        {
            auto copy = *this;
            copy.state.didBeginTransparencyLayer();
            if (copy.lastDrawingState)
                copy.lastDrawingState->didBeginTransparencyLayer();
            return copy;
        }

        void translate(float x, float y);
        void rotate(float angleInRadians);
        void scale(const FloatSize&);
        void concatCTM(const AffineTransform&);
        void setCTM(const AffineTransform&);
    };

    const Vector<ContextState, 4>& stateStack() const { return m_stateStack; }

    const ContextState& currentState() const;
    ContextState& currentState();

    WEBCORE_EXPORT RefPtr<ImageBuffer> createImageBuffer(const FloatSize&, float resolutionScale, const DestinationColorSpace&, std::optional<RenderingMode>, std::optional<RenderingMethod>) const override;

private:
    bool hasPlatformContext() const final { return false; }
    PlatformGraphicsContext* platformContext() const final { return nullptr; }

#if USE(CG)
    void setIsCALayerContext(bool) final { }
    bool isCALayerContext() const final { return false; }
    void setIsAcceleratedContext(bool) final { }
#endif

    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final { ASSERT_NOT_REACHED(); }

    WEBCORE_EXPORT const GraphicsContextState& state() const final;

    WEBCORE_EXPORT void didUpdateState(GraphicsContextState&) final;

    WEBCORE_EXPORT void setLineCap(LineCap) final;
    WEBCORE_EXPORT void setLineDash(const DashArray&, float dashOffset) final;
    WEBCORE_EXPORT void setLineJoin(LineJoin) final;
    WEBCORE_EXPORT void setMiterLimit(float) final;

    WEBCORE_EXPORT void fillRect(const FloatRect&) final;
    WEBCORE_EXPORT void fillRect(const FloatRect&, const Color&) final;
    WEBCORE_EXPORT void fillRect(const FloatRect&, Gradient&) final;
    WEBCORE_EXPORT void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode) final;
    WEBCORE_EXPORT void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode) final;
    WEBCORE_EXPORT void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&) final;
    WEBCORE_EXPORT void fillPath(const Path&) final;
    WEBCORE_EXPORT void fillEllipse(const FloatRect&) final;
    WEBCORE_EXPORT void strokeRect(const FloatRect&, float lineWidth) final;
    WEBCORE_EXPORT void strokePath(const Path&) final;
    WEBCORE_EXPORT void strokeEllipse(const FloatRect&) final;
    WEBCORE_EXPORT void clearRect(const FloatRect&) final;

#if USE(CG)
    WEBCORE_EXPORT void applyStrokePattern() final;
    WEBCORE_EXPORT void applyFillPattern() final;
#endif

    WEBCORE_EXPORT void drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter&, FilterResults&) final;

    WEBCORE_EXPORT void drawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode) final;
    WEBCORE_EXPORT void drawDecomposedGlyphs(const Font&, const DecomposedGlyphs&) override;
    WEBCORE_EXPORT void drawGlyphsAndCacheResources(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode) final;

    WEBCORE_EXPORT void drawImageBuffer(ImageBuffer&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions&) final;
    WEBCORE_EXPORT void drawNativeImage(NativeImage&, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions&) final;
    WEBCORE_EXPORT void drawSystemImage(SystemImage&, const FloatRect&) final;
    WEBCORE_EXPORT void drawPattern(NativeImage&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&) final;
    WEBCORE_EXPORT void drawPattern(ImageBuffer&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&) final;

    WEBCORE_EXPORT void drawRect(const FloatRect&, float borderThickness) final;
    WEBCORE_EXPORT void drawLine(const FloatPoint&, const FloatPoint&) final;
    WEBCORE_EXPORT void drawLinesForText(const FloatPoint&, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle) final;
    WEBCORE_EXPORT void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;
    WEBCORE_EXPORT void drawEllipse(const FloatRect&) final;

    WEBCORE_EXPORT void drawPath(const Path&) final;

    WEBCORE_EXPORT void drawFocusRing(const Path&, float width, float offset, const Color&) final;
    WEBCORE_EXPORT void drawFocusRing(const Vector<FloatRect>&, float width, float offset, const Color&) final;

#if PLATFORM(MAC)
    WEBCORE_EXPORT void drawFocusRing(const Path&, double timeOffset, bool& needsRedraw, const Color&) final;
    WEBCORE_EXPORT void drawFocusRing(const Vector<FloatRect>&, double timeOffset, bool& needsRedraw, const Color&) final;
#endif

    WEBCORE_EXPORT void save() final;
    WEBCORE_EXPORT void restore() final;

    WEBCORE_EXPORT void translate(float x, float y) final;
    WEBCORE_EXPORT void rotate(float angleInRadians) final;
    WEBCORE_EXPORT void scale(const FloatSize&) final;
    WEBCORE_EXPORT void concatCTM(const AffineTransform&) final;
    WEBCORE_EXPORT void setCTM(const AffineTransform&) final;
    WEBCORE_EXPORT AffineTransform getCTM(GraphicsContext::IncludeDeviceScale = PossiblyIncludeDeviceScale) const final;

    WEBCORE_EXPORT void beginTransparencyLayer(float opacity) final;
    WEBCORE_EXPORT void endTransparencyLayer() final;

    WEBCORE_EXPORT void clip(const FloatRect&) final;
    WEBCORE_EXPORT void clipOut(const FloatRect&) final;
    WEBCORE_EXPORT void clipOut(const Path&) final;
    WEBCORE_EXPORT void clipPath(const Path&, WindRule) final;
    WEBCORE_EXPORT IntRect clipBounds() const final;
    WEBCORE_EXPORT void clipToImageBuffer(ImageBuffer&, const FloatRect&) final;

#if ENABLE(VIDEO)
    WEBCORE_EXPORT void paintFrameForMedia(MediaPlayer&, const FloatRect& destination) final;
#endif
#if ENABLE(WEB_CODECS)
    WEBCORE_EXPORT void paintVideoFrame(VideoFrame&, const FloatRect&, bool shouldDiscardAlpha) final;
#endif

    WEBCORE_EXPORT void applyDeviceScaleFactor(float) final;

    void appendStateChangeItemIfNecessary();
    void appendStateChangeItem(const GraphicsContextState&);

    const AffineTransform& ctm() const;

    bool shouldDeconstructDrawGlyphs() const;

    Vector<ContextState, 4> m_stateStack;
    std::unique_ptr<DrawGlyphsRecorder> m_drawGlyphsRecorder;
    float m_initialScale { 1 };
    const DrawGlyphsMode m_drawGlyphsMode { DrawGlyphsMode::Normal };
};

} // namespace DisplayList
} // namespace WebCore

