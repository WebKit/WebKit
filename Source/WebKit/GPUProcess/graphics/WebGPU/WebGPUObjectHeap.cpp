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
#include <tuple>
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

template<typename Identifier, typename Object>
static void addToMap(HashMap<Identifier, IPC::ScopedActiveMessageReceiveQueue<Object>>& map, Identifier identifier, Object& object)
{
    auto result = map.add(identifier, IPC::ScopedActiveMessageReceiveQueue<Object> { protect(object) });
    ASSERT_UNUSED(result, result.isNewEntry);
}

template<typename Identifier, typename Object>
static void removeFromMap(HashMap<Identifier, IPC::ScopedActiveMessageReceiveQueue<Object>>& map, Identifier identifier)
{
    auto result = map.remove(identifier);
    ASSERT_UNUSED(result, result);
}

template<typename Identifier, typename Object>
auto* ObjectHeap::findBackingInMap(HashMap<Identifier, IPC::ScopedActiveMessageReceiveQueue<Object>>& map, Identifier identifier)
{
    auto iterator = map.find(identifier);
    return iterator == map.end() ? nullptr : &iterator->value->backing();
}

void ObjectHeap::addObject(WebGPUAdapterIdentifier identifier, RemoteAdapter& object)
{
    addToMap(m_adapters, identifier, object);
}

void ObjectHeap::addObject(WebGPUBindGroupIdentifier identifier, RemoteBindGroup& object)
{
    addToMap(m_bindGroups, identifier, object);
}

void ObjectHeap::addObject(WebGPUBindGroupLayoutIdentifier identifier, RemoteBindGroupLayout& object)
{
    addToMap(m_bindGroupLayouts, identifier, object);
}

void ObjectHeap::addObject(WebGPUBufferIdentifier identifier, RemoteBuffer& object)
{
    addToMap(m_buffers, identifier, object);
}

void ObjectHeap::addObject(WebGPUCommandBufferIdentifier identifier, RemoteCommandBuffer& object)
{
    addToMap(m_commandBuffers, identifier, object);
}

void ObjectHeap::addObject(WebGPUCommandEncoderIdentifier identifier, RemoteCommandEncoder& object)
{
    addToMap(m_commandEncoders, identifier, object);
}

void ObjectHeap::addObject(WebGPUCompositorIntegrationIdentifier identifier, RemoteCompositorIntegration& object)
{
    addToMap(m_compositorIntegrations, identifier, object);
}

void ObjectHeap::addObject(WebGPUComputePassEncoderIdentifier identifier, RemoteComputePassEncoder& object)
{
    addToMap(m_computePassEncoders, identifier, object);
}

void ObjectHeap::addObject(WebGPUComputePipelineIdentifier identifier, RemoteComputePipeline& object)
{
    addToMap(m_computePipelines, identifier, object);
}

void ObjectHeap::addObject(WebGPUDeviceIdentifier identifier, RemoteDevice& device)
{
    addToMap(m_devices, identifier, device);
    m_backingDevices.add(device.backing());
}

void ObjectHeap::addObject(WebGPUExternalTextureIdentifier identifier, RemoteExternalTexture& object)
{
    addToMap(m_externalTextures, identifier, object);
}

void ObjectHeap::addObject(WebGPUPipelineLayoutIdentifier identifier, RemotePipelineLayout& object)
{
    addToMap(m_pipelineLayouts, identifier, object);
}

void ObjectHeap::addObject(WebGPUPresentationContextIdentifier identifier, RemotePresentationContext& object)
{
    addToMap(m_presentationContexts, identifier, object);
}

void ObjectHeap::addObject(WebGPUQuerySetIdentifier identifier, RemoteQuerySet& object)
{
    addToMap(m_querySets, identifier, object);
}

void ObjectHeap::addObject(WebGPUQueueIdentifier identifier, RemoteQueue& object)
{
    addToMap(m_queues, identifier, object);
}

void ObjectHeap::addObject(WebGPURenderBundleIdentifier identifier, RemoteRenderBundle& object)
{
    addToMap(m_renderBundles, identifier, object);
}

void ObjectHeap::addObject(WebGPURenderBundleEncoderIdentifier identifier, RemoteRenderBundleEncoder& object)
{
    addToMap(m_renderBundleEncoders, identifier, object);
}

void ObjectHeap::addObject(WebGPURenderPassEncoderIdentifier identifier, RemoteRenderPassEncoder& object)
{
    addToMap(m_renderPassEncoders, identifier, object);
}

