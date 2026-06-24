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
#include "RemoteGraphicsContextProxy.h"

#if ENABLE(GPU_PROCESS)

#include "Logging.h"
#include "RemoteGraphicsContextMessages.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteSnapshotRecorderMessages.h"
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
#include <WebCore/SVGFilterRenderer.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/TextStream.h>

#if USE(SYSTEM_PREVIEW)
#include <WebCore/ARKitBadgeSystemImage.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGraphicsContextProxy);

RemoteGraphicsContextProxy::RemoteGraphicsContextProxy(const DestinationColorSpace& colorSpace, RenderingMode renderingMode, const FloatRect& initialClip, const AffineTransform& initialCTM, RemoteRenderingBackendProxy& renderingBackend)
    : RemoteGraphicsContextProxy(colorSpace, std::nullopt, renderingMode, initialClip, initialCTM, DrawGlyphsMode::Deconstruct, RemoteGraphicsContextIdentifier::generate(), renderingBackend)
{
}

RemoteGraphicsContextProxy::RemoteGraphicsContextProxy(const DestinationColorSpace& colorSpace, WebCore::ContentsFormat contentsFormat, RenderingMode renderingMode, const FloatRect& initialClip, const AffineTransform& initialCTM, RemoteGraphicsContextIdentifier identifier, RemoteRenderingBackendProxy& renderingBackend)
    : RemoteGraphicsContextProxy(colorSpace, contentsFormat, renderingMode, initialClip, initialCTM, DrawGlyphsMode::Deconstruct, identifier, renderingBackend)
{
}

RemoteGraphicsContextProxy::RemoteGraphicsContextProxy(const DestinationColorSpace& colorSpace, std::optional<ContentsFormat> contentsFormat, RenderingMode renderingMode, const FloatRect& initialClip, const AffineTransform& initialCTM, DrawGlyphsMode drawGlyphsMode, RemoteGraphicsContextIdentifier identifier, RemoteRenderingBackendProxy& renderingBackend)
    : DisplayList::Recorder(IsDeferred::No, { }, initialClip, initialCTM, colorSpace, drawGlyphsMode)
    , m_renderingMode(renderingMode)
    , m_identifier(identifier)
    , m_renderingBackend(renderingBackend)
    , m_contentsFormat(contentsFormat)
{
}

RemoteGraphicsContextProxy::~RemoteGraphicsContextProxy() = default;

template<typename T>
ALWAYS_INLINE void RemoteGraphicsContextProxy::send(T&& message)
{
    // Buffered line strokes sit in front of the IPC stream; each sender is
    // responsible for calling sendPendingDrawsIfNecessary() before reaching
    // here (see its declaration). This catches a missed call site: the buffer
    // must be empty unless we are sending the batch message itself.
    ASSERT(m_pendingLineStrokes.isEmpty() || (std::is_same_v<std::decay_t<T>, Messages::RemoteGraphicsContext::StrokeLinesWithColorAndThickness>));
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
        RELEASE_LOG(RemoteLayerBuffers, "RemoteGraphicsContextProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            IPC::description(T::name()).characters(), IPC::errorAsString(result).characters());
        didBecomeUnresponsive();
    }
}

void RemoteGraphicsContextProxy::didBecomeUnresponsive() const
{
    RefPtr backend = m_renderingBackend.get();
    if (!backend) [[unlikely]]
        return;
    backend->didBecomeUnresponsive();
}

bool RemoteGraphicsContextProxy::knownToHaveFloatBasedBacking() const
{
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    return m_contentsFormat && *m_contentsFormat == ContentsFormat::RGBA16F;
#else
    return false;
#endif
}

RenderingMode RemoteGraphicsContextProxy::renderingMode() const
{
    return m_renderingMode;
}

void RemoteGraphicsContextProxy::save(GraphicsContextState::Purpose purpose)
{
    sendPendingDrawsIfNecessary();
    updateStateForSave(purpose);
    send(Messages::RemoteGraphicsContext::Save());
}

