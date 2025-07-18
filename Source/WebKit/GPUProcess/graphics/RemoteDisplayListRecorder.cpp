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
#include "RemoteDisplayListRecorder.h"
#include "SharedPreferencesForWebProcess.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "ImageBufferShareableAllocator.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteImageBuffer.h"
#include "RemoteSharedResourceCache.h"
#include "SharedVideoFrame.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/FEImage.h>
#include <WebCore/FilterResults.h>
#include <WebCore/SVGFilter.h>

#if USE(SYSTEM_PREVIEW)
#include <WebCore/ARKitBadgeSystemImage.h>
#endif

#if PLATFORM(COCOA) && ENABLE(VIDEO)
#include "IPCSemaphore.h"
#include "RemoteVideoFrameObjectHeap.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<RemoteDisplayListRecorder> RemoteDisplayListRecorder::create(WebCore::ImageBuffer& imageBuffer, RemoteDisplayListRecorderIdentifier identifier, RemoteRenderingBackend& renderingBackend)
{
    auto instance = adoptRef(*new RemoteDisplayListRecorder(imageBuffer, identifier, renderingBackend));
    instance->startListeningForIPC();
    return instance;
}

RemoteDisplayListRecorder::RemoteDisplayListRecorder(ImageBuffer& imageBuffer, RemoteDisplayListRecorderIdentifier identifier, RemoteRenderingBackend& renderingBackend)
    : m_imageBuffer(imageBuffer)
    , m_identifier(identifier)
    , m_renderingBackend(renderingBackend)
    , m_sharedResourceCache(renderingBackend.sharedResourceCache())
{
}

RemoteDisplayListRecorder::~RemoteDisplayListRecorder() = default;

RemoteResourceCache& RemoteDisplayListRecorder::resourceCache() const
{
    return m_renderingBackend->remoteResourceCache();
}

RefPtr<ImageBuffer> RemoteDisplayListRecorder::imageBuffer(RenderingResourceIdentifier identifier) const
{
    return m_renderingBackend->imageBuffer(identifier);
}

std::optional<SourceImage> RemoteDisplayListRecorder::sourceImage(RenderingResourceIdentifier identifier) const
{
    if (auto sourceNativeImage = resourceCache().cachedNativeImage(identifier))
        return { { *sourceNativeImage } };

    if (auto sourceImageBuffer = imageBuffer(identifier))
        return { { *sourceImageBuffer } };

    return std::nullopt;
}

