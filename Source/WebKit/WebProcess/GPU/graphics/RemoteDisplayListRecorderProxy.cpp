/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RemoteDisplayListRecorderProxy.h"

#if ENABLE(GPU_PROCESS)

#include "Logging.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "SharedVideoFrame.h"
#include "StreamClientConnection.h"
#include "WebProcess.h"
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/FEImage.h>
#include <WebCore/Filter.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SVGFilter.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

#if USE(SYSTEM_PREVIEW)
#include <WebCore/ARKitBadgeSystemImage.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteDisplayListRecorderProxy);

RemoteDisplayListRecorderProxy::RemoteDisplayListRecorderProxy(const DestinationColorSpace& colorSpace, RenderingMode renderingMode, const FloatRect& initialClip, const AffineTransform& initialCTM, RemoteRenderingBackendProxy& renderingBackend)
    : DisplayList::Recorder(IsDeferred::No, { }, initialClip, initialCTM, colorSpace, DrawGlyphsMode::Deconstruct)
    , m_renderingMode(renderingMode)
    , m_identifier(RemoteDisplayListRecorderIdentifier::generate())
    , m_renderingBackend(renderingBackend)
{
}

RemoteDisplayListRecorderProxy::RemoteDisplayListRecorderProxy(const DestinationColorSpace& colorSpace, WebCore::ContentsFormat contentsFormat, RenderingMode renderingMode, const FloatRect& initialClip, const AffineTransform& initialCTM, RemoteDisplayListRecorderIdentifier identifier, RemoteRenderingBackendProxy& renderingBackend)
    : DisplayList::Recorder(IsDeferred::No, { }, initialClip, initialCTM, colorSpace, DrawGlyphsMode::Deconstruct)
    , m_renderingMode(renderingMode)
    , m_identifier(identifier)
    , m_renderingBackend(renderingBackend)
    , m_contentsFormat(contentsFormat)
{
}

RemoteDisplayListRecorderProxy::~RemoteDisplayListRecorderProxy() = default;

