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

#include "RemoteQueue.h"
#include "StreamMessageReceiver.h"
#include "WebGPUError.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUErrorFilter.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
class Device;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

namespace WebGPU {
struct BindGroupDescriptor;
struct BindGroupLayoutDescriptor;
struct BufferDescriptor;
struct CommandEncoderDescriptor;
struct ComputePipelineDescriptor;
struct ExternalTextureDescriptor;
class ObjectHeap;
struct PipelineLayoutDescriptor;
struct QuerySetDescriptor;
struct RenderBundleEncoderDescriptor;
struct RenderPipelineDescriptor;
struct SamplerDescriptor;
struct ShaderModuleDescriptor;
struct TextureDescriptor;
}

class RemoteDevice final : public IPC::StreamMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteDevice> create(PAL::WebGPU::Device& device, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    {
        return adoptRef(*new RemoteDevice(device, objectHeap, WTFMove(streamConnection), identifier, queueIdentifier));
    }

    ~RemoteDevice();

    void stopListeningForIPC();

    Ref<RemoteQueue> queue();

private:
    friend class WebGPU::ObjectHeap;

    RemoteDevice(PAL::WebGPU::Device&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebGPUIdentifier, WebGPUIdentifier queueIdentifier);

    RemoteDevice(const RemoteDevice&) = delete;
    RemoteDevice(RemoteDevice&&) = delete;
    RemoteDevice& operator=(const RemoteDevice&) = delete;
    RemoteDevice& operator=(RemoteDevice&&) = delete;

    PAL::WebGPU::Device& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void destroy();

    void createBuffer(const WebGPU::BufferDescriptor&, WebGPUIdentifier);
    void createTexture(const WebGPU::TextureDescriptor&, WebGPUIdentifier);
    void createSurfaceTexture(WebGPUIdentifier, const WebGPU::TextureDescriptor&, WebGPUIdentifier);
    void createSampler(const WebGPU::SamplerDescriptor&, WebGPUIdentifier);
    void importExternalTexture(const WebGPU::ExternalTextureDescriptor&, WebGPUIdentifier);

    void createBindGroupLayout(const WebGPU::BindGroupLayoutDescriptor&, WebGPUIdentifier);
    void createPipelineLayout(const WebGPU::PipelineLayoutDescriptor&, WebGPUIdentifier);
    void createBindGroup(const WebGPU::BindGroupDescriptor&, WebGPUIdentifier);

    void createShaderModule(const WebGPU::ShaderModuleDescriptor&, WebGPUIdentifier);
    void createComputePipeline(const WebGPU::ComputePipelineDescriptor&, WebGPUIdentifier);
    void createRenderPipeline(const WebGPU::RenderPipelineDescriptor&, WebGPUIdentifier);
    void createComputePipelineAsync(const WebGPU::ComputePipelineDescriptor&, WebGPUIdentifier, CompletionHandler<void()>&&);
    void createRenderPipelineAsync(const WebGPU::RenderPipelineDescriptor&, WebGPUIdentifier, CompletionHandler<void()>&&);

    void createCommandEncoder(const std::optional<WebGPU::CommandEncoderDescriptor>&, WebGPUIdentifier);
    void createRenderBundleEncoder(const WebGPU::RenderBundleEncoderDescriptor&, WebGPUIdentifier);

    void createQuerySet(const WebGPU::QuerySetDescriptor&, WebGPUIdentifier);

    void pushErrorScope(PAL::WebGPU::ErrorFilter);
    void popErrorScope(CompletionHandler<void(std::optional<WebGPU::Error>&&)>&&);

    void setLabel(String&&);

    Ref<PAL::WebGPU::Device> m_backing;
    WebGPU::ObjectHeap& m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WebGPUIdentifier m_identifier;
    Ref<RemoteQueue> m_queue;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
