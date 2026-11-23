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

#include "RemoteDeviceProxy.h"
#include "RemoteGPUProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>

namespace WebKit::WebGPU {

class RemoteTextureProxy;

class RemoteXRSubImageProxy final : public ::WebGPU::XRSubImage {
    WTF_MAKE_TZONE_ALLOCATED(RemoteXRSubImageProxy);
public:
    // The device is the one of the XR binding, which creates the textures of the sub-image.
    static Ref<RemoteXRSubImageProxy> create(RemoteDeviceProxy& device, WebGPUXRSubImageIdentifier identifier)
    {
        return adoptRef(*new RemoteXRSubImageProxy(device, identifier));
    }

    virtual ~RemoteXRSubImageProxy();

    RemoteGPUProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent; }
    WebGPUXRSubImageIdentifier backing() const { return m_backing; }

    RefPtr<::WebGPU::Texture> colorTexture() final;
    RefPtr<::WebGPU::Texture> depthStencilTexture() final;
    void setLabel(String&&) final { }
    bool isValid() const final;

private:
    RemoteXRSubImageProxy(RemoteDeviceProxy&, WebGPUXRSubImageIdentifier);

    RemoteXRSubImageProxy(const RemoteXRSubImageProxy&) = delete;
    RemoteXRSubImageProxy(RemoteXRSubImageProxy&&) = delete;
    RemoteXRSubImageProxy& operator=(const RemoteXRSubImageProxy&) = delete;
    RemoteXRSubImageProxy& operator=(RemoteXRSubImageProxy&&) = delete;

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }
    template<typename T>
    [[nodiscard]] IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return protect(root().streamClientConnection())->sendSync(std::forward<T>(message), backing());
    }

    WebGPUXRSubImageIdentifier m_backing;
    const Ref<RemoteGPUProxy> m_parent;
    const Ref<RemoteDeviceProxy> m_device;

    const RefPtr<RemoteTextureProxy> m_currentTexture;
    const RefPtr<RemoteTextureProxy> m_currentDepthTexture;
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteXRSubImageProxy)
    // In the Web Process, every WebGPU::XRSubImage is a RemoteXRSubImageProxy.
    static bool isType(const ::WebGPU::XRSubImage&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
