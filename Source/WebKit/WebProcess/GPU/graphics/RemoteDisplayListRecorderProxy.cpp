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
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListDrawingContext.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/Filter.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SVGFilter.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteDisplayListRecorderProxy);

RemoteDisplayListRecorderProxy::RemoteDisplayListRecorderProxy(RemoteImageBufferProxy& imageBuffer, RemoteRenderingBackendProxy& renderingBackend, const FloatRect& initialClip, const AffineTransform& initialCTM)
    : DisplayList::Recorder(IsDeferred::No, { }, initialClip, initialCTM, imageBuffer.colorSpace(), DrawGlyphsMode::DeconstructUsingDrawGlyphsCommands)
    , m_destinationBufferIdentifier(imageBuffer.renderingResourceIdentifier())
    , m_imageBuffer(imageBuffer)
    , m_renderingBackend(renderingBackend)
    , m_renderingMode(imageBuffer.renderingMode())
{
}

RemoteDisplayListRecorderProxy::RemoteDisplayListRecorderProxy(RemoteRenderingBackendProxy& renderingBackend, RenderingResourceIdentifier renderingResourceIdentifier, const DestinationColorSpace& colorSpace, RenderingMode renderingMode, const FloatRect& initialClip, const AffineTransform& initialCTM)
    : DisplayList::Recorder(IsDeferred::No, { }, initialClip, initialCTM, colorSpace, DrawGlyphsMode::DeconstructUsingDrawGlyphsCommands)
    , m_destinationBufferIdentifier(renderingResourceIdentifier)
    , m_renderingBackend(renderingBackend)
    , m_renderingMode(renderingMode)
{
}

RemoteDisplayListRecorderProxy::~RemoteDisplayListRecorderProxy() = default;

template<typename T>
ALWAYS_INLINE void RemoteDisplayListRecorderProxy::send(T&& message)
{
    RefPtr connection = this->connection();
    if (UNLIKELY(!connection))
        return;

    RefPtr imageBuffer = m_imageBuffer.get();
    if (LIKELY(imageBuffer))
        imageBuffer->backingStoreWillChange();
    auto result = connection->send(std::forward<T>(message), m_destinationBufferIdentifier);
    if (UNLIKELY(result != IPC::Error::NoError)) {
        RELEASE_LOG(RemoteLayerBuffers, "RemoteDisplayListRecorderProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            IPC::description(T::name()).characters(), IPC::errorAsString(result).characters());
        didBecomeUnresponsive();
    }
}

ALWAYS_INLINE RefPtr<IPC::StreamClientConnection> RemoteDisplayListRecorderProxy::connection() const
{
    auto* backend = m_renderingBackend.get();
    if (UNLIKELY(!backend))
        return nullptr;
    return backend->connection();
}

void RemoteDisplayListRecorderProxy::didBecomeUnresponsive() const
{
    auto* backend = m_renderingBackend.get();
    if (UNLIKELY(!backend))
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
    send(Messages::RemoteDisplayListRecorder::ConcatenateCTM(transform));
}

void RemoteDisplayListRecorderProxy::recordSetInlineFillColor(PackedColor::RGBA color)
{
    send(Messages::RemoteDisplayListRecorder::SetInlineFillColor(DisplayList::SetInlineFillColor { color }));
}

void RemoteDisplayListRecorderProxy::recordSetInlineStroke(DisplayList::SetInlineStroke&& strokeItem)
{
    send(Messages::RemoteDisplayListRecorder::SetInlineStroke(strokeItem));
}

void RemoteDisplayListRecorderProxy::recordSetState(const GraphicsContextState& state)
{
    send(Messages::RemoteDisplayListRecorder::SetState(DisplayList::SetState { state }));
}

void RemoteDisplayListRecorderProxy::setLineCap(LineCap lineCap)
{
    send(Messages::RemoteDisplayListRecorder::SetLineCap(lineCap));
}

void RemoteDisplayListRecorderProxy::setLineDash(const DashArray& array, float dashOffset)
{
    send(Messages::RemoteDisplayListRecorder::SetLineDash(DisplayList::SetLineDash { array, dashOffset }));
}

void RemoteDisplayListRecorderProxy::setLineJoin(LineJoin lineJoin)
{
    send(Messages::RemoteDisplayListRecorder::SetLineJoin(lineJoin));
}

void RemoteDisplayListRecorderProxy::setMiterLimit(float limit)
{
    send(Messages::RemoteDisplayListRecorder::SetMiterLimit(limit));
}

void RemoteDisplayListRecorderProxy::recordClearDropShadow()
{
    send(Messages::RemoteDisplayListRecorder::ClearDropShadow());
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

void RemoteDisplayListRecorderProxy::recordClipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destinationRect)
{
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

void RemoteDisplayListRecorderProxy::recordDrawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter)
{
    std::optional<RenderingResourceIdentifier> identifier;
    if (sourceImage)
        identifier = sourceImage->renderingResourceIdentifier();

    auto* svgFilter = dynamicDowncast<SVGFilter>(filter);
    if (svgFilter && svgFilter->hasValidRenderingResourceIdentifier())
        recordResourceUse(filter);

    send(Messages::RemoteDisplayListRecorder::DrawFilteredImageBuffer(WTFMove(identifier), sourceImageRect, filter));
}

void RemoteDisplayListRecorderProxy::recordDrawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode mode)
{
    send(Messages::RemoteDisplayListRecorder::DrawGlyphs(DisplayList::DrawGlyphs { font, glyphs, advances, count, localAnchor, mode }));
}

