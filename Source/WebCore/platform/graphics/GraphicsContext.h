/*
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#include "ControlPart.h"
#include "DashArray.h"
#include "DestinationColorSpace.h"
#include "FloatRect.h"
#include "FontCascade.h"
#include "GraphicsContextState.h"
#include "Image.h"
#include "ImageOrientation.h"
#include "ImagePaintingOptions.h"
#include "IntRect.h"
#include "Pattern.h"
#include "PlatformGraphicsContext.h"
#include "RenderingMode.h"
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>

namespace WebCore {

class AffineTransform;
class DecomposedGlyphs;
class Filter;
class FilterResults;
class FloatRoundedRect;
class Gradient;
class GraphicsContextPlatformPrivate;
class ImageBuffer;
class MediaPlayer;
class GraphicsContextGL;
class Path;
class SystemImage;
class TextRun;
class VideoFrame;

class GraphicsContext {
    WTF_MAKE_NONCOPYABLE(GraphicsContext); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT GraphicsContext(const GraphicsContextState::ChangeFlags& = { }, InterpolationQuality = InterpolationQuality::Default);
    WEBCORE_EXPORT GraphicsContext(const GraphicsContextState&);
    WEBCORE_EXPORT virtual ~GraphicsContext();

    virtual bool hasPlatformContext() const { return false; }
    virtual PlatformGraphicsContext* platformContext() const { return nullptr; }

    virtual bool paintingDisabled() const { return false; }
    virtual bool performingPaintInvalidation() const { return false; }
    virtual bool invalidatingControlTints() const { return false; }
    virtual bool invalidatingImagesWithAsyncDecodes() const { return false; }
    virtual bool detectingContentfulPaint() const { return false; }

    // Context State

    const SourceBrush& fillBrush() const { return m_state.fillBrush(); }
    const Color& fillColor() const { return fillBrush().color(); }
    Gradient* fillGradient() const { return fillBrush().gradient(); }
    const AffineTransform& fillGradientSpaceTransform() const { return fillBrush().gradientSpaceTransform(); }
    Pattern* fillPattern() const { return fillBrush().pattern(); }
    void setFillBrush(const SourceBrush& brush) { m_state.setFillBrush(brush); didUpdateState(m_state); }
    void setFillColor(const Color& color) { m_state.setFillColor(color); didUpdateState(m_state); }
    void setFillGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform = { }) { m_state.setFillGradient(WTFMove(gradient), spaceTransform); didUpdateState(m_state); }
    void setFillPattern(Ref<Pattern>&& pattern) { m_state.setFillPattern(WTFMove(pattern)); didUpdateState(m_state); }

    WindRule fillRule() const { return m_state.fillRule(); }
    void setFillRule(WindRule fillRule) { m_state.setFillRule(fillRule); didUpdateState(m_state); }

    const SourceBrush& strokeBrush() const { return m_state.strokeBrush(); }
    const Color& strokeColor() const { return strokeBrush().color(); }
    Gradient* strokeGradient() const { return strokeBrush().gradient(); }
    const AffineTransform& strokeGradientSpaceTransform() const { return strokeBrush().gradientSpaceTransform(); }
    Pattern* strokePattern() const { return strokeBrush().pattern(); }
    void setStrokeBrush(const SourceBrush& brush) { m_state.setStrokeBrush(brush); didUpdateState(m_state); }
    void setStrokeColor(const Color& color) { m_state.setStrokeColor(color); didUpdateState(m_state); }
    void setStrokeGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform = { }) { m_state.setStrokeGradient(WTFMove(gradient), spaceTransform); didUpdateState(m_state); }
    void setStrokePattern(Ref<Pattern>&& pattern) { m_state.setStrokePattern(WTFMove(pattern)); didUpdateState(m_state); }

    float strokeThickness() const { return m_state.strokeThickness(); }
    void setStrokeThickness(float thickness) { m_state.setStrokeThickness(thickness); didUpdateState(m_state); }

    StrokeStyle strokeStyle() const { return m_state.strokeStyle(); }
    void setStrokeStyle(StrokeStyle style) { m_state.setStrokeStyle(style); didUpdateState(m_state); }

    const DropShadow& dropShadow() const { return m_state.dropShadow(); }
    FloatSize shadowOffset() const { return dropShadow().offset; }
    float shadowBlur() const { return dropShadow().blurRadius; }
    const Color& shadowColor() const { return dropShadow().color; }
    void setDropShadow(const DropShadow& dropShadow) { m_state.setDropShadow(dropShadow); didUpdateState(m_state); }
    void clearShadow() { setDropShadow({ }); }

    // FIXME: Use dropShadow() and setDropShadow() instead of calling these functions.
    WEBCORE_EXPORT bool getShadow(FloatSize&, float&, Color&) const;
    void setShadow(const FloatSize& offset, float blurRadius, const Color& color, ShadowRadiusMode shadowRadiusMode = ShadowRadiusMode::Default) { setDropShadow({ offset, blurRadius, color, shadowRadiusMode }); }

    bool hasVisibleShadow() const { return dropShadow().isVisible(); }
    bool hasBlurredShadow() const { return dropShadow().isBlurred(); }
    bool hasShadow() const { return dropShadow().hasOutsets(); }

    std::optional<GraphicsStyle> style() const { return m_state.style(); }
    void setStyle(const std::optional<GraphicsStyle>& style) { m_state.setStyle(style); didUpdateState(m_state); }

    CompositeMode compositeMode() const { return m_state.compositeMode(); }
    CompositeOperator compositeOperation() const { return compositeMode().operation; }
    BlendMode blendMode() const { return compositeMode().blendMode; }
    void setCompositeMode(CompositeMode compositeMode) { m_state.setCompositeMode(compositeMode); didUpdateState(m_state); }
    void setCompositeOperation(CompositeOperator operation, BlendMode blendMode = BlendMode::Normal) { setCompositeMode({ operation, blendMode }); }

    float alpha() const { return m_state.alpha(); }
    void setAlpha(float alpha) { m_state.setAlpha(alpha); didUpdateState(m_state); }

    TextDrawingModeFlags textDrawingMode() const { return m_state.textDrawingMode(); }
    void setTextDrawingMode(TextDrawingModeFlags textDrawingMode) { m_state.setTextDrawingMode(textDrawingMode); didUpdateState(m_state); }
    
    InterpolationQuality imageInterpolationQuality() const { return m_state.imageInterpolationQuality(); }
    void setImageInterpolationQuality(InterpolationQuality imageInterpolationQuality) { m_state.setImageInterpolationQuality(imageInterpolationQuality); didUpdateState(m_state); }

    bool shouldAntialias() const { return m_state.shouldAntialias(); }
    void setShouldAntialias(bool shouldAntialias) { m_state.setShouldAntialias(shouldAntialias); didUpdateState(m_state); }

    bool shouldSmoothFonts() const { return m_state.shouldSmoothFonts(); }
    void setShouldSmoothFonts(bool shouldSmoothFonts) { m_state.setShouldSmoothFonts(shouldSmoothFonts); didUpdateState(m_state); }

    // Normally CG enables subpixel-quantization because it improves the performance of aligning glyphs.
    // In some cases we have to disable to to ensure a high-quality output of the glyphs.
    bool shouldSubpixelQuantizeFonts() const { return m_state.shouldSubpixelQuantizeFonts(); }
    void setShouldSubpixelQuantizeFonts(bool shouldSubpixelQuantizeFonts) { m_state.setShouldSubpixelQuantizeFonts(shouldSubpixelQuantizeFonts); didUpdateState(m_state); }

    bool shadowsIgnoreTransforms() const { return m_state.shadowsIgnoreTransforms(); }
    void setShadowsIgnoreTransforms(bool shadowsIgnoreTransforms) { m_state.setShadowsIgnoreTransforms(shadowsIgnoreTransforms); didUpdateState(m_state); }

    bool drawLuminanceMask() const { return m_state.drawLuminanceMask(); }
    void setDrawLuminanceMask(bool drawLuminanceMask) { m_state.setDrawLuminanceMask(drawLuminanceMask); didUpdateState(m_state); }

#if HAVE(OS_DARK_MODE_SUPPORT)
    bool useDarkAppearance() const { return m_state.useDarkAppearance(); }
    void setUseDarkAppearance(bool useDarkAppearance) { m_state.setUseDarkAppearance(useDarkAppearance); didUpdateState(m_state); }
#endif

    virtual const GraphicsContextState& state() const { return m_state; }
    void mergeLastChanges(const GraphicsContextState&, const std::optional<GraphicsContextState>& lastDrawingState = std::nullopt);
    void mergeAllChanges(const GraphicsContextState&);

    // Called *after* any change to GraphicsContextState; generally used to propagate changes
    // to the platform context's state.
    virtual void didUpdateState(GraphicsContextState&) = 0;

    WEBCORE_EXPORT virtual void save();
    WEBCORE_EXPORT virtual void restore();
    
    unsigned stackSize() const { return m_stack.size(); }

#if USE(CG)
    // FIXME: Should these really be public GraphicsContext methods?
    virtual void applyStrokePattern() = 0;
    virtual void applyFillPattern() = 0;

    // FIXME: Can we make this a why instead of a what, and then have it exist cross-platform?
    virtual void setIsCALayerContext(bool) = 0;
    virtual bool isCALayerContext() const = 0;

    // FIXME: Can this be a GraphicsContextCG constructor parameter? Or just be read off the context?
    virtual void setIsAcceleratedContext(bool) = 0;
#endif

    virtual RenderingMode renderingMode() const { return RenderingMode::Unaccelerated; }

    // Pixel Snapping

    enum RoundingMode {
        RoundAllSides,
        RoundOriginAndDimensions
    };
    WEBCORE_EXPORT static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle);

    // Shapes

    // These draw methods will do both stroking and filling.
    // FIXME: ...except drawRect(), which fills properly but always strokes
    // using a 1-pixel stroke inset from the rect borders (of the correct
    // stroke color).
    virtual void drawRect(const FloatRect&, float borderThickness = 1) = 0;
    virtual void drawLine(const FloatPoint&, const FloatPoint&) = 0;

    virtual void drawEllipse(const FloatRect&) = 0;
    WEBCORE_EXPORT virtual void drawRaisedEllipse(const FloatRect&, const Color& ellipseColor, const Color& shadowColor);

    virtual void fillPath(const Path&) = 0;
    virtual void strokePath(const Path&) = 0;
    WEBCORE_EXPORT virtual void drawPath(const Path&);

    virtual void fillEllipse(const FloatRect& ellipse) { fillEllipseAsPath(ellipse); }
    virtual void strokeEllipse(const FloatRect& ellipse) { strokeEllipseAsPath(ellipse); }

    virtual void fillRect(const FloatRect&) = 0;
    virtual void fillRect(const FloatRect&, const Color&) = 0;
    WEBCORE_EXPORT virtual void fillRect(const FloatRect&, Gradient&);
    WEBCORE_EXPORT virtual void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode = BlendMode::Normal);
    virtual void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) = 0;
    WEBCORE_EXPORT virtual void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode = BlendMode::Normal);
    WEBCORE_EXPORT virtual void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&);

    virtual void clearRect(const FloatRect&) = 0;

    virtual void strokeRect(const FloatRect&, float lineWidth) = 0;

    virtual void setLineCap(LineCap) = 0;
    virtual void setLineDash(const DashArray&, float dashOffset) = 0;
    virtual void setLineJoin(LineJoin) = 0;
    virtual void setMiterLimit(float) = 0;

    // Images, Patterns, ControlParts, and Media

    IntSize compatibleImageBufferSize(const FloatSize&) const;

    WEBCORE_EXPORT virtual RefPtr<ImageBuffer> createImageBuffer(const FloatSize&, float resolutionScale = 1, const DestinationColorSpace& = DestinationColorSpace::SRGB(), std::optional<RenderingMode> = std::nullopt, std::optional<RenderingMethod> = std::nullopt) const;

    WEBCORE_EXPORT RefPtr<ImageBuffer> createScaledImageBuffer(const FloatSize&, const FloatSize& scale = { 1, 1 }, const DestinationColorSpace& = DestinationColorSpace::SRGB(), std::optional<RenderingMode> = std::nullopt, std::optional<RenderingMethod> = std::nullopt) const;
    WEBCORE_EXPORT RefPtr<ImageBuffer> createScaledImageBuffer(const FloatRect&, const FloatSize& scale = { 1, 1 }, const DestinationColorSpace& = DestinationColorSpace::SRGB(), std::optional<RenderingMode> = std::nullopt, std::optional<RenderingMethod> = std::nullopt) const;

    WEBCORE_EXPORT virtual RefPtr<ImageBuffer> createAlignedImageBuffer(const FloatSize&, const DestinationColorSpace& = DestinationColorSpace::SRGB(), std::optional<RenderingMethod> = std::nullopt) const;
    WEBCORE_EXPORT virtual RefPtr<ImageBuffer> createAlignedImageBuffer(const FloatRect&, const DestinationColorSpace& = DestinationColorSpace::SRGB(), std::optional<RenderingMethod> = std::nullopt) const;

    virtual void drawNativeImage(NativeImage&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& = { }) = 0;

    WEBCORE_EXPORT virtual void drawSystemImage(SystemImage&, const FloatRect&);

    WEBCORE_EXPORT ImageDrawResult drawImage(Image&, const FloatPoint& destination, const ImagePaintingOptions& = { ImageOrientation::FromImage });
    WEBCORE_EXPORT ImageDrawResult drawImage(Image&, const FloatRect& destination, const ImagePaintingOptions& = { ImageOrientation::FromImage });
    WEBCORE_EXPORT virtual ImageDrawResult drawImage(Image&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& = { ImageOrientation::FromImage });

    WEBCORE_EXPORT virtual ImageDrawResult drawTiledImage(Image&, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& = { });
    WEBCORE_EXPORT virtual ImageDrawResult drawTiledImage(Image&, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule, Image::TileRule, const ImagePaintingOptions& = { });

    WEBCORE_EXPORT void drawImageBuffer(ImageBuffer&, const FloatPoint& destination, const ImagePaintingOptions& = { });
    WEBCORE_EXPORT void drawImageBuffer(ImageBuffer&, const FloatRect& destination, const ImagePaintingOptions& = { });
    WEBCORE_EXPORT virtual void drawImageBuffer(ImageBuffer&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& = { });

    WEBCORE_EXPORT void drawConsumingImageBuffer(RefPtr<ImageBuffer>, const FloatPoint& destination, const ImagePaintingOptions& = { });
    WEBCORE_EXPORT void drawConsumingImageBuffer(RefPtr<ImageBuffer>, const FloatRect& destination, const ImagePaintingOptions& = { });
    WEBCORE_EXPORT virtual void drawConsumingImageBuffer(RefPtr<ImageBuffer>, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& = { });

    WEBCORE_EXPORT virtual void drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter&, FilterResults&);

    virtual void drawPattern(NativeImage&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { }) = 0;
    WEBCORE_EXPORT virtual void drawPattern(ImageBuffer&, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { });

    WEBCORE_EXPORT virtual void drawControlPart(ControlPart&, const FloatRect&, float deviceScaleFactor, const ControlStyle&);

#if ENABLE(VIDEO)
    WEBCORE_EXPORT virtual void paintFrameForMedia(MediaPlayer&, const FloatRect& destination);
#endif
#if ENABLE(WEB_CODECS)
    WEBCORE_EXPORT virtual void paintVideoFrame(VideoFrame&, const FloatRect& destination, bool shouldDiscardAlpha);
#endif

    // Clipping

    virtual void clip(const FloatRect&) = 0;
    WEBCORE_EXPORT virtual void clipRoundedRect(const FloatRoundedRect&);

    virtual void clipOut(const FloatRect&) = 0;
    virtual void clipOut(const Path&) = 0;
    WEBCORE_EXPORT virtual void clipOutRoundedRect(const FloatRoundedRect&);
    virtual void clipPath(const Path&, WindRule = WindRule::EvenOdd) = 0;
    WEBCORE_EXPORT virtual void clipToImageBuffer(ImageBuffer&, const FloatRect&);
    WEBCORE_EXPORT virtual IntRect clipBounds() const;

    // Text

    WEBCORE_EXPORT virtual FloatSize drawText(const FontCascade&, const TextRun&, const FloatPoint&, unsigned from = 0, std::optional<unsigned> to = std::nullopt);
    WEBCORE_EXPORT virtual void drawEmphasisMarks(const FontCascade&, const TextRun&, const AtomString& mark, const FloatPoint&, unsigned from = 0, std::optional<unsigned> to = std::nullopt);
    WEBCORE_EXPORT virtual void drawBidiText(const FontCascade&, const TextRun&, const FloatPoint&, FontCascade::CustomFontNotReadyAction = FontCascade::DoNotPaintIfFontNotReady);

    virtual void drawGlyphsAndCacheResources(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode fontSmoothingMode)
    {
        drawGlyphs(font, glyphs, advances, numGlyphs, point, fontSmoothingMode);
    }

    WEBCORE_EXPORT virtual void drawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned numGlyphs, const FloatPoint&, FontSmoothingMode);
    WEBCORE_EXPORT virtual void drawDecomposedGlyphs(const Font&, const DecomposedGlyphs&);

    WEBCORE_EXPORT FloatRect computeUnderlineBoundsForText(const FloatRect&, bool printing);
    WEBCORE_EXPORT void drawLineForText(const FloatRect&, bool printing, bool doubleLines = false, StrokeStyle = SolidStroke);
    virtual void drawLinesForText(const FloatPoint&, float thickness, const DashArray& widths, bool printing, bool doubleLines = false, StrokeStyle = SolidStroke) = 0;
    virtual void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) = 0;

    // Transparency Layers

    WEBCORE_EXPORT virtual void beginTransparencyLayer(float opacity);
    WEBCORE_EXPORT virtual void endTransparencyLayer();
    bool isInTransparencyLayer() const { return (m_transparencyLayerCount > 0) && supportsTransparencyLayers(); }

    // Focus Rings

    virtual void drawFocusRing(const Vector<FloatRect>&, float width, float offset, const Color&) = 0;
    virtual void drawFocusRing(const Path&, float width, float offset, const Color&) = 0;
    // FIXME: Can we hide these in the CG implementation? Or elsewhere?
#if PLATFORM(MAC)
    virtual void drawFocusRing(const Path&, double timeOffset, bool& needsRedraw, const Color&) = 0;
    virtual void drawFocusRing(const Vector<FloatRect>&, double timeOffset, bool& needsRedraw, const Color&) = 0;
#endif

    // Transforms

    void scale(float s) { scale({ s, s }); }
    virtual void scale(const FloatSize&) = 0;
    virtual void rotate(float angleInRadians) = 0;
    void translate(const FloatSize& size) { translate(size.width(), size.height()); }
    void translate(const FloatPoint& p) { translate(p.x(), p.y()); }
    virtual void translate(float x, float y) = 0;

    virtual void concatCTM(const AffineTransform&) = 0;
    virtual void setCTM(const AffineTransform&) = 0;

    enum IncludeDeviceScale { DefinitelyIncludeDeviceScale, PossiblyIncludeDeviceScale };
    virtual AffineTransform getCTM(IncludeDeviceScale = PossiblyIncludeDeviceScale) const = 0;

    // This function applies the device scale factor to the context, making the context capable of
    // acting as a base-level context for a HiDPI environment.
    virtual void applyDeviceScaleFactor(float factor) { scale(factor); }
    WEBCORE_EXPORT FloatSize scaleFactor() const;
    WEBCORE_EXPORT FloatSize scaleFactorForDrawing(const FloatRect& destRect, const FloatRect& srcRect) const;

    // Links

    virtual void setURLForRect(const URL&, const FloatRect&) { }

    virtual void setDestinationForRect(const String&, const FloatRect&) { }
    virtual void addDestinationAtPoint(const String&, const FloatPoint&) { }

    virtual bool supportsInternalLinks() const { return false; }

    // Contentful Paint Detection

    void setContentfulPaintDetected() { m_contentfulPaintDetected = true; }
    bool contentfulPaintDetected() const { return m_contentfulPaintDetected; }

    // FIXME: Nothing in this section belongs here, and should be moved elsewhere.
#if OS(WINDOWS)
    HDC getWindowsContext(const IntRect&, bool supportAlphaBlend); // The passed in rect is used to create a bitmap for compositing inside transparency layers.
    void releaseWindowsContext(HDC, const IntRect&, bool supportAlphaBlend); // The passed in HDC should be the one handed back by getWindowsContext.
#endif

#if OS(WINDOWS) && !USE(CAIRO)
    // FIXME: This should not exist; we need a different place to
    // put code shared between Windows CG and Windows Cairo backends.
    virtual GraphicsContextPlatformPrivate* deprecatedPrivateContext() const { return nullptr; }
#endif

private:
    virtual bool supportsTransparencyLayers() const { return true; }

protected:
    WEBCORE_EXPORT void fillEllipseAsPath(const FloatRect&);
    WEBCORE_EXPORT void strokeEllipseAsPath(const FloatRect&);

    FloatRect computeLineBoundsAndAntialiasingModeForText(const FloatRect&, bool printing, Color&);

    float dashedLineCornerWidthForStrokeWidth(float) const;
    float dashedLinePatternWidthForStrokeWidth(float) const;
    float dashedLinePatternOffsetForPatternAndStrokeWidth(float patternWidth, float strokeWidth) const;
    Vector<FloatPoint> centerLineAndCutOffCorners(bool isVerticalLine, float cornerWidth, FloatPoint point1, FloatPoint point2) const;

    GraphicsContextState m_state;

private:
    Vector<GraphicsContextState, 1> m_stack;

    unsigned m_transparencyLayerCount { 0 };
    bool m_contentfulPaintDetected { false };
};

} // namespace WebCore

#include "GraphicsContextStateSaver.h"
