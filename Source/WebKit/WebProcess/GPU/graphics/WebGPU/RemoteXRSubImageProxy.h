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
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUXRSubImage.h>

namespace WebCore {
class ImageBuffer;
class NativeImage;
namespace WebGPU {
class Device;
}
}

namespace WebKit::WebGPU {

class ConvertToBackingContext;
class RemoteTextureProxy;

class RemoteXRSubImageProxy final : public WebCore::WebGPU::XRSubImage {
    WTF_MAKE_TZONE_ALLOCATED(RemoteXRSubImageProxy);
public:
    static Ref<RemoteXRSubImageProxy> create(Ref<RemoteGPUProxy>&& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteXRSubImageProxy(WTFMove(parent), convertToBackingContext, identifier));
    }

    virtual ~RemoteXRSubImageProxy();

    RemoteGPUProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent; }
    Ref<RemoteGPUProxy> protectedRoot() { return m_parent; }

private:
    friend class DowncastConvertToBackingContext;

    RemoteXRSubImageProxy(Ref<RemoteGPUProxy>&&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteXRSubImageProxy(const RemoteXRSubImageProxy&) = delete;
    RemoteXRSubImageProxy(RemoteXRSubImageProxy&&) = delete;
    RemoteXRSubImageProxy& operator=(const RemoteXRSubImageProxy&) = delete;
    RemoteXRSubImageProxy& operator=(RemoteXRSubImageProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    RefPtr<WebCore::WebGPU::Texture> colorTexture() final;
    RefPtr<WebCore::WebGPU::Texture> depthStencilTexture() final;
    RefPtr<WebCore::WebGPU::Texture> motionVectorTexture() final;

    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().protectedStreamClientConnection()->send(WTFMove(message), backing());
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().protectedStreamClientConnection()->sendSync(WTFMove(message), backing());
    }

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteGPUProxy> m_parent;

    RefPtr<RemoteTextureProxy> m_currentTexture;
    RefPtr<RemoteTextureProxy> m_currentDepthTexture;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