void RemoteGraphicsContextProxy::restore(GraphicsContextState::Purpose purpose)
{
    sendPendingDrawsIfNecessary();
    if (!updateStateForRestore(purpose))
        return;
    send(Messages::RemoteGraphicsContext::Restore());
}

void RemoteGraphicsContextProxy::translate(float x, float y)
{
    sendPendingDrawsIfNecessary();
    if (!updateStateForTranslate(x, y))
        return;
    send(Messages::RemoteGraphicsContext::Translate(x, y));
}

void RemoteGraphicsContextProxy::rotate(float angle)
{
    sendPendingDrawsIfNecessary();
    if (!updateStateForRotate(angle))
        return;
    send(Messages::RemoteGraphicsContext::Rotate(angle));
}

void RemoteGraphicsContextProxy::scale(const FloatSize& scale)
{
    sendPendingDrawsIfNecessary();
    if (!updateStateForScale(scale))
        return;
    send(Messages::RemoteGraphicsContext::Scale(scale));
}

void RemoteGraphicsContextProxy::setCTM(const AffineTransform& transform)
{
    sendPendingDrawsIfNecessary();
    updateStateForSetCTM(transform);
    send(Messages::RemoteGraphicsContext::SetCTM(transform));
}

void RemoteGraphicsContextProxy::concatCTM(const AffineTransform& transform)
{
    sendPendingDrawsIfNecessary();
    if (!updateStateForConcatCTM(transform))
        return;
    send(Messages::RemoteGraphicsContext::ConcatCTM(transform));
}

void RemoteGraphicsContextProxy::setLineCap(LineCap lineCap)
{
    sendPendingDrawsIfNecessary();
    send(Messages::RemoteGraphicsContext::SetLineCap(lineCap));
}

void RemoteGraphicsContextProxy::setLineDash(const DashArray& array, float dashOffset)
{
    sendPendingDrawsIfNecessary();
    send(Messages::RemoteGraphicsContext::SetLineDash(FixedVector<double>(array.span()), dashOffset));
}

void RemoteGraphicsContextProxy::setLineJoin(LineJoin lineJoin)
{
    sendPendingDrawsIfNecessary();
    send(Messages::RemoteGraphicsContext::SetLineJoin(lineJoin));
}

void RemoteGraphicsContextProxy::setMiterLimit(float limit)
{
    sendPendingDrawsIfNecessary();
    send(Messages::RemoteGraphicsContext::SetMiterLimit(limit));
}

void RemoteGraphicsContextProxy::clip(const FloatRect& rect)
{
    sendPendingDrawsIfNecessary();
    updateStateForClip(rect);
    send(Messages::RemoteGraphicsContext::Clip(rect));
}

void RemoteGraphicsContextProxy::clipRoundedRect(const FloatRoundedRect& rect)
{
    sendPendingDrawsIfNecessary();
    updateStateForClipRoundedRect(rect);
    send(Messages::RemoteGraphicsContext::ClipRoundedRect(rect));
}

void RemoteGraphicsContextProxy::clipOut(const FloatRect& rect)
{
    sendPendingDrawsIfNecessary();
    updateStateForClipOut(rect);
    send(Messages::RemoteGraphicsContext::ClipOut(rect));
}

void RemoteGraphicsContextProxy::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    sendPendingDrawsIfNecessary();
    updateStateForClipOutRoundedRect(rect);
    send(Messages::RemoteGraphicsContext::ClipOutRoundedRect(rect));
}

void RemoteGraphicsContextProxy::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destinationRect)
{
    sendPendingDrawsIfNecessary();
    updateStateForClipToImageBuffer(destinationRect);
    if (!recordResourceUse(imageBuffer))
        return;
    send(Messages::RemoteGraphicsContext::ClipToImageBuffer(imageBuffer.renderingResourceIdentifier(), destinationRect));
}

void RemoteGraphicsContextProxy::clipOut(const Path& path)
{
    sendPendingDrawsIfNecessary();
    updateStateForClipOut(path);
    send(Messages::RemoteGraphicsContext::ClipOutToPath(path));
}

