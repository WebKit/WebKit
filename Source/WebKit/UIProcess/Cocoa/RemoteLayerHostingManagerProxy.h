/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "PlaybackSessionContextIdentifier.h"
#include <WebCore/CocoaView.h>
#include <WebCore/FloatRect.h>
#include <WebCore/HostingContext.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS CALayer;
OBJC_CLASS WKVideoLayerHostView;

namespace WTF {
struct MachSendRightAnnotated;
}

namespace WebKit {

class GPUProcessProxy;
class WebPageProxy;

using LayerHostingContextID = uint32_t;

class RemoteLayerHostingManagerProxy
    : public RefCounted<RemoteLayerHostingManagerProxy>
    , public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerHostingManagerProxy);
public:
    static Ref<RemoteLayerHostingManagerProxy> create(GPUProcessProxy&);
    virtual ~RemoteLayerHostingManagerProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void invalidate();

    RetainPtr<WKVideoLayerHostView> ensureLayerHostViewForIdentifier(PlaybackSessionContextIdentifier);

    void setPageForIdentifier(PlaybackSessionContextIdentifier, WebPageProxy&);

    void releaseLayerHostForIdentifier(PlaybackSessionContextIdentifier);

    void setRemoteLayerHostSize(PlaybackSessionContextIdentifier, const WebCore::FloatSize&);

    void setRemoteLayerVideoGravity(PlaybackSessionContextIdentifier, const String&);

#if USE(EXTENSIONKIT)
    void addVisibilityPropogationView(PlaybackSessionContextIdentifier);
    void removeVisibilityPropogationView(PlaybackSessionContextIdentifier);
#endif

    uint64_t hostedLayerCountForTesting() const { return m_hostedLayers.size(); }
    void countGPUProcessRemoteLayersForTesting(CompletionHandler<void(uint64_t)>&&);

    String gpuProcessRemoteLayerTreeAsTextForTesting();

private:
    friend class GPUProcessProxy;

    explicit RemoteLayerHostingManagerProxy(GPUProcessProxy&);

    void runWithFenceForIdentifier(PlaybackSessionContextIdentifier, Function<void(WTF::MachSendRightAnnotated&&)>&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Messages from RemoteLayerHostingManager (GPUProcess → UIProcess)
    void didCreateRemoteLayer(PlaybackSessionContextIdentifier, WebCore::HostingContext&&);
    void didRemoveRemoteLayer(PlaybackSessionContextIdentifier);

    struct RemoteLayerHostInfo {
        RetainPtr<WKVideoLayerHostView> hostView;
        WebCore::HostingContext hostingContext;
        WebCore::FloatSize size;
        WeakPtr<WebPageProxy> page;
        String videoGravity;

        bool hasGPUProcessVideoLayer { false };
        bool hasUIProcessConsumer { false };
    };

    RemoteLayerHostInfo& ensureRemoteLayerHostInfo(PlaybackSessionContextIdentifier);
    void applyHostingContextToLayerHost(RemoteLayerHostInfo&);
    void removeRemoteLayerHostIfUnused(PlaybackSessionContextIdentifier);

    WeakPtr<GPUProcessProxy> m_gpuProcess;
    HashMap<PlaybackSessionContextIdentifier, RemoteLayerHostInfo> m_hostedLayers;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
