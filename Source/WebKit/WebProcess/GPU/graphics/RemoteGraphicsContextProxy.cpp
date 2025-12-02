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
    updateStateForSave(purpose);
    send(Messages::RemoteGraphicsContext::Save());
}

void RemoteGraphicsContextProxy::restore(GraphicsContextState::Purpose purpose)
{
    if (!updateStateForRestore(purpose))
        return;
    send(Messages::RemoteGraphicsContext::Restore());
}

void RemoteGraphicsContextProxy::translate(float x, float y)
{
    if (!updateStateForTranslate(x, y))
        return;
    send(Messages::RemoteGraphicsContext::Translate(x, y));
}

void RemoteGraphicsContextProxy::rotate(float angle)
{
    if (!updateStateForRotate(angle))
        return;
    send(Messages::RemoteGraphicsContext::Rotate(angle));
}

void RemoteGraphicsContextProxy::scale(const FloatSize& scale)
{
    if (!updateStateForScale(scale))
        return;
    send(Messages::RemoteGraphicsContext::Scale(scale));
}

void RemoteGraphicsContextProxy::setCTM(const AffineTransform& transform)
{
    updateStateForSetCTM(transform);
    send(Messages::RemoteGraphicsContext::SetCTM(transform));
}

void RemoteGraphicsContextProxy::concatCTM(const AffineTransform& transform)
{
    if (!updateStateForConcatCTM(transform))
        return;
    send(Messages::RemoteGraphicsContext::ConcatCTM(transform));
}

void RemoteGraphicsContextProxy::setLineCap(LineCap lineCap)
{
    send(Messages::RemoteGraphicsContext::SetLineCap(lineCap));
}

void RemoteGraphicsContextProxy::setLineDash(const DashArray& array, float dashOffset)
{
    send(Messages::RemoteGraphicsContext::SetLineDash(FixedVector<double>(array.span()), dashOffset));
}

void RemoteGraphicsContextProxy::setLineJoin(LineJoin lineJoin)
{
    send(Messages::RemoteGraphicsContext::SetLineJoin(lineJoin));
}

void RemoteGraphicsContextProxy::setMiterLimit(float limit)
{
    send(Messages::RemoteGraphicsContext::SetMiterLimit(limit));
}

void RemoteGraphicsContextProxy::clip(const FloatRect& rect)
{
    updateStateForClip(rect);
    send(Messages::RemoteGraphicsContext::Clip(rect));
}

void RemoteGraphicsContextProxy::clipRoundedRect(const FloatRoundedRect& rect)
{
    updateStateForClipRoundedRect(rect);
    send(Messages::RemoteGraphicsContext::ClipRoundedRect(rect));
}

void RemoteGraphicsContextProxy::clipOut(const FloatRect& rect)
{
    updateStateForClipOut(rect);
    send(Messages::RemoteGraphicsContext::ClipOut(rect));
}

void RemoteGraphicsContextProxy::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    updateStateForClipOutRoundedRect(rect);
    send(Messages::RemoteGraphicsContext::ClipOutRoundedRect(rect));
}

void RemoteGraphicsContextProxy::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destinationRect)
{
    updateStateForClipToImageBuffer(destinationRect);
    if (!recordResourceUse(imageBuffer))
        return;
    send(Messages::RemoteGraphicsContext::ClipToImageBuffer(imageBuffer.renderingResourceIdentifier(), destinationRect));
}

void RemoteGraphicsContextProxy::clipOut(const Path& path)
{
    updateStateForClipOut(path);
    send(Messages::RemoteGraphicsContext::ClipOutToPath(path));
}

void RemoteGraphicsContextProxy::clipPath(const Path& path, WindRule rule)
{
    updateStateForClipPath(path);
    send(Messages::RemoteGraphicsContext::ClipPath(path, rule));
}

void RemoteGraphicsContextProxy::resetClip()
{
    updateStateForResetClip();
    send(Messages::RemoteGraphicsContext::ResetClip());
    clip(initialClip());
}

void RemoteGraphicsContextProxy::drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
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

    send(Messages::RemoteGraphicsContext::DrawFilteredImageBuffer(WTFMove(identifier), sourceImageRect, filter));
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
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));
    send(Messages::RemoteGraphicsContext::DrawGlyphs(font.renderingResourceIdentifier(), { glyphs.data(), Vector<FloatSize>(advances).span().data(), glyphs.size() }, localAnchor, smoothingMode));
}

