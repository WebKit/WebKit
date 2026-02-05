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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteGraphicsContext.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "ImageBufferShareableAllocator.h"
#include "Logging.h"
#include "RemoteGraphicsContextMessages.h"
#include "RemoteImageBuffer.h"
#include "RemoteSharedResourceCache.h"
#include "SharedPreferencesForWebProcess.h"
#include "SharedVideoFrame.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/FEImage.h>
#include <WebCore/FilterResults.h>
#include <WebCore/SVGFilterRenderer.h>
#include <wtf/URL.h>

#if USE(SYSTEM_PREVIEW)
#include <WebCore/ARKitBadgeSystemImage.h>
#endif

#if PLATFORM(COCOA) && ENABLE(VIDEO)
#include "IPCSemaphore.h"
#include "RemoteVideoFrameObjectHeap.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_renderingBackend->streamConnection());

namespace WebKit {
using namespace WebCore;

RemoteGraphicsContext::RemoteGraphicsContext(GraphicsContext& context, RemoteRenderingBackend& renderingBackend)
    : m_context(context)
    , m_renderingBackend(renderingBackend)
    , m_sharedResourceCache(renderingBackend.sharedResourceCache())
{
}

RemoteGraphicsContext::~RemoteGraphicsContext() = default;

Ref<ControlFactory> RemoteGraphicsContext::controlFactory()
{
    if (!m_controlFactory)
        m_controlFactory = ControlFactory::create();
    return *m_controlFactory;
}

RemoteResourceCache& RemoteGraphicsContext::resourceCache() const
{
    return m_renderingBackend->remoteResourceCache();
}

RefPtr<ImageBuffer> RemoteGraphicsContext::imageBuffer(RenderingResourceIdentifier identifier) const
{
    return m_renderingBackend->imageBuffer(identifier);
}

std::optional<SourceImage> RemoteGraphicsContext::sourceImage(RenderingResourceIdentifier identifier) const
{
    if (auto sourceNativeImage = resourceCache().cachedNativeImage(identifier))
        return { { *sourceNativeImage } };

    if (auto sourceImageBuffer = imageBuffer(identifier))
        return { { *sourceImageBuffer } };

    return std::nullopt;
}

void RemoteGraphicsContext::save()
{
    context().save();
}

void RemoteGraphicsContext::restore()
{
    context().restore();
}

void RemoteGraphicsContext::translate(float x, float y)
{
    context().translate(x, y);
}

void RemoteGraphicsContext::rotate(float angle)
{
    context().rotate(angle);
}

void RemoteGraphicsContext::scale(const FloatSize& scale)
{
    context().scale(scale);
}

void RemoteGraphicsContext::setCTM(const AffineTransform& ctm)
{
    context().setCTM(ctm);
}

void RemoteGraphicsContext::concatCTM(const AffineTransform& ctm)
{
    context().concatCTM(ctm);
}

void RemoteGraphicsContext::setFillPackedColor(PackedColor::RGBA color)
{
    context().setFillColor(asSRGBA(color));
}

void RemoteGraphicsContext::setFillColor(const Color& color)
{
    context().setFillColor(color);
}

void RemoteGraphicsContext::setFillCachedGradient(RemoteGradientIdentifier identifier, const AffineTransform& spaceTransform)
{
    RefPtr gradient = resourceCache().cachedGradient(identifier);
    MESSAGE_CHECK(gradient);
    context().setFillGradient(gradient.releaseNonNull(), spaceTransform);
}

void RemoteGraphicsContext::setFillGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform)
{
    context().setFillGradient(WTF::move(gradient), spaceTransform);
}

void RemoteGraphicsContext::setFillPatternNativeImage(RenderingResourceIdentifier identifier, const PatternParameters& parameters)
{
    RefPtr tileImage = resourceCache().cachedNativeImage(identifier);
    MESSAGE_CHECK(tileImage);
    context().setFillPattern(Pattern::create({ tileImage.releaseNonNull() }, parameters));
}