template<typename T>
ALWAYS_INLINE void RemoteDisplayListRecorderProxy::send(T&& message)
{
    RefPtr connection = m_connection;
    if (!connection) [[unlikely]] {
        if (RefPtr backend = m_renderingBackend.get())
            connection = backend->connection();
        if (!connection)
            return;
        m_connection = connection;
    }

    if (!m_hasDrawn) {
        if (RefPtr client = m_client.get())
            client->backingStoreWillChange();
        m_hasDrawn = true;
    }
    auto result = connection->send(std::forward<T>(message), m_identifier);
    if (result != IPC::Error::NoError) [[unlikely]] {
        RELEASE_LOG(RemoteLayerBuffers, "RemoteDisplayListRecorderProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            IPC::description(T::name()).characters(), IPC::errorAsString(result).characters());
        didBecomeUnresponsive();
    }
}

void RemoteDisplayListRecorderProxy::didBecomeUnresponsive() const
{
    RefPtr backend = m_renderingBackend.get();
    if (!backend) [[unlikely]]
        return;
    backend->didBecomeUnresponsive();
}

RenderingMode RemoteDisplayListRecorderProxy::renderingMode() const
{
    return m_renderingMode;
}

void RemoteDisplayListRecorderProxy::save(GraphicsContextState::Purpose purpose)
{
    updateStateForSave(purpose);
    send(Messages::RemoteDisplayListRecorder::Save());
}

void RemoteDisplayListRecorderProxy::restore(GraphicsContextState::Purpose purpose)
{
    if (!updateStateForRestore(purpose))
        return;
    send(Messages::RemoteDisplayListRecorder::Restore());
}

void RemoteDisplayListRecorderProxy::translate(float x, float y)
{
    if (!updateStateForTranslate(x, y))
        return;
    send(Messages::RemoteDisplayListRecorder::Translate(x, y));
}

void RemoteDisplayListRecorderProxy::rotate(float angle)
{
    if (!updateStateForRotate(angle))
        return;
    send(Messages::RemoteDisplayListRecorder::Rotate(angle));
}

void RemoteDisplayListRecorderProxy::scale(const FloatSize& scale)
{
    if (!updateStateForScale(scale))
        return;
    send(Messages::RemoteDisplayListRecorder::Scale(scale));
}

void RemoteDisplayListRecorderProxy::setCTM(const AffineTransform& transform)
{
    updateStateForSetCTM(transform);
    send(Messages::RemoteDisplayListRecorder::SetCTM(transform));
}

void RemoteDisplayListRecorderProxy::concatCTM(const AffineTransform& transform)
{
    if (!updateStateForConcatCTM(transform))
        return;
    send(Messages::RemoteDisplayListRecorder::ConcatCTM(transform));
}

void RemoteDisplayListRecorderProxy::setLineCap(LineCap lineCap)
{
    send(Messages::RemoteDisplayListRecorder::SetLineCap(lineCap));
}

void RemoteDisplayListRecorderProxy::setLineDash(const DashArray& array, float dashOffset)
{
    send(Messages::RemoteDisplayListRecorder::SetLineDash(FixedVector<double>(array.span()), dashOffset));
}

void RemoteDisplayListRecorderProxy::setLineJoin(LineJoin lineJoin)
{
    send(Messages::RemoteDisplayListRecorder::SetLineJoin(lineJoin));
}

void RemoteDisplayListRecorderProxy::setMiterLimit(float limit)
{
    send(Messages::RemoteDisplayListRecorder::SetMiterLimit(limit));
}

void RemoteDisplayListRecorderProxy::clip(const FloatRect& rect)
{
    updateStateForClip(rect);
    send(Messages::RemoteDisplayListRecorder::Clip(rect));
}

void RemoteDisplayListRecorderProxy::clipRoundedRect(const FloatRoundedRect& rect)
{
    updateStateForClipRoundedRect(rect);
    send(Messages::RemoteDisplayListRecorder::ClipRoundedRect(rect));
}

void RemoteDisplayListRecorderProxy::clipOut(const FloatRect& rect)
{
    updateStateForClipOut(rect);
    send(Messages::RemoteDisplayListRecorder::ClipOut(rect));
}

void RemoteDisplayListRecorderProxy::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    updateStateForClipOutRoundedRect(rect);
    send(Messages::RemoteDisplayListRecorder::ClipOutRoundedRect(rect));
}

void RemoteDisplayListRecorderProxy::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destinationRect)
{
    updateStateForClipToImageBuffer(destinationRect);
    recordResourceUse(imageBuffer);
    send(Messages::RemoteDisplayListRecorder::ClipToImageBuffer(imageBuffer.renderingResourceIdentifier(), destinationRect));
}

void RemoteDisplayListRecorderProxy::clipOut(const Path& path)
{
    updateStateForClipOut(path);
    send(Messages::RemoteDisplayListRecorder::ClipOutToPath(path));
}

void RemoteDisplayListRecorderProxy::clipPath(const Path& path, WindRule rule)
{
    updateStateForClipPath(path);
    send(Messages::RemoteDisplayListRecorder::ClipPath(path, rule));
}

void RemoteDisplayListRecorderProxy::resetClip()
{
    updateStateForResetClip();
    send(Messages::RemoteDisplayListRecorder::ResetClip());
    clip(initialClip());
}

