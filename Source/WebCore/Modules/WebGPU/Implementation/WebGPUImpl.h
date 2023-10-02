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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPU.h"
#include "WebGPUPtr.h"
#include <WebGPU/WebGPU.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>

namespace WebCore {
class GraphicsContext;
class IntSize;
class NativeImage;
}

namespace WebCore::WebGPU {

class ConvertToBackingContext;

class GPUImpl final : public GPU, public RefCounted<GPUImpl> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<GPUImpl> create(WebGPUPtr<WGPUInstance>&& instance, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new GPUImpl(WTFMove(instance), convertToBackingContext));
    }

    virtual ~GPUImpl();

    void ref() const final { RefCounted<GPUImpl>::ref(); }
    void deref() const final { RefCounted<GPUImpl>::deref(); }

    void paintToCanvas(WebCore::NativeImage&, const WebCore::IntSize&, WebCore::GraphicsContext&) final;

private:
    friend class DowncastConvertToBackingContext;

    GPUImpl(WebGPUPtr<WGPUInstance>&&, ConvertToBackingContext&);

    GPUImpl(const GPUImpl&) = delete;
    GPUImpl(GPUImpl&&) = delete;
    GPUImpl& operator=(const GPUImpl&) = delete;
    GPUImpl& operator=(GPUImpl&&) = delete;

    WGPUInstance backing() const { return m_backing.get(); }

    void requestAdapter(const RequestAdapterOptions&, CompletionHandler<void(RefPtr<Adapter>&&)>&&) final;

    Ref<PresentationContext> createPresentationContext(const PresentationContextDescriptor&) final;

    Ref<CompositorIntegration> createCompositorIntegration() final;

    WebGPUPtr<WGPUInstance> m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
