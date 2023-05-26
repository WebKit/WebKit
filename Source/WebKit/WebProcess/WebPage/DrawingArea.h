/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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

#include "DrawingAreaInfo.h"
#include "MessageReceiver.h"
#include <WebCore/DisplayRefreshMonitorFactory.h>
#include <WebCore/IntSize.h>

namespace IPC {
class Connection;
class Decoder;
struct AsyncReplyIDType;
using AsyncReplyID = AtomicObjectIdentifier<AsyncReplyIDType>;
}

namespace WebCore {
class DestinationColorSpace;
class FloatPoint;
class FloatRect;
class Frame;
class GraphicsLayer;
class GraphicsLayerFactory;
class IntRect;
class LocalFrameView;
class TiledBacking;
enum LayoutMilestone : uint16_t;
enum class ActivityState : uint16_t;
enum class DelegatedScrollingMode : uint8_t;
struct FrameIdentifierType;
struct PlatformLayerIdentifierType;
struct ViewportAttributes;
template<typename> class ProcessQualified;
using FrameIdentifier = ProcessQualified<ObjectIdentifier<FrameIdentifierType>>;
using PlatformLayerIdentifier = ProcessQualified<ObjectIdentifier<PlatformLayerIdentifierType>>;
}

OBJC_CLASS CABasicAnimation;

namespace WebKit {

class LayerTreeHost;
class WebPage;
enum class LayerHostingMode : uint8_t;
struct WebPageCreationParameters;
struct WebPreferencesStore;

class DrawingArea : public IPC::MessageReceiver, public WebCore::DisplayRefreshMonitorFactory {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DrawingArea);

public:
    static std::unique_ptr<DrawingArea> create(WebPage&, const WebPageCreationParameters&);
    virtual ~DrawingArea();
    
    DrawingAreaType type() const { return m_type; }
    DrawingAreaIdentifier identifier() const { return m_identifier; }

    static bool supportsGPUProcessRendering(DrawingAreaType);

    virtual void setNeedsDisplay() = 0;
    virtual void setNeedsDisplayInRect(const WebCore::IntRect&) = 0;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) = 0;

    virtual void sendEnterAcceleratedCompositingModeIfNeeded() { }

    // FIXME: These should be pure virtual.
    virtual void forceRepaint() { }
    virtual void forceRepaintAsync(WebPage&, CompletionHandler<void()>&&) = 0;
    virtual void setLayerTreeStateIsFrozen(bool) { }
    virtual bool layerTreeStateIsFrozen() const { return false; }

    virtual void updatePreferences(const WebPreferencesStore&) { }
    virtual void mainFrameContentSizeChanged(WebCore::FrameIdentifier, const WebCore::IntSize&);

#if PLATFORM(COCOA)
    virtual void setViewExposedRect(std::optional<WebCore::FloatRect>) = 0;
    virtual std::optional<WebCore::FloatRect> viewExposedRect() const = 0;

    virtual void acceleratedAnimationDidStart(WebCore::PlatformLayerIdentifier, const String& key, MonotonicTime startTime);
    virtual void acceleratedAnimationDidEnd(WebCore::PlatformLayerIdentifier, const String& key);
    virtual void addFence(const WTF::MachSendRight&) { }

    virtual WebCore::FloatRect exposedContentRect() const = 0;
    virtual void setExposedContentRect(const WebCore::FloatRect&) = 0;