void RemoteDisplayListRecorder::startListeningForIPC()
{
    m_renderingBackend->protectedStreamConnection()->startReceivingMessages(*this, Messages::RemoteDisplayListRecorder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteDisplayListRecorder::stopListeningForIPC()
{
    m_renderingBackend->protectedStreamConnection()->stopReceivingMessages(Messages::RemoteDisplayListRecorder::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteDisplayListRecorder::save()
{
    context().save();
}

void RemoteDisplayListRecorder::restore()
{
    context().restore();
}

void RemoteDisplayListRecorder::translate(float x, float y)
{
    context().translate(x, y);
}

void RemoteDisplayListRecorder::rotate(float angle)
{
    context().rotate(angle);
}

void RemoteDisplayListRecorder::scale(const FloatSize& scale)
{
    context().scale(scale);
}

void RemoteDisplayListRecorder::setCTM(const AffineTransform& ctm)
{
    context().setCTM(ctm);
}

void RemoteDisplayListRecorder::concatCTM(const AffineTransform& ctm)
{
    context().concatCTM(ctm);
}

void RemoteDisplayListRecorder::setFillPackedColor(PackedColor::RGBA color)
{
    context().setFillColor(asSRGBA(color));
}

void RemoteDisplayListRecorder::setFillColor(const Color& color)
{
    context().setFillColor(color);
}

void RemoteDisplayListRecorder::setFillCachedGradient(RenderingResourceIdentifier identifier, const AffineTransform& spaceTransform)
{
    RefPtr gradient = resourceCache().cachedGradient(identifier);
    if (!gradient) {
        ASSERT_NOT_REACHED();
        return;
    }
    context().setFillGradient(gradient.releaseNonNull(), spaceTransform);
}

void RemoteDisplayListRecorder::setFillGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform)
{
    context().setFillGradient(WTFMove(gradient), spaceTransform);
}

void RemoteDisplayListRecorder::setFillPattern(RenderingResourceIdentifier tileImageIdentifier, const PatternParameters& parameters)
{
    auto tileImage = sourceImage(tileImageIdentifier);
    if (!tileImage) {
        ASSERT_NOT_REACHED();
        return;
    }
    context().setFillPattern(Pattern::create(WTFMove(*tileImage), parameters));
}

void RemoteDisplayListRecorder::setFillRule(WindRule rule)
{
    context().setFillRule(rule);
}

void RemoteDisplayListRecorder::setStrokePackedColor(WebCore::PackedColor::RGBA color)
{
    context().setStrokeColor(asSRGBA(color));
}

void RemoteDisplayListRecorder::setStrokeColor(const WebCore::Color& color)
{
    context().setStrokeColor(color);
}

void RemoteDisplayListRecorder::setStrokeCachedGradient(RenderingResourceIdentifier identifier, const AffineTransform& spaceTransform)
{
    RefPtr gradient = resourceCache().cachedGradient(identifier);
    if (!gradient) {
        ASSERT_NOT_REACHED();
        return;
    }
    context().setStrokeGradient(gradient.releaseNonNull(), spaceTransform);
}

void RemoteDisplayListRecorder::setStrokeGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform)
{
    context().setStrokeGradient(WTFMove(gradient), spaceTransform);
}

void RemoteDisplayListRecorder::setStrokePattern(RenderingResourceIdentifier tileImageIdentifier, const PatternParameters& parameters)
{
    auto tileImage = sourceImage(tileImageIdentifier);
    if (!tileImage) {
        ASSERT_NOT_REACHED();
        return;
    }
    context().setStrokePattern(Pattern::create(WTFMove(*tileImage), parameters));
}

void RemoteDisplayListRecorder::setStrokePackedColorAndThickness(PackedColor::RGBA color, float thickness)
{
    setStrokePackedColor(color);
    setStrokeThickness(thickness);
}

void RemoteDisplayListRecorder::setStrokeThickness(float thickness)
{
    context().setStrokeThickness(thickness);
}

void RemoteDisplayListRecorder::setStrokeStyle(WebCore::StrokeStyle value)
{
    context().setStrokeStyle(value);
}

void RemoteDisplayListRecorder::setCompositeMode(WebCore::CompositeMode value)
{
    context().setCompositeMode(value);
}

void RemoteDisplayListRecorder::setDropShadow(std::optional<WebCore::GraphicsDropShadow> value)
{
    if (value)
        context().setDropShadow(*value);
    else
        context().clearDropShadow();
}

void RemoteDisplayListRecorder::setStyle(std::optional<WebCore::GraphicsStyle> value)
{
    context().setStyle(value);
}

void RemoteDisplayListRecorder::setAlpha(float value)
{
    context().setAlpha(value);
}

void RemoteDisplayListRecorder::setTextDrawingMode(WebCore::TextDrawingModeFlags value)
{
    context().setTextDrawingMode(value);
}

void RemoteDisplayListRecorder::setImageInterpolationQuality(WebCore::InterpolationQuality value)
{
    context().setImageInterpolationQuality(value);
}

void RemoteDisplayListRecorder::setShouldAntialias(bool value)
{
    context().setShouldAntialias(value);
}

void RemoteDisplayListRecorder::setShouldSmoothFonts(bool value)
{
    context().setShouldSmoothFonts(value);
}

void RemoteDisplayListRecorder::setShouldSubpixelQuantizeFonts(bool value)
{
    context().setShouldSubpixelQuantizeFonts(value);
}

void RemoteDisplayListRecorder::setShadowsIgnoreTransforms(bool value)
{
    context().setShadowsIgnoreTransforms(value);
}

void RemoteDisplayListRecorder::setDrawLuminanceMask(bool value)
{
    context().setDrawLuminanceMask(value);
}

void RemoteDisplayListRecorder::setLineCap(LineCap lineCap)
{
    context().setLineCap(lineCap);
}

void RemoteDisplayListRecorder::setLineDash(FixedVector<double>&& dashArray, float dashOffset)
{
    context().setLineDash(DashArray(dashArray.span()), dashOffset);
}

void RemoteDisplayListRecorder::setLineJoin(LineJoin lineJoin)
{
    context().setLineJoin(lineJoin);
}

void RemoteDisplayListRecorder::setMiterLimit(float limit)
{
    context().setMiterLimit(limit);
}

void RemoteDisplayListRecorder::clip(const FloatRect& rect)
{
    context().clip(rect);
}

void RemoteDisplayListRecorder::clipRoundedRect(const FloatRoundedRect& rect)
{
    context().clipRoundedRect(rect);
}

void RemoteDisplayListRecorder::clipOut(const FloatRect& rect)
{
    context().clipOut(rect);
}

void RemoteDisplayListRecorder::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    context().clipOutRoundedRect(rect);
}

void RemoteDisplayListRecorder::clipToImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect)
{
    RefPtr clipImage = imageBuffer(imageBufferIdentifier);
    if (!clipImage) {
        ASSERT_NOT_REACHED();
        return;
    }

    context().clipToImageBuffer(*clipImage, destinationRect);
}