void RemoteGraphicsContext::setFillPatternImageBuffer(RenderingResourceIdentifier identifier, const PatternParameters& parameters)
{
    RefPtr tileImageBuffer = this->imageBuffer(identifier);
    MESSAGE_CHECK(tileImageBuffer);
    context().setFillPattern(Pattern::create({ tileImageBuffer.releaseNonNull() }, parameters));
}

void RemoteGraphicsContext::setFillRule(WindRule rule)
{
    context().setFillRule(rule);
}

void RemoteGraphicsContext::setStrokePackedColor(WebCore::PackedColor::RGBA color)
{
    context().setStrokeColor(asSRGBA(color));
}

void RemoteGraphicsContext::setStrokeColor(const WebCore::Color& color)
{
    context().setStrokeColor(color);
}

void RemoteGraphicsContext::setStrokeCachedGradient(RemoteGradientIdentifier identifier, const AffineTransform& spaceTransform)
{
    RefPtr gradient = resourceCache().cachedGradient(identifier);
    MESSAGE_CHECK(gradient);
    context().setStrokeGradient(gradient.releaseNonNull(), spaceTransform);
}

void RemoteGraphicsContext::setStrokeGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform)
{
    context().setStrokeGradient(WTF::move(gradient), spaceTransform);
}

void RemoteGraphicsContext::setStrokePatternNativeImage(RenderingResourceIdentifier identifier, const PatternParameters& parameters)
{
    RefPtr tileImage = resourceCache().cachedNativeImage(identifier);
    MESSAGE_CHECK(tileImage);
    context().setStrokePattern(Pattern::create({ tileImage.releaseNonNull() }, parameters));
}

void RemoteGraphicsContext::setStrokePatternImageBuffer(RenderingResourceIdentifier identifier, const PatternParameters& parameters)
{
    RefPtr tileImageBuffer = imageBuffer(identifier);
    MESSAGE_CHECK(tileImageBuffer);
    context().setStrokePattern(Pattern::create({ tileImageBuffer.releaseNonNull() }, parameters));
}

void RemoteGraphicsContext::setStrokePackedColorAndThickness(PackedColor::RGBA color, float thickness)
{
    setStrokePackedColor(color);
    setStrokeThickness(thickness);
}

void RemoteGraphicsContext::setStrokeThickness(float thickness)
{
    context().setStrokeThickness(thickness);
}

void RemoteGraphicsContext::setStrokeStyle(WebCore::StrokeStyle value)
{
    context().setStrokeStyle(value);
}

void RemoteGraphicsContext::setCompositeMode(WebCore::CompositeMode value)
{
    context().setCompositeMode(value);
}

void RemoteGraphicsContext::setDropShadow(std::optional<WebCore::GraphicsDropShadow> value)
{
    if (value)
        context().setDropShadow(*value);
    else
        context().clearDropShadow();
}

void RemoteGraphicsContext::setStyle(std::optional<WebCore::GraphicsStyle> value)
{
    context().setStyle(value);
}

void RemoteGraphicsContext::setAlpha(float value)
{
    context().setAlpha(value);
}

void RemoteGraphicsContext::setTextDrawingMode(WebCore::TextDrawingModeFlags value)
{
    context().setTextDrawingMode(value);
}

void RemoteGraphicsContext::setImageInterpolationQuality(WebCore::InterpolationQuality value)
{
    context().setImageInterpolationQuality(value);
}

void RemoteGraphicsContext::setShouldAntialias(bool value)
{
    context().setShouldAntialias(value);
}

void RemoteGraphicsContext::setShouldSmoothFonts(bool value)
{
    context().setShouldSmoothFonts(value);
}

void RemoteGraphicsContext::setShouldSubpixelQuantizeFonts(bool value)
{
    context().setShouldSubpixelQuantizeFonts(value);
}

void RemoteGraphicsContext::setShadowsIgnoreTransforms(bool value)
{
    context().setShadowsIgnoreTransforms(value);
}

