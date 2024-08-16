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
#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUIdentifier.h"
#include <functional>
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore::WebGPU {
class Adapter;
class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandBuffer;
class CommandEncoder;
class CompositorIntegration;
class ComputePassEncoder;
class ComputePipeline;
class Device;
class ExternalTexture;
class GPU;
class PipelineLayout;
class PresentationContext;
class QuerySet;
class Queue;
class RenderBundleEncoder;
class RenderBundle;
class RenderPassEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Texture;
class TextureView;
class XRBinding;
class XRProjectionLayer;
class XRSubImage;
class XRView;
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

class ObjectHeap final : public RefCounted<ObjectHeap>, public WebGPU::ConvertFromBackingContext, public CanMakeWeakPtr<ObjectHeap> {
    WTF_MAKE_TZONE_ALLOCATED(ObjectHeap);
public:
    static Ref<ObjectHeap> create()
    {
        return adoptRef(*new ObjectHeap());
    }

    ~ObjectHeap();

    void addObject(WebGPUIdentifier, RemoteAdapter&);
    void addObject(WebGPUIdentifier, RemoteBindGroup&);
    void addObject(WebGPUIdentifier, RemoteBindGroupLayout&);
    void addObject(WebGPUIdentifier, RemoteBuffer&);
    void addObject(WebGPUIdentifier, RemoteCommandBuffer&);
    void addObject(WebGPUIdentifier, RemoteCommandEncoder&);
    void addObject(WebGPUIdentifier, RemoteCompositorIntegration&);
    void addObject(WebGPUIdentifier, RemoteComputePassEncoder&);
    void addObject(WebGPUIdentifier, RemoteComputePipeline&);
    void addObject(WebGPUIdentifier, RemoteDevice&);
    void addObject(WebGPUIdentifier, RemoteExternalTexture&);
    void addObject(WebGPUIdentifier, RemotePipelineLayout&);
    void addObject(WebGPUIdentifier, RemotePresentationContext&);
    void addObject(WebGPUIdentifier, RemoteQuerySet&);
    void addObject(WebGPUIdentifier, RemoteQueue&);
    void addObject(WebGPUIdentifier, RemoteRenderBundleEncoder&);
    void addObject(WebGPUIdentifier, RemoteRenderBundle&);
    void addObject(WebGPUIdentifier, RemoteRenderPassEncoder&);
    void addObject(WebGPUIdentifier, RemoteRenderPipeline&);
    void addObject(WebGPUIdentifier, RemoteSampler&);
    void addObject(WebGPUIdentifier, RemoteShaderModule&);
    void addObject(WebGPUIdentifier, RemoteTexture&);
    void addObject(WebGPUIdentifier, RemoteTextureView&);
    void addObject(WebGPUIdentifier, RemoteXRBinding&);
    void addObject(WebGPUIdentifier, RemoteXRSubImage&);
    void addObject(WebGPUIdentifier, RemoteXRProjectionLayer&);
    void addObject(WebGPUIdentifier, RemoteXRView&);

    void removeObject(WebGPUIdentifier);

    void clear();

    WebCore::WebGPU::Adapter* convertAdapterFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::BindGroup* convertBindGroupFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::BindGroupLayout* convertBindGroupLayoutFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::Buffer* convertBufferFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::CommandBuffer* convertCommandBufferFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::CommandEncoder* convertCommandEncoderFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::CompositorIntegration* convertCompositorIntegrationFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::ComputePassEncoder* convertComputePassEncoderFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::ComputePipeline* convertComputePipelineFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::Device* convertDeviceFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::ExternalTexture* convertExternalTextureFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::PipelineLayout* convertPipelineLayoutFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::PresentationContext* convertPresentationContextFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::QuerySet* convertQuerySetFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::Queue* convertQueueFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::RenderBundleEncoder* convertRenderBundleEncoderFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::RenderBundle* convertRenderBundleFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::RenderPassEncoder* convertRenderPassEncoderFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::RenderPipeline* convertRenderPipelineFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::Sampler* convertSamplerFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::ShaderModule* convertShaderModuleFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::Texture* convertTextureFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::TextureView* convertTextureViewFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::XRBinding* convertXRBindingFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::XRSubImage* convertXRSubImageFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::XRProjectionLayer* convertXRProjectionLayerFromBacking(WebGPUIdentifier) final;
    WebCore::WebGPU::XRView* createXRViewFromBacking(WebGPUIdentifier) final;

    struct ExistsAndValid {
        bool exists { false };
        bool valid { false };
    };
    ExistsAndValid objectExistsAndValid(const WebCore::WebGPU::GPU&, WebGPUIdentifier) const;
private:
    ObjectHeap();

    using Object = std::variant<
        std::monostate,
        IPC::ScopedActiveMessageReceiveQueue<RemoteAdapter>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroup>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteBindGroupLayout>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteBuffer>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteCommandBuffer>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteCommandEncoder>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteCompositorIntegration>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>,
        IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>,
        IPC::ScopedActiveMessageReceiveQueue<RemotePresentationContext>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteXRBinding>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteXRSubImage>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteXRProjectionLayer>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteXRView>
    >;

    HashMap<WebGPUIdentifier, Object> m_objects;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
