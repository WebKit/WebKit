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
#include "WebGPUObjectHeap.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapter.h"
#include "RemoteBindGroup.h"
#include "RemoteBindGroupLayout.h"
#include "RemoteBuffer.h"
#include "RemoteCommandBuffer.h"
#include "RemoteCommandEncoder.h"
#include "RemoteCompositorIntegration.h"
#include "RemoteComputePassEncoder.h"
#include "RemoteComputePipeline.h"
#include "RemoteDevice.h"
#include "RemoteExternalTexture.h"
#include "RemoteGPU.h"
#include "RemotePipelineLayout.h"
#include "RemotePresentationContext.h"
#include "RemoteQuerySet.h"
#include "RemoteQueue.h"
#include "RemoteRenderBundle.h"
#include "RemoteRenderBundleEncoder.h"
#include "RemoteRenderPassEncoder.h"
#include "RemoteRenderPipeline.h"
#include "RemoteSampler.h"
#include "RemoteShaderModule.h"
#include "RemoteTexture.h"
#include "RemoteTextureView.h"
#include "RemoteXRBinding.h"
#include "RemoteXRProjectionLayer.h"
#include "RemoteXRSubImage.h"
#include "RemoteXRView.h"
#include <wtf/TZoneMallocInlines.h>

#if HAVE(WEBGPU_IMPLEMENTATION)
#include <WebCore/WebGPU.h>
#endif

#include <WebCore/WebGPUAdapter.h>
#include <WebCore/WebGPUBindGroup.h>
#include <WebCore/WebGPUBindGroupLayout.h>
#include <WebCore/WebGPUBuffer.h>
#include <WebCore/WebGPUCommandBuffer.h>
#include <WebCore/WebGPUCommandEncoder.h>
#include <WebCore/WebGPUCompositorIntegration.h>
#include <WebCore/WebGPUComputePassEncoder.h>
#include <WebCore/WebGPUComputePipeline.h>
#include <WebCore/WebGPUDevice.h>
#include <WebCore/WebGPUExternalTexture.h>
#include <WebCore/WebGPUPipelineLayout.h>
#include <WebCore/WebGPUPresentationContext.h>
#include <WebCore/WebGPUQuerySet.h>
#include <WebCore/WebGPUQueue.h>
#include <WebCore/WebGPURenderBundle.h>
#include <WebCore/WebGPURenderBundleEncoder.h>
#include <WebCore/WebGPURenderPassEncoder.h>
#include <WebCore/WebGPURenderPipeline.h>
#include <WebCore/WebGPUSampler.h>
#include <WebCore/WebGPUShaderModule.h>
#include <WebCore/WebGPUTexture.h>
#include <WebCore/WebGPUTextureView.h>
#include <WebCore/WebGPUXRBinding.h>
#include <WebCore/WebGPUXRProjectionLayer.h>
#include <WebCore/WebGPUXRSubImage.h>
#include <WebCore/WebGPUXRView.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ObjectHeap);

ObjectHeap::ObjectHeap()
{
    weakPtrFactory().prepareForUseOnlyOnNonMainThread();
}

