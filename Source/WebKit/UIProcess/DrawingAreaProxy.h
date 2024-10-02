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
#include "MessageSender.h"
#include <WebCore/FloatRect.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/ProcessIdentifier.h>
#include <stdint.h>
#include <wtf/Identified.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TypeCasts.h>
#include <wtf/WeakRef.h>

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
class RemotePageDrawingAreaProxy;
class WebPageProxy;
class WebProcessProxy;

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
struct UpdateInfo;
#endif

class DrawingAreaProxy : public IPC::MessageReceiver, public IPC::MessageSender, public Identified<DrawingAreaIdentifier>, public CanMakeCheckedPtr<DrawingAreaProxy> {
    WTF_MAKE_TZONE_ALLOCATED(DrawingAreaProxy);
    WTF_MAKE_NONCOPYABLE(DrawingAreaProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DrawingAreaProxy);
public:
    virtual ~DrawingAreaProxy();

    DrawingAreaType type() const { return m_type; }

    void startReceivingMessages(WebProcessProxy&);
    void stopReceivingMessages(WebProcessProxy&);
    virtual std::span<IPC::ReceiverName> messageReceiverNames() const;

    virtual WebCore::DelegatedScrollingMode delegatedScrollingMode() const;

    virtual void deviceScaleFactorDidChange(CompletionHandler<void()>&&) = 0;
    virtual void colorSpaceDidChange() { }
    virtual void windowScreenDidChange(WebCore::PlatformDisplayID) { }
    virtual std::optional<WebCore::FramesPerSecond> displayNominalFramesPerSecond() { return std::nullopt; }

    // FIXME: These should be pure virtual.
    virtual void setBackingStoreIsDiscardable(bool) { }

    const WebCore::IntSize& size() const { return m_size; }
    bool setSize(const WebCore::IntSize&, const WebCore::IntSize& scrollOffset = { });

    virtual void minimumSizeForAutoLayoutDidChange() { }
    virtual void sizeToContentAutoSizeMaximumSizeDidChange() { }
    virtual void windowKindDidChange() { }

    virtual void adjustTransientZoom(double, WebCore::FloatPoint) { }
    virtual void commitTransientZoom(double, WebCore::FloatPoint) { }

    virtual void viewIsBecomingVisible() { }
    virtual void viewIsBecomingInvisible() { }

#if PLATFORM(MAC)
    virtual void didChangeViewExposedRect();
    void viewExposedRectChangedTimerFired();
#endif

    virtual void updateDebugIndicator() { }

    virtual void waitForDidUpdateActivityState(ActivityStateChangeID) { }

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

    WebPageProxy& page() const;
    virtual void viewWillStartLiveResize() { };
    virtual void viewWillEndLiveResize() { };

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::MessageSender
    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>) final;
    bool sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&&, AsyncReplyHandler, OptionSet<IPC::SendOption>) final;
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    virtual void addRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy&) { }
    virtual void removeRemotePageDrawingAreaProxy(RemotePageDrawingAreaProxy&) { }

    virtual void remotePageProcessDidTerminate(WebCore::ProcessIdentifier) { }

protected:
    DrawingAreaProxy(DrawingAreaType, WebPageProxy&, WebProcessProxy&);

    Ref<WebPageProxy> protectedWebPageProxy() const;
    Ref<WebProcessProxy> protectedWebProcessProxy() const;

    DrawingAreaType m_type;
    WeakRef<WebPageProxy> m_webPageProxy;
    Ref<WebProcessProxy> m_webProcessProxy;

    WebCore::IntSize m_size;
    WebCore::IntSize m_scrollOffset;

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
    virtual void update(uint64_t /* backingStoreStateID */, UpdateInfo&&) { }
    virtual void exitAcceleratedCompositingMode(uint64_t /* backingStoreStateID */, UpdateInfo&&) { }
#endif
};

} // namespace WebKit

#define SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(ToValueTypeName, ProxyType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ToValueTypeName) \
    static bool isType(const WebKit::DrawingAreaProxy& proxy) { return proxy.type() == WebKit::ProxyType; } \
SPECIALIZE_TYPE_TRAITS_END()

