/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "Decoder.h"
#include "RemoteRenderingBackend.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <WebCore/ControlFactory.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class RemoteRenderingBackend;
class RemoteResourceCache;
class SharedVideoFrameReader;

class RemoteDisplayListRecorder : public IPC::StreamMessageReceiver, public CanMakeWeakPtr<RemoteDisplayListRecorder> {
public:
    static Ref<RemoteDisplayListRecorder> create(WebCore::ImageBuffer& imageBuffer, WebCore::RenderingResourceIdentifier imageBufferIdentifier, RemoteRenderingBackend& renderingBackend)
    {
        auto instance = adoptRef(*new RemoteDisplayListRecorder(imageBuffer, imageBufferIdentifier, renderingBackend));
        instance->startListeningForIPC();
        return instance;
    }

    void stopListeningForIPC();

    void save();
    void restore();
    void translate(float x, float y);
    void rotate(float angle);
    void scale(const WebCore::FloatSize& scale);
    void setCTM(const WebCore::AffineTransform&);
    void concatenateCTM(const WebCore::AffineTransform&);
    void setInlineFillColor(WebCore::DisplayList::SetInlineFillColor&&);
    void setInlineStroke(WebCore::DisplayList::SetInlineStroke&&);
    void setState(WebCore::DisplayList::SetState&&);
    void setLineCap(WebCore::LineCap);
    void setLineDash(WebCore::DisplayList::SetLineDash&&);
    void setLineJoin(WebCore::LineJoin);
    void setMiterLimit(float);
    void clearDropShadow();
    void clip(const WebCore::FloatRect&);
    void clipRoundedRect(const WebCore::FloatRoundedRect&);
    void clipOut(const WebCore::FloatRect&);
    void clipOutRoundedRect(const WebCore::FloatRoundedRect&);
    void clipToImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::FloatRect& destinationRect);
    void clipOutToPath(const WebCore::Path&);
    void clipPath(const WebCore::Path&, WebCore::WindRule);
    void resetClip();
    void drawGlyphs(WebCore::DisplayList::DrawGlyphs&&);
    void drawDecomposedGlyphs(WebCore::RenderingResourceIdentifier fontIdentifier, WebCore::RenderingResourceIdentifier decomposedGlyphsIdentifier);
    void drawDisplayListItems(Vector<WebCore::DisplayList::Item>&&, const WebCore::FloatPoint& destination);
    void drawFilteredImageBuffer(std::optional<WebCore::RenderingResourceIdentifier> sourceImageIdentifier, const WebCore::FloatRect& sourceImageRect, Ref<WebCore::Filter>);
    void drawImageBuffer(WebCore::RenderingResourceIdentifier imageBufferIdentifier, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& srcRect, WebCore::ImagePaintingOptions);
    void drawNativeImage(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, WebCore::ImagePaintingOptions);
    void drawSystemImage(Ref<WebCore::SystemImage>, const WebCore::FloatRect&);
    void drawPattern(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint&, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions);
    void beginTransparencyLayer(float opacity);
    void endTransparencyLayer();
    void drawRect(const WebCore::FloatRect&, float borderThickness);
    void drawLine(const WebCore::FloatPoint& point1, const WebCore::FloatPoint& point2);
    void drawLinesForText(WebCore::DisplayList::DrawLinesForText&&);
    void drawDotsForDocumentMarker(const WebCore::FloatRect&, const WebCore::DocumentMarkerLineStyle&);
    void drawEllipse(const WebCore::FloatRect&);
    void drawPath(const WebCore::Path&);
    void drawFocusRingPath(const WebCore::Path&, float outlineWidth, const WebCore::Color&);
    void drawFocusRingRects(const Vector<WebCore::FloatRect>&, float outlineOffset, float outlineWidth, const WebCore::Color&);
    void fillRect(const WebCore::FloatRect&);
    void fillRectWithColor(const WebCore::FloatRect&, const WebCore::Color&);
    void fillRectWithGradient(WebCore::DisplayList::FillRectWithGradient&&);
    void fillRectWithGradientAndSpaceTransform(WebCore::DisplayList::FillRectWithGradientAndSpaceTransform&&);
    void fillCompositedRect(const WebCore::FloatRect&, const WebCore::Color&, WebCore::CompositeOperator, WebCore::BlendMode);
    void fillRoundedRect(const WebCore::FloatRoundedRect&, const WebCore::Color&, WebCore::BlendMode);
    void fillRectWithRoundedHole(const WebCore::FloatRect&, const WebCore::FloatRoundedRect&, const WebCore::Color&);