void RemoteGraphicsContext::setDrawLuminanceMask(bool value)
{
    context().setDrawLuminanceMask(value);
}

void RemoteGraphicsContext::setLineCap(LineCap lineCap)
{
    context().setLineCap(lineCap);
}

void RemoteGraphicsContext::setLineDash(FixedVector<double>&& dashArray, float dashOffset)
{
    context().setLineDash(DashArray(dashArray.span()), dashOffset);
}

void RemoteGraphicsContext::setLineJoin(LineJoin lineJoin)
{
    context().setLineJoin(lineJoin);
}

void RemoteGraphicsContext::setMiterLimit(float limit)
{
    context().setMiterLimit(limit);
}

void RemoteGraphicsContext::clip(const FloatRect& rect)
{
    context().clip(rect);
}

void RemoteGraphicsContext::clipRoundedRect(const FloatRoundedRect& rect)
{
    context().clipRoundedRect(rect);
}

void RemoteGraphicsContext::clipOut(const FloatRect& rect)
{
    context().clipOut(rect);
}

void RemoteGraphicsContext::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    context().clipOutRoundedRect(rect);
}

void RemoteGraphicsContext::clipToImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect)
{
    RefPtr clipImage = imageBuffer(imageBufferIdentifier);
    if (!clipImage) {
        ASSERT_NOT_REACHED();
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=298384
        // Switch to MESSAGE_CHECK(clipImage) when root cause is clear.
        return;
    }
    context().clipToImageBuffer(*clipImage, destinationRect);
}

void RemoteGraphicsContext::clipOutToPath(const Path& path)
{
    context().clipOut(path);
}

void RemoteGraphicsContext::clipPath(const Path& path, WindRule rule)
{
    context().clipPath(path, rule);
}

void RemoteGraphicsContext::clipCachedPath(RemotePathImplIdentifier identifier, WebCore::WindRule rule)
{
    RefPtr pathImpl = resourceCache().cachedPathImpl(identifier);
    MESSAGE_CHECK(pathImpl);

    Path path(pathImpl.releaseNonNull());
    context().clipPath(path, rule);
}

void RemoteGraphicsContext::resetClip()
{
    context().resetClip();
}

void RemoteGraphicsContext::drawFilteredImageBufferInternal(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
    RefPtr<ImageBuffer> sourceImageBuffer;

    if (sourceImageIdentifier) {
        sourceImageBuffer = imageBuffer(*sourceImageIdentifier);
        MESSAGE_CHECK(sourceImageBuffer);
    }

    for (auto& effect : filter.effectsOfType(FilterEffect::Type::FEImage)) {
        Ref feImage = downcast<FEImage>(effect.get());

        auto effectImage = sourceImage(feImage->sourceImage().imageIdentifier());
        MESSAGE_CHECK(effectImage);
        feImage->setImageSource(WTF::move(*effectImage));
    }

    context().drawFilteredImageBuffer(sourceImageBuffer.get(), sourceImageRect, filter, results);
}

void RemoteGraphicsContext::drawFilteredImageBuffer(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Ref<Filter>&& filter)
{
    RefPtr svgFilter = dynamicDowncast<SVGFilterRenderer>(filter);

    if (!svgFilter || !svgFilter->hasValidRenderingResourceIdentifier()) {
#if HAVE(IOSURFACE)
        FilterResults results(makeUnique<ImageBufferShareableAllocator>(m_sharedResourceCache->resourceOwner(), &m_sharedResourceCache->ioSurfacePool()));
#else
        FilterResults results(makeUnique<ImageBufferShareableAllocator>(m_sharedResourceCache->resourceOwner()));
#endif
        drawFilteredImageBufferInternal(sourceImageIdentifier, sourceImageRect, filter, results);
        return;
    }

    RefPtr cachedFilter = resourceCache().cachedFilter(filter->renderingResourceIdentifier());
    RefPtr cachedSVGFilter = dynamicDowncast<SVGFilterRenderer>(WTF::move(cachedFilter));
    MESSAGE_CHECK(cachedSVGFilter);

    cachedSVGFilter->mergeEffects(svgFilter->effects());

    auto& results = cachedSVGFilter->ensureResults([&]() {
#if HAVE(IOSURFACE)
        auto allocator = makeUnique<ImageBufferShareableAllocator>(m_sharedResourceCache->resourceOwner(), &m_sharedResourceCache->ioSurfacePool());
#else
        auto allocator = makeUnique<ImageBufferShareableAllocator>(m_sharedResourceCache->resourceOwner());
#endif
        return makeUnique<FilterResults>(WTF::move(allocator));
    });

    drawFilteredImageBufferInternal(sourceImageIdentifier, sourceImageRect, *cachedSVGFilter, results);
}

