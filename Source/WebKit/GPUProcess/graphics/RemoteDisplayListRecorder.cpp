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

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "ImageBufferShareableAllocator.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteImageBuffer.h"
#include <WebCore/BitmapImage.h>
#include <WebCore/FEImage.h>
#include <WebCore/FilterResults.h>
#include <WebCore/SVGFilter.h>

#if USE(SYSTEM_PREVIEW)
#include <WebCore/ARKitBadgeSystemImage.h>
#endif

namespace WebKit {
using namespace WebCore;

RemoteDisplayListRecorder::RemoteDisplayListRecorder(ImageBuffer& imageBuffer, QualifiedRenderingResourceIdentifier imageBufferIdentifier, ProcessIdentifier webProcessIdentifier, RemoteRenderingBackend& renderingBackend)
    : m_imageBuffer(imageBuffer)
    , m_imageBufferIdentifier(imageBufferIdentifier)
    , m_webProcessIdentifier(webProcessIdentifier)
    , m_renderingBackend(&renderingBackend)
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    , m_sharedVideoFrameReader(Ref { renderingBackend.gpuConnectionToWebProcess().videoFrameObjectHeap() }, renderingBackend.gpuConnectionToWebProcess().webProcessIdentity())
#endif
{
}

RemoteResourceCache& RemoteDisplayListRecorder::resourceCache()
{
    return m_renderingBackend->remoteResourceCache();
}

GraphicsContext& RemoteDisplayListRecorder::drawingContext()
{
    auto imageBuffer = m_imageBuffer.get();
    return imageBuffer->context();
}

void RemoteDisplayListRecorder::startListeningForIPC()
{
    m_renderingBackend->streamConnection().startReceivingMessages(*this, Messages::RemoteDisplayListRecorder::messageReceiverName(), m_imageBufferIdentifier.object().toUInt64());
}

void RemoteDisplayListRecorder::stopListeningForIPC()
{
    if (auto renderingBackend = std::exchange(m_renderingBackend, { }))
        renderingBackend->streamConnection().stopReceivingMessages(Messages::RemoteDisplayListRecorder::messageReceiverName(), m_imageBufferIdentifier.object().toUInt64());
}

void RemoteDisplayListRecorder::save()
{
    handleItem(DisplayList::Save());
}

void RemoteDisplayListRecorder::restore()
{
    handleItem(DisplayList::Restore());
}

void RemoteDisplayListRecorder::translate(float x, float y)
{
    handleItem(DisplayList::Translate(x, y));
}

void RemoteDisplayListRecorder::rotate(float angle)
{
    handleItem(DisplayList::Rotate(angle));
}

void RemoteDisplayListRecorder::scale(const FloatSize& scale)
{
    handleItem(DisplayList::Scale(scale));
}

void RemoteDisplayListRecorder::setCTM(const AffineTransform& ctm)
{
    handleItem(DisplayList::SetCTM(ctm));
}

void RemoteDisplayListRecorder::concatenateCTM(const AffineTransform& ctm)
{
    handleItem(DisplayList::ConcatenateCTM(ctm));
}

void RemoteDisplayListRecorder::setInlineFillColor(DisplayList::SetInlineFillColor&& item)
{
    handleItem(WTFMove(item));
}

void RemoteDisplayListRecorder::setInlineStrokeColor(DisplayList::SetInlineStrokeColor&& item)
{
    handleItem(WTFMove(item));
}

void RemoteDisplayListRecorder::setStrokeThickness(float thickness)
{
    handleItem(DisplayList::SetStrokeThickness(thickness));
}

void RemoteDisplayListRecorder::setState(DisplayList::SetState&& item)
{
    auto fixPatternTileImage = [&](Pattern* pattern) -> bool {
        if (!pattern)
            return true;
        auto sourceImage = resourceCache().cachedSourceImage({ pattern->tileImage().imageIdentifier(), m_webProcessIdentifier });
        if (!sourceImage) {
            ASSERT_NOT_REACHED();
            return false;
        }
        pattern->setTileImage(WTFMove(*sourceImage));
        return true;
    };

    auto fixBrushGradient = [&](SourceBrush& brush) -> bool {
        auto gradientIdentifier = brush.gradientIdentifier();
        if (!gradientIdentifier)
            return true;
        auto gradient = resourceCache().cachedGradient({ *gradientIdentifier, m_webProcessIdentifier });
        if (!gradient) {
            ASSERT_NOT_REACHED();
            return false;
        }
        brush.setGradient(*gradient, brush.gradientSpaceTransform());
        return true;
    };

    if (!fixPatternTileImage(item.state().fillBrush().pattern()) || !fixBrushGradient(item.state().fillBrush()))
        return;

    if (!fixPatternTileImage(item.state().strokeBrush().pattern()) || !fixBrushGradient(item.state().strokeBrush()))
        return;

    handleItem(WTFMove(item));
}

void RemoteDisplayListRecorder::setLineCap(LineCap lineCap)
{
    handleItem(DisplayList::SetLineCap(lineCap));
}

void RemoteDisplayListRecorder::setLineDash(DisplayList::SetLineDash&& item)
{
    handleItem(WTFMove(item));
}

void RemoteDisplayListRecorder::setLineJoin(LineJoin lineJoin)
{
    handleItem(DisplayList::SetLineJoin(lineJoin));
}

void RemoteDisplayListRecorder::setMiterLimit(float limit)
{
    handleItem(DisplayList::SetMiterLimit(limit));
}

void RemoteDisplayListRecorder::clearShadow()
{
    handleItem(DisplayList::ClearShadow());
}

void RemoteDisplayListRecorder::clip(const FloatRect& rect)
{
    handleItem(DisplayList::Clip(rect));
}

void RemoteDisplayListRecorder::clipRoundedRect(const FloatRoundedRect& rect)
{
    handleItem(DisplayList::ClipRoundedRect(rect));
}

void RemoteDisplayListRecorder::clipOut(const FloatRect& rect)
{
    handleItem(DisplayList::ClipOut(rect));
}

void RemoteDisplayListRecorder::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    handleItem(DisplayList::ClipOutRoundedRect(rect));
}