void RemoteGraphicsContextProxy::clipPath(const Path& path, WindRule rule)
{
    sendPendingDrawsIfNecessary();
    updateStateForClipPath(path);
    if (RefPtr impl = path.asImpl(); impl && !impl->isTransient()) {
        if (auto identifier = recordResourceUse(*impl)) {
            send(Messages::RemoteGraphicsContext::ClipCachedPath(*identifier, rule));
            return;
        }
    }

    send(Messages::RemoteGraphicsContext::ClipPath(path, rule));
}

void RemoteGraphicsContextProxy::resetClip()
{
    sendPendingDrawsIfNecessary();
    updateStateForResetClip();
    send(Messages::RemoteGraphicsContext::ResetClip());
    clip(initialClip());
}

void RemoteGraphicsContextProxy::drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();

    for (auto& effect : filter.effectsOfType(FilterEffect::Type::FEImage)) {
        Ref feImage = downcast<FEImage>(effect.get());
        if (!recordResourceUse(feImage->sourceImage())) {
            GraphicsContext::drawFilteredImageBuffer(sourceImage, sourceImageRect, filter, results);
            return;
        }
    }

    RefPtr svgFilter = dynamicDowncast<SVGFilterRenderer>(filter);
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

    send(Messages::RemoteGraphicsContext::DrawFilteredImageBuffer(WTF::move(identifier), sourceImageRect, filter));
}

void RemoteGraphicsContextProxy::drawGlyphs(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    if (decomposeDrawGlyphsIfNeeded(font, glyphs, advances, localAnchor, smoothingMode))
        return;
    drawGlyphsImmediate(font, glyphs, advances, localAnchor, smoothingMode);
}

void RemoteGraphicsContextProxy::drawGlyphsImmediate(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    ASSERT(glyphs.size() == advances.size());
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));
    send(Messages::RemoteGraphicsContext::DrawGlyphs(font.renderingResourceIdentifier(), { glyphs.data(), Vector<FloatSize>(advances).span().data(), glyphs.size() }, localAnchor, smoothingMode));
}

void RemoteGraphicsContextProxy::drawDisplayList(const DisplayList::DisplayList& displayList, ControlFactory&)
{
    auto identifier = recordResourceUse(displayList);
    if (!identifier)
        return;
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawDisplayList(*identifier));
}

void RemoteGraphicsContextProxy::drawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();

    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawImageBuffer(imageBuffer, destRect, srcRect, options);
        return;
    }

    send(Messages::RemoteGraphicsContext::DrawImageBuffer(imageBuffer.renderingResourceIdentifier(), destRect, srcRect, options));
}

void RemoteGraphicsContextProxy::drawNativeImage(const NativeImage& image, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
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
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    if (!recordResourceUse(const_cast<NativeImage&>(image)))
        return;
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    send(Messages::RemoteGraphicsContext::DrawNativeImage(image.renderingResourceIdentifier(), destRect, srcRect, clampedOptions));
#else
    send(Messages::RemoteGraphicsContext::DrawNativeImage(image.renderingResourceIdentifier(), destRect, srcRect, options));
#endif
}

void RemoteGraphicsContextProxy::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
#if USE(SYSTEM_PREVIEW)
    if (RefPtr badgeSystemImage = dynamicDowncast<ARKitBadgeSystemImage>(systemImage)) {
        if (RefPtr image = badgeSystemImage->image()) {
            auto nativeImage = image->nativeImage();
            if (!nativeImage)
                return;
            if (!recordResourceUse(*nativeImage))
                return;
        }
    }
#endif
    send(Messages::RemoteGraphicsContext::DrawSystemImage(systemImage, destinationRect));
}

void RemoteGraphicsContextProxy::drawPattern(const NativeImage& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    if (!recordResourceUse(const_cast<NativeImage&>(image)))
        return;
    send(Messages::RemoteGraphicsContext::DrawPatternNativeImage(image.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options));
}

void RemoteGraphicsContextProxy::drawPattern(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawPattern(imageBuffer, destRect, tileRect, patternTransform, phase, spacing, options);
        return;
    }

    send(Messages::RemoteGraphicsContext::DrawPatternImageBuffer(imageBuffer.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options));
}

