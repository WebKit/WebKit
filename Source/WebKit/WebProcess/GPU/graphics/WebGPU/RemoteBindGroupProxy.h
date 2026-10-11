/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteBindGroupProxy final : public ::WebGPU::BindGroup {
    WTF_MAKE_TZONE_ALLOCATED(RemoteBindGroupProxy);
public:
    static Ref<RemoteBindGroupProxy> create(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteBindGroupProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteBindGroupProxy();

    RemoteDeviceProxy& parent() const { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

    bool updateExternalTextures(::WebGPU::ExternalTexture&) final;
    void setLabel(String&&) final;
    bool isValid() const final;

private:
    friend class DowncastConvertToBackingContext;

    RemoteBindGroupProxy(RemoteDeviceProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteBindGroupProxy(const RemoteBindGroupProxy&) = delete;
    RemoteBindGroupProxy(RemoteBindGroupProxy&&) = delete;
    RemoteBindGroupProxy& operator=(const RemoteBindGroupProxy&) = delete;
    RemoteBindGroupProxy& operator=(RemoteBindGroupProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
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

    WebGPUIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteDeviceProxy> m_parent;
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteBindGroupProxy)
    // In the Web Process, every WebGPU::BindGroup is a RemoteBindGroupProxy.
    static bool isType(const ::WebGPU::BindGroup&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
