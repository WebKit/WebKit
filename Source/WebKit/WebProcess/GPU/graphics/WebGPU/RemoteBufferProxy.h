/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUBuffer.h>
#include <wtf/Deque.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteBufferProxy final : public PAL::WebGPU::Buffer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteBufferProxy> create(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteBufferProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteBufferProxy();

    RemoteDeviceProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteBufferProxy(RemoteDeviceProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteBufferProxy(const RemoteBufferProxy&) = delete;
    RemoteBufferProxy(RemoteBufferProxy&&) = delete;
    RemoteBufferProxy& operator=(const RemoteBufferProxy&) = delete;
    RemoteBufferProxy& operator=(RemoteBufferProxy&&) = delete;

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

    void mapAsync(PAL::WebGPU::MapModeFlags, PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64> sizeForMap, CompletionHandler<void()>&&) final;
    MappedRange getMappedRange(PAL::WebGPU::Size64 offset, std::optional<PAL::WebGPU::Size64>) final;
    void unmap() final;

    void destroy() final;

    void setLabelInternal(const String&) final;

    Deque<CompletionHandler<void()>> m_callbacks;

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteDeviceProxy> m_parent;
    std::optional<Vector<uint8_t>> m_data;
    PAL::WebGPU::MapModeFlags m_mapModeFlags;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
