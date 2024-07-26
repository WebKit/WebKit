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

RemoteDisplayListRecorder::RemoteDisplayListRecorder(ImageBuffer& imageBuffer, RenderingResourceIdentifier imageBufferIdentifier, RemoteRenderingBackend& renderingBackend)
    : m_imageBuffer(imageBuffer)
    , m_imageBufferIdentifier(imageBufferIdentifier)
    , m_renderingBackend(&renderingBackend)
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
    m_renderingBackend->streamConnection().startReceivingMessages(*this, Messages::RemoteDisplayListRecorder::messageReceiverName(), m_imageBufferIdentifier.toUInt64());
}

void RemoteDisplayListRecorder::stopListeningForIPC()
{
    if (auto renderingBackend = std::exchange(m_renderingBackend, { }))
        renderingBackend->streamConnection().stopReceivingMessages(Messages::RemoteDisplayListRecorder::messageReceiverName(), m_imageBufferIdentifier.toUInt64());
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

void RemoteDisplayListRecorder::setInlineFillColor(DisplayList::SetInlineFillColor&& fillColorItem)
{
    handleItem(WTFMove(fillColorItem));
}

void RemoteDisplayListRecorder::setInlineStroke(DisplayList::SetInlineStroke&& strokeItem)
{
    handleItem(WTFMove(strokeItem));
}

void RemoteDisplayListRecorder::setState(DisplayList::SetState&& item)
{
    auto fixPatternTileImage = [&](Pattern* pattern) -> bool {
        if (!pattern)
            return true;
        auto tileImage = sourceImage(pattern->tileImage().imageIdentifier());
        if (!tileImage) {
            ASSERT_NOT_REACHED();
            return false;
        }
        pattern->setTileImage(WTFMove(*tileImage));
        return true;
    };

    auto fixBrushGradient = [&](SourceBrush& brush) -> bool {
        auto gradientIdentifier = brush.gradientIdentifier();
        if (!gradientIdentifier)
            return true;
        RefPtr gradient = resourceCache().cachedGradient(*gradientIdentifier);
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

void RemoteDisplayListRecorder::clearDropShadow()
{
    handleItem(DisplayList::ClearDropShadow());
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
    RefPtr clipImage = imageBuffer(imageBufferIdentifier);
    if (!clipImage) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(DisplayList::ClipToImageBuffer(imageBufferIdentifier, destinationRect), *clipImage);
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
    RefPtr<ImageBuffer> sourceImageBuffer;

    if (sourceImageIdentifier) {
        sourceImageBuffer = imageBuffer(*sourceImageIdentifier);
        if (!sourceImageBuffer) {
            ASSERT_NOT_REACHED();
            return;
        }
    }

    for (auto& effect : filter.effectsOfType(FilterEffect::Type::FEImage)) {
        auto& feImage = downcast<FEImage>(effect.get());

        auto effectImage = sourceImage(feImage.sourceImage().imageIdentifier());
        if (!effectImage) {
            ASSERT_NOT_REACHED();
            return;
        }

        feImage.setImageSource(WTFMove(*effectImage));
    }

    handleItem(DisplayList::DrawFilteredImageBuffer(sourceImageIdentifier, sourceImageRect, filter), sourceImageBuffer.get(), results);
}

void RemoteDisplayListRecorder::drawFilteredImageBuffer(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Ref<Filter> filter)
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

void RemoteDisplayListRecorder::drawGlyphs(DisplayList::DrawGlyphs&& item)
{
    auto fontIdentifier = item.fontIdentifier();
    RefPtr font = resourceCache().cachedFont(fontIdentifier);
    if (!font) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(WTFMove(item), *font);
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

    handleItem(DisplayList::DrawDecomposedGlyphs(fontIdentifier, decomposedGlyphsIdentifier), *font, *decomposedGlyphs);
}

void RemoteDisplayListRecorder::drawDisplayListItems(Vector<WebCore::DisplayList::Item>&& items, const FloatPoint& destination)
{
    handleItem(DisplayList::DrawDisplayListItems(WTFMove(items), destination), resourceCache().resourceHeap());
}

void RemoteDisplayListRecorder::drawImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr sourceImage = imageBuffer(imageBufferIdentifier);
    if (!sourceImage) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(DisplayList::DrawImageBuffer(imageBufferIdentifier, destinationRect, srcRect, options), *sourceImage);
}

void RemoteDisplayListRecorder::drawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr image = resourceCache().cachedNativeImage(imageIdentifier);
    if (!image) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(DisplayList::DrawNativeImage(imageIdentifier, destRect, srcRect, options), *image);
}

void RemoteDisplayListRecorder::drawSystemImage(Ref<SystemImage> systemImage, const FloatRect& destinationRect)
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
    handleItem(DisplayList::DrawSystemImage(systemImage, destinationRect));
}