void RemoteDisplayListRecorder::clipOutToPath(const Path& path)
{
    context().clipOut(path);
}

void RemoteDisplayListRecorder::clipPath(const Path& path, WindRule rule)
{
    context().clipPath(path, rule);
}

void RemoteDisplayListRecorder::resetClip()
{
    context().resetClip();
}

void RemoteDisplayListRecorder::drawFilteredImageBufferInternal(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
    RefPtr<ImageBuffer> sourceImageBuffer;

    if (sourceImageIdentifier) {
        sourceImageBuffer = imageBuffer(*sourceImageIdentifier);
        if (!sourceImageBuffer) {
            ASSERT_NOT_REACHED();
            return;
        }
    }

    for (auto& effect : filter.effectsOfType(FilterEffect::Type::FEImage)) {
        Ref feImage = downcast<FEImage>(effect.get());

        auto effectImage = sourceImage(feImage->sourceImage().imageIdentifier());
        if (!effectImage) {
            ASSERT_NOT_REACHED();
            return;
        }

        feImage->setImageSource(WTFMove(*effectImage));
    }

    context().drawFilteredImageBuffer(sourceImageBuffer.get(), sourceImageRect, filter, results);
}

void RemoteDisplayListRecorder::drawFilteredImageBuffer(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Ref<Filter>&& filter)
{
    RefPtr svgFilter = dynamicDowncast<SVGFilter>(filter);

    if (!svgFilter || !svgFilter->hasValidRenderingResourceIdentifier()) {
        FilterResults results(makeUnique<ImageBufferShareableAllocator>(m_sharedResourceCache->resourceOwner()));
        drawFilteredImageBufferInternal(sourceImageIdentifier, sourceImageRect, filter, results);
        return;
    }

    RefPtr cachedFilter = resourceCache().cachedFilter(filter->renderingResourceIdentifier());
    RefPtr cachedSVGFilter = dynamicDowncast<SVGFilter>(WTFMove(cachedFilter));
    if (!cachedSVGFilter) {
        ASSERT_NOT_REACHED();
        return;
    }

    cachedSVGFilter->mergeEffects(svgFilter->effects());

    auto& results = cachedSVGFilter->ensureResults([&]() {
        auto allocator = makeUnique<ImageBufferShareableAllocator>(m_sharedResourceCache->resourceOwner());
        return makeUnique<FilterResults>(WTFMove(allocator));
    });

    drawFilteredImageBufferInternal(sourceImageIdentifier, sourceImageRect, *cachedSVGFilter, results);
}

