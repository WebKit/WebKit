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

#include "GPUConnectionToWebProcess.h"
#include "RemoteQueue.h"
#include "RemoteVideoFrameIdentifier.h"
#include "SharedVideoFrame.h"
#include "StreamMessageReceiver.h"
#include "WebGPUError.h"
#include "WebGPUIdentifier.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/WebGPUErrorFilter.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#endif

typedef struct __CVBuffer* CVPixelBufferRef;

namespace WebCore::WebGPU {
class Device;
enum class DeviceLostReason : uint8_t;
}

namespace IPC {
class Connection;
class Semaphore;
class StreamServerConnection;
}

namespace WebCore {
class MediaPlayer;
class SharedMemoryHandle;
class VideoFrame;
}

namespace WebKit {

class RemoteGPU;
struct SharedVideoFrame;

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
    WTF_MAKE_TZONE_ALLOCATED(RemoteDevice);
public:
    static Ref<RemoteDevice> create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, WebCore::WebGPU::Device& device, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    {
        return adoptRef(*new RemoteDevice(gpuConnectionToWebProcess, gpu, device, objectHeap, WTFMove(streamConnection), identifier, queueIdentifier));
    }

    ~RemoteDevice();

    // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once https://github.com/llvm/llvm-project/pull/111198 lands.
    SUPPRESS_UNCOUNTED_ARG std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_gpu->sharedPreferencesForWebProcess(); }

    void stopListeningForIPC();

    Ref<RemoteQueue> queue();

private:
    friend class WebGPU::ObjectHeap;

    RemoteDevice(GPUConnectionToWebProcess&, RemoteGPU&, WebCore::WebGPU::Device&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebGPUIdentifier, WebGPUIdentifier queueIdentifier);

    RemoteDevice(const RemoteDevice&) = delete;
    RemoteDevice(RemoteDevice&&) = delete;
    RemoteDevice& operator=(const RemoteDevice&) = delete;
    RemoteDevice& operator=(RemoteDevice&&) = delete;

    WebCore::WebGPU::Device& backing() { return m_backing; }
    Ref<WebCore::WebGPU::Device> protectedBacking();
    Ref<IPC::StreamServerConnection> protectedStreamConnection() const;
    Ref<WebGPU::ObjectHeap> protectedObjectHeap() const;
    Ref<RemoteGPU> protectedGPU() const { return m_gpu.get(); }

    RefPtr<IPC::Connection> connection() const;

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void destroy();
    void destruct();

    void createXRBinding(WebGPUIdentifier);
    void createBuffer(const WebGPU::BufferDescriptor&, WebGPUIdentifier);
    void createTexture(const WebGPU::TextureDescriptor&, WebGPUIdentifier);
    void createSampler(const WebGPU::SamplerDescriptor&, WebGPUIdentifier);
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    void importExternalTextureFromVideoFrame(const WebGPU::ExternalTextureDescriptor&, WebGPUIdentifier);
    void updateExternalTexture(WebGPUIdentifier, const WebCore::MediaPlayerIdentifier&);
#endif

    void createBindGroupLayout(const WebGPU::BindGroupLayoutDescriptor&, WebGPUIdentifier);
    void createPipelineLayout(const WebGPU::PipelineLayoutDescriptor&, WebGPUIdentifier);
    void createBindGroup(const WebGPU::BindGroupDescriptor&, WebGPUIdentifier);

    void createShaderModule(const WebGPU::ShaderModuleDescriptor&, WebGPUIdentifier);
    void createComputePipeline(const WebGPU::ComputePipelineDescriptor&, WebGPUIdentifier);
    void createRenderPipeline(const WebGPU::RenderPipelineDescriptor&, WebGPUIdentifier);
    void createComputePipelineAsync(const WebGPU::ComputePipelineDescriptor&, WebGPUIdentifier, CompletionHandler<void(bool, String&&)>&&);
    void createRenderPipelineAsync(const WebGPU::RenderPipelineDescriptor&, WebGPUIdentifier, CompletionHandler<void(bool, String&&)>&&);

    void createCommandEncoder(const std::optional<WebGPU::CommandEncoderDescriptor>&, WebGPUIdentifier);
    void createRenderBundleEncoder(const WebGPU::RenderBundleEncoderDescriptor&, WebGPUIdentifier);

    void createQuerySet(const WebGPU::QuerySetDescriptor&, WebGPUIdentifier);

    void pushErrorScope(WebCore::WebGPU::ErrorFilter);
    void popErrorScope(CompletionHandler<void(bool, std::optional<WebGPU::Error>&&)>&&);
    void resolveUncapturedErrorEvent(CompletionHandler<void(bool, std::optional<WebGPU::Error>&&)>&&);
    void resolveDeviceLostPromise(CompletionHandler<void(WebCore::WebGPU::DeviceLostReason)>&&);

    void setLabel(String&&);
    void setSharedVideoFrameSemaphore(IPC::Semaphore&&);
    void setSharedVideoFrameMemory(WebCore::SharedMemoryHandle&&);

    Ref<WebCore::WebGPU::Device> m_backing;
    WeakRef<WebGPU::ObjectHeap> m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WebGPUIdentifier m_identifier;
    Ref<RemoteQueue> m_queue;
#if ENABLE(VIDEO)
    Ref<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;
#if PLATFORM(COCOA)
    SharedVideoFrameReader m_sharedVideoFrameReader;
#endif
#endif
    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    WeakRef<RemoteGPU> m_gpu;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
