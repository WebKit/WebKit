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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteDisplayListIdentifier.h"
#include "RemoteGradientIdentifier.h"
#include "RemoteGraphicsContextIdentifier.h"
#include <WebCore/DisplayListRecorder.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace IPC {
class Signal;
class Semaphore;
class StreamClientConnection;
}

namespace WebKit {

class RemoteRenderingBackendProxy;
class RemoteImageBufferProxy;
class SharedVideoFrameWriter;

class RemoteGraphicsContextProxy : public WebCore::DisplayList::Recorder {
    WTF_MAKE_TZONE_ALLOCATED(RemoteGraphicsContextProxy);
public:
    RemoteGraphicsContextProxy(const WebCore::DestinationColorSpace&, WebCore::RenderingMode, const WebCore::FloatRect& initialClip, const WebCore::AffineTransform&, RemoteRenderingBackendProxy&);
    RemoteGraphicsContextProxy(const WebCore::DestinationColorSpace&, WebCore::ContentsFormat, WebCore::RenderingMode, const WebCore::FloatRect& initialClip, const WebCore::AffineTransform&, RemoteGraphicsContextIdentifier, RemoteRenderingBackendProxy&);
    ~RemoteGraphicsContextProxy();
    RemoteGraphicsContextIdentifier identifier() const { return m_identifier; }

    // Called when rendering backend connection is lost.
    void disconnect();

    // Called when rendering backend gets discarded but reference to the owning image buffer is still referenced.
    void abandon();
    using GraphicsContext::drawDisplayList;
    void drawDisplayList(const WebCore::DisplayList::DisplayList&, WebCore::ControlFactory&) final;

    void setClient(ThreadSafeWeakPtr<RemoteImageBufferProxy>&& client) { m_client = WTFMove(client); }
    // Returns false if there has not been any potential draws since last call.
    // Returns true if there has been potential draws since last call.
    bool consumeHasDrawn();

protected:
    RemoteGraphicsContextProxy(const WebCore::DestinationColorSpace&, std::optional<WebCore::ContentsFormat>, WebCore::RenderingMode, const WebCore::FloatRect& initialClip, const WebCore::AffineTransform&, DrawGlyphsMode, RemoteGraphicsContextIdentifier, RemoteRenderingBackendProxy&);

    template<typename T> void send(T&& message);

private:
    void didBecomeUnresponsive() const;

    bool knownToHaveFloatBasedBacking() const final;

    WebCore::RenderingMode renderingMode() const final;

    void save(WebCore::GraphicsContextState::Purpose) final;
    void restore(WebCore::GraphicsContextState::Purpose) final;
    void translate(float x, float y) final;
    void rotate(float angle) final;
    void scale(const WebCore::FloatSize&) final;
    void setCTM(const WebCore::AffineTransform&) final;
    void concatCTM(const WebCore::AffineTransform&) final;
    void setLineCap(WebCore::LineCap) final;
    void setLineDash(const WebCore::DashArray&, float dashOffset) final;
    void setLineJoin(WebCore::LineJoin) final;
    void setMiterLimit(float) final;
    void clip(const WebCore::FloatRect&) final;
    void clipRoundedRect(const WebCore::FloatRoundedRect&) final;
    void clipOut(const WebCore::FloatRect&) final;
    void clipOut(const WebCore::Path&) final;
    void clipOutRoundedRect(const WebCore::FloatRoundedRect&) final;
    void clipPath(const WebCore::Path&, WebCore::WindRule) final;
    void clipToImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect& destinationRect) final;
    void resetClip() final;
    void beginTransparencyLayer(float) final;
    void beginTransparencyLayer(WebCore::CompositeOperator, WebCore::BlendMode) final;
    void endTransparencyLayer() final;
    void drawFilteredImageBuffer(WebCore::ImageBuffer*, const WebCore::FloatRect&, WebCore::Filter&, WebCore::FilterResults&) final;
    void drawImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, WebCore::ImagePaintingOptions) final;
    void drawNativeImage(WebCore::NativeImage&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, WebCore::ImagePaintingOptions) final;
    void drawSystemImage(WebCore::SystemImage&, const WebCore::FloatRect&) final;
    void drawRect(const WebCore::FloatRect&, float) final;
    void drawLine(const WebCore::FloatPoint& point1, const WebCore::FloatPoint& point2) final;
    void drawLinesForText(const WebCore::FloatPoint&, float thickness, std::span<const WebCore::FloatSegment>, bool isPrinting, bool doubleLines, WebCore::StrokeStyle) final;
    void drawDotsForDocumentMarker(const WebCore::FloatRect&, WebCore::DocumentMarkerLineStyle) final;
    void drawEllipse(const WebCore::FloatRect&) final;
    void drawPath(const WebCore::Path&) final;
    void drawFocusRing(const WebCore::Path&, float outlineWidth, const WebCore::Color&) final;
    void drawFocusRing(const Vector<WebCore::FloatRect>&, float outlineOffset, float outlineWidth, const WebCore::Color&) final;
    void drawPattern(WebCore::NativeImage&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint& phase, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions = { }) final;
    void drawPattern(WebCore::ImageBuffer&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint& phase, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions = { }) final;
    void fillPath(const WebCore::Path&) final;
    void fillRect(const WebCore::FloatRect&, RequiresClipToRect) final;
    void fillRect(const WebCore::FloatRect&, const WebCore::Color&) final;
    void fillRect(const WebCore::FloatRect&, WebCore::Gradient&) final;
    void fillRect(const WebCore::FloatRect&, WebCore::Gradient&, const WebCore::AffineTransform&, RequiresClipToRect) final;
    void fillRect(const WebCore::FloatRect&, const WebCore::Color&, WebCore::CompositeOperator, WebCore::BlendMode) final;
    void fillRoundedRect(const WebCore::FloatRoundedRect&, const WebCore::Color&, WebCore::BlendMode) final;
    void fillRectWithRoundedHole(const WebCore::FloatRect&, const WebCore::FloatRoundedRect&, const WebCore::Color&) final;
    void fillEllipse(const WebCore::FloatRect&) final;
