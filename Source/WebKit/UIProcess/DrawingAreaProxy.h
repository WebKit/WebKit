/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#include "Connection.h"
#include "DrawingAreaInfo.h"
#include "MessageReceiver.h"
#include <WebCore/FloatRect.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <stdint.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/TypeCasts.h>

#if PLATFORM(COCOA)
namespace WTF {
class MachSendRight;
}
#endif

namespace WebCore {
enum class DelegatedScrollingMode : uint8_t;
using FramesPerSecond = unsigned;
using PlatformDisplayID = uint32_t;
}

namespace WebKit {

class LayerTreeContext;
class WebPageProxy;
class WebProcessProxy;

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
class UpdateInfo;
#endif

class DrawingAreaProxy : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DrawingAreaProxy);

public:
    virtual ~DrawingAreaProxy();

    DrawingAreaType type() const { return m_type; }
    DrawingAreaIdentifier identifier() const { return m_identifier; }

    void startReceivingMessages(WebProcessProxy&);
    virtual void attachToProvisionalFrameProcess(WebProcessProxy&) = 0;

    virtual WebCore::DelegatedScrollingMode delegatedScrollingMode() const;

    virtual void deviceScaleFactorDidChange() = 0;
    virtual void colorSpaceDidChange() { }
    virtual void windowScreenDidChange(WebCore::PlatformDisplayID, std::optional<WebCore::FramesPerSecond> /* nominalFramesPerSecond */) { }

    // FIXME: These should be pure virtual.
    virtual void setBackingStoreIsDiscardable(bool) { }

    virtual void waitForBackingStoreUpdateOnNextPaint() { }

    const WebCore::IntSize& size() const { return m_size; }
    bool setSize(const WebCore::IntSize&, const WebCore::IntSize& scrollOffset = { });

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    // The timeout we use when waiting for a UpdateGeometry reply.
    static constexpr Seconds didUpdateBackingStoreStateTimeout() { return Seconds::fromMilliseconds(500); }
    virtual void targetRefreshRateDidChange(unsigned) { }
#endif

    virtual void minimumSizeForAutoLayoutDidChange() { }
    virtual void sizeToContentAutoSizeMaximumSizeDidChange() { }
    virtual void windowKindDidChange() { }

    virtual void adjustTransientZoom(double, WebCore::FloatPoint) { }
    virtual void commitTransientZoom(double, WebCore::FloatPoint) { }

#if PLATFORM(MAC)
    virtual void didChangeViewExposedRect();
    void viewExposedRectChangedTimerFired();
#endif

    virtual void updateDebugIndicator() { }

    virtual void waitForDidUpdateActivityState(ActivityStateChangeID, WebProcessProxy&) { }
    
    virtual void dispatchAfterEnsuringDrawing(CompletionHandler<void()>&&) = 0;

    // Hide the content until the currently pending update arrives.
    virtual void hideContentUntilPendingUpdate() { ASSERT_NOT_REACHED(); }

    // Hide the content until any update arrives.
    virtual void hideContentUntilAnyUpdate() { ASSERT_NOT_REACHED(); }

    virtual bool hasVisibleContent() const { return true; }

    virtual void prepareForAppSuspension() { }

#if PLATFORM(COCOA)
    virtual WTF::MachSendRight createFence();
#endif

    virtual void dispatchPresentationCallbacksAfterFlushingLayers(IPC::Connection&, Vector<IPC::AsyncReplyID>&&) { }

    virtual bool shouldCoalesceVisualEditorStateUpdates() const { return false; }
    virtual bool shouldSendWheelEventsToEventDispatcher() const { return false; }

    WebPageProxy& page() const { return m_webPageProxy; }

protected:
    DrawingAreaProxy(DrawingAreaType, WebPageProxy&);

    DrawingAreaType m_type;
    DrawingAreaIdentifier m_identifier;
    WebPageProxy& m_webPageProxy;
    Vector<Ref<WebProcessProxy>> m_processesWithRegisteredDrawingAreaProxyMessageReceiver;

    WebCore::IntSize m_size;
    WebCore::IntSize m_scrollOffset;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

private:
    virtual void sizeDidChange() = 0;

    // Message handlers.
    // FIXME: These should be pure virtual.
    virtual void enterAcceleratedCompositingMode(uint64_t /* backingStoreStateID */, const LayerTreeContext&) { }
    virtual void updateAcceleratedCompositingMode(uint64_t /* backingStoreStateID */, const LayerTreeContext&) { }
    virtual void didFirstLayerFlush(uint64_t /* backingStoreStateID */, const LayerTreeContext&) { }
#if PLATFORM(MAC)
    RunLoop::Timer m_viewExposedRectChangedTimer;
    std::optional<WebCore::FloatRect> m_lastSentViewExposedRect;
#endif // PLATFORM(MAC)

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    virtual void update(uint64_t /* backingStoreStateID */, const UpdateInfo&) { }
    virtual void didUpdateBackingStoreState(uint64_t /* backingStoreStateID */, const UpdateInfo&, const LayerTreeContext&) { }
    virtual void exitAcceleratedCompositingMode(uint64_t /* backingStoreStateID */, const UpdateInfo&) { }
#endif
};

} // namespace WebKit

#define SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(ToValueTypeName, ProxyType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ToValueTypeName) \
    static bool isType(const WebKit::DrawingAreaProxy& proxy) { return proxy.type() == WebKit::ProxyType; } \
SPECIALIZE_TYPE_TRAITS_END()

