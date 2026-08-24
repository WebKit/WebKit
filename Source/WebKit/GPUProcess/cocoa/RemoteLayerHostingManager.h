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
#include <WebCore/FloatSize.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS CALayer;

namespace IPC {
class Connection;
}

namespace WTF {
struct MachSendRightAnnotated;
}

namespace WebKit {

class GPUProcess;
class LayerHostingContext;

class RemoteLayerHostingManager
    : public RefCounted<RemoteLayerHostingManager>
    , private IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerHostingManager);
public:
    static Ref<RemoteLayerHostingManager> create(GPUProcess&);
    virtual ~RemoteLayerHostingManager();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void invalidate();
    void createRemoteLayerForPlayer(PlaybackSessionContextIdentifier, RetainPtr<CALayer>&&, bool canShowWhileLocked);
    void removeRemoteLayerForPlayer(PlaybackSessionContextIdentifier);
    void removeAllRemoteLayersForProcess(WebCore::ProcessIdentifier);
    void setContentLayerForRemoteLayer(PlaybackSessionContextIdentifier, RetainPtr<CALayer>&&);
    uint64_t remoteLayerCountForTesting() const { return m_remoteLayers.size(); }

private:
    friend class GPUProcess;

    explicit RemoteLayerHostingManager(GPUProcess&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    // Messages from RemoteLayerHostingManagerProxy
    void setRemoteLayerSizeFenced(PlaybackSessionContextIdentifier, const WebCore::FloatSize&, WTF::MachSendRightAnnotated&&);
    void setRemoteLayerVideoGravityFenced(PlaybackSessionContextIdentifier, const String&, WTF::MachSendRightAnnotated&&);
    void countRemoteLayersForTesting(CompletionHandler<void(uint64_t)>&&);
    void remoteLayerTreeAsTextForTesting(CompletionHandler<void(String&&)>&&);

    void takeAndInvalidateRemoteLayer(PlaybackSessionContextIdentifier);

    struct RemoteLayerInfo {
        std::unique_ptr<LayerHostingContext> hostingContext;
        RetainPtr<CALayer> contentLayer;
        WebCore::FloatSize size;
        String videoGravity;
    };

    WeakPtr<GPUProcess> m_gpuProcess;
    HashMap<PlaybackSessionContextIdentifier, RemoteLayerInfo> m_remoteLayers;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