void RemoteGraphicsContextProxy::beginTransparencyLayer(float opacity)
{
    sendPendingDrawsIfNecessary();
    updateStateForBeginTransparencyLayer(opacity);
    send(Messages::RemoteGraphicsContext::BeginTransparencyLayer(opacity));
}

void RemoteGraphicsContextProxy::beginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    sendPendingDrawsIfNecessary();
    updateStateForBeginTransparencyLayer(compositeOperator, blendMode);
    send(Messages::RemoteGraphicsContext::BeginTransparencyLayerWithCompositeMode({ compositeOperator, blendMode }));
}

void RemoteGraphicsContextProxy::endTransparencyLayer()
{
    sendPendingDrawsIfNecessary();
    if (updateStateForEndTransparencyLayer())
        send(Messages::RemoteGraphicsContext::EndTransparencyLayer());
}

void RemoteGraphicsContextProxy::drawRect(const FloatRect& rect, float width)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawRect(rect, width));
}

void RemoteGraphicsContextProxy::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawLine(point1, point2));
}

void RemoteGraphicsContextProxy::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle style)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawLinesForText(point, thickness, lineSegments, printing, doubleLines, style));
}

void RemoteGraphicsContextProxy::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawDotsForDocumentMarker(rect, style));
}

void RemoteGraphicsContextProxy::drawEllipse(const FloatRect& rect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawEllipse(rect));
}

void RemoteGraphicsContextProxy::drawPath(const Path& path)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawPath(path));
}

void RemoteGraphicsContextProxy::drawFocusRing(const Path& path, float outlineWidth, const Color& color, float zoomFactor)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawFocusRingPath(path, outlineWidth, color, zoomFactor));
}

void RemoteGraphicsContextProxy::drawFocusRing(const Vector<FloatRect>& rects, float outlineWidth, const Color& color, float zoomFactor)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawFocusRingRects(rects, outlineWidth, color, zoomFactor));
}

void RemoteGraphicsContextProxy::fillPath(const Path& path)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();

    if (auto segment = path.singleSegment()) {
        WTF::switchOn(segment->data(),
        [&](const PathArc &arc) {
            send(Messages::RemoteGraphicsContext::FillArc(arc));
        },
        [&](const PathClosedArc& closedArc) {
            send(Messages::RemoteGraphicsContext::FillClosedArc(closedArc));
        },
        [&](const PathDataLine& line) {
            send(Messages::RemoteGraphicsContext::FillLine(line));
        },
        [&](const PathDataQuadCurve& curve) {
            send(Messages::RemoteGraphicsContext::FillQuadCurve(curve));
        },
        [&](const PathDataBezierCurve& curve) {
            send(Messages::RemoteGraphicsContext::FillBezierCurve(curve));
        },
        [&](auto&&) {
            send(Messages::RemoteGraphicsContext::FillPathSegment(*segment));
        });
        return;
    }

    if (RefPtr impl = path.asImpl(); impl && !impl->isTransient()) {
        if (auto identifier = recordResourceUse(*impl)) {
            send(Messages::RemoteGraphicsContext::FillCachedPath(*identifier));
            return;
        }
    }

    send(Messages::RemoteGraphicsContext::FillPath(path));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRect(rect, requiresClipToRect));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, const Color& color)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRectWithColor(rect, color));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, Gradient& gradient)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRectWithGradient(rect, gradient));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRectWithGradientAndSpaceTransform(rect, gradient, gradientSpaceTransform, requiresClipToRect));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode mode)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillCompositedRect(rect, color, op, mode));
}

void RemoteGraphicsContextProxy::fillRoundedRect(const FloatRoundedRect& roundedRect, const Color& color, BlendMode mode)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRoundedRect(roundedRect, color, mode));
}

void RemoteGraphicsContextProxy::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedRect, const Color& color)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRectWithRoundedHole(rect, roundedRect, color));
}

void RemoteGraphicsContextProxy::fillEllipse(const FloatRect& rect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillEllipse(rect));
}