void RemoteGraphicsContextProxy::drawDisplayList(const DisplayList::DisplayList& displayList, ControlFactory&)
{
    auto identifier = recordResourceUse(displayList);
    if (!identifier)
        return;
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawDisplayList(*identifier));
}

void RemoteGraphicsContextProxy::drawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();

    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawImageBuffer(imageBuffer, destRect, srcRect, options);
        return;
    }

    send(Messages::RemoteGraphicsContext::DrawImageBuffer(imageBuffer.renderingResourceIdentifier(), destRect, srcRect, options));
}

void RemoteGraphicsContextProxy::drawNativeImage(NativeImage& image, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
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
    if (!recordResourceUse(image))
        return;
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    send(Messages::RemoteGraphicsContext::DrawNativeImage(image.renderingResourceIdentifier(), destRect, srcRect, clampedOptions));
#else
    send(Messages::RemoteGraphicsContext::DrawNativeImage(image.renderingResourceIdentifier(), destRect, srcRect, options));
#endif
}

void RemoteGraphicsContextProxy::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    appendStateChangeItemIfNecessary();
#if USE(SYSTEM_PREVIEW)
    if (auto* badgeSystemImage = dynamicDowncast<ARKitBadgeSystemImage>(systemImage)) {
        if (auto image = badgeSystemImage->image()) {
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

void RemoteGraphicsContextProxy::drawPattern(NativeImage& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    if (!recordResourceUse(image))
        return;
    send(Messages::RemoteGraphicsContext::DrawPatternNativeImage(image.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options));
}

void RemoteGraphicsContextProxy::drawPattern(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawPattern(imageBuffer, destRect, tileRect, patternTransform, phase, spacing, options);
        return;
    }

    send(Messages::RemoteGraphicsContext::DrawPatternImageBuffer(imageBuffer.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options));
}

void RemoteGraphicsContextProxy::beginTransparencyLayer(float opacity)
{
    updateStateForBeginTransparencyLayer(opacity);
    send(Messages::RemoteGraphicsContext::BeginTransparencyLayer(opacity));
}

void RemoteGraphicsContextProxy::beginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    updateStateForBeginTransparencyLayer(compositeOperator, blendMode);
    send(Messages::RemoteGraphicsContext::BeginTransparencyLayerWithCompositeMode({ compositeOperator, blendMode }));
}

void RemoteGraphicsContextProxy::endTransparencyLayer()
{
    if (updateStateForEndTransparencyLayer())
        send(Messages::RemoteGraphicsContext::EndTransparencyLayer());
}

void RemoteGraphicsContextProxy::drawRect(const FloatRect& rect, float width)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawRect(rect, width));
}

void RemoteGraphicsContextProxy::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawLine(point1, point2));
}

void RemoteGraphicsContextProxy::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle style)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawLinesForText(point, thickness, lineSegments, printing, doubleLines, style));
}

void RemoteGraphicsContextProxy::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawDotsForDocumentMarker(rect, style));
}

void RemoteGraphicsContextProxy::drawEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawEllipse(rect));
}

void RemoteGraphicsContextProxy::drawPath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawPath(path));
}

void RemoteGraphicsContextProxy::drawFocusRing(const Path& path, float outlineWidth, const Color& color)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawFocusRingPath(path, outlineWidth, color));
}

void RemoteGraphicsContextProxy::drawFocusRing(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawFocusRingRects(rects, outlineOffset, outlineWidth, color));
}

void RemoteGraphicsContextProxy::fillPath(const Path& path)
{
    appendStateChangeItemIfNecessary();

    if (auto segment = path.singleSegment()) {
        WTF::switchOn(segment->data(),
#if ENABLE(INLINE_PATH_DATA)
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
#endif
        [&](auto&&) {
            send(Messages::RemoteGraphicsContext::FillPathSegment(*segment));
        });
        return;
    }

    send(Messages::RemoteGraphicsContext::FillPath(path));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRect(rect, requiresClipToRect));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRectWithColor(rect, color));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, Gradient& gradient)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRectWithGradient(rect, gradient));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRectWithGradientAndSpaceTransform(rect, gradient, gradientSpaceTransform, requiresClipToRect));
}