#endif

    virtual void mainFrameScrollabilityChanged(bool) { }

    virtual bool supportsAsyncScrolling() const { return false; }
    virtual bool usesDelegatedPageScaling() const { return false; }
    virtual WebCore::DelegatedScrollingMode delegatedScrollingMode() const;

    virtual void registerScrollingTree() { }
    virtual void unregisterScrollingTree() { }

    virtual bool shouldUseTiledBackingForFrameView(const WebCore::LocalFrameView&) const { return false; }

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() { return nullptr; }
    virtual void setRootCompositingLayer(WebCore::Frame&, WebCore::GraphicsLayer*) = 0;
    virtual void addRootFrame(WebCore::FrameIdentifier);
    // FIXME: Add a corresponding removeRootFrame.
    virtual void triggerRenderingUpdate() = 0;

    virtual void willStartRenderingUpdateDisplay();
    virtual void didCompleteRenderingUpdateDisplay();
    // Called after didCompleteRenderingUpdateDisplay, but in the same run loop iteration.
    virtual void didCompleteRenderingFrame();

    virtual void dispatchAfterEnsuringUpdatedScrollPosition(WTF::Function<void ()>&&);

    virtual void activityStateDidChange(OptionSet<WebCore::ActivityState>, ActivityStateChangeID, CompletionHandler<void()>&&);
    virtual void setLayerHostingMode(LayerHostingMode) { }

    virtual void tryMarkLayersVolatile(CompletionHandler<void(bool)>&&);

    virtual void attachViewOverlayGraphicsLayer(WebCore::FrameIdentifier, WebCore::GraphicsLayer*);

    virtual std::optional<WebCore::DestinationColorSpace> displayColorSpace() const;

    virtual bool addMilestonesToDispatch(OptionSet<WebCore::LayoutMilestone>) { return false; }

#if PLATFORM(COCOA)
    virtual void updateGeometry(const WebCore::IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight& fencePort, CompletionHandler<void()>&&) = 0;
#endif

#if USE(GRAPHICS_LAYER_WC)
    virtual void updateGeometryWC(uint64_t, WebCore::IntSize) { };
#endif

#if USE(COORDINATED_GRAPHICS) || USE(GRAPHICS_LAYER_TEXTURE_MAPPER)
    virtual void layerHostDidFlushLayers() { }
#endif

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    virtual void updateGeometry(const WebCore::IntSize&, CompletionHandler<void()>&&) = 0;
    virtual void didChangeViewportAttributes(WebCore::ViewportAttributes&&) = 0;
    virtual void deviceOrPageScaleFactorChanged() = 0;
#endif

    virtual void adoptLayersFromDrawingArea(DrawingArea&) { }
    virtual void adoptDisplayRefreshMonitorsFromDrawingArea(DrawingArea&) { }

    virtual void setNextRenderingUpdateRequiresSynchronousImageDecoding() { }

    void removeMessageReceiverIfNeeded();
    
    WebCore::TiledBacking* mainFrameTiledBacking() const;
    void prepopulateRectForZoom(double scale, WebCore::FloatPoint origin);
    void setShouldScaleViewToFitDocument(bool);
    void scaleViewToFitDocumentIfNeeded();
    
    static RetainPtr<CABasicAnimation> transientZoomSnapAnimationForKeyPath(ASCIILiteral);

protected:
    DrawingArea(DrawingAreaType, DrawingAreaIdentifier, WebPage&);

    template<typename T> inline bool send(T&& message);

    const DrawingAreaType m_type;
    DrawingAreaIdentifier m_identifier;
    WebPage& m_webPage;
    WebCore::IntSize m_lastViewSizeForScaleToFit;
    WebCore::IntSize m_lastDocumentSizeForScaleToFit;
    bool m_isScalingViewToFitDocument { false };
    bool m_shouldScaleViewToFitDocument { false };

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Message handlers.
    // FIXME: These should be pure virtual.
#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    virtual void targetRefreshRateDidChange(unsigned /*rate*/) { }
    virtual void setDeviceScaleFactor(float) { }
#endif
    virtual void displayDidRefresh() { }

    // DisplayRefreshMonitorFactory.
    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID) override;

#if PLATFORM(COCOA)
    virtual void setDeviceScaleFactor(float) { }
    virtual void setColorSpace(std::optional<WebCore::DestinationColorSpace>);

    virtual void dispatchAfterEnsuringDrawing(IPC::AsyncReplyID) = 0;
#endif

    virtual void adjustTransientZoom(double scale, WebCore::FloatPoint);
    virtual void commitTransientZoom(double scale, WebCore::FloatPoint);

    bool m_hasRemovedMessageReceiver { false };
};

} // namespace WebKit

#define SPECIALIZE_TYPE_TRAITS_DRAWING_AREA(ToValueTypeName, AreaType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ToValueTypeName) \
    static bool isType(const WebKit::DrawingArea& area) { return area.type() == WebKit::AreaType; } \
SPECIALIZE_TYPE_TRAITS_END()
