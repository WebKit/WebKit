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
#include <WebCore/GraphicsContext.h>
#include <WebCore/IntSize.h>
#include <WebCore/NativeImage.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>

namespace WebCore::WebGPU {

GPUImpl::GPUImpl(WebGPUPtr<WGPUInstance>&& instance, ConvertToBackingContext& convertToBackingContext)
    : m_backing(WTFMove(instance))
    , m_convertToBackingContext(convertToBackingContext)
{
}

GPUImpl::~GPUImpl() = default;

static void requestAdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPURequestAdapterStatus, WGPUAdapter, const char*)>(userdata);
    block(status, adapter, message);
    Block_release(block); // Block_release is matched with Block_copy below in GPUImpl::requestAdapter().
}

void GPUImpl::requestAdapter(const RequestAdapterOptions& options, CompletionHandler<void(RefPtr<Adapter>&&)>&& callback)
{
    WGPURequestAdapterOptions backingOptions {
        nullptr,
        nullptr,
        options.powerPreference ? m_convertToBackingContext->convertToBacking(*options.powerPreference) : static_cast<WGPUPowerPreference>(WGPUPowerPreference_Undefined),
        WGPUBackendType_Metal,
        options.forceFallbackAdapter,
    };

    auto blockPtr = makeBlockPtr([convertToBackingContext = m_convertToBackingContext.copyRef(), callback = WTFMove(callback)](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char*) mutable {
        if (status == WGPURequestAdapterStatus_Success)
            callback(AdapterImpl::create(adoptWebGPU(adapter), convertToBackingContext));
        else
            callback(nullptr);
    });
    wgpuInstanceRequestAdapter(m_backing.get(), &backingOptions, &requestAdapterCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in requestAdapterCallback().
}

static WTF::Function<void(CompletionHandler<void()>&&)> convert(WGPUOnSubmittedWorkScheduledCallback&& onSubmittedWorkScheduledCallback)
{
    return [onSubmittedWorkScheduledCallback = makeBlockPtr(WTFMove(onSubmittedWorkScheduledCallback))](CompletionHandler<void()>&& completionHandler) {
        onSubmittedWorkScheduledCallback(makeBlockPtr(WTFMove(completionHandler)).get());
    };
}

Ref<PresentationContext> GPUImpl::createPresentationContext(const PresentationContextDescriptor& presentationContextDescriptor)
{
    auto& compositorIntegration = m_convertToBackingContext->convertToBacking(presentationContextDescriptor.compositorIntegration);

    auto registerCallbacksBlock = makeBlockPtr([&](WGPURenderBuffersWereRecreatedBlockCallback renderBuffersWereRecreatedCallback, WGPUOnSubmittedWorkScheduledCallback onSubmittedWorkScheduledCallback) {
        compositorIntegration.registerCallbacks(makeBlockPtr(WTFMove(renderBuffersWereRecreatedCallback)), convert(WTFMove(onSubmittedWorkScheduledCallback)));
    });

    WGPUSurfaceDescriptorCocoaCustomSurface cocoaSurface {
        {
            nullptr,
            static_cast<WGPUSType>(WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking),
        },
        registerCallbacksBlock.get(),
    };

    WGPUSurfaceDescriptor surfaceDescriptor {
        &cocoaSurface.chain,
        nullptr,
    };

    auto result = PresentationContextImpl::create(adoptWebGPU(wgpuInstanceCreateSurface(m_backing.get(), &surfaceDescriptor)), m_convertToBackingContext);
    compositorIntegration.setPresentationContext(result);
    return result;
}

Ref<CompositorIntegration> GPUImpl::createCompositorIntegration()
{
    return CompositorIntegrationImpl::create(m_convertToBackingContext);
}

void GPUImpl::paintToCanvas(WebCore::NativeImage& image, const WebCore::IntSize& canvasSize, WebCore::GraphicsContext& context)
{
    auto imageSize = image.size();
    FloatRect canvasRect(FloatPoint(), canvasSize);
    GraphicsContextStateSaver stateSaver(context);
    context.setImageInterpolationQuality(InterpolationQuality::DoNotInterpolate);
    context.drawNativeImage(image, canvasRect, FloatRect(FloatPoint(), imageSize), { CompositeOperator::Copy });
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