void RemoteDisplayListRecorderProxy::recordDrawDecomposedGlyphs(const Font& font, const DecomposedGlyphs& decomposedGlyphs)
{
    send(Messages::RemoteDisplayListRecorder::DrawDecomposedGlyphs(font.renderingResourceIdentifier(), decomposedGlyphs.renderingResourceIdentifier()));
}

void RemoteDisplayListRecorderProxy::recordDrawDisplayListItems(const Vector<DisplayList::Item>& items, const FloatPoint& destination)
{
    send(Messages::RemoteDisplayListRecorder::DrawDisplayListItems(items, destination));
}

void RemoteDisplayListRecorderProxy::recordDrawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    send(Messages::RemoteDisplayListRecorder::DrawImageBuffer(imageBuffer.renderingResourceIdentifier(), destRect, srcRect, options));
}

void RemoteDisplayListRecorderProxy::recordDrawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    send(Messages::RemoteDisplayListRecorder::DrawNativeImage(imageIdentifier, destRect, srcRect, options));
}

void RemoteDisplayListRecorderProxy::recordDrawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    send(Messages::RemoteDisplayListRecorder::DrawSystemImage(systemImage, destinationRect));
}

void RemoteDisplayListRecorderProxy::recordDrawPattern(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    send(Messages::RemoteDisplayListRecorder::DrawPattern(imageIdentifier, destRect, tileRect, transform, phase, spacing, options));
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

void RemoteDisplayListRecorderProxy::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle style)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::DrawLinesForText(DisplayList::DrawLinesForText { point, widths, thickness, printing, doubleLines, style }));
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
    send(Messages::RemoteDisplayListRecorder::FillRectWithGradient(DisplayList::FillRectWithGradient { rect, gradient }));
}

void RemoteDisplayListRecorderProxy::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::FillRectWithGradientAndSpaceTransform(DisplayList::FillRectWithGradientAndSpaceTransform { rect, gradient, gradientSpaceTransform, requiresClipToRect }));
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

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorderProxy::recordFillLine(const PathDataLine& line)
{
    send(Messages::RemoteDisplayListRecorder::FillLine(line));
}

void RemoteDisplayListRecorderProxy::recordFillArc(const PathArc& arc)
{
    send(Messages::RemoteDisplayListRecorder::FillArc(arc));
}

void RemoteDisplayListRecorderProxy::recordFillClosedArc(const PathClosedArc& closedArc)
{
    send(Messages::RemoteDisplayListRecorder::FillClosedArc(closedArc));
}

void RemoteDisplayListRecorderProxy::recordFillQuadCurve(const PathDataQuadCurve& curve)
{
    send(Messages::RemoteDisplayListRecorder::FillQuadCurve(curve));
}

