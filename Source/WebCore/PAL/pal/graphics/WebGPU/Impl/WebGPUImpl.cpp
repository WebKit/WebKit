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

#include "config.h"
#include "WebGPUImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUAdapterImpl.h"
#include "WebGPUDowncastConvertToBackingContext.h"
#include <WebGPU/WebGPUExt.h>

#if PLATFORM(COCOA)
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(wgpuCreateInstance);
#endif

namespace PAL::WebGPU {

RefPtr<GPUImpl> GPUImpl::create()
{
    WGPUInstanceDescriptor descriptor = { nullptr };
    if (!&wgpuCreateInstance)
        return nullptr;
    auto instance = wgpuCreateInstance(&descriptor);
    if (!instance)
        return nullptr;
    auto convertToBackingContext = DowncastConvertToBackingContext::create();
    return create(instance, convertToBackingContext);
}

GPUImpl::GPUImpl(WGPUInstance instance, ConvertToBackingContext& convertToBackingContext)
    : m_backing(instance)
    , m_convertToBackingContext(convertToBackingContext)
{
}

GPUImpl::~GPUImpl()
{
    wgpuInstanceRelease(m_backing);
}

void requestAdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata)
{
    auto gpu = adoptRef(*static_cast<GPUImpl*>(userdata)); // adoptRef is balanced by leakRef in requestAdapter() below. We have to do this because we're using a C API with no concept of reference counting or blocks.
    gpu->requestAdapterCallback(status, adapter, message);
}

void GPUImpl::requestAdapter(const RequestAdapterOptions& options, WTF::Function<void(RefPtr<Adapter>&&)>&& callback)
{
    Ref protectedThis(*this);

    WGPURequestAdapterOptions backingOptions {
        nullptr,
        nullptr,
        options.powerPreference ? m_convertToBackingContext->convertToBacking(*options.powerPreference) : static_cast<WGPUPowerPreference>(WGPUPowerPreference_Undefined),
        options.forceFallbackAdapter,
    };

    m_callbacks.append(WTFMove(callback));

    wgpuInstanceRequestAdapter(m_backing, &backingOptions, &WebGPU::requestAdapterCallback, &protectedThis.leakRef()); // leakRef is balanced by adoptRef in requestAdapterCallback() above. We have to do this because we're using a C API with no concept of reference counting or blocks.
}

void GPUImpl::requestAdapterCallback(WGPURequestAdapterStatus, WGPUAdapter adapter, const char* message)
{
    UNUSED_PARAM(message);
    auto callback = m_callbacks.takeFirst();
    callback(AdapterImpl::create(adapter, m_convertToBackingContext));
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
