/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "RemoteWCLayerTreeHost.h"

#if USE(GRAPHICS_LAYER_WC)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "RemoteGraphicsContextGL.h"
#include "RemoteWCLayerTreeHostMessages.h"
#include "StreamConnectionWorkQueue.h"
#include "WCScene.h"
#include "WCUpdateInfo.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

IPC::StreamConnectionWorkQueue& remoteGraphicsStreamWorkQueue()
{
#if ENABLE(WEBGL)
    return remoteGraphicsContextGLStreamWorkQueue();
#else
    static LazyNeverDestroyed<IPC::StreamConnectionWorkQueue> instance;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        instance.construct("RemoteWCLayerTreeHost work queue"_s); // LazyNeverDestroyed owns the initial ref.
    });
    return instance.get();
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteWCLayerTreeHost);

std::unique_ptr<RemoteWCLayerTreeHost> RemoteWCLayerTreeHost::create(GPUConnectionToWebProcess& connectionToWebProcess, WebKit::WCLayerTreeHostIdentifier identifier, uint64_t nativeWindow, bool usesOffscreenRendering)
{
    return makeUnique<RemoteWCLayerTreeHost>(connectionToWebProcess, identifier, nativeWindow, usesOffscreenRendering);
}

RemoteWCLayerTreeHost::RemoteWCLayerTreeHost(GPUConnectionToWebProcess& connectionToWebProcess, WebKit::WCLayerTreeHostIdentifier identifier, uint64_t nativeWindow, bool usesOffscreenRendering)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_webProcessIdentifier(connectionToWebProcess.webProcessIdentifier())
    , m_identifier(identifier)
    , m_sharedSceneContextHolder(connectionToWebProcess.gpuProcess().sharedSceneContext().ensureHolderForWindow(nativeWindow))
{
    m_connectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteWCLayerTreeHost::messageReceiverName(), m_identifier.toUInt64(), *this);
    m_scene = makeUnique<WCScene>(m_webProcessIdentifier, usesOffscreenRendering);

    remoteGraphicsStreamWorkQueue().dispatch([scene = m_scene.get(), sceneContextHolder = m_sharedSceneContextHolder.get(), nativeWindow] {
        if (!sceneContextHolder->context)
            sceneContextHolder->context.emplace(nativeWindow);
        scene->initialize(*sceneContextHolder->context);
    });
}

RemoteWCLayerTreeHost::~RemoteWCLayerTreeHost()
{
    ASSERT(m_connectionToWebProcess);
    m_connectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteWCLayerTreeHost::messageReceiverName(), m_identifier.toUInt64());
    auto sceneContextHolder = m_connectionToWebProcess->gpuProcess().sharedSceneContext().removeHolder(m_sharedSceneContextHolder.releaseNonNull());

    remoteGraphicsStreamWorkQueue().dispatch([sceneContextHolder = WTFMove(sceneContextHolder), scene = WTFMove(m_scene)]() mutable {
        // Destroy scene on the StreamWorkQueue thread.
        scene = nullptr;
        // sceneContextHolder can be destroyed on the StreamWorkQueue thread because it hasOneRef.
    });
}

IPC::Connection* RemoteWCLayerTreeHost::messageSenderConnection() const
{
    return &m_connectionToWebProcess->connection();
}

uint64_t RemoteWCLayerTreeHost::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

void RemoteWCLayerTreeHost::update(WCUpdateInfo&& update, CompletionHandler<void(std::optional<WebKit::UpdateInfo>)>&& completionHandler)
{
    remoteGraphicsStreamWorkQueue().dispatch([scene = m_scene.get(), update = WTFMove(update), completionHandler = WTFMove(completionHandler)]() mutable {
        auto updateInfo = scene->update(WTFMove(update));
        RunLoop::main().dispatch([updateInfo = WTFMove(updateInfo), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(updateInfo));
        });
    });
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