void RemoteDisplayListRecorderProxy::drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
    appendStateChangeItemIfNecessary();

    for (auto& effect : filter.effectsOfType(FilterEffect::Type::FEImage)) {
        Ref feImage = downcast<FEImage>(effect.get());
        if (!recordResourceUse(feImage->sourceImage())) {
            GraphicsContext::drawFilteredImageBuffer(sourceImage, sourceImageRect, filter, results);
            return;
        }
    }

    RefPtr svgFilter = dynamicDowncast<SVGFilter>(filter);
    if (svgFilter && svgFilter->hasValidRenderingResourceIdentifier())
        recordResourceUse(filter);

    std::optional<RenderingResourceIdentifier> identifier;
    if (sourceImage) {
        if (!recordResourceUse(*sourceImage)) {
            GraphicsContext::drawFilteredImageBuffer(sourceImage, sourceImageRect, filter, results);
            return;
        }
        identifier = sourceImage->renderingResourceIdentifier();
    }

    send(Messages::RemoteDisplayListRecorder::DrawFilteredImageBuffer(WTFMove(identifier), sourceImageRect, filter));
}

void RemoteDisplayListRecorderProxy::drawGlyphs(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    if (decomposeDrawGlyphsIfNeeded(font, glyphs, advances, localAnchor, smoothingMode))
        return;
    drawGlyphsImmediate(font, glyphs, advances, localAnchor, smoothingMode);
}

void RemoteDisplayListRecorderProxy::drawGlyphsImmediate(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    ASSERT(glyphs.size() == advances.size());
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));
    send(Messages::RemoteDisplayListRecorder::DrawGlyphs(font.renderingResourceIdentifier(), { glyphs.data(), Vector<FloatSize>(advances).span().data(), glyphs.size() }, localAnchor, smoothingMode));
}

void RemoteDisplayListRecorderProxy::drawDecomposedGlyphs(const Font& font, const DecomposedGlyphs& decomposedGlyphs)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));
    recordResourceUse(const_cast<DecomposedGlyphs&>(decomposedGlyphs));
    send(Messages::RemoteDisplayListRecorder::DrawDecomposedGlyphs(font.renderingResourceIdentifier(), decomposedGlyphs.renderingResourceIdentifier()));
}

void RemoteDisplayListRecorderProxy::drawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();

    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawImageBuffer(imageBuffer, destRect, srcRect, options);
        return;
    }

    send(Messages::RemoteDisplayListRecorder::DrawImageBuffer(imageBuffer.renderingResourceIdentifier(), destRect, srcRect, options));
}

void RemoteDisplayListRecorderProxy::drawNativeImageInternal(NativeImage& image, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    auto headroom = options.headroom();
    if (headroom == Headroom::FromImage)
        headroom = image.headroom();
    if (m_maxEDRHeadroom) {
        if (*m_maxEDRHeadroom < headroom)
            headroom = Headroom(*m_maxEDRHeadroom);
    }
    m_maxPaintedEDRHeadroom = std::max(m_maxPaintedEDRHeadroom, headroom.headroom);
    m_maxRequestedEDRHeadroom = std::max(m_maxRequestedEDRHeadroom, image.headroom().headroom);
    ImagePaintingOptions clampedOptions(options, headroom);
#endif
    appendStateChangeItemIfNecessary();
    recordResourceUse(image);
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    send(Messages::RemoteDisplayListRecorder::DrawNativeImage(image.renderingResourceIdentifier(), destRect, srcRect, clampedOptions));
#else
    send(Messages::RemoteDisplayListRecorder::DrawNativeImage(image.renderingResourceIdentifier(), destRect, srcRect, options));
#endif
}

void RemoteDisplayListRecorderProxy::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    appendStateChangeItemIfNecessary();
#if USE(SYSTEM_PREVIEW)
    if (auto* badgeSystemImage = dynamicDowncast<ARKitBadgeSystemImage>(systemImage)) {
        if (auto image = badgeSystemImage->image()) {
            auto nativeImage = image->nativeImage();
            if (!nativeImage)
                return;
            recordResourceUse(*nativeImage);
        }
    }
#endif
    send(Messages::RemoteDisplayListRecorder::DrawSystemImage(systemImage, destinationRect));
}

void RemoteDisplayListRecorderProxy::drawPattern(NativeImage& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(image);
    send(Messages::RemoteDisplayListRecorder::DrawPatternNativeImage(image.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options));
}

