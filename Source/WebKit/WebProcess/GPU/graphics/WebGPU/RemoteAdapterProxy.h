/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include <WebCore/WebGPUAdapter.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {
class RemoteGPUProxy;
}

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteAdapterProxy final : public WebCore::WebGPU::Adapter {
    WTF_MAKE_TZONE_ALLOCATED(RemoteAdapterProxy);
public:
    static Ref<RemoteAdapterProxy> create(String&& name, WebCore::WebGPU::SupportedFeatures& features, WebCore::WebGPU::SupportedLimits& limits, bool isFallbackAdapter, bool xrCompatible, RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteAdapterProxy(WTFMove(name), features, limits, isFallbackAdapter, xrCompatible, parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteAdapterProxy();

    RemoteGPUProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteAdapterProxy(String&& name, WebCore::WebGPU::SupportedFeatures&, WebCore::WebGPU::SupportedLimits&, bool isFallbackAdapter, bool xrCompatible, RemoteGPUProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteAdapterProxy(const RemoteAdapterProxy&) = delete;
    RemoteAdapterProxy(RemoteAdapterProxy&&) = delete;
    RemoteAdapterProxy& operator=(const RemoteAdapterProxy&) = delete;
    RemoteAdapterProxy& operator=(RemoteAdapterProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    bool xrCompatible() final;
    
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

    void requestDevice(const WebCore::WebGPU::DeviceDescriptor&, CompletionHandler<void(RefPtr<WebCore::WebGPU::Device>&&)>&&) final;

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteGPUProxy> m_parent;
    bool m_xrCompatible { false };
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
