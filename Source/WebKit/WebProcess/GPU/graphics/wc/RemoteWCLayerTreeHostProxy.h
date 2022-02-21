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

#pragma once

#if USE(GRAPHICS_LAYER_WC)

#include "GPUProcessConnection.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "UpdateInfo.h"
#include "WCLayerTreeHostIdentifier.h"

namespace WebKit {

struct WCUpateInfo;

class RemoteWCLayerTreeHostProxy
    : private IPC::MessageSender
    , private GPUProcessConnection::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteWCLayerTreeHostProxy(WebPage&, bool usesOffscreenRendering);
    ~RemoteWCLayerTreeHostProxy();

    void update(WCUpateInfo&&, CompletionHandler<void(std::optional<WebKit::UpdateInfo>)>&&);

private:
    WCLayerTreeHostIdentifier wcLayerTreeHostIdentifier() const { return m_wcLayerTreeHostIdentifier; };
    GPUProcessConnection& ensureGPUProcessConnection();
    void disconnectGpuProcessIfNeeded();

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    WeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    WCLayerTreeHostIdentifier m_wcLayerTreeHostIdentifier { WCLayerTreeHostIdentifier::generate() };
    WebPage& m_page;
    bool m_usesOffscreenRendering { false };
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
