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

#if ENABLE(GPU_PROCESS)

#include "ScopedActiveMessageReceiveQueue.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>
#include <functional>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class WebGPUCompositorIntegration;
}

namespace WebKit {
class RemoteAdapter;
class RemoteBindGroup;
class RemoteBindGroupLayout;
class RemoteBuffer;
class RemoteCommandBuffer;
class RemoteCommandEncoder;
class RemoteCompositorIntegration;
class RemoteComputePassEncoder;
class RemoteComputePipeline;
class RemoteDevice;
class RemoteExternalTexture;
class RemotePipelineLayout;
class RemotePresentationContext;
class RemoteQuerySet;
class RemoteQueue;
class RemoteRenderBundleEncoder;
class RemoteRenderBundle;
class RemoteRenderPassEncoder;
class RemoteRenderPipeline;
class RemoteSampler;
class RemoteShaderModule;
class RemoteTexture;
class RemoteTextureView;
class RemoteXRBinding;
class RemoteXRProjectionLayer;
class RemoteXRSubImage;
class RemoteXRView;
}

namespace WebKit::WebGPU {

class ObjectHeap final : public RefCountedAndCanMakeWeakPtr<ObjectHeap> {
    WTF_MAKE_TZONE_ALLOCATED(ObjectHeap);
public:
    static Ref<ObjectHeap> create()
    {
        return adoptRef(*new ObjectHeap());
    }

    ~ObjectHeap();

    void addObject(WebGPUAdapterIdentifier, RemoteAdapter&);
    void addObject(WebGPUBindGroupIdentifier, RemoteBindGroup&);
    void addObject(WebGPUBindGroupLayoutIdentifier, RemoteBindGroupLayout&);
    void addObject(WebGPUBufferIdentifier, RemoteBuffer&);
    void addObject(WebGPUCommandBufferIdentifier, RemoteCommandBuffer&);
    void addObject(WebGPUCommandEncoderIdentifier, RemoteCommandEncoder&);
    void addObject(WebGPUCompositorIntegrationIdentifier, RemoteCompositorIntegration&);
    void addObject(WebGPUComputePassEncoderIdentifier, RemoteComputePassEncoder&);
    void addObject(WebGPUComputePipelineIdentifier, RemoteComputePipeline&);
    void addObject(WebGPUDeviceIdentifier, RemoteDevice&);
    void addObject(WebGPUExternalTextureIdentifier, RemoteExternalTexture&);
    void addObject(WebGPUPipelineLayoutIdentifier, RemotePipelineLayout&);
    void addObject(WebGPUPresentationContextIdentifier, RemotePresentationContext&);
    void addObject(WebGPUQuerySetIdentifier, RemoteQuerySet&);
    void addObject(WebGPUQueueIdentifier, RemoteQueue&);
    void addObject(WebGPURenderBundleIdentifier, RemoteRenderBundle&);
    void addObject(WebGPURenderBundleEncoderIdentifier, RemoteRenderBundleEncoder&);
    void addObject(WebGPURenderPassEncoderIdentifier, RemoteRenderPassEncoder&);
    void addObject(WebGPURenderPipelineIdentifier, RemoteRenderPipeline&);
    void addObject(WebGPUSamplerIdentifier, RemoteSampler&);
    void addObject(WebGPUShaderModuleIdentifier, RemoteShaderModule&);
    void addObject(WebGPUTextureIdentifier, RemoteTexture&);
    void addObject(WebGPUTextureViewIdentifier, RemoteTextureView&);
    void addObject(WebGPUXRBindingIdentifier, RemoteXRBinding&);
    void addObject(WebGPUXRProjectionLayerIdentifier, RemoteXRProjectionLayer&);
    void addObject(WebGPUXRSubImageIdentifier, RemoteXRSubImage&);
    void addObject(WebGPUXRViewIdentifier, RemoteXRView&);

    void removeObject(WebGPUAdapterIdentifier);
    void removeObject(WebGPUBindGroupIdentifier);
    void removeObject(WebGPUBindGroupLayoutIdentifier);
    void removeObject(WebGPUBufferIdentifier);
    void removeObject(WebGPUCommandBufferIdentifier);
    void removeObject(WebGPUCommandEncoderIdentifier);
    void removeObject(WebGPUCompositorIntegrationIdentifier);
    void removeObject(WebGPUComputePassEncoderIdentifier);
    void removeObject(WebGPUComputePipelineIdentifier);
    void removeObject(WebGPUDeviceIdentifier);
    void removeObject(WebGPUExternalTextureIdentifier);
    void removeObject(WebGPUPipelineLayoutIdentifier);
    void removeObject(WebGPUPresentationContextIdentifier);
    void removeObject(WebGPUQuerySetIdentifier);
    void removeObject(WebGPUQueueIdentifier);
    void removeObject(WebGPURenderBundleIdentifier);
    void removeObject(WebGPURenderBundleEncoderIdentifier);
    void removeObject(WebGPURenderPassEncoderIdentifier);
    void removeObject(WebGPURenderPipelineIdentifier);
    void removeObject(WebGPUSamplerIdentifier);
    void removeObject(WebGPUShaderModuleIdentifier);
    void removeObject(WebGPUTextureIdentifier);
    void removeObject(WebGPUTextureViewIdentifier);
    void removeObject(WebGPUXRBindingIdentifier);
    void removeObject(WebGPUXRProjectionLayerIdentifier);
    void removeObject(WebGPUXRSubImageIdentifier);
    void removeObject(WebGPUXRViewIdentifier);

    void clear();

    Vector<Ref<::WebGPU::Device>> devices() const;