void RemoteDisplayListRecorderProxy::drawPattern(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawPattern(imageBuffer, destRect, tileRect, patternTransform, phase, spacing, options);
        return;
    }

    send(Messages::RemoteDisplayListRecorder::DrawPatternImageBuffer(imageBuffer.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options));
}

void RemoteDisplayListRecorderProxy::beginTransparencyLayer(float opacity)
{
    updateStateForBeginTransparencyLayer(opacity);
    send(Messages::RemoteDisplayListRecorder::BeginTransparencyLayer(opacity));
}

void RemoteDisplayListRecorderProxy::beginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    updateStateForBeginTransparencyLayer(compositeOperator, blendMode);
    send(Messages::RemoteDisplayListRecorder::BeginTransparencyLayerWithCompositeMode({ compositeOperator, blendMode }));
}

void RemoteDisplayListRecorderProxy::endTransparencyLayer()
{
    updateStateForEndTransparencyLayer();
    send(Messages::RemoteDisplayListRecorder::EndTransparencyLayer());
}

void RemoteDisplayListRecorderProxy::drawRect(const FloatRect& rect, float width)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawRect(rect, width));
}

void RemoteDisplayListRecorderProxy::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawLine(point1, point2));
}

void RemoteDisplayListRecorderProxy::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle style)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawLinesForText(point, thickness, lineSegments, printing, doubleLines, style));
}

void RemoteDisplayListRecorderProxy::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawDotsForDocumentMarker(rect, style));
}

void RemoteDisplayListRecorderProxy::drawEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawEllipse(rect));
}

void RemoteDisplayListRecorderProxy::drawPath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawPath(path));
}

void RemoteDisplayListRecorderProxy::drawFocusRing(const Path& path, float outlineWidth, const Color& color)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawFocusRingPath(path, outlineWidth, color));
}

void RemoteDisplayListRecorderProxy::drawFocusRing(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawFocusRingRects(rects, outlineOffset, outlineWidth, color));
}

void RemoteDisplayListRecorderProxy::fillPath(const Path& path)
{
    appendStateChangeItemIfNecessary();

    if (auto segment = path.singleSegment()) {
        WTF::switchOn(segment->data(),
#if ENABLE(INLINE_PATH_DATA)
        [&](const PathArc &arc) {
            send(Messages::RemoteDisplayListRecorder::FillArc(arc));
        },
        [&](const PathClosedArc& closedArc) {
            send(Messages::RemoteDisplayListRecorder::FillClosedArc(closedArc));
        },
        [&](const PathDataLine& line) {
            send(Messages::RemoteDisplayListRecorder::FillLine(line));
        },
        [&](const PathDataQuadCurve& curve) {
            send(Messages::RemoteDisplayListRecorder::FillQuadCurve(curve));
        },
        [&](const PathDataBezierCurve& curve) {
            send(Messages::RemoteDisplayListRecorder::FillBezierCurve(curve));
        },
#endif
        [&](auto&&) {
            send(Messages::RemoteDisplayListRecorder::FillPathSegment(*segment));
        });
        return;
    }

    send(Messages::RemoteDisplayListRecorder::FillPath(path));
}

void RemoteDisplayListRecorderProxy::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillRect(rect, requiresClipToRect));
}

void RemoteDisplayListRecorderProxy::fillRect(const FloatRect& rect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillRectWithColor(rect, color));
}

void RemoteDisplayListRecorderProxy::fillRect(const FloatRect& rect, Gradient& gradient)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillRectWithGradient(rect, gradient));
}

void RemoteDisplayListRecorderProxy::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillRectWithGradientAndSpaceTransform(rect, gradient, gradientSpaceTransform, requiresClipToRect));
}

void RemoteDisplayListRecorderProxy::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode mode)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillCompositedRect(rect, color, op, mode));
}

void RemoteDisplayListRecorderProxy::fillRoundedRect(const FloatRoundedRect& roundedRect, const Color& color, BlendMode mode)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillRoundedRect(roundedRect, color, mode));
}