void RemoteGraphicsContext::drawGlyphs(RenderingResourceIdentifier fontIdentifier, IPC::ArrayReferenceTuple<GlyphBufferGlyph, FloatSize> glyphsAdvances, FloatPoint localAnchor, FontSmoothingMode fontSmoothingMode)
{
    RefPtr font = resourceCache().cachedFont(fontIdentifier);
    MESSAGE_CHECK(font);
    context().drawGlyphs(*font, glyphsAdvances.span<0>(), Vector<GlyphBufferAdvance>(glyphsAdvances.span<1>()), localAnchor, fontSmoothingMode);
}

void RemoteGraphicsContext::drawImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr sourceImage = imageBuffer(imageBufferIdentifier);
    MESSAGE_CHECK(sourceImage);
    context().drawImageBuffer(*sourceImage, destinationRect, srcRect, options);
}


void RemoteGraphicsContext::drawDisplayList(RemoteDisplayListIdentifier identifier)
{
    RefPtr displayList = resourceCache().cachedDisplayList(identifier);
    MESSAGE_CHECK(displayList);
    context().drawDisplayList(*displayList, controlFactory());
}

void RemoteGraphicsContext::drawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr image = resourceCache().cachedNativeImage(imageIdentifier);
    MESSAGE_CHECK(image);
    context().drawNativeImage(*image, destRect, srcRect, options);
}

void RemoteGraphicsContext::drawSystemImage(Ref<SystemImage>&& systemImage, const FloatRect& destinationRect)
{
#if USE(SYSTEM_PREVIEW)
    if (auto* badge = dynamicDowncast<ARKitBadgeSystemImage>(systemImage.get())) {
        RefPtr nativeImage = resourceCache().cachedNativeImage(badge->imageIdentifier());
        MESSAGE_CHECK(nativeImage);
        badge->setImage(BitmapImage::create(nativeImage.releaseNonNull()));
    }
#endif
    context().drawSystemImage(systemImage, destinationRect);
}

void RemoteGraphicsContext::drawPatternNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    RefPtr image = resourceCache().cachedNativeImage(imageIdentifier);
    MESSAGE_CHECK(image);
    context().drawPattern(*image, destRect, tileRect, transform, phase, spacing, options);
}

void RemoteGraphicsContext::drawPatternImageBuffer(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    RefPtr image = imageBuffer(imageIdentifier);
    MESSAGE_CHECK(image);
    context().drawPattern(*image, destRect, tileRect, transform, phase, spacing, options);
}

void RemoteGraphicsContext::beginTransparencyLayer(float opacity)
{
    context().beginTransparencyLayer(opacity);
}

void RemoteGraphicsContext::beginTransparencyLayerWithCompositeMode(CompositeMode compositeMode)
{
    context().beginTransparencyLayer(compositeMode.operation, compositeMode.blendMode);
}

void RemoteGraphicsContext::endTransparencyLayer()
{
    context().endTransparencyLayer();
}

void RemoteGraphicsContext::drawRect(const FloatRect& rect, float borderThickness)
{
    context().drawRect(rect, borderThickness);
}