ObjectHeap::~ObjectHeap() = default;

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteAdapter& adapter)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter> { Ref { adapter } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBindGroup& bindGroup)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup> { Ref { bindGroup } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBindGroupLayout& bindGroupLayout)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout> { Ref { bindGroupLayout } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBuffer& buffer)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer> { Ref { buffer } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCommandBuffer& commandBuffer)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer> { Ref { commandBuffer } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCommandEncoder& commandEncoder)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder> { Ref { commandEncoder } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCompositorIntegration& querySet)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration> { Ref { querySet } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteComputePassEncoder& computePassEncoder)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder> { Ref { computePassEncoder } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteComputePipeline& computePipeline)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline> { Ref { computePipeline } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteDevice& device)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteDevice> { Ref { device } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteExternalTexture& externalTexture)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture> { Ref { externalTexture } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemotePipelineLayout& pipelineLayout)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout> { Ref { pipelineLayout } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemotePresentationContext& presentationContext)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext> { Ref { presentationContext } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteQuerySet& querySet)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet> { Ref { querySet } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteQueue& queue)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteQueue> { Ref { queue } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderBundleEncoder& renderBundleEncoder)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder> { Ref { renderBundleEncoder } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderBundle& renderBundle)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle> { Ref { renderBundle } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderPassEncoder& renderPassEncoder)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder> { Ref { renderPassEncoder } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderPipeline& renderPipeline)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline> { Ref { renderPipeline } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteSampler& sampler)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteSampler> { Ref { sampler } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteShaderModule& shaderModule)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule> { Ref { shaderModule } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteTexture& texture)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteTexture> { Ref { texture } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteTextureView& textureView)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView> { Ref { textureView } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteXRBinding& xrBinding)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteXRBinding> { Ref { xrBinding } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteXRSubImage& xrSubImage)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteXRSubImage> { Ref { xrSubImage } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteXRProjectionLayer& layer)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteXRProjectionLayer> { Ref { layer } } });
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteXRView& view)
{
    m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteXRView> { Ref { view } } });
}

void ObjectHeap::removeObject(WebGPUIdentifier identifier)
{
    m_objects.remove(identifier);
}

void ObjectHeap::clear()
{
    m_objects.clear();
}

WeakPtr<WebCore::WebGPU::Adapter> ObjectHeap::convertAdapterFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::BindGroup> ObjectHeap::convertBindGroupFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::BindGroupLayout> ObjectHeap::convertBindGroupLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::Buffer> ObjectHeap::convertBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::CommandBuffer> ObjectHeap::convertCommandBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::CommandEncoder> ObjectHeap::convertCommandEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::CompositorIntegration> ObjectHeap::convertCompositorIntegrationFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::ComputePassEncoder> ObjectHeap::convertComputePassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::ComputePipeline> ObjectHeap::convertComputePipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::Device> ObjectHeap::convertDeviceFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>>(iterator->value)->backing();
}

ThreadSafeWeakPtr<WebCore::WebGPU::ExternalTexture> ObjectHeap::convertExternalTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::PipelineLayout> ObjectHeap::convertPipelineLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::PresentationContext> ObjectHeap::convertPresentationContextFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::QuerySet> ObjectHeap::convertQuerySetFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::Queue> ObjectHeap::convertQueueFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::RenderBundleEncoder> ObjectHeap::convertRenderBundleEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::RenderBundle> ObjectHeap::convertRenderBundleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::RenderPassEncoder> ObjectHeap::convertRenderPassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::RenderPipeline> ObjectHeap::convertRenderPipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::Sampler> ObjectHeap::convertSamplerFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::ShaderModule> ObjectHeap::convertShaderModuleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::Texture> ObjectHeap::convertTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::TextureView> ObjectHeap::convertTextureViewFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::XRBinding> ObjectHeap::convertXRBindingFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteXRBinding>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteXRBinding>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::XRSubImage> ObjectHeap::convertXRSubImageFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteXRSubImage>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteXRSubImage>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::XRProjectionLayer> ObjectHeap::convertXRProjectionLayerFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteXRProjectionLayer>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteXRProjectionLayer>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPU::XRView> ObjectHeap::createXRViewFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteXRView>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteXRView>>(iterator->value)->backing();
}

ObjectHeap::ExistsAndValid ObjectHeap::objectExistsAndValid(const WebCore::WebGPU::GPU& gpu, WebGPUIdentifier identifier) const
{
    ExistsAndValid result;
#if HAVE(WEBGPU_IMPLEMENTATION)
    auto it = m_objects.find(identifier);
    if (it == m_objects.end())
        return result;

    result.exists = true;
    result.valid = WTF::switchOn(it->value, [&](std::monostate) -> bool {
        return false;
    },
    [&](auto& object) -> bool {
        return gpu.isValid(object->backing());
    });
#else
    UNUSED_PARAM(gpu);
    UNUSED_PARAM(identifier);
#endif
    return result;
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