void RemoteDisplayListRecorderProxy::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedRect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillRectWithRoundedHole(rect, roundedRect, color));
}

void RemoteDisplayListRecorderProxy::fillEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillEllipse(rect));
}

#if ENABLE(VIDEO)
void RemoteDisplayListRecorderProxy::drawVideoFrame(VideoFrame& frame, const FloatRect& destination, ImageOrientation orientation, bool shouldDiscardAlpha)
{
    appendStateChangeItemIfNecessary();
#if PLATFORM(COCOA)
    Locker locker { m_sharedVideoFrameWriterLock };
    if (!m_sharedVideoFrameWriter)
        m_sharedVideoFrameWriter = makeUnique<SharedVideoFrameWriter>();

    auto sharedVideoFrame = m_sharedVideoFrameWriter->write(frame, [&](auto& semaphore) {
        send(Messages::RemoteDisplayListRecorder::SetSharedVideoFrameSemaphore { semaphore });
    }, [&](SharedMemory::Handle&& handle) {
        send(Messages::RemoteDisplayListRecorder::SetSharedVideoFrameMemory { WTFMove(handle) });
    });
    if (!sharedVideoFrame)
        return;
    send(Messages::RemoteDisplayListRecorder::DrawVideoFrame(WTFMove(*sharedVideoFrame), destination, orientation, shouldDiscardAlpha));
#endif
}
#endif

void RemoteDisplayListRecorderProxy::strokePath(const Path& path)
{
    if (const auto* segment = path.singleSegmentIfExists()) {
#if ENABLE(INLINE_PATH_DATA)
        if (const auto* line = std::get_if<PathDataLine>(&segment->data())) {
            auto strokeData = appendStateChangeItemForInlineStrokeIfNecessary();
            if (!strokeData.color && !strokeData.thickness)
                send(Messages::RemoteDisplayListRecorder::StrokeLine(*line));
            else
                send(Messages::RemoteDisplayListRecorder::StrokeLineWithColorAndThickness(*line, strokeData.color, strokeData.thickness));
            return;
        }
#endif
        appendStateChangeItemIfNecessary();
        WTF::switchOn(segment->data(),
#if ENABLE(INLINE_PATH_DATA)
            [&](const PathArc &arc) {
                send(Messages::RemoteDisplayListRecorder::StrokeArc(arc));
            },
            [&](const PathClosedArc& closedArc) {
                send(Messages::RemoteDisplayListRecorder::StrokeClosedArc(closedArc));
            },
            [&](const PathDataLine& line) {
                send(Messages::RemoteDisplayListRecorder::StrokeLine(line));
            },
            [&](const PathDataQuadCurve& curve) {
                send(Messages::RemoteDisplayListRecorder::StrokeQuadCurve(curve));
            },
            [&](const PathDataBezierCurve& curve) {
                send(Messages::RemoteDisplayListRecorder::StrokeBezierCurve(curve));
            },
#endif
            [&](auto&&) {
                send(Messages::RemoteDisplayListRecorder::StrokePathSegment(*segment));
            });
        return;
    }
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::StrokePath(path));
}

void RemoteDisplayListRecorderProxy::strokeRect(const FloatRect& rect, float width)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::StrokeRect(rect, width));
}

void RemoteDisplayListRecorderProxy::strokeEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::StrokeEllipse(rect));
}

void RemoteDisplayListRecorderProxy::clearRect(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::ClearRect(rect));
}

void RemoteDisplayListRecorderProxy::drawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawControlPart(part, borderRect, deviceScaleFactor, style));
}

#if USE(CG)

void RemoteDisplayListRecorderProxy::applyStrokePattern()
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::ApplyStrokePattern());
}

void RemoteDisplayListRecorderProxy::applyFillPattern()
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::ApplyFillPattern());
}

#endif // USE(CG)