void RemoteDisplayListRecorderProxy::recordFillBezierCurve(const PathDataBezierCurve& curve)
{
    send(Messages::RemoteDisplayListRecorder::FillBezierCurve(curve));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorderProxy::recordFillPathSegment(const PathSegment& segment)
{
    send(Messages::RemoteDisplayListRecorder::FillPathSegment(segment));
}

void RemoteDisplayListRecorderProxy::recordFillPath(const Path& path)
{
    send(Messages::RemoteDisplayListRecorder::FillPath(path));
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

void RemoteDisplayListRecorderProxy::strokeRect(const FloatRect& rect, float width)
{
    appendStateChangeItemIfNecessary();
    send(Messages::RemoteDisplayListRecorder::StrokeRect(rect, width));
}

#if ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorderProxy::recordStrokeLine(const PathDataLine& line)
{
    send(Messages::RemoteDisplayListRecorder::StrokeLine(line));
}

void RemoteDisplayListRecorderProxy::recordStrokeLineWithColorAndThickness(const PathDataLine& line, DisplayList::SetInlineStroke&& strokeItem)
{
    send(Messages::RemoteDisplayListRecorder::StrokeLineWithColorAndThickness(line, strokeItem));
}

void RemoteDisplayListRecorderProxy::recordStrokeArc(const PathArc& arc)
{
    send(Messages::RemoteDisplayListRecorder::StrokeArc(arc));
}

void RemoteDisplayListRecorderProxy::recordStrokeClosedArc(const PathClosedArc& closedArc)
{
    send(Messages::RemoteDisplayListRecorder::StrokeClosedArc(closedArc));
}

void RemoteDisplayListRecorderProxy::recordStrokeQuadCurve(const PathDataQuadCurve& curve)
{
    send(Messages::RemoteDisplayListRecorder::StrokeQuadCurve(curve));
}

void RemoteDisplayListRecorderProxy::recordStrokeBezierCurve(const PathDataBezierCurve& curve)
{
    send(Messages::RemoteDisplayListRecorder::StrokeBezierCurve(curve));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RemoteDisplayListRecorderProxy::recordStrokePathSegment(const PathSegment& segment)
{
    send(Messages::RemoteDisplayListRecorder::StrokePathSegment(segment));
}

void RemoteDisplayListRecorderProxy::recordStrokePath(const Path& path)
{
    send(Messages::RemoteDisplayListRecorder::StrokePath(path));
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

bool RemoteDisplayListRecorderProxy::recordResourceUse(NativeImage& image)
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    m_renderingBackend->remoteResourceCacheProxy().recordNativeImageUse(image);
    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(ImageBuffer& imageBuffer)
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (!m_renderingBackend->isCached(imageBuffer))
        return false;

    m_renderingBackend->remoteResourceCacheProxy().recordImageBufferUse(imageBuffer);
    return true;
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
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    m_renderingBackend->remoteResourceCacheProxy().recordFontUse(font);
    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(DecomposedGlyphs& decomposedGlyphs)
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    m_renderingBackend->remoteResourceCacheProxy().recordDecomposedGlyphsUse(decomposedGlyphs);
    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(Gradient& gradient)
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    m_renderingBackend->remoteResourceCacheProxy().recordGradientUse(gradient);
    return true;
}

bool RemoteDisplayListRecorderProxy::recordResourceUse(Filter& filter)
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    m_renderingBackend->remoteResourceCacheProxy().recordFilterUse(filter);
    return true;
}

RefPtr<ImageBuffer> RemoteDisplayListRecorderProxy::createImageBuffer(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, std::optional<RenderingMode> renderingMode, std::optional<RenderingMethod> renderingMethod) const
{
    if (UNLIKELY(!m_renderingBackend)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (renderingMethod)
        return Recorder::createImageBuffer(size, resolutionScale, colorSpace, renderingMode, renderingMethod);

    // FIXME: Ideally we'd plumb the purpose through for callers of GraphicsContext::createImageBuffer().
    RenderingPurpose purpose = RenderingPurpose::Unspecified;
    // FIXME: Use purpose to decide the acceleration status and remove this.
    OptionSet<ImageBufferOptions> options;
    if (renderingMode.value_or(this->renderingMode()) == RenderingMode::Accelerated)
        options.add(ImageBufferOptions::Accelerated);
    return m_renderingBackend->createImageBuffer(size, purpose, resolutionScale, colorSpace, ImageBufferPixelFormat::BGRA8, options);
}

RefPtr<ImageBuffer> RemoteDisplayListRecorderProxy::createAlignedImageBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, std::optional<RenderingMethod> renderingMethod) const
{
    auto renderingMode = !renderingMethod ? this->renderingMode() : RenderingMode::Unaccelerated;
    return GraphicsContext::createScaledImageBuffer(size, scaleFactor(), colorSpace, renderingMode, renderingMethod);
}

RefPtr<ImageBuffer> RemoteDisplayListRecorderProxy::createAlignedImageBuffer(const FloatRect& rect, const DestinationColorSpace& colorSpace, std::optional<RenderingMethod> renderingMethod) const
{
    auto renderingMode = !renderingMethod ? this->renderingMode() : RenderingMode::Unaccelerated;
    return GraphicsContext::createScaledImageBuffer(rect, scaleFactor(), colorSpace, renderingMode, renderingMethod);
}

void RemoteDisplayListRecorderProxy::disconnect()
{
    m_renderingBackend = nullptr;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    Locker locker { m_sharedVideoFrameWriterLock };
    if (m_sharedVideoFrameWriter)
        m_sharedVideoFrameWriter->disable();
#endif
}

} // namespace WebCore

#endif // ENABLE(GPU_PROCESS)
