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
#include "RemotePresentationContextProxy.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUCompositorIntegration.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteCompositorIntegrationProxy final : public PAL::WebGPU::CompositorIntegration {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteCompositorIntegrationProxy> create(RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteCompositorIntegrationProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteCompositorIntegrationProxy();

    RemoteGPUProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent; }

    void setPresentationContext(RemotePresentationContextProxy& presentationContext)
    {
        ASSERT(!m_presentationContext);
        m_presentationContext = &presentationContext;
    }

private:
    friend class DowncastConvertToBackingContext;

    RemoteCompositorIntegrationProxy(RemoteGPUProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteCompositorIntegrationProxy(const RemoteCompositorIntegrationProxy&) = delete;
    RemoteCompositorIntegrationProxy(RemoteCompositorIntegrationProxy&&) = delete;
    RemoteCompositorIntegrationProxy& operator=(const RemoteCompositorIntegrationProxy&) = delete;
    RemoteCompositorIntegrationProxy& operator=(RemoteCompositorIntegrationProxy&&) = delete;

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

#if PLATFORM(COCOA)
    Vector<MachSendRight> recreateRenderBuffers(int width, int height) override;
#endif

    void prepareForDisplay(CompletionHandler<void()>&&) override;

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteGPUProxy> m_parent;
    RefPtr<RemotePresentationContextProxy> m_presentationContext;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