void RemoteDisplayListRecorderProxy::applyDeviceScaleFactor(float scaleFactor)
{
    updateStateForApplyDeviceScaleFactor(scaleFactor);
    send(Messages::RemoteDisplayListRecorder::ApplyDeviceScaleFactor(scaleFactor));
}

void RemoteDisplayListRecorderProxy::beginPage(const IntSize& pageSize)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::BeginPage(pageSize));
}

void RemoteDisplayListRecorderProxy::endPage()
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::EndPage());
}

void RemoteDisplayListRecorderProxy::setURLForRect(const URL& link, const FloatRect& destRect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::SetURLForRect(link, destRect));
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(NativeImage& image)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }

    auto colorSpace = image.colorSpace();

    if (image.headroom() > Headroom::None) {
#if ENABLE(PIXEL_FORMAT_RGBA16F) && USE(CG)
        // The image will be drawn to a Float16 layer, so use extended range sRGB
        // to preserve the HDR contents.
        if (m_contentsFormat && *m_contentsFormat == ContentsFormat::RGBA16F)
            colorSpace = DestinationColorSpace::ExtendedSRGB();
        else
#endif
#if PLATFORM(IOS_FAMILY)
            // iOS typically renders into extended range sRGB to preserve wide gamut colors, but we want
            // a non-extended range colorspace here so that the contents are tone mapped to SDR range.
            colorSpace = DestinationColorSpace::DisplayP3();
#else
            colorSpace = DestinationColorSpace::SRGB();
#endif
    }

    renderingBackend->remoteResourceCacheProxy().recordNativeImageUse(image, colorSpace);
    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(ImageBuffer& imageBuffer)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }
    return renderingBackend->isCached(imageBuffer);
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(const SourceImage& image)
{
    if (RefPtr imageBuffer = image.imageBufferIfExists())
        return recordResourceUse(*imageBuffer);

    if (RefPtr nativeImage = image.nativeImageIfExists())
        return recordResourceUse(*nativeImage);

    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(Font& font)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }

    renderingBackend->remoteResourceCacheProxy().recordFontUse(font);
    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(DecomposedGlyphs& decomposedGlyphs)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }

    renderingBackend->remoteResourceCacheProxy().recordDecomposedGlyphsUse(decomposedGlyphs);
    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(Gradient& gradient)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }

    renderingBackend->remoteResourceCacheProxy().recordGradientUse(gradient);
    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(Filter& filter)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }

    renderingBackend->remoteResourceCacheProxy().recordFilterUse(filter);
    return true;
}

RefPtr<ImageBuffer> RemoteDisplayListRecorderProxy::createImageBuffer(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, std::optional<RenderingMode> renderingMode, std::optional<RenderingMethod> renderingMethod, WebCore::ImageBufferPixelFormat pixelFormat) const
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (renderingMethod)
        return Recorder::createImageBuffer(size, resolutionScale, colorSpace, renderingMode, renderingMethod);

    // FIXME: Ideally we'd plumb the purpose through for callers of GraphicsContext::createImageBuffer().
    RenderingPurpose purpose = RenderingPurpose::Unspecified;
    return renderingBackend->createImageBuffer(size, renderingMode.value_or(this->renderingModeForCompatibleBuffer()), purpose, resolutionScale, colorSpace, pixelFormat);
}

RefPtr<ImageBuffer> RemoteDisplayListRecorderProxy::createAlignedImageBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, std::optional<RenderingMethod> renderingMethod) const
{
    auto renderingMode = !renderingMethod ? this->renderingModeForCompatibleBuffer() : RenderingMode::Unaccelerated;
    return GraphicsContext::createScaledImageBuffer(size, scaleFactor(), colorSpace, renderingMode, renderingMethod);
}

RefPtr<ImageBuffer> RemoteDisplayListRecorderProxy::createAlignedImageBuffer(const FloatRect& rect, const DestinationColorSpace& colorSpace, std::optional<RenderingMethod> renderingMethod) const
{
    auto renderingMode = !renderingMethod ? this->renderingModeForCompatibleBuffer() : RenderingMode::Unaccelerated;
    return GraphicsContext::createScaledImageBuffer(rect, scaleFactor(), colorSpace, renderingMode, renderingMethod);
}

