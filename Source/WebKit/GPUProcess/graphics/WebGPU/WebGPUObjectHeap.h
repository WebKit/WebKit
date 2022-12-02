/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

namespace PAL::WebGPU {
class Adapter;
class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandBuffer;
class CommandEncoder;
class ComputePassEncoder;
class ComputePipeline;
class Device;
class ExternalTexture;
class GPU;
class PipelineLayout;
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
}

namespace WebKit {
class RemoteAdapter;
class RemoteBindGroup;
class RemoteBindGroupLayout;
class RemoteBuffer;
class RemoteCommandBuffer;
class RemoteCommandEncoder;
class RemoteComputePassEncoder;
class RemoteComputePipeline;
class RemoteDevice;
class RemoteExternalTexture;
class RemotePipelineLayout;
class RemoteQuerySet;
class RemoteQueue;
class RemoteRenderBundleEncoder;
class RemoteRenderBundle;
class RemoteRenderPassEncoder;
class RemoteRenderPipeline;
class RemoteSampler;
class RemoteShaderModule;
class RemoteSurface;
class RemoteSwapChain;
class RemoteTexture;
class RemoteTextureView;
}

namespace WebKit::WebGPU {

class ObjectHeap final : public RefCounted<ObjectHeap>, public WebGPU::ConvertFromBackingContext {
    WTF_MAKE_FAST_ALLOCATED;
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
    void addObject(WebGPUIdentifier, RemoteComputePassEncoder&);
    void addObject(WebGPUIdentifier, RemoteComputePipeline&);
    void addObject(WebGPUIdentifier, RemoteDevice&);
    void addObject(WebGPUIdentifier, RemoteExternalTexture&);
    void addObject(WebGPUIdentifier, RemotePipelineLayout&);
    void addObject(WebGPUIdentifier, RemoteQuerySet&);
    void addObject(WebGPUIdentifier, RemoteQueue&);
    void addObject(WebGPUIdentifier, RemoteRenderBundleEncoder&);
    void addObject(WebGPUIdentifier, RemoteRenderBundle&);
    void addObject(WebGPUIdentifier, RemoteRenderPassEncoder&);
    void addObject(WebGPUIdentifier, RemoteRenderPipeline&);
    void addObject(WebGPUIdentifier, RemoteSampler&);
    void addObject(WebGPUIdentifier, RemoteShaderModule&);
    void addObject(WebGPUIdentifier, RemoteSurface&);
    void addObject(WebGPUIdentifier, RemoteSwapChain&);
    void addObject(WebGPUIdentifier, RemoteTexture&);
    void addObject(WebGPUIdentifier, RemoteTextureView&);

    void removeObject(WebGPUIdentifier);

    void clear();

    PAL::WebGPU::Adapter* convertAdapterFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::BindGroup* convertBindGroupFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::BindGroupLayout* convertBindGroupLayoutFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Buffer* convertBufferFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::CommandBuffer* convertCommandBufferFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::CommandEncoder* convertCommandEncoderFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::ComputePassEncoder* convertComputePassEncoderFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::ComputePipeline* convertComputePipelineFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Device* convertDeviceFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::ExternalTexture* convertExternalTextureFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::PipelineLayout* convertPipelineLayoutFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::QuerySet* convertQuerySetFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Queue* convertQueueFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::RenderBundleEncoder* convertRenderBundleEncoderFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::RenderBundle* convertRenderBundleFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::RenderPassEncoder* convertRenderPassEncoderFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::RenderPipeline* convertRenderPipelineFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Sampler* convertSamplerFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::ShaderModule* convertShaderModuleFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Surface* convertSurfaceFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::SwapChain* convertSwapChainFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::Texture* convertTextureFromBacking(WebGPUIdentifier) final;
    PAL::WebGPU::TextureView* convertTextureViewFromBacking(WebGPUIdentifier) final;

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
        IPC::ScopedActiveMessageReceiveQueue<RemoteComputePassEncoder>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteComputePipeline>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteDevice>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteExternalTexture>,
        IPC::ScopedActiveMessageReceiveQueue<RemotePipelineLayout>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteQuerySet>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteQueue>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundleEncoder>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteRenderBundle>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPassEncoder>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteRenderPipeline>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteSampler>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteShaderModule>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteSurface>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteSwapChain>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteTexture>,
        IPC::ScopedActiveMessageReceiveQueue<RemoteTextureView>
    >;
    HashMap<WebGPUIdentifier, Object> m_objects;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