void ObjectHeap::addObject(WebGPURenderPipelineIdentifier identifier, RemoteRenderPipeline& object)
{
    addToMap(m_renderPipelines, identifier, object);
}

void ObjectHeap::addObject(WebGPUSamplerIdentifier identifier, RemoteSampler& object)
{
    addToMap(m_samplers, identifier, object);
}

void ObjectHeap::addObject(WebGPUShaderModuleIdentifier identifier, RemoteShaderModule& object)
{
    addToMap(m_shaderModules, identifier, object);
}

void ObjectHeap::addObject(WebGPUTextureIdentifier identifier, RemoteTexture& object)
{
    addToMap(m_textures, identifier, object);
}

void ObjectHeap::addObject(WebGPUTextureViewIdentifier identifier, RemoteTextureView& object)
{
    addToMap(m_textureViews, identifier, object);
}

void ObjectHeap::addObject(WebGPUXRBindingIdentifier identifier, RemoteXRBinding& object)
{
    addToMap(m_xrBindings, identifier, object);
}

void ObjectHeap::addObject(WebGPUXRProjectionLayerIdentifier identifier, RemoteXRProjectionLayer& object)
{
    addToMap(m_xrProjectionLayers, identifier, object);
}

void ObjectHeap::addObject(WebGPUXRSubImageIdentifier identifier, RemoteXRSubImage& object)
{
    addToMap(m_xrSubImages, identifier, object);
}

void ObjectHeap::addObject(WebGPUXRViewIdentifier identifier, RemoteXRView& object)
{
    addToMap(m_xrViews, identifier, object);
}

void ObjectHeap::removeObject(WebGPUAdapterIdentifier identifier)
{
    removeFromMap(m_adapters, identifier);
}

void ObjectHeap::removeObject(WebGPUBindGroupIdentifier identifier)
{
    removeFromMap(m_bindGroups, identifier);
}

void ObjectHeap::removeObject(WebGPUBindGroupLayoutIdentifier identifier)
{
    removeFromMap(m_bindGroupLayouts, identifier);
}

void ObjectHeap::removeObject(WebGPUBufferIdentifier identifier)
{
    removeFromMap(m_buffers, identifier);
}

void ObjectHeap::removeObject(WebGPUCommandBufferIdentifier identifier)
{
    removeFromMap(m_commandBuffers, identifier);
}

void ObjectHeap::removeObject(WebGPUCommandEncoderIdentifier identifier)
{
    removeFromMap(m_commandEncoders, identifier);
}

void ObjectHeap::removeObject(WebGPUCompositorIntegrationIdentifier identifier)
{
    removeFromMap(m_compositorIntegrations, identifier);
}

void ObjectHeap::removeObject(WebGPUComputePassEncoderIdentifier identifier)
{
    removeFromMap(m_computePassEncoders, identifier);
}

void ObjectHeap::removeObject(WebGPUComputePipelineIdentifier identifier)
{
    removeFromMap(m_computePipelines, identifier);
}

void ObjectHeap::removeObject(WebGPUDeviceIdentifier identifier)
{
    removeFromMap(m_devices, identifier);
}

void ObjectHeap::removeObject(WebGPUExternalTextureIdentifier identifier)
{
    removeFromMap(m_externalTextures, identifier);
}

void ObjectHeap::removeObject(WebGPUPipelineLayoutIdentifier identifier)
{
    removeFromMap(m_pipelineLayouts, identifier);
}

void ObjectHeap::removeObject(WebGPUPresentationContextIdentifier identifier)
{
    removeFromMap(m_presentationContexts, identifier);
}

void ObjectHeap::removeObject(WebGPUQuerySetIdentifier identifier)
{
    removeFromMap(m_querySets, identifier);
}

void ObjectHeap::removeObject(WebGPUQueueIdentifier identifier)
{
    removeFromMap(m_queues, identifier);
}

void ObjectHeap::removeObject(WebGPURenderBundleIdentifier identifier)
{
    removeFromMap(m_renderBundles, identifier);
}

void ObjectHeap::removeObject(WebGPURenderBundleEncoderIdentifier identifier)
{
    removeFromMap(m_renderBundleEncoders, identifier);
}

void ObjectHeap::removeObject(WebGPURenderPassEncoderIdentifier identifier)
{
    removeFromMap(m_renderPassEncoders, identifier);
}

void ObjectHeap::removeObject(WebGPURenderPipelineIdentifier identifier)
{
    removeFromMap(m_renderPipelines, identifier);
}

