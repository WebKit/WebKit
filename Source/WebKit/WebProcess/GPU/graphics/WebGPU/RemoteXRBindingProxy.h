/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "RemoteGPUProxy.h"
#include "RemotePresentationContextProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUXRBinding.h>
#include <WebCore/WebGPUXREye.h>

namespace WebCore {
class WebXRFrame;
}

namespace WebCore::WebGPU {
class Device;
class XRProjectionLayer;
class XRView;
}

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteXRBindingProxy final : public WebCore::WebGPU::XRBinding {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteXRBindingProxy> create(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteXRBindingProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteXRBindingProxy();

    RemoteDeviceProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteXRBindingProxy(RemoteDeviceProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteXRBindingProxy(const RemoteXRBindingProxy&) = delete;
    RemoteXRBindingProxy(RemoteXRBindingProxy&&) = delete;
    RemoteXRBindingProxy& operator=(const RemoteXRBindingProxy&) = delete;
    RemoteXRBindingProxy& operator=(RemoteXRBindingProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }

    RefPtr<WebCore::WebGPU::XRProjectionLayer> createProjectionLayer(const WebCore::WebGPU::XRProjectionLayerInit&) final;
    RefPtr<WebCore::WebGPU::XRSubImage> getSubImage(WebCore::WebGPU::XRProjectionLayer&, WebCore::WebXRFrame&, std::optional<WebCore::WebGPU::XREye>/* = "none"*/) final;
    RefPtr<WebCore::WebGPU::XRSubImage> getViewSubImage(WebCore::WebGPU::XRProjectionLayer&, WebCore::WebGPU::XREye) final;
    WebCore::WebGPU::TextureFormat getPreferredColorFormat() final;

    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().streamClientConnection().send(WTFMove(message), backing());
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().streamClientConnection().sendSync(WTFMove(message), backing());
    }

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteDeviceProxy> m_parent;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
