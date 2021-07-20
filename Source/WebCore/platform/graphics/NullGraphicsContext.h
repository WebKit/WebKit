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

namespace WebCore {

class NullGraphicsContext : public GraphicsContext {
public:
    enum class PaintInvalidationReasons : uint8_t {
        None,
        InvalidatingControlTints,
        InvalidatingImagesWithAsyncDecodes,
        DetectingContentfulPaint
    };

    NullGraphicsContext(PaintInvalidationReasons reasons)
        : m_paintInvalidationReasons(reasons)
    {
    }

private:
#if USE(CG) || USE(DIRECT2D)
    void setIsCALayerContext(bool) final { }
    bool isCALayerContext() const final { return false; }
#endif

    bool paintingDisabled() const final { return true; }
    bool performingPaintInvalidation() const final { return m_paintInvalidationReasons != PaintInvalidationReasons::None; }
    bool invalidatingControlTints() const final { return m_paintInvalidationReasons == PaintInvalidationReasons::InvalidatingControlTints; }
    bool invalidatingImagesWithAsyncDecodes() const final { return m_paintInvalidationReasons == PaintInvalidationReasons::InvalidatingImagesWithAsyncDecodes; }
    bool detectingContentfulPaint() const final { return m_paintInvalidationReasons == PaintInvalidationReasons::DetectingContentfulPaint; }

    void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags) final { }

#if USE(CG) || USE(DIRECT2D)
    void setIsAcceleratedContext(bool) final { }
#endif

    void drawNativeImage(NativeImage&, const FloatSize&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&) final { }

    void drawPattern(NativeImage&, const FloatSize&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, const ImagePaintingOptions&) final { }

    IntRect clipBounds() const final { return { }; }

#if USE(CG) || USE(DIRECT2D)
    void applyStrokePattern() final { }
    void applyFillPattern() final { }
    void drawPath(const Path&) final { }
#endif

    void drawRect(const FloatRect&, float = 1) final { }
    void drawLine(const FloatPoint&, const FloatPoint&) final { }
    void drawEllipse(const FloatRect&) final { }
    void fillPath(const Path&) final { }
    void strokePath(const Path&) final { }
    void fillRect(const FloatRect&) final { }
    void fillRect(const FloatRect&, const Color&) final { }
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final { }
    void strokeRect(const FloatRect&, float) final { }
    void clipPath(const Path&, WindRule = WindRule::EvenOdd) final { }
    FloatRect roundToDevicePixels(const FloatRect& rect, RoundingMode = RoundAllSides) final { return rect; }
    void drawLineForText(const FloatRect&, bool, bool = false, StrokeStyle = SolidStroke) final { }
    void drawLinesForText(const FloatPoint&, float, const DashArray&, bool, bool = false, StrokeStyle = SolidStroke) final { }
    void setLineCap(LineCap) final { }
    void setLineDash(const DashArray&, float) final { }
    void setLineJoin(LineJoin) final { }
    void setMiterLimit(float) final { }
    void clipOut(const Path&) final { }
    void scale(const FloatSize&) final { }
    void rotate(float) final { }
    void translate(float, float) final { }
    void concatCTM(const AffineTransform&) final { }
    void setCTM(const AffineTransform&) final { }
    AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const final { return { }; }
    void clearRect(const FloatRect&) final { }
    void clip(const FloatRect&) final { }
    void clipOut(const FloatRect&) final { }
    void save() final { }
    void restore() final { }

    void drawRaisedEllipse(const FloatRect&, const Color&, const Color&) final { }

    FloatSize drawText(const FontCascade&, const TextRun&, const FloatPoint&, unsigned = 0, std::optional<unsigned> = std::nullopt) final { return { }; }

    void drawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned, const FloatPoint&, FontSmoothingMode) final { }

    void drawEmphasisMarks(const FontCascade&, const TextRun&, const AtomString&, const FloatPoint&, unsigned = 0, std::optional<unsigned> = std::nullopt) final { }
    void drawBidiText(const FontCascade&, const TextRun&, const FloatPoint&, FontCascade::CustomFontNotReadyAction = FontCascade::DoNotPaintIfFontNotReady) final { }

    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final { }

    ImageDrawResult drawImage(Image&, const FloatPoint&, const ImagePaintingOptions& = { ImageOrientation::FromImage }) final { return ImageDrawResult::DidNothing; }
    ImageDrawResult drawImage(Image&, const FloatRect&, const ImagePaintingOptions& = { ImageOrientation::FromImage }) final { return ImageDrawResult::DidNothing; }
    ImageDrawResult drawImage(Image&, const FloatRect&, const FloatRect&, const ImagePaintingOptions& = { ImageOrientation::FromImage }) final { return ImageDrawResult::DidNothing; }

    ImageDrawResult drawTiledImage(Image&, const FloatRect&, const FloatPoint&, const FloatSize&, const FloatSize&, const ImagePaintingOptions& = { }) final { return ImageDrawResult::DidNothing; }
    ImageDrawResult drawTiledImage(Image&, const FloatRect&, const FloatRect&, const FloatSize&, Image::TileRule, Image::TileRule, const ImagePaintingOptions& = { }) final { return ImageDrawResult::DidNothing; }

    void drawFocusRing(const Vector<FloatRect>&, float, float, const Color&) final { }
    void drawFocusRing(const Path&, float, float, const Color&) final { }
#if PLATFORM(MAC)
    void drawFocusRing(const Path&, double, bool&, const Color&) final { }
    void drawFocusRing(const Vector<FloatRect>&, double, bool&, const Color&) final { }
#endif

    void drawImageBuffer(ImageBuffer&, const FloatRect&, const FloatRect&, const ImagePaintingOptions& = { }) final { }
    void drawConsumingImageBuffer(RefPtr<ImageBuffer>, const FloatRect&, const FloatRect&, const ImagePaintingOptions& = { }) final;

    void clipRoundedRect(const FloatRoundedRect&) final { }
    void clipOutRoundedRect(const FloatRoundedRect&) final { }

    ClipToDrawingCommandsResult clipToDrawingCommands(const FloatRect&, const DestinationColorSpace&, Function<void(GraphicsContext&)>&&) final { return ClipToDrawingCommandsResult::Success; }
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) final { }

    void fillRect(const FloatRect&, Gradient&) final { }
    void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode = BlendMode::Normal) final { }

    void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode = BlendMode::Normal) final { }
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&) final { }

#if ENABLE(VIDEO)
    void paintFrameForMedia(MediaPlayer&, const FloatRect&) final { }
#endif

#if OS(WINDOWS) && !USE(CAIRO)
    GraphicsContextPlatformPrivate* deprecatedPrivateContext() const final { return nullptr; }
#endif

private:
    const PaintInvalidationReasons m_paintInvalidationReasons { PaintInvalidationReasons::None };
};

} // namespace WebCore