void ObjectHeap::removeObject(WebGPUSamplerIdentifier identifier)
{
    removeFromMap(m_samplers, identifier);
}

void ObjectHeap::removeObject(WebGPUShaderModuleIdentifier identifier)
{
    removeFromMap(m_shaderModules, identifier);
}

void ObjectHeap::removeObject(WebGPUTextureIdentifier identifier)
{
    removeFromMap(m_textures, identifier);
}

void ObjectHeap::removeObject(WebGPUTextureViewIdentifier identifier)
{
    removeFromMap(m_textureViews, identifier);
}

void ObjectHeap::removeObject(WebGPUXRBindingIdentifier identifier)
{
    removeFromMap(m_xrBindings, identifier);
}

void ObjectHeap::removeObject(WebGPUXRProjectionLayerIdentifier identifier)
{
    removeFromMap(m_xrProjectionLayers, identifier);
}

void ObjectHeap::removeObject(WebGPUXRSubImageIdentifier identifier)
{
    removeFromMap(m_xrSubImages, identifier);
}

void ObjectHeap::removeObject(WebGPUXRViewIdentifier identifier)
{
    removeFromMap(m_xrViews, identifier);
}

template<typename Self, typename Functor>
decltype(auto) ObjectHeap::applyToMaps(Self& self, NOESCAPE Functor&& functor)
{
    return functor(
        self.m_adapters,
        self.m_bindGroups,
        self.m_bindGroupLayouts,
        self.m_buffers,
        self.m_commandBuffers,
        self.m_commandEncoders,
        self.m_compositorIntegrations,
        self.m_computePassEncoders,
        self.m_computePipelines,
        self.m_devices,
        self.m_externalTextures,
        self.m_pipelineLayouts,
        self.m_presentationContexts,
        self.m_querySets,
        self.m_queues,
        self.m_renderBundles,
        self.m_renderBundleEncoders,
        self.m_renderPassEncoders,
        self.m_renderPipelines,
        self.m_samplers,
        self.m_shaderModules,
        self.m_textures,
        self.m_textureViews,
        self.m_xrBindings,
        self.m_xrProjectionLayers,
        self.m_xrSubImages,
        self.m_xrViews);
}

void ObjectHeap::clear()
{
    // Take every object out before destroying any, so that no object sees a partially cleared heap.
    auto objects = applyToMaps(*this, [](auto&... maps) {
        return std::make_tuple(std::exchange(maps, { })...);
    });
}

Vector<Ref<::WebGPU::Device>> ObjectHeap::devices() const
{
    return m_backingDevices.values();
}

RefPtr<::WebGPU::Adapter> ObjectHeap::get(WebGPUAdapterIdentifier identifier)
{
    return findBackingInMap(m_adapters, identifier);
}

RefPtr<::WebGPU::BindGroup> ObjectHeap::get(WebGPUBindGroupIdentifier identifier)
{
    return findBackingInMap(m_bindGroups, identifier);
}

RefPtr<::WebGPU::BindGroupLayout> ObjectHeap::get(WebGPUBindGroupLayoutIdentifier identifier)
{
    return findBackingInMap(m_bindGroupLayouts, identifier);
}

RefPtr<::WebGPU::Buffer> ObjectHeap::get(WebGPUBufferIdentifier identifier)
{
    return findBackingInMap(m_buffers, identifier);
}

RefPtr<::WebGPU::CommandBuffer> ObjectHeap::get(WebGPUCommandBufferIdentifier identifier)
{
    return findBackingInMap(m_commandBuffers, identifier);
}

RefPtr<::WebGPU::CommandEncoder> ObjectHeap::get(WebGPUCommandEncoderIdentifier identifier)
{
    return findBackingInMap(m_commandEncoders, identifier);
}

WeakPtr<WebCore::WebGPUCompositorIntegration> ObjectHeap::get(WebGPUCompositorIntegrationIdentifier identifier)
{
    return findBackingInMap(m_compositorIntegrations, identifier);
}

RefPtr<::WebGPU::ComputePassEncoder> ObjectHeap::get(WebGPUComputePassEncoderIdentifier identifier)
{
    return findBackingInMap(m_computePassEncoders, identifier);
}

RefPtr<::WebGPU::ComputePipeline> ObjectHeap::get(WebGPUComputePipelineIdentifier identifier)
{
    return findBackingInMap(m_computePipelines, identifier);
}