void RemoteGraphicsContextProxy::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode mode)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillCompositedRect(rect, color, op, mode));
}

void RemoteGraphicsContextProxy::fillRoundedRect(const FloatRoundedRect& roundedRect, const Color& color, BlendMode mode)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRoundedRect(roundedRect, color, mode));
}

void RemoteGraphicsContextProxy::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedRect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillRectWithRoundedHole(rect, roundedRect, color));
}

void RemoteGraphicsContextProxy::fillEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::FillEllipse(rect));
}

#if ENABLE(VIDEO)
void RemoteGraphicsContextProxy::drawVideoFrame(const VideoFrame& frame, const FloatRect& destination, ImageOrientation orientation, bool shouldDiscardAlpha)
{
    appendStateChangeItemIfNecessary();
#if PLATFORM(COCOA)
    Locker locker { m_sharedVideoFrameWriterLock };
    if (!m_sharedVideoFrameWriter)
        m_sharedVideoFrameWriter = makeUnique<SharedVideoFrameWriter>();

    auto sharedVideoFrame = m_sharedVideoFrameWriter->write(frame, [&](auto& semaphore) {
        send(Messages::RemoteGraphicsContext::SetSharedVideoFrameSemaphore { semaphore });
    }, [&](SharedMemory::Handle&& handle) {
        send(Messages::RemoteGraphicsContext::SetSharedVideoFrameMemory { WTFMove(handle) });
    });
    if (!sharedVideoFrame)
        return;
    send(Messages::RemoteGraphicsContext::DrawVideoFrame(WTFMove(*sharedVideoFrame), destination, orientation, shouldDiscardAlpha));
#endif
}
#endif

void RemoteGraphicsContextProxy::strokePath(const Path& path)
{
    if (const auto* segment = path.singleSegmentIfExists()) {
#if ENABLE(INLINE_PATH_DATA)
        if (const auto* line = std::get_if<PathDataLine>(&segment->data())) {
            auto strokeData = appendStateChangeItemForInlineStrokeIfNecessary();
            if (!strokeData.color && !strokeData.thickness)
                send(Messages::RemoteGraphicsContext::StrokeLine(*line));
            else
                send(Messages::RemoteGraphicsContext::StrokeLineWithColorAndThickness(*line, strokeData.color, strokeData.thickness));
            return;
        }
#endif
        appendStateChangeItemIfNecessary();
        WTF::switchOn(segment->data(),
#if ENABLE(INLINE_PATH_DATA)
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
#endif
            [&](auto&&) {
                send(Messages::RemoteGraphicsContext::StrokePathSegment(*segment));
            });
        return;
    }
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::StrokePath(path));
}

void RemoteGraphicsContextProxy::strokeRect(const FloatRect& rect, float width)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::StrokeRect(rect, width));
}

void RemoteGraphicsContextProxy::strokeEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::StrokeEllipse(rect));
}

void RemoteGraphicsContextProxy::clearRect(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::ClearRect(rect));
}

void RemoteGraphicsContextProxy::drawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::DrawControlPart(part, borderRect, deviceScaleFactor, style));
}

#if USE(CG)

void RemoteGraphicsContextProxy::applyStrokePattern()
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::ApplyStrokePattern());
}

void RemoteGraphicsContextProxy::applyFillPattern()
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::ApplyFillPattern());
}

#endif // USE(CG)

void RemoteGraphicsContextProxy::applyDeviceScaleFactor(float scaleFactor)
{
    updateStateForApplyDeviceScaleFactor(scaleFactor);
    send(Messages::RemoteGraphicsContext::ApplyDeviceScaleFactor(scaleFactor));
}

void RemoteGraphicsContextProxy::beginPage(const FloatRect& pageRect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::BeginPage(pageRect));
}

void RemoteGraphicsContextProxy::endPage()
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::EndPage());
}

void RemoteGraphicsContextProxy::setURLForRect(const URL& link, const FloatRect& destRect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteGraphicsContext::SetURLForRect(link, destRect));
}

bool RemoteGraphicsContextProxy::recordResourceUse(NativeImage& image)
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
    return renderingBackend->isCached(imageBuffer);
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

RemoteGraphicsContextProxy::InlineStrokeData RemoteGraphicsContextProxy::appendStateChangeItemForInlineStrokeIfNecessary()
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
