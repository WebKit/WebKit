/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <pal/graphics/WebGPU/WebGPUAdapter.h>
#include <wtf/Deque.h>

namespace WebKit {
class RemoteGPUProxy;
}

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteAdapterProxy final : public PAL::WebGPU::Adapter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteAdapterProxy> create(String&& name, PAL::WebGPU::SupportedFeatures& features, PAL::WebGPU::SupportedLimits& limits, bool isFallbackAdapter, RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteAdapterProxy(WTFMove(name), features, limits, isFallbackAdapter, parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteAdapterProxy();

    RemoteGPUProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteAdapterProxy(String&& name, PAL::WebGPU::SupportedFeatures&, PAL::WebGPU::SupportedLimits&, bool isFallbackAdapter, RemoteGPUProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteAdapterProxy(const RemoteAdapterProxy&) = delete;
    RemoteAdapterProxy(RemoteAdapterProxy&&) = delete;
    RemoteAdapterProxy& operator=(const RemoteAdapterProxy&) = delete;
    RemoteAdapterProxy& operator=(RemoteAdapterProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    static inline constexpr Seconds defaultSendTimeout = 30_s;
    template<typename T>
    WARN_UNUSED_RETURN bool send(T&& message)
    {
        return root().streamClientConnection().send(WTFMove(message), backing(), defaultSendTimeout);
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().streamClientConnection().sendSync(WTFMove(message), backing(), defaultSendTimeout);
    }

    void requestDevice(const PAL::WebGPU::DeviceDescriptor&, CompletionHandler<void(Ref<PAL::WebGPU::Device>&&)>&&) final;

    Deque<CompletionHandler<void(Ref<PAL::WebGPU::Device>)>> m_callbacks;

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteGPUProxy> m_parent;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