    RefPtr<::WebGPU::Adapter> get(WebGPUAdapterIdentifier);
    RefPtr<::WebGPU::BindGroup> get(WebGPUBindGroupIdentifier);
    RefPtr<::WebGPU::BindGroupLayout> get(WebGPUBindGroupLayoutIdentifier);
    RefPtr<::WebGPU::Buffer> get(WebGPUBufferIdentifier);
    RefPtr<::WebGPU::CommandBuffer> get(WebGPUCommandBufferIdentifier);
    RefPtr<::WebGPU::CommandEncoder> get(WebGPUCommandEncoderIdentifier);
    WeakPtr<WebCore::WebGPUCompositorIntegration> get(WebGPUCompositorIntegrationIdentifier);
    RefPtr<::WebGPU::ComputePassEncoder> get(WebGPUComputePassEncoderIdentifier);
    RefPtr<::WebGPU::ComputePipeline> get(WebGPUComputePipelineIdentifier);
    RefPtr<::WebGPU::Device> get(WebGPUDeviceIdentifier);
    ThreadSafeWeakPtr<::WebGPU::ExternalTexture> get(WebGPUExternalTextureIdentifier);
    RefPtr<::WebGPU::PipelineLayout> get(WebGPUPipelineLayoutIdentifier);
    RefPtr<::WebGPU::PresentationContext> get(WebGPUPresentationContextIdentifier);
    RefPtr<::WebGPU::QuerySet> get(WebGPUQuerySetIdentifier);
    RefPtr<::WebGPU::Queue> get(WebGPUQueueIdentifier);
    RefPtr<::WebGPU::RenderBundle> get(WebGPURenderBundleIdentifier);
    RefPtr<::WebGPU::RenderBundleEncoder> get(WebGPURenderBundleEncoderIdentifier);
    RefPtr<::WebGPU::RenderPassEncoder> get(WebGPURenderPassEncoderIdentifier);
    RefPtr<::WebGPU::RenderPipeline> get(WebGPURenderPipelineIdentifier);
    RefPtr<::WebGPU::Sampler> get(WebGPUSamplerIdentifier);
    RefPtr<::WebGPU::ShaderModule> get(WebGPUShaderModuleIdentifier);
    RefPtr<::WebGPU::Texture> get(WebGPUTextureIdentifier);
    RefPtr<::WebGPU::TextureView> get(WebGPUTextureViewIdentifier);
    RefPtr<::WebGPU::XRBinding> get(WebGPUXRBindingIdentifier);
    RefPtr<::WebGPU::XRProjectionLayer> get(WebGPUXRProjectionLayerIdentifier);
    RefPtr<::WebGPU::XRSubImage> get(WebGPUXRSubImageIdentifier);
    RefPtr<::WebGPU::XRView> get(WebGPUXRViewIdentifier);

    struct ExistsAndValid {
        bool exists { false };
        bool valid { false };
    };
    // Identifiers of different types may have the same value. The object exists when an object of any
    // type has the value, and is valid when every object that has it is valid.
    ExistsAndValid objectExistsAndValid(WebGPUIdentifier) const;
private:
    ObjectHeap();

    // Calls the functor with every per-type map.
    template<typename Self, typename Functor> static decltype(auto) applyToMaps(Self&, NOESCAPE Functor&&);
    // A member, since the Remote* classes expose backing() only to ObjectHeap.
    template<typename Identifier, typename Object> static auto* findBackingInMap(HashMap<Identifier, IPC::ScopedActiveMessageReceiveQueue<Object>>&, Identifier);

    HashMap<WebGPUAdapterIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter>> m_adapters;
    HashMap<WebGPUBindGroupIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup>> m_bindGroups;
    HashMap<WebGPUBindGroupLayoutIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout>> m_bindGroupLayouts;
    HashMap<WebGPUBufferIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer>> m_buffers;
    HashMap<WebGPUCommandBufferIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer>> m_commandBuffers;
    HashMap<WebGPUCommandEncoderIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder>> m_commandEncoders;
    HashMap<WebGPUCompositorIntegrationIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration>> m_compositorIntegrations;
    HashMap<WebGPUComputePassEncoderIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>> m_computePassEncoders;
    HashMap<WebGPUComputePipelineIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>> m_computePipelines;
    HashMap<WebGPUDeviceIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>> m_devices;
    HashMap<WebGPUExternalTextureIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>> m_externalTextures;
    HashMap<WebGPUPipelineLayoutIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>> m_pipelineLayouts;
    HashMap<WebGPUPresentationContextIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext>> m_presentationContexts;
    HashMap<WebGPUQuerySetIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>> m_querySets;
    HashMap<WebGPUQueueIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>> m_queues;
    HashMap<WebGPURenderBundleIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>> m_renderBundles;
    HashMap<WebGPURenderBundleEncoderIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>> m_renderBundleEncoders;
    HashMap<WebGPURenderPassEncoderIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>> m_renderPassEncoders;
    HashMap<WebGPURenderPipelineIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>> m_renderPipelines;
    HashMap<WebGPUSamplerIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>> m_samplers;
    HashMap<WebGPUShaderModuleIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>> m_shaderModules;
    HashMap<WebGPUTextureIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>> m_textures;
    HashMap<WebGPUTextureViewIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>> m_textureViews;
    HashMap<WebGPUXRBindingIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteXRBinding>> m_xrBindings;
    HashMap<WebGPUXRProjectionLayerIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteXRProjectionLayer>> m_xrProjectionLayers;
    HashMap<WebGPUXRSubImageIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteXRSubImage>> m_xrSubImages;
    HashMap<WebGPUXRViewIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteXRView>> m_xrViews;
    // Every device added that is still alive: work in flight can keep a device alive after its object is removed.
    ThreadSafeWeakHashSet<::WebGPU::Device> m_backingDevices;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
