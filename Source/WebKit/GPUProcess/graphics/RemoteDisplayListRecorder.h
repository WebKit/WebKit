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
#include "DisplayListRecorderFlushIdentifier.h"
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteRenderingBackend.h"
#include "SharedVideoFrame.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <WebCore/ControlFactory.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class FilterReference;
}

namespace WebKit {

class RemoteRenderingBackend;
class RemoteResourceCache;

class RemoteDisplayListRecorder : public IPC::StreamMessageReceiver, public CanMakeWeakPtr<RemoteDisplayListRecorder> {
public:
    static Ref<RemoteDisplayListRecorder> create(WebCore::ImageBuffer& imageBuffer, QualifiedRenderingResourceIdentifier imageBufferIdentifier, WebCore::ProcessIdentifier webProcessIdentifier, RemoteRenderingBackend& renderingBackend)
    {
        auto instance = adoptRef(*new RemoteDisplayListRecorder(imageBuffer, imageBufferIdentifier, webProcessIdentifier, renderingBackend));
        instance->startListeningForIPC();
        return instance;
    }

    void stopListeningForIPC();
    void clearImageBufferReference();

    void save();
    void restore();
    void translate(float x, float y);
    void rotate(float angle);
    void scale(const WebCore::FloatSize& scale);
    void setCTM(const WebCore::AffineTransform&);
    void concatenateCTM(const WebCore::AffineTransform&);
    void setInlineFillColor(WebCore::DisplayList::SetInlineFillColor&&);
    void setInlineStrokeColor(WebCore::DisplayList::SetInlineStrokeColor&&);
    void setStrokeThickness(float);
    void setState(WebCore::DisplayList::SetState&&);
    void setLineCap(WebCore::LineCap);
    void setLineDash(WebCore::DisplayList::SetLineDash&&);
    void setLineJoin(WebCore::LineJoin);
    void setMiterLimit(float);
    void clearShadow();
    void clip(const WebCore::FloatRect&);
    void clipOut(const WebCore::FloatRect&);
    void clipToImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::FloatRect& destinationRect);
    void clipOutToPath(const WebCore::Path&);
    void clipPath(const WebCore::Path&, WebCore::WindRule);
    void drawGlyphs(WebCore::DisplayList::DrawGlyphs&&);
    void drawDecomposedGlyphs(WebCore::RenderingResourceIdentifier fontIdentifier, WebCore::RenderingResourceIdentifier decomposedGlyphsIdentifier);
    void drawFilteredImageBuffer(std::optional<WebCore::RenderingResourceIdentifier> sourceImageIdentifier, const WebCore::FloatRect& sourceImageRect, IPC::FilterReference);
    void drawImageBuffer(WebCore::RenderingResourceIdentifier imageBufferIdentifier, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& srcRect, const WebCore::ImagePaintingOptions&);
    void drawNativeImage(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatSize& imageSize, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, const WebCore::ImagePaintingOptions&);
    void drawSystemImage(Ref<WebCore::SystemImage>, const WebCore::FloatRect&);
    void drawPattern(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint&, const WebCore::FloatSize& spacing, const WebCore::ImagePaintingOptions&);
    void beginTransparencyLayer(float opacity);
    void endTransparencyLayer();
    void drawRect(const WebCore::FloatRect&, float borderThickness);
    void drawLine(const WebCore::FloatPoint& point1, const WebCore::FloatPoint& point2);
    void drawLinesForText(WebCore::DisplayList::DrawLinesForText&&);
    void drawDotsForDocumentMarker(const WebCore::FloatRect&, const WebCore::DocumentMarkerLineStyle&);
    void drawEllipse(const WebCore::FloatRect&);
    void drawPath(const WebCore::Path&);
    void drawFocusRingPath(const WebCore::Path&, float width, float offset, const WebCore::Color&);
    void drawFocusRingRects(const Vector<WebCore::FloatRect>& rects, float width, float offset, const WebCore::Color&);
    void fillRect(const WebCore::FloatRect&);
    void fillRectWithColor(const WebCore::FloatRect&, const WebCore::Color&);
    void fillRectWithGradient(WebCore::DisplayList::FillRectWithGradient&&);
    void fillCompositedRect(const WebCore::FloatRect&, const WebCore::Color&, WebCore::CompositeOperator, WebCore::BlendMode);
    void fillRoundedRect(const WebCore::FloatRoundedRect&, const WebCore::Color&, WebCore::BlendMode);
    void fillRectWithRoundedHole(const WebCore::FloatRect&, const WebCore::FloatRoundedRect&, const WebCore::Color&);
#if ENABLE(INLINE_PATH_DATA)
    void fillLine(const WebCore::LineData&);
    void fillArc(const WebCore::ArcData&);
    void fillQuadCurve(const WebCore::QuadCurveData&);
    void fillBezierCurve(const WebCore::BezierCurveData&);
#endif
    void fillPath(const WebCore::Path&);
    void fillEllipse(const WebCore::FloatRect&);
    void convertToLuminanceMask();
    void transformToColorSpace(const WebCore::DestinationColorSpace&);
    void paintFrameForMedia(WebCore::MediaPlayerIdentifier, const WebCore::FloatRect& destination);
    void strokeRect(const WebCore::FloatRect&, float lineWidth);
#if ENABLE(INLINE_PATH_DATA)
    void strokeLine(const WebCore::LineData&);
    void strokeLineWithColorAndThickness(WebCore::DisplayList::SetInlineStrokeColor&&, float, const WebCore::LineData&);
    void strokeArc(const WebCore::ArcData&);
    void strokeQuadCurve(const WebCore::QuadCurveData&);
    void strokeBezierCurve(const WebCore::BezierCurveData&);
#endif
    void strokePath(const WebCore::Path&);
    void strokeEllipse(const WebCore::FloatRect&);
    void clearRect(const WebCore::FloatRect&);
    void drawControlPart(Ref<WebCore::ControlPart>, const WebCore::FloatRect&, float deviceScaleFactor, const WebCore::ControlStyle&);
#if USE(CG)
    void applyStrokePattern();
    void applyFillPattern();
#endif
    void applyDeviceScaleFactor(float);
    void flushContext(DisplayListRecorderFlushIdentifier);

private:
    RemoteDisplayListRecorder(WebCore::ImageBuffer&, QualifiedRenderingResourceIdentifier, WebCore::ProcessIdentifier webProcessIdentifier, RemoteRenderingBackend&);

