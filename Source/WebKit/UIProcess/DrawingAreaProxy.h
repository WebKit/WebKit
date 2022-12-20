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

#include "DrawingAreaInfo.h"
#include "GenericCallback.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
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

class DrawingAreaProxy : public IPC::MessageReceiver, protected IPC::MessageSender {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DrawingAreaProxy);

public:
    virtual ~DrawingAreaProxy();

    DrawingAreaType type() const { return m_type; }
    DrawingAreaIdentifier identifier() const { return m_identifier; }

    void startReceivingMessages();

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
    // The timeout we use when waiting for a DidUpdateGeometry message.
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

    virtual void waitForDidUpdateActivityState(ActivityStateChangeID) { }
    
    virtual void dispatchAfterEnsuringDrawing(WTF::Function<void (CallbackBase::Error)>&&) { ASSERT_NOT_REACHED(); }

    // Hide the content until the currently pending update arrives.
    virtual void hideContentUntilPendingUpdate() { ASSERT_NOT_REACHED(); }

    // Hide the content until any update arrives.
    virtual void hideContentUntilAnyUpdate() { ASSERT_NOT_REACHED(); }

    virtual bool hasVisibleContent() const { return true; }

    virtual void willSendUpdateGeometry() { }

    virtual void prepareForAppSuspension() { }

#if PLATFORM(COCOA)
    virtual WTF::MachSendRight createFence();
#endif

    virtual void dispatchPresentationCallbacksAfterFlushingLayers(const Vector<CallbackID>&) { }

    virtual bool shouldCoalesceVisualEditorStateUpdates() const { return false; }
    virtual bool shouldSendWheelEventsToEventDispatcher() const { return false; }

    WebPageProxy& page() const { return m_webPageProxy; }
    WebProcessProxy& process() { return m_process.get(); }
    const WebProcessProxy& process() const { return m_process.get(); }

protected:
    DrawingAreaProxy(DrawingAreaType, WebPageProxy&, WebProcessProxy&);

    DrawingAreaType m_type;
    DrawingAreaIdentifier m_identifier;
    WebPageProxy& m_webPageProxy;
    Ref<WebProcessProxy> m_process;

    WebCore::IntSize m_size;
    WebCore::IntSize m_scrollOffset;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

private:
    virtual void sizeDidChange() = 0;

    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final { return m_identifier.toUInt64(); }
    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>) final;
    bool sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&&, AsyncReplyHandler, OptionSet<IPC::SendOption>) final;

    // Message handlers.
    // FIXME: These should be pure virtual.
    virtual void enterAcceleratedCompositingMode(uint64_t /* backingStoreStateID */, const LayerTreeContext&) { }
    virtual void updateAcceleratedCompositingMode(uint64_t /* backingStoreStateID */, const LayerTreeContext&) { }
    virtual void didFirstLayerFlush(uint64_t /* backingStoreStateID */, const LayerTreeContext&) { }
#if PLATFORM(COCOA)
    virtual void didUpdateGeometry() { }

#if PLATFORM(MAC)
    RunLoop::Timer m_viewExposedRectChangedTimer;
    std::optional<WebCore::FloatRect> m_lastSentViewExposedRect;
#endif // PLATFORM(MAC)
#endif

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    virtual void update(uint64_t /* backingStoreStateID */, const UpdateInfo&) { }
    virtual void didUpdateBackingStoreState(uint64_t /* backingStoreStateID */, const UpdateInfo&, const LayerTreeContext&) { }
    virtual void exitAcceleratedCompositingMode(uint64_t /* backingStoreStateID */, const UpdateInfo&) { }
#endif
    bool m_startedReceivingMessages { false };
};

} // namespace WebKit

#define SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(ToValueTypeName, ProxyType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ToValueTypeName) \
    static bool isType(const WebKit::DrawingAreaProxy& proxy) { return proxy.type() == WebKit::ProxyType; } \
SPECIALIZE_TYPE_TRAITS_END()