void RemoteDisplayListRecorder::clipToImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const WebCore::FloatRect& destinationRect)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    clipToImageBufferWithQualifiedIdentifier({ imageBufferIdentifier, m_webProcessIdentifier }, destinationRect);
}

void RemoteDisplayListRecorder::clipToImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageBufferIdentifier, const WebCore::FloatRect& destinationRect)
{
    RefPtr imageBuffer = resourceCache().cachedImageBuffer(imageBufferIdentifier);
    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(DisplayList::ClipToImageBuffer(imageBufferIdentifier.object(), destinationRect), *imageBuffer);
}

void RemoteDisplayListRecorder::clipOutToPath(const Path& path)
{
    handleItem(DisplayList::ClipOutToPath(path));
}

void RemoteDisplayListRecorder::clipPath(const Path& path, WindRule rule)
{
    handleItem(DisplayList::ClipPath(path, rule));
}

void RemoteDisplayListRecorder::resetClip()
{
    handleItem(DisplayList::ResetClip());
}

void RemoteDisplayListRecorder::drawFilteredImageBufferInternal(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
    RefPtr<ImageBuffer> sourceImage;

    if (sourceImageIdentifier) {
        sourceImage = resourceCache().cachedImageBuffer({ *sourceImageIdentifier, m_webProcessIdentifier });
        if (!sourceImage) {
            ASSERT_NOT_REACHED();
            return;
        }
    }

    for (auto& effect : filter.effectsOfType(FilterEffect::Type::FEImage)) {
        auto& feImage = downcast<FEImage>(effect.get());

        auto sourceImage = resourceCache().cachedSourceImage({ feImage.sourceImage().imageIdentifier(), m_webProcessIdentifier });
        if (!sourceImage) {
            ASSERT_NOT_REACHED();
            return;
        }

        feImage.setImageSource(WTFMove(*sourceImage));
    }

    handleItem(DisplayList::DrawFilteredImageBuffer(sourceImageIdentifier, sourceImageRect, filter), sourceImage.get(), results);
}

