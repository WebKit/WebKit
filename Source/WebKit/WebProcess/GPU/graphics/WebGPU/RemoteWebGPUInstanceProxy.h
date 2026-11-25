/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

// The WebGPU::Instance of the Web Process. It has no object of its own in the GPU Process: it
// sends through the RemoteGPU of its parent, as RemoteGPUProxy did before there was an instance.
class RemoteWebGPUInstanceProxy final : public ::WebGPU::Instance {
    WTF_MAKE_TZONE_ALLOCATED(RemoteWebGPUInstanceProxy);
public:
    static Ref<RemoteWebGPUInstanceProxy> create(RemoteGPUProxy& parent)
    {
        return adoptRef(*new RemoteWebGPUInstanceProxy(parent));
    }

    virtual ~RemoteWebGPUInstanceProxy();

    RemoteGPUProxy& parent() const { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

    ::WebGPU::DispatchEventsResult dispatchEvents(Seconds timeout) final;
    void requestAdapter(const ::WebGPU::RequestAdapterOptions&, CompletionHandler<void(RefPtr<::WebGPU::Adapter>&&)>&&) final;
#if PLATFORM(COCOA)
    RefPtr<::WebGPU::PresentationContext> createPresentationContext(const ::WebGPU::PresentationContextDescriptor&) final;
#endif
    // The instance has no GPU Process counterpart to label.
    void setLabel(String&&) final { }
    bool isValid() const final;
    // The instance only sends RequestAdapter, synchronously, so no work in flight on other threads
    // holds it.
    void destroy() final { }

private:
    explicit RemoteWebGPUInstanceProxy(RemoteGPUProxy&);

    RemoteWebGPUInstanceProxy(const RemoteWebGPUInstanceProxy&) = delete;
    RemoteWebGPUInstanceProxy(RemoteWebGPUInstanceProxy&&) = delete;
    RemoteWebGPUInstanceProxy& operator=(const RemoteWebGPUInstanceProxy&) = delete;
    RemoteWebGPUInstanceProxy& operator=(RemoteWebGPUInstanceProxy&&) = delete;

    template<typename T>
    [[nodiscard]] IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return protect(root().streamClientConnection())->sendSync(std::forward<T>(message), root().backing());
    }

    const Ref<RemoteGPUProxy> m_parent;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