void RemoteDisplayListRecorder::drawGlyphs(RenderingResourceIdentifier fontIdentifier, IPC::ArrayReferenceTuple<GlyphBufferGlyph, FloatSize> glyphsAdvances, FloatPoint localAnchor, FontSmoothingMode fontSmoothingMode)
{
    RefPtr font = resourceCache().cachedFont(fontIdentifier);
    if (!font) {
        ASSERT_NOT_REACHED();
        return;
    }

    context().drawGlyphs(*font, glyphsAdvances.span<0>(), Vector<GlyphBufferAdvance>(glyphsAdvances.span<1>()), localAnchor, fontSmoothingMode);
}

void RemoteDisplayListRecorder::drawDecomposedGlyphs(RenderingResourceIdentifier fontIdentifier, RenderingResourceIdentifier decomposedGlyphsIdentifier)
{
    RefPtr font = resourceCache().cachedFont(fontIdentifier);
    if (!font) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr decomposedGlyphs = resourceCache().cachedDecomposedGlyphs(decomposedGlyphsIdentifier);
    if (!decomposedGlyphs) {
        ASSERT_NOT_REACHED();
        return;
    }
    context().drawDecomposedGlyphs(*font, *decomposedGlyphs);
}

void RemoteDisplayListRecorder::drawImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr sourceImage = imageBuffer(imageBufferIdentifier);
    if (!sourceImage) {
        ASSERT_NOT_REACHED();
        return;
    }

    context().drawImageBuffer(*sourceImage, destinationRect, srcRect, options);
}

void RemoteDisplayListRecorder::drawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr image = resourceCache().cachedNativeImage(imageIdentifier);
    if (!image) {
        ASSERT_NOT_REACHED();
        return;
    }

    context().drawNativeImage(*image, destRect, srcRect, options);
}

void RemoteDisplayListRecorder::drawSystemImage(Ref<SystemImage>&& systemImage, const FloatRect& destinationRect)
{
#if USE(SYSTEM_PREVIEW)
    if (auto* badge = dynamicDowncast<ARKitBadgeSystemImage>(systemImage.get())) {
        RefPtr nativeImage = resourceCache().cachedNativeImage(badge->imageIdentifier());
        if (!nativeImage) {
            ASSERT_NOT_REACHED();
            return;
        }
        badge->setImage(BitmapImage::create(nativeImage.releaseNonNull()));
    }
#endif
    context().drawSystemImage(systemImage, destinationRect);
}

void RemoteDisplayListRecorder::drawPatternNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    RefPtr image = resourceCache().cachedNativeImage(imageIdentifier);
    if (!image) {
        ASSERT_NOT_REACHED();
        return;
    }
    context().drawPattern(*image, destRect, tileRect, transform, phase, spacing, options);
}

void RemoteDisplayListRecorder::drawPatternImageBuffer(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    RefPtr image = imageBuffer(imageIdentifier);
    if (!image) {
        ASSERT_NOT_REACHED();
        return;
    }
    context().drawPattern(*image, destRect, tileRect, transform, phase, spacing, options);
}

void RemoteDisplayListRecorder::beginTransparencyLayer(float opacity)
{
    context().beginTransparencyLayer(opacity);
}

void RemoteDisplayListRecorder::beginTransparencyLayerWithCompositeMode(CompositeMode compositeMode)
{
    context().beginTransparencyLayer(compositeMode.operation, compositeMode.blendMode);
}

void RemoteDisplayListRecorder::endTransparencyLayer()
{
    context().endTransparencyLayer();
}

