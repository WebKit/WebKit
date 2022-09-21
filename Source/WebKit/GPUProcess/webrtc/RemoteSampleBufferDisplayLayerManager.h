/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "Connection.h"
#include "RemoteSampleBufferDisplayLayerManagerMessagesReplies.h"
#include "SampleBufferDisplayLayerIdentifier.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/IntSize.h>
#include <wtf/HashMap.h>

namespace IPC {
class Decoder;
}

namespace WebCore {
class IntSize;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteSampleBufferDisplayLayer;

class RemoteSampleBufferDisplayLayerManager final : public IPC::WorkQueueMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteSampleBufferDisplayLayerManager> create(GPUConnectionToWebProcess& connection)
    {
        auto instance = adoptRef(*new RemoteSampleBufferDisplayLayerManager(connection));
        instance->startListeningForIPC();
        return instance;
    }
    ~RemoteSampleBufferDisplayLayerManager();

    void close();

    bool allowsExitUnderMemoryPressure() const;

private:
    explicit RemoteSampleBufferDisplayLayerManager(GPUConnectionToWebProcess&);
    void startListeningForIPC();

    // IPC::WorkQueueMessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);

    using LayerCreationCallback = CompletionHandler<void(std::optional<LayerHostingContextID>)>&&;
    void createLayer(SampleBufferDisplayLayerIdentifier, bool hideRootLayer, WebCore::IntSize, LayerCreationCallback);
    void releaseLayer(SampleBufferDisplayLayerIdentifier);

    GPUConnectionToWebProcess& m_connectionToWebProcess;
    Ref<IPC::Connection> m_connection;
    Ref<WorkQueue> m_queue;
    HashMap<SampleBufferDisplayLayerIdentifier, std::unique_ptr<RemoteSampleBufferDisplayLayer>> m_layers;
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