void RemoteGraphicsContext::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    context().drawLine(point1, point2);
}

void RemoteGraphicsContext::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    context().drawLinesForText(point, thickness, Vector(lineSegments), printing, doubleLines, strokeStyle);
}

void RemoteGraphicsContext::drawDotsForDocumentMarker(const FloatRect& rect, const DocumentMarkerLineStyle& style)
{
    context().drawDotsForDocumentMarker(rect, style);
}

void RemoteGraphicsContext::drawEllipse(const FloatRect& rect)
{
    context().drawEllipse(rect);
}

void RemoteGraphicsContext::drawPath(const Path& path)
{
    context().drawPath(path);
}

void RemoteGraphicsContext::drawFocusRingPath(const Path& path, float outlineWidth, const Color& color)
{
    context().drawFocusRing(path, outlineWidth, color);
}

void RemoteGraphicsContext::drawFocusRingRects(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    context().drawFocusRing(rects, outlineOffset, outlineWidth, color);
}

void RemoteGraphicsContext::fillRect(const FloatRect& rect, GraphicsContext::RequiresClipToRect requiresClipToRect)
{
    context().fillRect(rect, requiresClipToRect);
}

void RemoteGraphicsContext::fillRectWithColor(const FloatRect& rect, const Color& color)
{
    context().fillRect(rect, color);
}

void RemoteGraphicsContext::fillRectWithGradient(const FloatRect& rect, Ref<Gradient>&& gradient)
{
    context().fillRect(rect, gradient);
}

void RemoteGraphicsContext::fillRectWithGradientAndSpaceTransform(const FloatRect& rect, Ref<Gradient>&& gradient, const AffineTransform& transform, GraphicsContext::RequiresClipToRect requiresClipToRect)
{
    context().fillRect(rect, gradient, transform, requiresClipToRect);
}

void RemoteGraphicsContext::fillCompositedRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    context().fillRect(rect, color, op, blendMode);
}

void RemoteGraphicsContext::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    context().fillRoundedRect(rect, color, blendMode);
}

void RemoteGraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    context().fillRectWithRoundedHole(rect, roundedHoleRect, color);
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteGraphicsContext::fillLine(const PathDataLine& line)
{
    context().fillPath(Path({ PathSegment { line } }));
}

void RemoteGraphicsContext::fillArc(const PathArc& arc)
{
    context().fillPath(Path({ PathSegment { arc } }));
}

void RemoteGraphicsContext::fillClosedArc(const PathClosedArc& closedArc)
{
    context().fillPath(Path({ PathSegment { closedArc } }));
}

void RemoteGraphicsContext::fillQuadCurve(const PathDataQuadCurve& curve)
{
    context().fillPath(Path({ PathSegment { curve } }));
}

void RemoteGraphicsContext::fillBezierCurve(const PathDataBezierCurve& curve)
{
    context().fillPath(Path({ PathSegment { curve } }));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteGraphicsContext::fillPath(const Path& path)
{
    context().fillPath(path);
}

void RemoteGraphicsContext::fillCachedPath(RemotePathImplIdentifier identifier)
{
    RefPtr pathImpl = resourceCache().cachedPathImpl(identifier);
    MESSAGE_CHECK(pathImpl);

    Path path(pathImpl.releaseNonNull());
    context().fillPath(path);
}

void RemoteGraphicsContext::fillPathSegment(const PathSegment& segment)
{
    context().fillPath(Path({ segment }));
}

void RemoteGraphicsContext::fillEllipse(const FloatRect& rect)
{
    context().fillEllipse(rect);
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
SharedVideoFrameReader& RemoteGraphicsContext::sharedVideoFrameReader()
{
    if (!m_sharedVideoFrameReader) {
        Ref gpuConnectionToWebProcess = m_renderingBackend->gpuConnectionToWebProcess();
        m_sharedVideoFrameReader = makeUnique<SharedVideoFrameReader>(Ref { gpuConnectionToWebProcess->videoFrameObjectHeap() }, gpuConnectionToWebProcess->webProcessIdentity());
    }
    return *m_sharedVideoFrameReader;
}

void RemoteGraphicsContext::drawVideoFrame(SharedVideoFrame&& frame, const FloatRect& destination, ImageOrientation orientation, bool shouldDiscardAlpha)
{
    if (auto videoFrame = sharedVideoFrameReader().read(WTF::move(frame)))
        context().drawVideoFrame(*videoFrame, destination, orientation, shouldDiscardAlpha);
}

void RemoteGraphicsContext::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    sharedVideoFrameReader().setSemaphore(WTF::move(semaphore));
}

void RemoteGraphicsContext::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    sharedVideoFrameReader().setSharedMemory(WTF::move(handle));
}
#endif // PLATFORM(COCOA) && ENABLE(VIDEO)

void RemoteGraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    context().strokeRect(rect, lineWidth);
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteGraphicsContext::strokeLine(const PathDataLine& line)
{
#if ENABLE(INLINE_PATH_DATA)
    auto path = Path({ PathSegment { PathDataLine { { line.start() }, { line.end() } } } });
#else
    Path path;
    path.moveTo(line.start);
    path.addLineTo(line.end);
#endif
    context().strokePath(path);
}

void RemoteGraphicsContext::strokeLineWithColorAndThickness(const PathDataLine& line, std::optional<PackedColor::RGBA> strokeColor, std::optional<float> strokeThickness)
{
    if (strokeColor)
        setStrokePackedColor(*strokeColor);
    if (strokeThickness)
        setStrokeThickness(*strokeThickness);
    strokeLine(line);
}

void RemoteGraphicsContext::strokeArc(const PathArc& arc)
{
    context().strokeArc(arc);
}

void RemoteGraphicsContext::strokeClosedArc(const PathClosedArc& closedArc)
{
    context().strokePath(Path({ PathSegment { closedArc } }));
}

void RemoteGraphicsContext::strokeQuadCurve(const PathDataQuadCurve& curve)
{
    context().strokePath(Path({ PathSegment { curve } }));
}

void RemoteGraphicsContext::strokeBezierCurve(const PathDataBezierCurve& curve)
{
    context().strokePath(Path({ PathSegment { curve } }));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteGraphicsContext::strokePathSegment(const PathSegment& segment)
{
    context().strokePath(PathSegment { segment });
}

void RemoteGraphicsContext::strokePath(const Path& path)
{
    context().strokePath(path);
}

void RemoteGraphicsContext::strokeEllipse(const FloatRect& rect)
{
    context().strokeEllipse(rect);
}

void RemoteGraphicsContext::clearRect(const FloatRect& rect)
{
    context().clearRect(rect);
}

void RemoteGraphicsContext::drawControlPart(Ref<ControlPart>&& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    part->setOverrideControlFactory(controlFactory().ptr());
    context().drawControlPart(part, borderRect, deviceScaleFactor, style);
    part->setOverrideControlFactory(nullptr);
}

#if USE(CG)

void RemoteGraphicsContext::applyStrokePattern()
{
    context().applyStrokePattern();
}

void RemoteGraphicsContext::applyFillPattern()
{
    context().applyFillPattern();
}

#endif // USE(CG)

void RemoteGraphicsContext::applyDeviceScaleFactor(float scaleFactor)
{
    context().applyDeviceScaleFactor(scaleFactor);
}

void RemoteGraphicsContext::beginPage(const FloatRect& pageRect)
{
    context().beginPage(pageRect);
}

void RemoteGraphicsContext::endPage()
{
    context().endPage();
}

void RemoteGraphicsContext::setURLForRect(const URL& link, const FloatRect& destRect)
{
    context().setURLForRect(link, destRect);
}

std::optional<SharedPreferencesForWebProcess> RemoteGraphicsContext::sharedPreferencesForWebProcess() const
{
    return m_renderingBackend->sharedPreferencesForWebProcess();
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