#if ENABLE(VIDEO)
void RemoteGraphicsContextProxy::drawVideoFrame(const VideoFrame& frame, const FloatRect& destination, ImageOrientation orientation, bool shouldDiscardAlpha)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
#if PLATFORM(COCOA)
    Locker locker { m_sharedVideoFrameWriterLock };
    if (!m_sharedVideoFrameWriter)
        m_sharedVideoFrameWriter = makeUnique<SharedVideoFrameWriter>();

    auto sharedVideoFrame = m_sharedVideoFrameWriter->write(frame, [&](auto& semaphore) {
        send(Messages::RemoteGraphicsContext::SetSharedVideoFrameSemaphore { semaphore });
    }, [&](SharedMemory::Handle&& handle) {
        send(Messages::RemoteGraphicsContext::SetSharedVideoFrameMemory { WTF::move(handle) });
    });
    if (!sharedVideoFrame)
        return;
    send(Messages::RemoteGraphicsContext::DrawVideoFrame(WTF::move(*sharedVideoFrame), destination, orientation, shouldDiscardAlpha));
#endif
}
#endif

void RemoteGraphicsContextProxy::strokePath(const Path& path)
{
    if (const auto* segment = path.singleSegmentIfExists()) {
        if (const auto* line = std::get_if<PathDataLine>(&segment->data())) {
            if (auto inlineStroke = inlineStrokeStateIfBatchable()) {
                bufferLine(*line, *inlineStroke);
                return;
            }
            // Non-stroke state is pending, or this is a gradient/pattern stroke:
            // emit the batch so far and the new state, then draw the line alone.
            sendPendingDrawsIfNecessary();
            appendStateChangeItemIfNecessary();
            send(Messages::RemoteGraphicsContext::StrokeLine(*line));
            return;
        }
        sendPendingDrawsIfNecessary();
        appendStateChangeItemIfNecessary();
        WTF::switchOn(segment->data(),
            [&](const PathArc &arc) {
                send(Messages::RemoteGraphicsContext::StrokeArc(arc));
            },
            [&](const PathClosedArc& closedArc) {
                send(Messages::RemoteGraphicsContext::StrokeClosedArc(closedArc));
            },
            [&](const PathDataLine& line) {
                send(Messages::RemoteGraphicsContext::StrokeLine(line));
            },
            [&](const PathDataQuadCurve& curve) {
                send(Messages::RemoteGraphicsContext::StrokeQuadCurve(curve));
            },
            [&](const PathDataBezierCurve& curve) {
                send(Messages::RemoteGraphicsContext::StrokeBezierCurve(curve));
            },
            [&](auto&&) {
                send(Messages::RemoteGraphicsContext::StrokePathSegment(*segment));
            });
        return;
    }
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::StrokePath(path));
}

void RemoteGraphicsContextProxy::strokeRect(const FloatRect& rect, float width)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::StrokeRect(rect, width));
}

void RemoteGraphicsContextProxy::strokeEllipse(const FloatRect& rect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::StrokeEllipse(rect));
}

void RemoteGraphicsContextProxy::bufferLine(const PathDataLine& line, const InlineStrokeData& strokeData)
{
    if (!m_pendingLineStrokes.capacity())
        m_pendingLineStrokes.reserveInitialCapacity(maxPendingLineStrokes);

    m_pendingLineStrokes.append({ line, strokeData.color, strokeData.thickness });

    if (m_pendingLineStrokes.size() >= maxPendingLineStrokes)
        sendPendingDraws();
}

void RemoteGraphicsContextProxy::sendPendingDraws()
{
    if (m_pendingLineStrokes.isEmpty())
        return;

    send(Messages::RemoteGraphicsContext::StrokeLinesWithColorAndThickness(m_pendingLineStrokes.span()));

    m_pendingLineStrokes.shrink(0);
}

void RemoteGraphicsContextProxy::clearRect(const FloatRect& rect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::ClearRect(rect));
}

void RemoteGraphicsContextProxy::drawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawControlPart(part, borderRect, deviceScaleFactor, style));
}

#if USE(CG)

void RemoteGraphicsContextProxy::applyStrokePattern()
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::ApplyStrokePattern());
}