#if ENABLE(INLINE_PATH_DATA)
    void fillLine(const WebCore::PathDataLine&);
    void fillArc(const WebCore::PathArc&);
    void fillClosedArc(const WebCore::PathClosedArc&);
    void fillQuadCurve(const WebCore::PathDataQuadCurve&);
    void fillBezierCurve(const WebCore::PathDataBezierCurve&);
#endif
    void fillPathSegment(const WebCore::PathSegment&);
    void fillPath(const WebCore::Path&);
    void fillEllipse(const WebCore::FloatRect&);
#if ENABLE(VIDEO)
    void paintFrameForMedia(WebCore::MediaPlayerIdentifier, const WebCore::FloatRect& destination);
#endif
    void strokeRect(const WebCore::FloatRect&, float lineWidth);
#if ENABLE(INLINE_PATH_DATA)
    void strokeLine(const WebCore::PathDataLine&);
    void strokeLineWithColorAndThickness(const WebCore::PathDataLine&, WebCore::DisplayList::SetInlineStroke&&);
    void strokeArc(const WebCore::PathArc&);
    void strokeClosedArc(const WebCore::PathClosedArc&);
    void strokeQuadCurve(const WebCore::PathDataQuadCurve&);
    void strokeBezierCurve(const WebCore::PathDataBezierCurve&);
#endif
    void strokePathSegment(const WebCore::PathSegment&);
    void strokePath(const WebCore::Path&);
    void strokeEllipse(const WebCore::FloatRect&);
    void clearRect(const WebCore::FloatRect&);
    void drawControlPart(Ref<WebCore::ControlPart>, const WebCore::FloatRoundedRect& borderRect, float deviceScaleFactor, const WebCore::ControlStyle&);
#if USE(CG)
    void applyStrokePattern();
    void applyFillPattern();
#endif
    void applyDeviceScaleFactor(float);

private:
    RemoteDisplayListRecorder(WebCore::ImageBuffer&, WebCore::RenderingResourceIdentifier, RemoteRenderingBackend&);

    void drawFilteredImageBufferInternal(std::optional<WebCore::RenderingResourceIdentifier> sourceImageIdentifier, const WebCore::FloatRect& sourceImageRect, WebCore::Filter&, WebCore::FilterResults&);

    RemoteResourceCache& resourceCache() const;
    WebCore::GraphicsContext& drawingContext() { return m_imageBuffer->context(); }
    RefPtr<WebCore::ImageBuffer> imageBuffer(WebCore::RenderingResourceIdentifier) const;
    std::optional<WebCore::SourceImage> sourceImage(WebCore::RenderingResourceIdentifier) const;

    template<typename T, typename ... AdditionalArgs>
    void handleItem(T&& item, AdditionalArgs&&... args)
    {
        // FIXME: In the future, we should consider buffering up batches of display list items before
        // applying them instead of applying them immediately, so that we can apply clipping and occlusion
        // optimizations to skip over parts of a display list, if possible.
        item.apply(drawingContext(), std::forward<AdditionalArgs>(args)...);
    }

    void startListeningForIPC();
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

#if PLATFORM(COCOA) && ENABLE(VIDEO)
    SharedVideoFrameReader& sharedVideoFrameReader();

    void paintVideoFrame(SharedVideoFrame&&, const WebCore::FloatRect&, bool shouldDiscardAlpha);
    void setSharedVideoFrameSemaphore(IPC::Semaphore&&);
    void setSharedVideoFrameMemory(WebCore::SharedMemory::Handle&&);
#endif

    Ref<WebCore::ImageBuffer> m_imageBuffer;
    WebCore::RenderingResourceIdentifier m_imageBufferIdentifier;
    RefPtr<RemoteRenderingBackend> m_renderingBackend;
    std::unique_ptr<WebCore::ControlFactory> m_controlFactory;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    std::unique_ptr<SharedVideoFrameReader> m_sharedVideoFrameReader;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