void RemoteDisplayListRecorder::drawFilteredImageBuffer(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Ref<Filter> filter)
{
    auto* svgFilter = dynamicDowncast<SVGFilter>(filter.get());

    if (!svgFilter || !svgFilter->hasValidRenderingResourceIdentifier()) {
        FilterResults results(makeUnique<ImageBufferShareableAllocator>(m_renderingBackend->resourceOwner()));
        drawFilteredImageBufferInternal(sourceImageIdentifier, sourceImageRect, filter, results);
        return;
    }

    RefPtr cachedFilter = resourceCache().cachedFilter({ filter->renderingResourceIdentifier(), m_webProcessIdentifier });
    auto* cachedSVGFilter = dynamicDowncast<SVGFilter>(cachedFilter.get());
    if (!cachedSVGFilter) {
        ASSERT_NOT_REACHED();
        return;
    }

    cachedSVGFilter->mergeEffects(svgFilter->effects());

    auto& results = cachedSVGFilter->ensureResults([&]() {
        auto allocator = makeUnique<ImageBufferShareableAllocator>(m_renderingBackend->resourceOwner());
        return makeUnique<FilterResults>(WTFMove(allocator));
    });

    drawFilteredImageBufferInternal(sourceImageIdentifier, sourceImageRect, *cachedSVGFilter, results);
}

void RemoteDisplayListRecorder::drawGlyphs(DisplayList::DrawGlyphs&& item)
{
    auto fontIdentifier = item.fontIdentifier();
    drawGlyphsWithQualifiedIdentifier(WTFMove(item), { fontIdentifier, m_webProcessIdentifier });
}

void RemoteDisplayListRecorder::drawGlyphsWithQualifiedIdentifier(DisplayList::DrawGlyphs&& item, QualifiedRenderingResourceIdentifier fontIdentifier)
{
    RefPtr font = resourceCache().cachedFont(fontIdentifier);
    if (!font) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(WTFMove(item), *font);
}

void RemoteDisplayListRecorder::drawDecomposedGlyphs(RenderingResourceIdentifier fontIdentifier, RenderingResourceIdentifier decomposedGlyphsIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    drawDecomposedGlyphsWithQualifiedIdentifiers({ fontIdentifier, m_webProcessIdentifier }, { decomposedGlyphsIdentifier, m_webProcessIdentifier });
}

void RemoteDisplayListRecorder::drawDecomposedGlyphsWithQualifiedIdentifiers(QualifiedRenderingResourceIdentifier fontIdentifier, QualifiedRenderingResourceIdentifier decomposedGlyphsIdentifier)
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

    handleItem(DisplayList::DrawDecomposedGlyphs(fontIdentifier.object(), decomposedGlyphsIdentifier.object()), *font, *decomposedGlyphs);
}

void RemoteDisplayListRecorder::drawImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    drawImageBufferWithQualifiedIdentifier({ imageBufferIdentifier, m_webProcessIdentifier }, destinationRect, srcRect, options);
}

void RemoteDisplayListRecorder::drawImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    RefPtr imageBuffer = resourceCache().cachedImageBuffer(imageBufferIdentifier);
    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(DisplayList::DrawImageBuffer(imageBufferIdentifier.object(), destinationRect, srcRect, options), *imageBuffer);
}

void RemoteDisplayListRecorder::drawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    drawNativeImageWithQualifiedIdentifier({ imageIdentifier, m_webProcessIdentifier }, imageSize, destRect, srcRect, options);
}

void RemoteDisplayListRecorder::drawNativeImageWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageIdentifier, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    RefPtr image = resourceCache().cachedNativeImage(imageIdentifier);
    if (!image) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(DisplayList::DrawNativeImage(imageIdentifier.object(), imageSize, destRect, srcRect, options), *image);
}

void RemoteDisplayListRecorder::drawSystemImage(Ref<SystemImage> systemImage, const FloatRect& destinationRect)
{
#if USE(SYSTEM_PREVIEW)
    if (is<ARKitBadgeSystemImage>(systemImage.get())) {
        ARKitBadgeSystemImage& badge = downcast<ARKitBadgeSystemImage>(systemImage.get());
        RefPtr nativeImage = resourceCache().cachedNativeImage({ badge.imageIdentifier(), m_webProcessIdentifier });
        if (!nativeImage) {
            ASSERT_NOT_REACHED();
            return;
        }
        badge.setImage(BitmapImage::create(nativeImage.releaseNonNull()));
    }
#endif
    handleItem(DisplayList::DrawSystemImage(systemImage, destinationRect));
}