void RemoteDisplayListRecorder::drawPattern(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    auto patternImage = sourceImage(imageIdentifier);
    if (!patternImage) {
        ASSERT_NOT_REACHED();
        return;
    }

    handleItem(DisplayList::DrawPattern(imageIdentifier, destRect, tileRect, transform, phase, spacing, options), *patternImage);
}

void RemoteDisplayListRecorder::beginTransparencyLayer(float opacity)
{
    handleItem(DisplayList::BeginTransparencyLayer(opacity));
}

void RemoteDisplayListRecorder::beginTransparencyLayerWithCompositeMode(CompositeMode compositeMode)
{
    handleItem(DisplayList::BeginTransparencyLayerWithCompositeMode(compositeMode));
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

void RemoteDisplayListRecorder::fillRect(const FloatRect& rect, GraphicsContext::RequiresClipToRect requiresClipToRect)
{
    handleItem(DisplayList::FillRect(rect, requiresClipToRect));
}

void RemoteDisplayListRecorder::fillRectWithColor(const FloatRect& rect, const Color& color)
{
    handleItem(DisplayList::FillRectWithColor(rect, color));
}

void RemoteDisplayListRecorder::fillRectWithGradient(DisplayList::FillRectWithGradient&& item)
{
    handleItem(WTFMove(item));
}

void RemoteDisplayListRecorder::fillRectWithGradientAndSpaceTransform(DisplayList::FillRectWithGradientAndSpaceTransform&& item)
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

void RemoteDisplayListRecorder::fillClosedArc(const PathClosedArc& closedArc)
{
    handleItem(DisplayList::FillClosedArc(closedArc));
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

#if ENABLE(VIDEO)
void RemoteDisplayListRecorder::paintFrameForMedia(MediaPlayerIdentifier identifier, const FloatRect& destination)
{
    m_renderingBackend->gpuConnectionToWebProcess().performWithMediaPlayerOnMainThread(identifier, [imageBuffer = m_imageBuffer, destination](MediaPlayer& player) {
        // It is currently not safe to call paintFrameForMedia() off the main thread.
        imageBuffer->context().paintFrameForMedia(player, destination);
    });
}
#endif

#if PLATFORM(COCOA) && ENABLE(VIDEO)
SharedVideoFrameReader& RemoteDisplayListRecorder::sharedVideoFrameReader()
{
    if (!m_sharedVideoFrameReader)
        m_sharedVideoFrameReader = makeUnique<SharedVideoFrameReader>(Ref { m_renderingBackend->gpuConnectionToWebProcess().videoFrameObjectHeap() }, m_renderingBackend->gpuConnectionToWebProcess().webProcessIdentity());

    return *m_sharedVideoFrameReader;
}

void RemoteDisplayListRecorder::paintVideoFrame(SharedVideoFrame&& frame, const WebCore::FloatRect& destination, bool shouldDiscardAlpha)
{
    if (auto videoFrame = sharedVideoFrameReader().read(WTFMove(frame)))
        drawingContext().paintVideoFrame(*videoFrame, destination, shouldDiscardAlpha);
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
    handleItem(DisplayList::StrokeRect(rect, lineWidth));
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorder::strokeLine(const PathDataLine& line)
{
    handleItem(DisplayList::StrokeLine(line));
}

void RemoteDisplayListRecorder::strokeLineWithColorAndThickness(const PathDataLine& line, DisplayList::SetInlineStroke&& strokeItem)
{
    handleItem(WTFMove(strokeItem));
    handleItem(DisplayList::StrokeLine(line));
}

void RemoteDisplayListRecorder::strokeArc(const PathArc& arc)
{
    handleItem(DisplayList::StrokeArc(arc));
}

void RemoteDisplayListRecorder::strokeClosedArc(const PathClosedArc& closedArc)
{
    handleItem(DisplayList::StrokeClosedArc(closedArc));
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

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