void RemoteGraphicsContextProxy::applyFillPattern()
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::ApplyFillPattern());
}

#endif // USE(CG)

void RemoteGraphicsContextProxy::applyDeviceScaleFactor(float scaleFactor)
{
    sendPendingDrawsIfNecessary();
    updateStateForApplyDeviceScaleFactor(scaleFactor);
    send(Messages::RemoteGraphicsContext::ApplyDeviceScaleFactor(scaleFactor));
}

void RemoteGraphicsContextProxy::beginPage(const FloatRect& pageRect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::BeginPage(pageRect));
}

void RemoteGraphicsContextProxy::endPage()
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::EndPage());
}

void RemoteGraphicsContextProxy::setURLForRect(const URL& link, const FloatRect& destRect)
{
    sendPendingDrawsIfNecessary();
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::SetURLForRect(link, destRect));
}

bool RemoteGraphicsContextProxy::recordResourceUse(const NativeImage& image)
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

    return renderingBackend->remoteResourceCacheProxy().recordNativeImageUse(image, colorSpace);
}

bool RemoteGraphicsContextProxy::recordResourceUse(ImageBuffer& imageBuffer)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }
    RefPtr cachedImageBuffer = renderingBackend->cachedImageBuffer(imageBuffer);
    if (!cachedImageBuffer)
        return false;
    // This draw consumes the source buffer's contents, so its buffered line
    // strokes must reach the GPU process first.
    cachedImageBuffer->sendPendingDrawsIfNecessary();
    return true;
}

bool RemoteGraphicsContextProxy::recordResourceUse(const SourceImage& image)
{
    if (RefPtr imageBuffer = image.imageBufferIfExists())
        return recordResourceUse(*imageBuffer);

    if (RefPtr nativeImage = image.nativeImageIfExists())
        return recordResourceUse(*nativeImage);

    return true;
}

bool RemoteGraphicsContextProxy::recordResourceUse(Font& font)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }

    renderingBackend->remoteResourceCacheProxy().recordFontUse(font);
    return true;
}

std::optional<RemoteGradientIdentifier> RemoteGraphicsContextProxy::recordResourceUse(Gradient& gradient)
{
    if (gradient.isTransient())
        return std::nullopt;
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return renderingBackend->remoteResourceCacheProxy().recordGradientUse(gradient);
}

std::optional<RemotePathImplIdentifier> RemoteGraphicsContextProxy::recordResourceUse(const WebCore::PathImpl& path)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return renderingBackend->remoteResourceCacheProxy().recordPathImplUse(path);
}

bool RemoteGraphicsContextProxy::recordResourceUse(Filter& filter)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return false;
    }

    renderingBackend->remoteResourceCacheProxy().recordFilterUse(filter);
    return true;
}

std::optional<RemoteDisplayListIdentifier> RemoteGraphicsContextProxy::recordResourceUse(const DisplayList::DisplayList& displayList)
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return { };
    }
    return renderingBackend->remoteResourceCacheProxy().recordDisplayListUse(displayList);
}

RefPtr<ImageBuffer> RemoteGraphicsContextProxy::createImageBuffer(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, std::optional<RenderingMode> renderingMode, std::optional<RenderingMethod> renderingMethod, WebCore::ImageBufferFormat pixelFormat) const
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

RefPtr<ImageBuffer> RemoteGraphicsContextProxy::createAlignedImageBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, std::optional<RenderingMethod> renderingMethod) const
{
    auto renderingMode = !renderingMethod ? this->renderingModeForCompatibleBuffer() : RenderingMode::Unaccelerated;
    return GraphicsContext::createScaledImageBuffer(size, scaleFactor(), colorSpace, renderingMode, renderingMethod);
}

RefPtr<ImageBuffer> RemoteGraphicsContextProxy::createAlignedImageBuffer(const FloatRect& rect, const DestinationColorSpace& colorSpace, std::optional<RenderingMethod> renderingMethod) const
{
    auto renderingMode = !renderingMethod ? this->renderingModeForCompatibleBuffer() : RenderingMode::Unaccelerated;
    return GraphicsContext::createScaledImageBuffer(rect, scaleFactor(), colorSpace, renderingMode, renderingMethod);
}