void RemoteDisplayListRecorder::drawRect(const FloatRect& rect, float borderThickness)
{
    context().drawRect(rect, borderThickness);
}

void RemoteDisplayListRecorder::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    context().drawLine(point1, point2);
}

void RemoteDisplayListRecorder::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    context().drawLinesForText(point, thickness, Vector(lineSegments), printing, doubleLines, strokeStyle);
}

void RemoteDisplayListRecorder::drawDotsForDocumentMarker(const FloatRect& rect, const DocumentMarkerLineStyle& style)
{
    context().drawDotsForDocumentMarker(rect, style);
}

void RemoteDisplayListRecorder::drawEllipse(const FloatRect& rect)
{
    context().drawEllipse(rect);
}

void RemoteDisplayListRecorder::drawPath(const Path& path)
{
    context().drawPath(path);
}

void RemoteDisplayListRecorder::drawFocusRingPath(const Path& path, float outlineWidth, const Color& color)
{
    context().drawFocusRing(path, outlineWidth, color);
}

void RemoteDisplayListRecorder::drawFocusRingRects(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    context().drawFocusRing(rects, outlineOffset, outlineWidth, color);
}

void RemoteDisplayListRecorder::fillRect(const FloatRect& rect, GraphicsContext::RequiresClipToRect requiresClipToRect)
{
    context().fillRect(rect, requiresClipToRect);
}

void RemoteDisplayListRecorder::fillRectWithColor(const FloatRect& rect, const Color& color)
{
    context().fillRect(rect, color);
}

void RemoteDisplayListRecorder::fillRectWithGradient(const FloatRect& rect, Ref<Gradient>&& gradient)
{
    context().fillRect(rect, gradient);
}

void RemoteDisplayListRecorder::fillRectWithGradientAndSpaceTransform(const FloatRect& rect, Ref<Gradient>&& gradient, const AffineTransform& transform, GraphicsContext::RequiresClipToRect requiresClipToRect)
{
    context().fillRect(rect, gradient, transform, requiresClipToRect);
}

void RemoteDisplayListRecorder::fillCompositedRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    context().fillRect(rect, color, op, blendMode);
}

void RemoteDisplayListRecorder::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    context().fillRoundedRect(rect, color, blendMode);
}

void RemoteDisplayListRecorder::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    context().fillRectWithRoundedHole(rect, roundedHoleRect, color);
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::fillLine(const PathDataLine& line)
{
    context().fillPath(Path({ PathSegment { line } }));
}

void RemoteDisplayListRecorder::fillArc(const PathArc& arc)
{
    context().fillPath(Path({ PathSegment { arc } }));
}

void RemoteDisplayListRecorder::fillClosedArc(const PathClosedArc& closedArc)
{
    context().fillPath(Path({ PathSegment { closedArc } }));
}

void RemoteDisplayListRecorder::fillQuadCurve(const PathDataQuadCurve& curve)
{
    context().fillPath(Path({ PathSegment { curve } }));
}

void RemoteDisplayListRecorder::fillBezierCurve(const PathDataBezierCurve& curve)
{
    context().fillPath(Path({ PathSegment { curve } }));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::fillPath(const Path& path)
{
    context().fillPath(path);
}

void RemoteDisplayListRecorder::fillPathSegment(const PathSegment& segment)
{
    context().fillPath(Path({ segment }));
}

void RemoteDisplayListRecorder::fillEllipse(const FloatRect& rect)
{
    context().fillEllipse(rect);
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
SharedVideoFrameReader& RemoteDisplayListRecorder::sharedVideoFrameReader()
{
    if (!m_sharedVideoFrameReader) {
        Ref gpuConnectionToWebProcess = m_renderingBackend->gpuConnectionToWebProcess();
        m_sharedVideoFrameReader = makeUnique<SharedVideoFrameReader>(Ref { gpuConnectionToWebProcess->videoFrameObjectHeap() }, gpuConnectionToWebProcess->webProcessIdentity());
    }
    return *m_sharedVideoFrameReader;
}

void RemoteDisplayListRecorder::drawVideoFrame(SharedVideoFrame&& frame, const FloatRect& destination, ImageOrientation orientation, bool shouldDiscardAlpha)
{
    if (auto videoFrame = sharedVideoFrameReader().read(WTFMove(frame)))
        context().drawVideoFrame(*videoFrame, destination, orientation, shouldDiscardAlpha);
}

void RemoteDisplayListRecorder::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    sharedVideoFrameReader().setSemaphore(WTFMove(semaphore));
}

void RemoteDisplayListRecorder::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    sharedVideoFrameReader().setSharedMemory(WTFMove(handle));
}
#endif // PLATFORM(COCOA) && ENABLE(VIDEO)