void RemoteDisplayListRecorder::drawPattern(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    drawPatternWithQualifiedIdentifier({ imageIdentifier, m_webProcessIdentifier }, destRect, tileRect, transform, phase, spacing, options);
}

void RemoteDisplayListRecorder::drawPatternWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    auto sourceImage = resourceCache().cachedSourceImage(imageIdentifier);
    if (!sourceImage) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(DisplayList::DrawPattern(imageIdentifier.object(), destRect, tileRect, transform, phase, spacing, options), *sourceImage);
}

void RemoteDisplayListRecorder::beginTransparencyLayer(float opacity)
{
    handleItem(DisplayList::BeginTransparencyLayer(opacity));
}

void RemoteDisplayListRecorder::endTransparencyLayer()
{
    handleItem(DisplayList::EndTransparencyLayer());
}

void RemoteDisplayListRecorder::drawRect(const FloatRect& rect, float borderThickness)
{
    handleItem(DisplayList::DrawRect(rect, borderThickness));
}

void RemoteDisplayListRecorder::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    handleItem(DisplayList::DrawLine(point1, point2));
}

void RemoteDisplayListRecorder::drawLinesForText(DisplayList::DrawLinesForText&& item)
{
    handleItem(WTFMove(item));
}

void RemoteDisplayListRecorder::drawDotsForDocumentMarker(const FloatRect& rect, const DocumentMarkerLineStyle& style)
{
    handleItem(DisplayList::DrawDotsForDocumentMarker(rect, style));
}

void RemoteDisplayListRecorder::drawEllipse(const FloatRect& rect)
{
    handleItem(DisplayList::DrawEllipse(rect));
}

void RemoteDisplayListRecorder::drawPath(const Path& path)
{
    handleItem(DisplayList::DrawPath(path));
}

void RemoteDisplayListRecorder::drawFocusRingPath(const Path& path, float outlineWidth, const Color& color)
{
    handleItem(DisplayList::DrawFocusRingPath(path, outlineWidth, color));
}

void RemoteDisplayListRecorder::drawFocusRingRects(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    handleItem(DisplayList::DrawFocusRingRects(rects, outlineOffset, outlineWidth, color));
}

void RemoteDisplayListRecorder::fillRect(const FloatRect& rect)
{
    handleItem(DisplayList::FillRect(rect));
}

void RemoteDisplayListRecorder::fillRectWithColor(const FloatRect& rect, const Color& color)
{
    handleItem(DisplayList::FillRectWithColor(rect, color));
}

void RemoteDisplayListRecorder::fillRectWithGradient(DisplayList::FillRectWithGradient&& item)
{
    handleItem(WTFMove(item));
}

void RemoteDisplayListRecorder::fillCompositedRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    handleItem(DisplayList::FillCompositedRect(rect, color, op, blendMode));
}

void RemoteDisplayListRecorder::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    handleItem(DisplayList::FillRoundedRect(rect, color, blendMode));
}

void RemoteDisplayListRecorder::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    handleItem(DisplayList::FillRectWithRoundedHole(rect, roundedHoleRect, color));
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::fillLine(const PathDataLine& line)
{
    handleItem(DisplayList::FillLine(line));
}

void RemoteDisplayListRecorder::fillArc(const PathArc& arc)
{
    handleItem(DisplayList::FillArc(arc));
}

void RemoteDisplayListRecorder::fillQuadCurve(const PathDataQuadCurve& curve)
{
    handleItem(DisplayList::FillQuadCurve(curve));
}