void RemoteGraphicsContextProxy::appendStateChangeItemIfNecessary()
{
    auto& state = currentState().state;
    auto changes = state.changes();
    if (!changes)
        return;
    if (changes.contains(GraphicsContextState::Change::FillBrush)) {
        const auto& fillBrush = state.fillBrush();
        if (auto packedColor = fillBrush.packedColor())
            send(Messages::RemoteGraphicsContext::SetFillPackedColor(*packedColor));
        else if (RefPtr pattern = fillBrush.pattern()) {
            if (RefPtr image = pattern->tileNativeImage()) {
                if (recordResourceUse(*image))
                    send(Messages::RemoteGraphicsContext::SetFillPatternNativeImage(image->renderingResourceIdentifier(), pattern->parameters()));
            } else if (RefPtr buffer = pattern->tileImageBuffer()) {
                if (recordResourceUse(*buffer))
                    send(Messages::RemoteGraphicsContext::SetFillPatternImageBuffer(buffer->renderingResourceIdentifier(), pattern->parameters()));
            }
        } else if (RefPtr gradient = fillBrush.gradient()) {
            if (auto identifier = recordResourceUse(*gradient))
                send(Messages::RemoteGraphicsContext::SetFillCachedGradient(*identifier, fillBrush.gradientSpaceTransform()));
            else
                send(Messages::RemoteGraphicsContext::SetFillGradient(*gradient, fillBrush.gradientSpaceTransform()));
        } else
            send(Messages::RemoteGraphicsContext::SetFillColor(fillBrush.color()));
    }
    if (changes.contains(GraphicsContextState::Change::StrokeBrush)) {
        const auto& strokeBrush = state.strokeBrush();
        if (auto packedColor = strokeBrush.packedColor()) {
            if (changes.contains(GraphicsContextState::Change::StrokeThickness)) {
                send(Messages::RemoteGraphicsContext::SetStrokePackedColorAndThickness(*packedColor, state.strokeThickness()));
                changes.remove(GraphicsContextState::Change::StrokeThickness);
            } else
                send(Messages::RemoteGraphicsContext::SetStrokePackedColor(*packedColor));
        } else if (RefPtr pattern = strokeBrush.pattern()) {
            if (RefPtr image = pattern->tileNativeImage()) {
                if (recordResourceUse(*image))
                    send(Messages::RemoteGraphicsContext::SetStrokePatternNativeImage(image->renderingResourceIdentifier(), pattern->parameters()));
            } else if (RefPtr buffer = pattern->tileImageBuffer()) {
                if (recordResourceUse(*buffer))
                    send(Messages::RemoteGraphicsContext::SetStrokePatternImageBuffer(buffer->renderingResourceIdentifier(), pattern->parameters()));
            }
        } else if (RefPtr gradient = strokeBrush.gradient()) {
            if (auto identifier = recordResourceUse(*gradient))
                send(Messages::RemoteGraphicsContext::SetStrokeCachedGradient(*identifier, strokeBrush.gradientSpaceTransform()));
            else
                send(Messages::RemoteGraphicsContext::SetStrokeGradient(*gradient, strokeBrush.gradientSpaceTransform()));
        } else
            send(Messages::RemoteGraphicsContext::SetStrokeColor(strokeBrush.color()));
    }
    if (changes.contains(GraphicsContextState::Change::FillRule))
        send(Messages::RemoteGraphicsContext::SetFillRule(state.fillRule()));
    if (changes.contains(GraphicsContextState::Change::StrokeThickness))
        send(Messages::RemoteGraphicsContext::SetStrokeThickness(state.strokeThickness()));
    if (changes.contains(GraphicsContextState::Change::StrokeStyle))
        send(Messages::RemoteGraphicsContext::SetStrokeStyle(state.strokeStyle()));
    if (changes.contains(GraphicsContextState::Change::CompositeMode))
        send(Messages::RemoteGraphicsContext::SetCompositeMode(state.compositeMode()));
    // Note: due to bugs in GraphicsContext interface and GraphicsContextCG, we have to send ShadowsIgnoreTransforms
    // before the DropShadow and Style.
    if (changes.contains(GraphicsContextState::Change::ShadowsIgnoreTransforms))
        send(Messages::RemoteGraphicsContext::SetShadowsIgnoreTransforms(state.shadowsIgnoreTransforms()));
    if (changes.contains(GraphicsContextState::Change::DropShadow))
        send(Messages::RemoteGraphicsContext::SetDropShadow(state.dropShadow()));
    if (changes.contains(GraphicsContextState::Change::Style))
        send(Messages::RemoteGraphicsContext::SetStyle(state.style()));
    if (changes.contains(GraphicsContextState::Change::Alpha))
        send(Messages::RemoteGraphicsContext::SetAlpha(state.alpha()));
    if (changes.contains(GraphicsContextState::Change::TextDrawingMode))
        send(Messages::RemoteGraphicsContext::SetTextDrawingMode(state.textDrawingMode()));
    if (changes.contains(GraphicsContextState::Change::ImageInterpolationQuality))
        send(Messages::RemoteGraphicsContext::SetImageInterpolationQuality(state.imageInterpolationQuality()));
    if (changes.contains(GraphicsContextState::Change::ShouldAntialias))
        send(Messages::RemoteGraphicsContext::SetShouldAntialias(state.shouldAntialias()));
    if (changes.contains(GraphicsContextState::Change::ShouldSmoothFonts))
        send(Messages::RemoteGraphicsContext::SetShouldSmoothFonts(state.shouldSmoothFonts()));
    if (changes.contains(GraphicsContextState::Change::ShouldSubpixelQuantizeFonts))
        send(Messages::RemoteGraphicsContext::SetShouldSubpixelQuantizeFonts(state.shouldSubpixelQuantizeFonts()));
    if (changes.contains(GraphicsContextState::Change::DrawLuminanceMask))
        send(Messages::RemoteGraphicsContext::SetDrawLuminanceMask(state.drawLuminanceMask()));

    state.didApplyChanges();
    currentState().lastDrawingState = state;
}