void RemoteDisplayListRecorderProxy::appendStateChangeItemIfNecessary()
{
    auto& state = currentState().state;
    auto changes = state.changes();
    if (!changes)
        return;
    if (changes.contains(GraphicsContextState::Change::FillBrush)) {
        const auto& fillBrush = state.fillBrush();
        if (auto packedColor = fillBrush.packedColor())
            send(Messages::RemoteDisplayListRecorder::SetFillPackedColor(*packedColor));
        else if (RefPtr pattern = fillBrush.pattern()) {
            recordResourceUse(pattern->tileImage());
            send(Messages::RemoteDisplayListRecorder::SetFillPattern(pattern->tileImage().imageIdentifier(), pattern->parameters()));
        } else if (RefPtr gradient = fillBrush.gradient()) {
            if (gradient->hasValidRenderingResourceIdentifier()) {
                recordResourceUse(*gradient);
                send(Messages::RemoteDisplayListRecorder::SetFillCachedGradient(gradient->renderingResourceIdentifier(), fillBrush.gradientSpaceTransform()));
            } else
                send(Messages::RemoteDisplayListRecorder::SetFillGradient(*gradient, fillBrush.gradientSpaceTransform()));
        } else
            send(Messages::RemoteDisplayListRecorder::SetFillColor(fillBrush.color()));
    }
    if (changes.contains(GraphicsContextState::Change::StrokeBrush)) {
        const auto& strokeBrush = state.strokeBrush();
        if (auto packedColor = strokeBrush.packedColor()) {
            if (changes.contains(GraphicsContextState::Change::StrokeThickness)) {
                send(Messages::RemoteDisplayListRecorder::SetStrokePackedColorAndThickness(*packedColor, state.strokeThickness()));
                changes.remove(GraphicsContextState::Change::StrokeThickness);
            } else
                send(Messages::RemoteDisplayListRecorder::SetStrokePackedColor(*packedColor));
        } else if (RefPtr pattern = strokeBrush.pattern()) {
            recordResourceUse(pattern->tileImage());
            send(Messages::RemoteDisplayListRecorder::SetStrokePattern(pattern->tileImage().imageIdentifier(), pattern->parameters()));
        } else if (RefPtr gradient = strokeBrush.gradient()) {
            if (gradient->hasValidRenderingResourceIdentifier()) {
                recordResourceUse(*gradient);
                send(Messages::RemoteDisplayListRecorder::SetStrokeCachedGradient(gradient->renderingResourceIdentifier(), strokeBrush.gradientSpaceTransform()));
            } else
                send(Messages::RemoteDisplayListRecorder::SetStrokeGradient(*gradient, strokeBrush.gradientSpaceTransform()));
        } else
            send(Messages::RemoteDisplayListRecorder::SetStrokeColor(strokeBrush.color()));
    }
    if (changes.contains(GraphicsContextState::Change::FillRule))
        send(Messages::RemoteDisplayListRecorder::SetFillRule(state.fillRule()));
    if (changes.contains(GraphicsContextState::Change::StrokeThickness))
        send(Messages::RemoteDisplayListRecorder::SetStrokeThickness(state.strokeThickness()));
    if (changes.contains(GraphicsContextState::Change::StrokeStyle))
        send(Messages::RemoteDisplayListRecorder::SetStrokeStyle(state.strokeStyle()));
    if (changes.contains(GraphicsContextState::Change::CompositeMode))
        send(Messages::RemoteDisplayListRecorder::SetCompositeMode(state.compositeMode()));
    // Note: due to bugs in GraphicsContext interface and GraphicsContextCG, we have to send ShadowsIgnoreTransforms
    // before the DropShadow and Style.
    if (changes.contains(GraphicsContextState::Change::ShadowsIgnoreTransforms))
        send(Messages::RemoteDisplayListRecorder::SetShadowsIgnoreTransforms(state.shadowsIgnoreTransforms()));
    if (changes.contains(GraphicsContextState::Change::DropShadow))
        send(Messages::RemoteDisplayListRecorder::SetDropShadow(state.dropShadow()));
    if (changes.contains(GraphicsContextState::Change::Style))
        send(Messages::RemoteDisplayListRecorder::SetStyle(state.style()));
    if (changes.contains(GraphicsContextState::Change::Alpha))
        send(Messages::RemoteDisplayListRecorder::SetAlpha(state.alpha()));
    if (changes.contains(GraphicsContextState::Change::TextDrawingMode))
        send(Messages::RemoteDisplayListRecorder::SetTextDrawingMode(state.textDrawingMode()));
    if (changes.contains(GraphicsContextState::Change::ImageInterpolationQuality))
        send(Messages::RemoteDisplayListRecorder::SetImageInterpolationQuality(state.imageInterpolationQuality()));
    if (changes.contains(GraphicsContextState::Change::ShouldAntialias))
        send(Messages::RemoteDisplayListRecorder::SetShouldAntialias(state.shouldAntialias()));
    if (changes.contains(GraphicsContextState::Change::ShouldSmoothFonts))
        send(Messages::RemoteDisplayListRecorder::SetShouldSmoothFonts(state.shouldSmoothFonts()));
    if (changes.contains(GraphicsContextState::Change::ShouldSubpixelQuantizeFonts))
        send(Messages::RemoteDisplayListRecorder::SetShouldSubpixelQuantizeFonts(state.shouldSubpixelQuantizeFonts()));
    if (changes.contains(GraphicsContextState::Change::DrawLuminanceMask))
        send(Messages::RemoteDisplayListRecorder::SetDrawLuminanceMask(state.drawLuminanceMask()));

    state.didApplyChanges();
    currentState().lastDrawingState = state;
}

