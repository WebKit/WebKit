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

#include "config.h"
#include "GPU.h"

#include "GPUAdapter.h"
#include "GPUPresentationContext.h"
#include "GPUPresentationContextDescriptor.h"
#include "GPURequestAdapterOptions.h"
#include "GPUTextureFormat.h"
#include "JSDOMPromiseDeferred.h"
#include "JSGPUAdapter.h"
#include "JSWGSLLanguageFeatures.h"
#include "WGSLLanguageFeatures.h"

namespace WebCore {

GPU::GPU(Ref<WebGPUIntegration>&& backing, CreateBacking&& createBacking)
    : m_backing(WTF::move(backing))
    , m_wgslLanguageFeatures(WGSLLanguageFeatures::create())
    , m_createBacking(WTF::move(createBacking))
{
}

GPU::~GPU() = default;

NODELETE static WebGPU::RequestAdapterOptions convertToBacking(const std::optional<GPURequestAdapterOptions>& options)
{
    if (!options)
        return { std::nullopt, false };

    return options->convertToBacking();
}

struct GPU::PendingRequestAdapterArguments {
    std::optional<GPURequestAdapterOptions> options;
    RequestAdapterPromise promise;
};

void GPU::requestAdapter(const std::optional<GPURequestAdapterOptions>& options, RequestAdapterPromise&& promise)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpu-requestadapter
    // A feature level that is spelled out but is not one we recognize gets no adapter at all, rather
    // than being treated as if it had been left out.
    if (options && !options->featureLevel.isNull() && options->featureLevel != "core"_s && options->featureLevel != "compatibility"_s) {
        promise.resolve(nullptr);
        return;
    }

    requestAdapter(convertToBacking(options), WTF::move(promise), true);
}

Ref<WebGPUIntegration> GPU::backing()
{
    // A backing whose GPU process has exited stays lost, so whatever is made from here on has to come
    // from a new one. There is no backoff: if the GPU process cannot come back, each new backing is
    // lost straight away, and each call that finds it lost makes another, with its own connection.
    // That is bounded by how often the page asks for something, which then fails as it would anyway.
    if (protect(m_backing)->isLost() && m_createBacking) {
        if (RefPtr backing = m_createBacking())
            m_backing = backing.releaseNonNull();
    }
    return m_backing;
}

void GPU::requestAdapter(WebGPU::RequestAdapterOptions&& options, RequestAdapterPromise&& promise, bool retryIfBackingIsLost)
{
    Ref backing = this->backing();
    backing->requestAdapter(options, [protectedThis = Ref { *this }, options, promise = WTF::move(promise), backing, retryIfBackingIsLost](RefPtr<WebGPU::Adapter>&& adapter) mutable {
        if (adapter) {
            promise.resolve(GPUAdapter::create(adapter.releaseNonNull(), WTF::move(backing)).ptr());
            return;
        }
        // The GPU process can exit after backing() checked it, and then nothing else was wrong with
        // the request. A failing RemoteGPUProxy::requestAdapter() calls back before it returns, so
        // this can run inside the outer backing->requestAdapter() call above; that is safe, as the
        // outer call holds its own reference to the old backing and the retry only replaces m_backing.
        if (retryIfBackingIsLost && backing->isLost()) {
            protectedThis->requestAdapter(WTF::move(options), WTF::move(promise), false);
            return;
        }
        promise.resolve(nullptr);
    });
}

GPUTextureFormat GPU::getPreferredCanvasFormat() const
{
    return GPUTextureFormat::Bgra8unorm;
}

RefPtr<GPUPresentationContext> GPU::createPresentationContext(WebGPUIntegration& backing, const GPUPresentationContextDescriptor& presentationContextDescriptor)
{
    RefPtr context = backing.createPresentationContext(presentationContextDescriptor.convertToBacking());
    if (!context)
        return nullptr;
    return GPUPresentationContext::create(context.releaseNonNull());
}

RefPtr<GPUCompositorIntegration> GPU::createCompositorIntegration(WebGPUIntegration& backing)
{
    RefPtr integration = backing.createCompositorIntegration();
    if (!integration)
        return nullptr;
    return GPUCompositorIntegration::create(integration.releaseNonNull());
}

} // namespace WebCore
