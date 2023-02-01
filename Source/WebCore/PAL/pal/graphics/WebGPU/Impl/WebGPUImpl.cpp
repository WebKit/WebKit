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

#include "config.h"
#include "WebGPUImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUAdapterImpl.h"
#include "WebGPUCompositorIntegrationImpl.h"
#include "WebGPUDowncastConvertToBackingContext.h"
#include "WebGPUPresentationContextDescriptor.h"
#include "WebGPUPresentationContextImpl.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>

namespace PAL::WebGPU {

GPUImpl::GPUImpl(WGPUInstance instance, ConvertToBackingContext& convertToBackingContext)
    : m_backing(instance)
    , m_convertToBackingContext(convertToBackingContext)
{
}

GPUImpl::~GPUImpl()
{
    wgpuInstanceRelease(m_backing);
}

void GPUImpl::requestAdapter(const RequestAdapterOptions& options, CompletionHandler<void(RefPtr<Adapter>&&)>&& callback)
{
    WGPURequestAdapterOptions backingOptions {
        nullptr,
        nullptr,
        options.powerPreference ? m_convertToBackingContext->convertToBacking(*options.powerPreference) : static_cast<WGPUPowerPreference>(WGPUPowerPreference_Undefined),
        options.forceFallbackAdapter,
    };

    wgpuInstanceRequestAdapterWithBlock(m_backing, &backingOptions, makeBlockPtr([convertToBackingContext = m_convertToBackingContext.copyRef(), callback = WTFMove(callback)](WGPURequestAdapterStatus, WGPUAdapter adapter, const char*) mutable {
        callback(AdapterImpl::create(adapter, convertToBackingContext));
    }).get());
}

Ref<PresentationContext> GPUImpl::createPresentationContext(const PresentationContextDescriptor& presentationContextDescriptor)
{
    // FIXME: Do something with the presentationContextDescriptor
    UNUSED_PARAM(presentationContextDescriptor);

    WGPUSurfaceDescriptorCocoaCustomSurface cocoaSurface {
        { nullptr, static_cast<WGPUSType>(WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking) },
    };

    WGPUSurfaceDescriptor surfaceDescriptor {
        &cocoaSurface.chain,
        nullptr,
    };

    return PresentationContextImpl::create(wgpuInstanceCreateSurface(m_backing, &surfaceDescriptor), m_convertToBackingContext);
}

Ref<CompositorIntegration> GPUImpl::createCompositorIntegration()
{
    return CompositorIntegrationImpl::create(m_convertToBackingContext);
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
