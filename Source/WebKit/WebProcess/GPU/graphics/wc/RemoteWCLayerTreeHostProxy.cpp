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
#include "RemoteWCLayerTreeHostProxy.h"

#if USE(GRAPHICS_LAYER_WC)

#include "GPUConnectionToWebProcess.h"
#include "MessageSenderInlines.h"
#include "RemoteWCLayerTreeHostMessages.h"
#include "WCUpdateInfo.h"
#include "WebPage.h"
#include "WebProcess.h"

namespace WebKit {

RemoteWCLayerTreeHostProxy::RemoteWCLayerTreeHostProxy(WebPage& page, bool usesOffscreenRendering)
    : m_page(page)
    , m_usesOffscreenRendering(usesOffscreenRendering)
{
}

RemoteWCLayerTreeHostProxy::~RemoteWCLayerTreeHostProxy()
{
    disconnectGpuProcessIfNeeded();
}

IPC::Connection* RemoteWCLayerTreeHostProxy::messageSenderConnection() const
{
    return &const_cast<RemoteWCLayerTreeHostProxy&>(*this).ensureGPUProcessConnection().connection();
}

GPUProcessConnection& RemoteWCLayerTreeHostProxy::ensureGPUProcessConnection()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection) {
        gpuProcessConnection = &WebProcess::singleton().ensureGPUProcessConnection();
        m_gpuProcessConnection = gpuProcessConnection;
        gpuProcessConnection->addClient(*this);
        gpuProcessConnection->connection().send(
            Messages::GPUConnectionToWebProcess::CreateWCLayerTreeHost(wcLayerTreeHostIdentifier(), m_page.nativeWindowHandle(), m_usesOffscreenRendering),
            0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    }
    return *gpuProcessConnection;
}

void RemoteWCLayerTreeHostProxy::disconnectGpuProcessIfNeeded()
{
    if (auto gpuProcessConnection = std::exchange(m_gpuProcessConnection, nullptr).get()) {
        gpuProcessConnection->connection().send(Messages::GPUConnectionToWebProcess::ReleaseWCLayerTreeHost(wcLayerTreeHostIdentifier()), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    }
}

void RemoteWCLayerTreeHostProxy::gpuProcessConnectionDidClose(GPUProcessConnection& previousConnection)
{
    m_gpuProcessConnection = nullptr;
}

uint64_t RemoteWCLayerTreeHostProxy::messageSenderDestinationID() const
{
    return wcLayerTreeHostIdentifier().toUInt64();
}

void RemoteWCLayerTreeHostProxy::update(WCUpdateInfo&& updateInfo, CompletionHandler<void(std::optional<WebKit::UpdateInfo>)>&& completionHandler)
{
    sendWithAsyncReply(Messages::RemoteWCLayerTreeHost::Update(updateInfo), WTFMove(completionHandler));
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
