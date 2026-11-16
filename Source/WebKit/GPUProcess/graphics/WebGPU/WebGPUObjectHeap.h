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
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>
#include <functional>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class WebGPUCompositorIntegration;
class WebGPUIntegration;
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

class ObjectHeap final : public RefCountedAndCanMakeWeakPtr<ObjectHeap>, public WebGPU::ConvertFromBackingContext {
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

    RefPtr<::WebGPU::Adapter> convertAdapterFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::BindGroup> convertBindGroupFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::BindGroupLayout> convertBindGroupLayoutFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::Buffer> convertBufferFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::CommandBuffer> convertCommandBufferFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::CommandEncoder> convertCommandEncoderFromBacking(WebGPUIdentifier) final;
    WeakPtr<WebCore::WebGPUCompositorIntegration> convertCompositorIntegrationFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::ComputePassEncoder> convertComputePassEncoderFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::ComputePipeline> convertComputePipelineFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::Device> convertDeviceFromBacking(WebGPUIdentifier) final;
    ThreadSafeWeakPtr<::WebGPU::ExternalTexture> convertExternalTextureFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::PipelineLayout> convertPipelineLayoutFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::PresentationContext> convertPresentationContextFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::QuerySet> convertQuerySetFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::Queue> convertQueueFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::RenderBundleEncoder> convertRenderBundleEncoderFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::RenderBundle> convertRenderBundleFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::RenderPassEncoder> convertRenderPassEncoderFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::RenderPipeline> convertRenderPipelineFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::Sampler> convertSamplerFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::ShaderModule> convertShaderModuleFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::Texture> convertTextureFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::TextureView> convertTextureViewFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::XRBinding> convertXRBindingFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::XRSubImage> convertXRSubImageFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::XRProjectionLayer> convertXRProjectionLayerFromBacking(WebGPUIdentifier) final;
    RefPtr<::WebGPU::XRView> createXRViewFromBacking(WebGPUIdentifier) final;

    struct ExistsAndValid {
        bool exists { false };
        bool valid { false };
    };
    ExistsAndValid objectExistsAndValid(const WebCore::WebGPUIntegration&, WebGPUIdentifier) const;
private:
    ObjectHeap();

    using Object = Variant<
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