std::optional<RemoteGraphicsContextProxy::InlineStrokeData> RemoteGraphicsContextProxy::inlineStrokeStateIfBatchable()
{
    auto& state = currentState().state;
    auto changes = state.changes();
    // Only fold the line into the batch if the sole pending state changes are
    // stroke color and/or thickness; anything else has to go through the normal
    // state-flush path.
    if (changes && !changes.containsOnly({ GraphicsContextState::Change::StrokeBrush, GraphicsContextState::Change::StrokeThickness }))
        return std::nullopt;

    auto packedColor = state.strokeBrush().packedColor();
    if (!packedColor)
        return std::nullopt; // Gradient/pattern stroke: not representable inline.

    if (changes) {
        // The stroke color/thickness travel inline with each buffered line, so
        // mark them applied and keep lastDrawingState in sync for future diffs.
        auto& lastDrawingState = currentState().lastDrawingState;
        if (!lastDrawingState)
            lastDrawingState = state;
        else {
            if (changes.contains(GraphicsContextState::Change::StrokeBrush)) {
                // Set through strokeBrush() to avoid comparison.
                lastDrawingState->strokeBrush().setColor(state.strokeBrush().color());
            }
            if (changes.contains(GraphicsContextState::Change::StrokeThickness))
                lastDrawingState->setStrokeThickness(state.strokeThickness());
        }
        state.didApplyChanges();
        lastDrawingState->didApplyChanges();
    }
    return InlineStrokeData { *packedColor, state.strokeThickness() };
}

void RemoteGraphicsContextProxy::disconnect()
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

void RemoteGraphicsContextProxy::abandon()
{
    disconnect();
    m_renderingBackend = nullptr;
}

// Instantiate the send() helper for the few subclass messages here to avoid listing it in the header.
template void RemoteGraphicsContextProxy::send<Messages::RemoteSnapshotRecorder::DrawSnapshotFrame>(Messages::RemoteSnapshotRecorder::DrawSnapshotFrame&&);

}

#endif