    void setStateWithQualifiedIdentifiers(WebCore::DisplayList::SetState&&, QualifiedRenderingResourceIdentifier strokePatternImageIdentifier, QualifiedRenderingResourceIdentifier fillPatternImageIdentifier);
    void clipToImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier, const WebCore::FloatRect& destinationRect);
    void drawGlyphsWithQualifiedIdentifier(WebCore::DisplayList::DrawGlyphs&&, QualifiedRenderingResourceIdentifier fontIdentifier);
    void drawDecomposedGlyphsWithQualifiedIdentifiers(QualifiedRenderingResourceIdentifier fontIdentifier, QualifiedRenderingResourceIdentifier decomposedGlyphsIdentifier);
    void drawImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageBufferIdentifier, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& srcRect, const WebCore::ImagePaintingOptions&);
    void drawNativeImageWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageIdentifier, const WebCore::FloatSize& imageSize, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, const WebCore::ImagePaintingOptions&);
    void drawPatternWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint&, const WebCore::FloatSize& spacing, const WebCore::ImagePaintingOptions&);

    RemoteResourceCache& resourceCache();
    WebCore::GraphicsContext& drawingContext();

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
    void paintVideoFrame(SharedVideoFrame&&, const WebCore::FloatRect&, bool shouldDiscardAlpha);
    void setSharedVideoFrameSemaphore(IPC::Semaphore&&);
    void setSharedVideoFrameMemory(const SharedMemory::Handle&);
#endif

    WeakPtr<WebCore::ImageBuffer> m_imageBuffer;
    QualifiedRenderingResourceIdentifier m_imageBufferIdentifier;
    WebCore::ProcessIdentifier m_webProcessIdentifier;
    RefPtr<RemoteRenderingBackend> m_renderingBackend;
    std::unique_ptr<WebCore::ControlFactory> m_controlFactory;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    SharedVideoFrameReader m_sharedVideoFrameReader;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
