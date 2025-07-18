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

#include "ArrayReferenceTuple.h"
#include "Decoder.h"
#include "RemoteDisplayListRecorderIdentifier.h"
#include "RemoteRenderingBackend.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <WebCore/ControlFactory.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

#if !LOG_DISABLED
#include "Logging.h"
#include <wtf/text/TextStream.h>
#endif

namespace WebKit {

class RemoteRenderingBackend;
class RemoteResourceCache;
class SharedVideoFrameReader;
struct SharedPreferencesForWebProcess;

class RemoteDisplayListRecorder : public IPC::StreamMessageReceiver, public CanMakeWeakPtr<RemoteDisplayListRecorder> {
public:
    static Ref<RemoteDisplayListRecorder> create(WebCore::ImageBuffer&, RemoteDisplayListRecorderIdentifier, RemoteRenderingBackend&);
    ~RemoteDisplayListRecorder();

    void stopListeningForIPC();

    void save();
    void restore();
    void translate(float x, float y);
    void rotate(float angle);
    void scale(const WebCore::FloatSize& scale);
    void setCTM(const WebCore::AffineTransform&);
    void concatCTM(const WebCore::AffineTransform&);
    void setFillPackedColor(WebCore::PackedColor::RGBA);
    void setFillColor(const WebCore::Color&);
    void setFillCachedGradient(WebCore::RenderingResourceIdentifier, const WebCore::AffineTransform&);
    void setFillGradient(Ref<WebCore::Gradient>&&, const WebCore::AffineTransform&);
    void setFillPattern(WebCore::RenderingResourceIdentifier tileImageIdentifier, const WebCore::PatternParameters&);
    void setFillRule(WebCore::WindRule);
    void setStrokePackedColor(WebCore::PackedColor::RGBA);
    void setStrokeColor(const WebCore::Color&);
    void setStrokeCachedGradient(WebCore::RenderingResourceIdentifier, const WebCore::AffineTransform&);
    void setStrokeGradient(Ref<WebCore::Gradient>&&, const WebCore::AffineTransform&);
    void setStrokePattern(WebCore::RenderingResourceIdentifier tileImageIdentifier, const WebCore::PatternParameters&);
    void setStrokePackedColorAndThickness(WebCore::PackedColor::RGBA, float);
    void setStrokeThickness(float);
    void setStrokeStyle(WebCore::StrokeStyle);
    void setCompositeMode(WebCore::CompositeMode);
    void setDropShadow(std::optional<WebCore::GraphicsDropShadow>);
    void setStyle(std::optional<WebCore::GraphicsStyle>);
    void setAlpha(float);
    void setTextDrawingMode(WebCore::TextDrawingModeFlags);
    void setImageInterpolationQuality(WebCore::InterpolationQuality);
    void setShouldAntialias(bool);
    void setShouldSmoothFonts(bool);
    void setShouldSubpixelQuantizeFonts(bool);
    void setShadowsIgnoreTransforms(bool);
    void setDrawLuminanceMask(bool);
    void setLineCap(WebCore::LineCap);
    void setLineDash(FixedVector<double>&&, float dashOffset);
    void setLineJoin(WebCore::LineJoin);
    void setMiterLimit(float);
    void clip(const WebCore::FloatRect&);
    void clipRoundedRect(const WebCore::FloatRoundedRect&);
    void clipOut(const WebCore::FloatRect&);
    void clipOutRoundedRect(const WebCore::FloatRoundedRect&);
    void clipToImageBuffer(WebCore::RenderingResourceIdentifier, const WebCore::FloatRect& destinationRect);
    void clipOutToPath(const WebCore::Path&);
    void clipPath(const WebCore::Path&, WebCore::WindRule);
    void resetClip();
    void drawGlyphs(WebCore::RenderingResourceIdentifier fontIdentifier, IPC::ArrayReferenceTuple<WebCore::GlyphBufferGlyph, WebCore::FloatSize>, WebCore::FloatPoint localAnchor, WebCore::FontSmoothingMode);
    void drawDecomposedGlyphs(WebCore::RenderingResourceIdentifier fontIdentifier, WebCore::RenderingResourceIdentifier decomposedGlyphsIdentifier);
    void drawFilteredImageBuffer(std::optional<WebCore::RenderingResourceIdentifier> sourceImageIdentifier, const WebCore::FloatRect& sourceImageRect, Ref<WebCore::Filter>&&);
    void drawImageBuffer(WebCore::RenderingResourceIdentifier imageBufferIdentifier, const WebCore::FloatRect& destinationRect, const WebCore::FloatRect& srcRect, WebCore::ImagePaintingOptions);
    void drawNativeImage(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& srcRect, WebCore::ImagePaintingOptions);
    void drawSystemImage(Ref<WebCore::SystemImage>&&, const WebCore::FloatRect&);
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    void drawVideoFrame(SharedVideoFrame&&, const WebCore::FloatRect& destination, WebCore::ImageOrientation, bool shouldDiscardAlpha);
#endif
    void drawPatternNativeImage(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint&, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions);
    void drawPatternImageBuffer(WebCore::RenderingResourceIdentifier imageIdentifier, const WebCore::FloatRect& destRect, const WebCore::FloatRect& tileRect, const WebCore::AffineTransform&, const WebCore::FloatPoint&, const WebCore::FloatSize& spacing, WebCore::ImagePaintingOptions);
    void beginTransparencyLayer(float opacity);
    void beginTransparencyLayerWithCompositeMode(WebCore::CompositeMode);
    void endTransparencyLayer();
    void drawRect(const WebCore::FloatRect&, float borderThickness);
    void drawLine(const WebCore::FloatPoint& point1, const WebCore::FloatPoint& point2);
    void drawLinesForText(const WebCore::FloatPoint&, float thickness, std::span<const WebCore::FloatSegment> lineSegments, bool printing, bool doubleLines, WebCore::StrokeStyle);
    void drawDotsForDocumentMarker(const WebCore::FloatRect&, const WebCore::DocumentMarkerLineStyle&);
    void drawEllipse(const WebCore::FloatRect&);
    void drawPath(const WebCore::Path&);
    void drawFocusRingPath(const WebCore::Path&, float outlineWidth, const WebCore::Color&);
    void drawFocusRingRects(const Vector<WebCore::FloatRect>&, float outlineOffset, float outlineWidth, const WebCore::Color&);
    void fillRect(const WebCore::FloatRect&, WebCore::RequiresClipToRect);
    void fillRectWithColor(const WebCore::FloatRect&, const WebCore::Color&);
    void fillRectWithGradient(const WebCore::FloatRect&, Ref<WebCore::Gradient>&&);
    void fillRectWithGradientAndSpaceTransform(const WebCore::FloatRect&, Ref<WebCore::Gradient>&&, const WebCore::AffineTransform&, WebCore::RequiresClipToRect);
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
    void strokeRect(const WebCore::FloatRect&, float lineWidth);
#if ENABLE(INLINE_PATH_DATA)
    void strokeLine(const WebCore::PathDataLine&);
    void strokeLineWithColorAndThickness(const WebCore::PathDataLine&, std::optional<WebCore::PackedColor::RGBA> strokeColor, std::optional<float> strokeThickness);
    void strokeArc(const WebCore::PathArc&);
    void strokeClosedArc(const WebCore::PathClosedArc&);
    void strokeQuadCurve(const WebCore::PathDataQuadCurve&);
    void strokeBezierCurve(const WebCore::PathDataBezierCurve&);
#endif
    void strokePathSegment(const WebCore::PathSegment&);
    void strokePath(const WebCore::Path&);
    void strokeEllipse(const WebCore::FloatRect&);
    void clearRect(const WebCore::FloatRect&);
    void drawControlPart(Ref<WebCore::ControlPart>&&, const WebCore::FloatRoundedRect& borderRect, float deviceScaleFactor, const WebCore::ControlStyle&);
#if USE(CG)
    void applyStrokePattern();
    void applyFillPattern();
#endif
    void applyDeviceScaleFactor(float);
    std::optional<WebKit::SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    void beginPage(const WebCore::IntSize& pageSize);
    void endPage();