#if ENABLE(VIDEO)
    void drawVideoFrame(const WebCore::VideoFrame&, const WebCore::FloatRect& distination, WebCore::ImageOrientation, bool shouldDiscardAlpha) final;
#endif
    void strokePath(const WebCore::Path&) final;
    void strokeRect(const WebCore::FloatRect&, float) final;
    void strokeEllipse(const WebCore::FloatRect&) final;
    void clearRect(const WebCore::FloatRect&) final;
    void drawControlPart(WebCore::ControlPart&, const WebCore::FloatRoundedRect& borderRect, float deviceScaleFactor, const WebCore::ControlStyle&) final;
    void drawGlyphs(const WebCore::Font&, std::span<const WebCore::GlyphBufferGlyph>, std::span<const WebCore::GlyphBufferAdvance>, const WebCore::FloatPoint& localAnchor, WebCore::FontSmoothingMode) final;
    void drawGlyphsImmediate(const WebCore::Font&, std::span<const WebCore::GlyphBufferGlyph>, std::span<const WebCore::GlyphBufferAdvance>, const WebCore::FloatPoint& localAnchor, WebCore::FontSmoothingMode) final;

#if USE(CG)
    void applyStrokePattern() final;
    void applyFillPattern() final;
#endif
    void applyDeviceScaleFactor(float) final;

    void beginPage(const WebCore::FloatRect&) final;
    void endPage() final;
    void setURLForRect(const URL&, const WebCore::FloatRect&) final;

    [[nodiscard]] bool recordResourceUse(WebCore::NativeImage&);
    [[nodiscard]] bool recordResourceUse(WebCore::ImageBuffer&);
    bool recordResourceUse(const WebCore::SourceImage&);
    bool recordResourceUse(WebCore::Font&);
    std::optional<RemoteGradientIdentifier> recordResourceUse(WebCore::Gradient&);
    bool recordResourceUse(WebCore::Filter&);
    std::optional<RemoteDisplayListIdentifier> recordResourceUse(const WebCore::DisplayList::DisplayList&);

    // Synchronizes draw state.
protected:
    void appendStateChangeItemIfNecessary() final;

private:
    struct InlineStrokeData {
        std::optional<WebCore::PackedColor::RGBA> color;
        std::optional<float> thickness;
    };
    // Synchronizes draw state and returns stroke state that needs to be sent inline with the stroke command.
    InlineStrokeData appendStateChangeItemForInlineStrokeIfNecessary();

    RefPtr<WebCore::ImageBuffer> createImageBuffer(const WebCore::FloatSize&, float resolutionScale, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMode>, std::optional<WebCore::RenderingMethod>, WebCore::ImageBufferFormat) const final;
    RefPtr<WebCore::ImageBuffer> createAlignedImageBuffer(const WebCore::FloatSize&, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMethod>) const final;
    RefPtr<WebCore::ImageBuffer> createAlignedImageBuffer(const WebCore::FloatRect&, const WebCore::DestinationColorSpace&, std::optional<WebCore::RenderingMethod>) const final;

#if HAVE(SUPPORT_HDR_DISPLAY)
    void setMaxEDRHeadroom(std::optional<float> headroom) final { m_maxEDRHeadroom = headroom; }
    float maxPaintedEDRHeadroom() const final { return m_maxPaintedEDRHeadroom; }
    float maxRequestedEDRHeadroom() const final { return m_maxRequestedEDRHeadroom; }
    void clearMaxEDRHeadrooms() final { m_maxPaintedEDRHeadroom = 1; m_maxRequestedEDRHeadroom = 1; }
#endif

    const WebCore::RenderingMode m_renderingMode;
    const RemoteGraphicsContextIdentifier m_identifier;
    RefPtr<IPC::StreamClientConnection> m_connection;
    WeakPtr<RemoteRenderingBackendProxy> m_renderingBackend;
    std::optional<WebCore::ContentsFormat> m_contentsFormat;
    ThreadSafeWeakPtr<RemoteImageBufferProxy> m_client;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    Lock m_sharedVideoFrameWriterLock;
    std::unique_ptr<SharedVideoFrameWriter> m_sharedVideoFrameWriter WTF_GUARDED_BY_LOCK(m_sharedVideoFrameWriterLock);
#endif
#if HAVE(SUPPORT_HDR_DISPLAY)
    std::optional<float> m_maxEDRHeadroom;
    float m_maxPaintedEDRHeadroom { 1 };
    float m_maxRequestedEDRHeadroom { 1 };
#endif
    // Flag for pending draws. Start with true because we do not know what commands have been scheduled to the context.
    bool m_hasDrawn { true };
};

inline bool RemoteGraphicsContextProxy::consumeHasDrawn()
{
    return std::exchange(m_hasDrawn, false);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