void RemoteDisplayListRecorder::fillBezierCurve(const PathDataBezierCurve& curve)
{
    handleItem(DisplayList::FillBezierCurve(curve));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::fillPath(const Path& path)
{
    handleItem(DisplayList::FillPath(path));
}

void RemoteDisplayListRecorder::fillPathSegment(const PathSegment& segment)
{
    handleItem(DisplayList::FillPathSegment(segment));
}

void RemoteDisplayListRecorder::fillEllipse(const FloatRect& rect)
{
    handleItem(DisplayList::FillEllipse(rect));
}

void RemoteDisplayListRecorder::convertToLuminanceMask()
{
    auto imageBuffer = m_imageBuffer.get();
    imageBuffer->convertToLuminanceMask();
}

void RemoteDisplayListRecorder::transformToColorSpace(const WebCore::DestinationColorSpace& colorSpace)
{
    auto imageBuffer = m_imageBuffer.get();
    imageBuffer->transformToColorSpace(colorSpace);
}

#if ENABLE(VIDEO)
void RemoteDisplayListRecorder::paintFrameForMedia(MediaPlayerIdentifier identifier, const FloatRect& destination)
{
    m_renderingBackend->gpuConnectionToWebProcess().performWithMediaPlayerOnMainThread(identifier, [imageBuffer = m_imageBuffer.get(), destination](MediaPlayer& player) {
        // It is currently not safe to call paintFrameForMedia() off the main thread.
        imageBuffer->context().paintFrameForMedia(player, destination);
    });
}
#endif

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteDisplayListRecorder::paintVideoFrame(SharedVideoFrame&& frame, const WebCore::FloatRect& destination, bool shouldDiscardAlpha)
{
    if (auto videoFrame = m_sharedVideoFrameReader.read(WTFMove(frame)))
        drawingContext().paintVideoFrame(*videoFrame, destination, shouldDiscardAlpha);
}

void RemoteDisplayListRecorder::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteDisplayListRecorder::setSharedVideoFrameMemory(SharedMemory::Handle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTFMove(handle));
}
#endif // PLATFORM(COCOA) && ENABLE(VIDEO)

void RemoteDisplayListRecorder::strokeRect(const FloatRect& rect, float lineWidth)
{
    handleItem(DisplayList::StrokeRect(rect, lineWidth));
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::strokeLine(const PathDataLine& line)
{
    handleItem(DisplayList::StrokeLine(line));
}

void RemoteDisplayListRecorder::strokeLineWithColorAndThickness(const PathDataLine& line, DisplayList::SetInlineStrokeColor&& color, float thickness)
{
    handleItem(WTFMove(color));
    handleItem(DisplayList::SetStrokeThickness(thickness));
    handleItem(DisplayList::StrokeLine(line));
}

void RemoteDisplayListRecorder::strokeArc(const PathArc& arc)
{
    handleItem(DisplayList::StrokeArc(arc));
}

void RemoteDisplayListRecorder::strokeQuadCurve(const PathDataQuadCurve& curve)
{
    handleItem(DisplayList::StrokeQuadCurve(curve));
}

void RemoteDisplayListRecorder::strokeBezierCurve(const PathDataBezierCurve& curve)
{
    handleItem(DisplayList::StrokeBezierCurve(curve));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::strokePathSegment(const PathSegment& segment)
{
    handleItem(DisplayList::StrokePathSegment(segment));
}

void RemoteDisplayListRecorder::strokePath(const Path& path)
{
    handleItem(DisplayList::StrokePath(path));
}

void RemoteDisplayListRecorder::strokeEllipse(const FloatRect& rect)
{
    handleItem(DisplayList::StrokeEllipse(rect));
}

void RemoteDisplayListRecorder::clearRect(const FloatRect& rect)
{
    handleItem(DisplayList::ClearRect(rect));
}

void RemoteDisplayListRecorder::drawControlPart(Ref<ControlPart> part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    if (!m_controlFactory)
        m_controlFactory = ControlFactory::createControlFactory();
    part->setControlFactory(m_controlFactory.get());
    handleItem(DisplayList::DrawControlPart(WTFMove(part), borderRect, deviceScaleFactor, style));
}

#if USE(CG)

void RemoteDisplayListRecorder::applyStrokePattern()
{
    handleItem(DisplayList::ApplyStrokePattern());
}

void RemoteDisplayListRecorder::applyFillPattern()
{
    handleItem(DisplayList::ApplyFillPattern());
}

#endif // USE(CG)

void RemoteDisplayListRecorder::applyDeviceScaleFactor(float scaleFactor)
{
    handleItem(DisplayList::ApplyDeviceScaleFactor(scaleFactor));
}

void RemoteDisplayListRecorder::flushContext(IPC::Semaphore&& semaphore)
{
    auto imageBuffer = m_imageBuffer.get();
    imageBuffer->flushDrawingContext();
    semaphore.signal();
}

void RemoteDisplayListRecorder::flushContextSync(CompletionHandler<void()>&& completionHandler)
{
    auto imageBuffer = m_imageBuffer.get();
    imageBuffer->flushDrawingContext();
    completionHandler();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