    void setURLForRect(const URL&, const WebCore::FloatRect&);

private:
    RemoteDisplayListRecorder(WebCore::ImageBuffer&, RemoteDisplayListRecorderIdentifier, RemoteRenderingBackend&);

    void drawFilteredImageBufferInternal(std::optional<WebCore::RenderingResourceIdentifier> sourceImageIdentifier, const WebCore::FloatRect& sourceImageRect, WebCore::Filter&, WebCore::FilterResults&);

    RemoteResourceCache& resourceCache() const;
    WebCore::GraphicsContext& context() { return m_imageBuffer->context(); }
    RefPtr<WebCore::ImageBuffer> imageBuffer(WebCore::RenderingResourceIdentifier) const;
    std::optional<WebCore::SourceImage> sourceImage(WebCore::RenderingResourceIdentifier) const;

    void startListeningForIPC();
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

#if PLATFORM(COCOA) && ENABLE(VIDEO)
    SharedVideoFrameReader& sharedVideoFrameReader();
    void setSharedVideoFrameSemaphore(IPC::Semaphore&&);
    void setSharedVideoFrameMemory(WebCore::SharedMemory::Handle&&);
#endif

    const Ref<WebCore::ImageBuffer> m_imageBuffer;
    const RemoteDisplayListRecorderIdentifier m_identifier;
    const Ref<RemoteRenderingBackend> m_renderingBackend;
    const Ref<RemoteSharedResourceCache> m_sharedResourceCache;
    RefPtr<WebCore::ControlFactory> m_controlFactory;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    std::unique_ptr<SharedVideoFrameReader> m_sharedVideoFrameReader;
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
