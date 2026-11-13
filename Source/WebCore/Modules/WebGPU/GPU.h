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

#include <WebCore/JSDOMPromiseDeferredForward.h>
#include <WebCore/WebGPU.h>
#include <optional>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class GPUAdapter;
class GPUCompositorIntegration;
class GPUPresentationContext;
struct GPUPresentationContextDescriptor;
class GraphicsContext;
class NativeImage;
class WGSLLanguageFeatures;

enum class GPUTextureFormat : uint8_t;

struct GPURequestAdapterOptions;

class GPU : public RefCounted<GPU> {
public:
    using CreateBacking = Function<RefPtr<WebGPUIntegration>()>;

    // createBacking makes a new backing for backing() once the current one is lost, as it is when
    // the GPU process exits.
    static Ref<GPU> create(Ref<WebGPUIntegration>&& backing, CreateBacking&& createBacking)
    {
        return adoptRef(*new GPU(WTF::move(backing), WTF::move(createBacking)));
    }
    WEBCORE_EXPORT ~GPU();

    using RequestAdapterPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<GPUAdapter>>>;
    void requestAdapter(const std::optional<GPURequestAdapterOptions>&, RequestAdapterPromise&&);

    NODELETE GPUTextureFormat getPreferredCanvasFormat() const;
    WGSLLanguageFeatures& wgslLanguageFeatures() const { return m_wgslLanguageFeatures; }

    static RefPtr<GPUPresentationContext> createPresentationContext(WebGPUIntegration&, const GPUPresentationContextDescriptor&);

    static RefPtr<GPUCompositorIntegration> createCompositorIntegration(WebGPUIntegration&);

    void paintToCanvas(NativeImage&, const IntSize&, GraphicsContext&);
    // The backing to make new objects from: the current one, or a new one in its place if it is lost.
    // Everything that makes objects from the GPU, such as adapters, canvas contexts and models, has to
    // go through here, so that none of them is made from a backing that cannot create anything.
    Ref<WebGPUIntegration> backing();

private:
    GPU(Ref<WebGPUIntegration>&&, CreateBacking&&);

    void requestAdapter(::WebGPU::RequestAdapterOptions&&, RequestAdapterPromise&&, bool retryIfBackingIsLost);

    struct PendingRequestAdapterArguments;
    Ref<WebGPUIntegration> m_backing;
    const Ref<WGSLLanguageFeatures> m_wgslLanguageFeatures;
    const CreateBacking m_createBacking;
};

}
