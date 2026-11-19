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

#include <WebCore/WebGPUCompositorIntegration.h>
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ObjectHeap);

ObjectHeap::ObjectHeap()
{
    weakPtrFactory().prepareForUseOnlyOnNonMainThread();
}

ObjectHeap::~ObjectHeap() = default;

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteAdapter& adapter)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter> { protect(adapter) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBindGroup& bindGroup)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup> { protect(bindGroup) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBindGroupLayout& bindGroupLayout)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout> { protect(bindGroupLayout) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteBuffer& buffer)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer> { protect(buffer) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCommandBuffer& commandBuffer)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer> { protect(commandBuffer) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCommandEncoder& commandEncoder)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder> { protect(commandEncoder) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteCompositorIntegration& querySet)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration> { protect(querySet) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteComputePassEncoder& computePassEncoder)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder> { protect(computePassEncoder) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteComputePipeline& computePipeline)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline> { protect(computePipeline) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteDevice& device)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteDevice> { protect(device) } });
    ASSERT_UNUSED(result, result.isNewEntry);
    m_devices.add(device.backing());
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteExternalTexture& externalTexture)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture> { protect(externalTexture) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemotePipelineLayout& pipelineLayout)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout> { protect(pipelineLayout) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemotePresentationContext& presentationContext)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext> { protect(presentationContext) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteQuerySet& querySet)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet> { protect(querySet) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteQueue& queue)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteQueue> { protect(queue) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderBundleEncoder& renderBundleEncoder)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder> { protect(renderBundleEncoder) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderBundle& renderBundle)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle> { protect(renderBundle) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderPassEncoder& renderPassEncoder)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder> { protect(renderPassEncoder) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteRenderPipeline& renderPipeline)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline> { protect(renderPipeline) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteSampler& sampler)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteSampler> { protect(sampler) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteShaderModule& shaderModule)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule> { protect(shaderModule) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteTexture& texture)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteTexture> { protect(texture) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteTextureView& textureView)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView> { protect(textureView) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteXRBinding& xrBinding)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteXRBinding> { protect(xrBinding) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteXRSubImage& xrSubImage)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteXRSubImage> { protect(xrSubImage) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteXRProjectionLayer& layer)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteXRProjectionLayer> { protect(layer) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(WebGPUIdentifier identifier, RemoteXRView& view)
{
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteXRView> { protect(view) } });
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::removeObject(WebGPUIdentifier identifier)
{
    auto result = m_objects.remove(identifier);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::clear()
{
    m_objects.clear();
}

Vector<Ref<::WebGPU::Device>> ObjectHeap::devices() const
{
    return m_devices.values();
}

RefPtr<::WebGPU::Adapter> ObjectHeap::convertAdapterFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter>>(iterator->value)->backing();
}

RefPtr<::WebGPU::BindGroup> ObjectHeap::convertBindGroupFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup>>(iterator->value)->backing();
}

RefPtr<::WebGPU::BindGroupLayout> ObjectHeap::convertBindGroupLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout>>(iterator->value)->backing();
}

RefPtr<::WebGPU::Buffer> ObjectHeap::convertBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer>>(iterator->value)->backing();
}

RefPtr<::WebGPU::CommandBuffer> ObjectHeap::convertCommandBufferFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer>>(iterator->value)->backing();
}

RefPtr<::WebGPU::CommandEncoder> ObjectHeap::convertCommandEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder>>(iterator->value)->backing();
}

WeakPtr<WebCore::WebGPUCompositorIntegration> ObjectHeap::convertCompositorIntegrationFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration>>(iterator->value)->backing();
}

RefPtr<::WebGPU::ComputePassEncoder> ObjectHeap::convertComputePassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>>(iterator->value)->backing();
}

RefPtr<::WebGPU::ComputePipeline> ObjectHeap::convertComputePipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>>(iterator->value)->backing();
}

RefPtr<::WebGPU::Device> ObjectHeap::convertDeviceFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>>(iterator->value)->backing();
}

ThreadSafeWeakPtr<::WebGPU::ExternalTexture> ObjectHeap::convertExternalTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>>(iterator->value)->backing();
}

RefPtr<::WebGPU::PipelineLayout> ObjectHeap::convertPipelineLayoutFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>>(iterator->value)->backing();
}

RefPtr<::WebGPU::PresentationContext> ObjectHeap::convertPresentationContextFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext>>(iterator->value)->backing();
}

RefPtr<::WebGPU::QuerySet> ObjectHeap::convertQuerySetFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>>(iterator->value)->backing();
}

RefPtr<::WebGPU::Queue> ObjectHeap::convertQueueFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>>(iterator->value)->backing();
}

RefPtr<::WebGPU::RenderBundleEncoder> ObjectHeap::convertRenderBundleEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>>(iterator->value)->backing();
}

RefPtr<::WebGPU::RenderBundle> ObjectHeap::convertRenderBundleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>>(iterator->value)->backing();
}

RefPtr<::WebGPU::RenderPassEncoder> ObjectHeap::convertRenderPassEncoderFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>>(iterator->value)->backing();
}

RefPtr<::WebGPU::RenderPipeline> ObjectHeap::convertRenderPipelineFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>>(iterator->value)->backing();
}

RefPtr<::WebGPU::Sampler> ObjectHeap::convertSamplerFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>>(iterator->value)->backing();
}

RefPtr<::WebGPU::ShaderModule> ObjectHeap::convertShaderModuleFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>>(iterator->value)->backing();
}

RefPtr<::WebGPU::Texture> ObjectHeap::convertTextureFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>>(iterator->value)->backing();
}

RefPtr<::WebGPU::TextureView> ObjectHeap::convertTextureViewFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>>(iterator->value)->backing();
}

RefPtr<::WebGPU::XRBinding> ObjectHeap::convertXRBindingFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteXRBinding>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteXRBinding>>(iterator->value)->backing();
}

RefPtr<::WebGPU::XRSubImage> ObjectHeap::convertXRSubImageFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteXRSubImage>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteXRSubImage>>(iterator->value)->backing();
}

RefPtr<::WebGPU::XRProjectionLayer> ObjectHeap::convertXRProjectionLayerFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteXRProjectionLayer>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteXRProjectionLayer>>(iterator->value)->backing();
}

RefPtr<::WebGPU::XRView> ObjectHeap::createXRViewFromBacking(WebGPUIdentifier identifier)
{
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteXRView>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteXRView>>(iterator->value)->backing();
}

ObjectHeap::ExistsAndValid ObjectHeap::objectExistsAndValid(const WebCore::WebGPUIntegration& gpu, WebGPUIdentifier identifier) const
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
        return gpu.isValid(protect(object->backing()));
    });
#else
    UNUSED_PARAM(gpu);
    UNUSED_PARAM(identifier);
#endif
    return result;
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