void RemoteDisplayListRecorder::strokeRect(const FloatRect& rect, float lineWidth)
{
    context().strokeRect(rect, lineWidth);
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::strokeLine(const PathDataLine& line)
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

void RemoteDisplayListRecorder::strokeLineWithColorAndThickness(const PathDataLine& line, std::optional<PackedColor::RGBA> strokeColor, std::optional<float> strokeThickness)
{
    if (strokeColor)
        setStrokePackedColor(*strokeColor);
    if (strokeThickness)
        setStrokeThickness(*strokeThickness);
    strokeLine(line);
}

void RemoteDisplayListRecorder::strokeArc(const PathArc& arc)
{
    context().strokePath(Path({ PathSegment { arc } }));
}

void RemoteDisplayListRecorder::strokeClosedArc(const PathClosedArc& closedArc)
{
    context().strokePath(Path({ PathSegment { closedArc } }));
}

void RemoteDisplayListRecorder::strokeQuadCurve(const PathDataQuadCurve& curve)
{
    context().strokePath(Path({ PathSegment { curve } }));
}

void RemoteDisplayListRecorder::strokeBezierCurve(const PathDataBezierCurve& curve)
{
    context().strokePath(Path({ PathSegment { curve } }));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::strokePathSegment(const PathSegment& segment)
{
    context().strokePath(PathSegment { segment });
}

void RemoteDisplayListRecorder::strokePath(const Path& path)
{
    context().strokePath(path);
}

void RemoteDisplayListRecorder::strokeEllipse(const FloatRect& rect)
{
    context().strokeEllipse(rect);
}

void RemoteDisplayListRecorder::clearRect(const FloatRect& rect)
{
    context().clearRect(rect);
}

void RemoteDisplayListRecorder::drawControlPart(Ref<ControlPart>&& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    if (!m_controlFactory)
        m_controlFactory = ControlFactory::create();
    part->setOverrideControlFactory(m_controlFactory.get());
    context().drawControlPart(part, borderRect, deviceScaleFactor, style);
    part->setOverrideControlFactory(nullptr);
}

#if USE(CG)

void RemoteDisplayListRecorder::applyStrokePattern()
{
    context().applyStrokePattern();
}

void RemoteDisplayListRecorder::applyFillPattern()
{
    context().applyFillPattern();
}

#endif // USE(CG)

void RemoteDisplayListRecorder::applyDeviceScaleFactor(float scaleFactor)
{
    context().applyDeviceScaleFactor(scaleFactor);
}

void RemoteDisplayListRecorder::beginPage(const IntSize& pageSize)
{
    context().beginPage(pageSize);
}

void RemoteDisplayListRecorder::endPage()
{
    context().endPage();
}

void RemoteDisplayListRecorder::setURLForRect(const URL& link, const FloatRect& destRect)
{
    context().setURLForRect(link, destRect);
}

std::optional<SharedPreferencesForWebProcess> RemoteDisplayListRecorder::sharedPreferencesForWebProcess() const
{
    return m_renderingBackend->sharedPreferencesForWebProcess();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