RefPtr<::WebGPU::Device> ObjectHeap::get(WebGPUDeviceIdentifier identifier)
{
    return findBackingInMap(m_devices, identifier);
}

ThreadSafeWeakPtr<::WebGPU::ExternalTexture> ObjectHeap::get(WebGPUExternalTextureIdentifier identifier)
{
    return findBackingInMap(m_externalTextures, identifier);
}

RefPtr<::WebGPU::PipelineLayout> ObjectHeap::get(WebGPUPipelineLayoutIdentifier identifier)
{
    return findBackingInMap(m_pipelineLayouts, identifier);
}

RefPtr<::WebGPU::PresentationContext> ObjectHeap::get(WebGPUPresentationContextIdentifier identifier)
{
    return findBackingInMap(m_presentationContexts, identifier);
}

RefPtr<::WebGPU::QuerySet> ObjectHeap::get(WebGPUQuerySetIdentifier identifier)
{
    return findBackingInMap(m_querySets, identifier);
}

RefPtr<::WebGPU::Queue> ObjectHeap::get(WebGPUQueueIdentifier identifier)
{
    return findBackingInMap(m_queues, identifier);
}

RefPtr<::WebGPU::RenderBundle> ObjectHeap::get(WebGPURenderBundleIdentifier identifier)
{
    return findBackingInMap(m_renderBundles, identifier);
}

RefPtr<::WebGPU::RenderBundleEncoder> ObjectHeap::get(WebGPURenderBundleEncoderIdentifier identifier)
{
    return findBackingInMap(m_renderBundleEncoders, identifier);
}

RefPtr<::WebGPU::RenderPassEncoder> ObjectHeap::get(WebGPURenderPassEncoderIdentifier identifier)
{
    return findBackingInMap(m_renderPassEncoders, identifier);
}

RefPtr<::WebGPU::RenderPipeline> ObjectHeap::get(WebGPURenderPipelineIdentifier identifier)
{
    return findBackingInMap(m_renderPipelines, identifier);
}

RefPtr<::WebGPU::Sampler> ObjectHeap::get(WebGPUSamplerIdentifier identifier)
{
    return findBackingInMap(m_samplers, identifier);
}

RefPtr<::WebGPU::ShaderModule> ObjectHeap::get(WebGPUShaderModuleIdentifier identifier)
{
    return findBackingInMap(m_shaderModules, identifier);
}

RefPtr<::WebGPU::Texture> ObjectHeap::get(WebGPUTextureIdentifier identifier)
{
    return findBackingInMap(m_textures, identifier);
}

RefPtr<::WebGPU::TextureView> ObjectHeap::get(WebGPUTextureViewIdentifier identifier)
{
    return findBackingInMap(m_textureViews, identifier);
}

RefPtr<::WebGPU::XRBinding> ObjectHeap::get(WebGPUXRBindingIdentifier identifier)
{
    return findBackingInMap(m_xrBindings, identifier);
}

RefPtr<::WebGPU::XRProjectionLayer> ObjectHeap::get(WebGPUXRProjectionLayerIdentifier identifier)
{
    return findBackingInMap(m_xrProjectionLayers, identifier);
}

RefPtr<::WebGPU::XRSubImage> ObjectHeap::get(WebGPUXRSubImageIdentifier identifier)
{
    return findBackingInMap(m_xrSubImages, identifier);
}

RefPtr<::WebGPU::XRView> ObjectHeap::get(WebGPUXRViewIdentifier identifier)
{
    return findBackingInMap(m_xrViews, identifier);
}

ObjectHeap::ExistsAndValid ObjectHeap::objectExistsAndValid(WebGPUIdentifier identifier) const
{
    ExistsAndValid result;
#if HAVE(WEBGPU_IMPLEMENTATION)
    bool everyObjectIsValid = true;
    applyToMaps(*this, [&](const auto&... maps) {
        auto checkMap = [&](const auto& map) {
            using Identifier = typename std::remove_cvref_t<decltype(map)>::KeyType;
            auto iterator = map.find(Identifier { identifier.toUInt64() });
            if (iterator == map.end())
                return;
            result.exists = true;
            // A compositor integration has no invalid state.
            if constexpr (!std::is_same_v<Identifier, WebGPUCompositorIntegrationIdentifier>)
                everyObjectIsValid = everyObjectIsValid && protect(iterator->value->backing())->isValid();
        };
        (checkMap(maps), ...);
    });
    result.valid = result.exists && everyObjectIsValid;
#else
    UNUSED_PARAM(identifier);
#endif
    return result;
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