RemoteDisplayListRecorderProxy::InlineStrokeData RemoteDisplayListRecorderProxy::appendStateChangeItemForInlineStrokeIfNecessary()
{
    auto& state = currentState().state;
    auto changes = state.changes();
    if (!changes)
        return { };
    if (!changes.containsOnly({ GraphicsContextState::Change::StrokeBrush, GraphicsContextState::Change::StrokeThickness })) {
        appendStateChangeItemIfNecessary();
        return { };
    }
    auto& lastDrawingState = currentState().lastDrawingState;
    std::optional<PackedColor::RGBA> packedColor;
    if (changes.contains(GraphicsContextState::Change::StrokeBrush)) {
        packedColor = state.strokeBrush().packedColor();
        if (!packedColor) {
            appendStateChangeItemIfNecessary();
            return { };
        }
        if (!lastDrawingState)
            lastDrawingState = state;
        else {
            // Set through strokeBrush() to avoid comparison.
            lastDrawingState->strokeBrush().setColor(state.strokeBrush().color());
        }
    }
    std::optional<float> strokeThickness;
    if (changes.contains(GraphicsContextState::Change::StrokeThickness)) {
        strokeThickness = state.strokeThickness();
        if (!lastDrawingState)
            lastDrawingState = state;
        else
            lastDrawingState->setStrokeThickness(*strokeThickness);
    }
    state.didApplyChanges();
    lastDrawingState->didApplyChanges();
    return { packedColor, strokeThickness };
}

void RemoteDisplayListRecorderProxy::disconnect()
{
    m_connection = nullptr;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    Locker locker { m_sharedVideoFrameWriterLock };
    if (m_sharedVideoFrameWriter) {
        m_sharedVideoFrameWriter->disable();
        m_sharedVideoFrameWriter = nullptr;
    }
#endif
}

void RemoteDisplayListRecorderProxy::abandon()
{
    disconnect();
    m_renderingBackend = nullptr;
}

} // namespace WebCore

#endif // ENABLE(GPU_PROCESS)
