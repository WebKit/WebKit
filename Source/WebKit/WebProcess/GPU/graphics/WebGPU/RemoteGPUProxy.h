/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "GPUProcessConnection.h"
#include "RemoteRenderingBackendIdentifier.h"
#include "StreamClientConnection.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPU.h>
#include <WebCore/WebGPUPresentationContext.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebModel {
struct ImageAsset;
struct MeshDescriptor;
}

namespace WebCore {
class IntSize;
class GraphicsContext;
class Mesh;
class NativeImage;
class Mesh;
class ModelConvertToBackingContext;
}

namespace WebKit {
class ConvertToBackingContext;
class ModelConvertToBackingContext;
class RemoteRenderingBackendProxy;
class WebPage;

namespace WebGPU {
class ConvertToBackingContext;
class DowncastConvertToBackingContext;
}

class RemoteGPUProxy final : public WebCore::WebGPU::GPU, private IPC::Connection::Client, public ThreadSafeRefCounted<RemoteGPUProxy>, SerialFunctionDispatcher {
    WTF_MAKE_TZONE_ALLOCATED(RemoteGPUProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGPUProxy);
public:
    static RefPtr<RemoteGPUProxy> create(WebGPU::ConvertToBackingContext&, ModelConvertToBackingContext&, WebPage&);
    static RefPtr<RemoteGPUProxy> create(WebGPU::ConvertToBackingContext&, ModelConvertToBackingContext&, RemoteRenderingBackendProxy&, SerialFunctionDispatcher&);

    virtual ~RemoteGPUProxy();

    RemoteGPUProxy& root() { return *this; }

    IPC::StreamClientConnection& streamClientConnection() { return *m_streamConnection; }
    Ref<IPC::StreamClientConnection> protectedStreamClientConnection() { return *m_streamConnection; }

    void ref() const final { return ThreadSafeRefCounted<RemoteGPUProxy>::ref(); }
    void deref() const final { return ThreadSafeRefCounted<RemoteGPUProxy>::deref(); }

    void paintToCanvas(WebCore::NativeImage&, const WebCore::IntSize&, WebCore::GraphicsContext&) final;
    WebGPUIdentifier backing() const { return m_backing; }

private:
    friend class WebGPU::DowncastConvertToBackingContext;

    RemoteGPUProxy(WebGPU::ConvertToBackingContext&, ModelConvertToBackingContext&, SerialFunctionDispatcher&);
    void initializeIPC(Ref<IPC::StreamClientConnection>&&, RemoteRenderingBackendIdentifier, IPC::StreamServerConnection::Handle&&);

    RemoteGPUProxy(const RemoteGPUProxy&) = delete;
    RemoteGPUProxy(RemoteGPUProxy&&) = delete;
    RemoteGPUProxy& operator=(const RemoteGPUProxy&) = delete;
    RemoteGPUProxy& operator=(RemoteGPUProxy&&) = delete;

    bool isRemoteGPUProxy() const final { return true; }

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding) final { }

    // Messages to be received.
    void wasCreated(bool didSucceed, IPC::Semaphore&& wakeUpSemaphore, IPC::Semaphore&& clientWaitSemaphore);

    void waitUntilInitialized();

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return root().protectedStreamClientConnection()->send(std::forward<T>(message), backing());
    }
    template<typename T>
    [[nodiscard]] IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().protectedStreamClientConnection()->sendSync(std::forward<T>(message), backing());
    }
    template<typename T, typename C>
    [[nodiscard]] std::optional<IPC::StreamClientConnection::AsyncReplyID> sendWithAsyncReply(T&& message, C&& completionHandler)
    {
        return root().protectedStreamClientConnection()->sendWithAsyncReply(WTF::move(message), completionHandler, backing());
    }

    void requestAdapter(const WebCore::WebGPU::RequestAdapterOptions&, CompletionHandler<void(RefPtr<WebCore::WebGPU::Adapter>&&)>&&) final;
    RefPtr<WebCore::Mesh> createModelBacking(unsigned width, unsigned height, const WebModel::ImageAsset& diffuseTexture, const WebModel::ImageAsset& specularTexture, CompletionHandler<void(Vector<MachSendRight>&&)>&&) final;

    RefPtr<WebCore::WebGPU::PresentationContext> createPresentationContext(const WebCore::WebGPU::PresentationContextDescriptor&) final;

    RefPtr<WebCore::WebGPU::CompositorIntegration> createCompositorIntegration() final;
    bool isValid(const WebCore::WebGPU::CompositorIntegration&) const final;
    bool isValid(const WebCore::WebGPU::Buffer&) const final;
    bool isValid(const WebCore::WebGPU::Adapter&) const final;
    bool isValid(const WebCore::WebGPU::BindGroup&) const final;
    bool isValid(const WebCore::WebGPU::BindGroupLayout&) const final;
    bool isValid(const WebCore::WebGPU::CommandBuffer&) const final;
    bool isValid(const WebCore::WebGPU::CommandEncoder&) const final;
    bool isValid(const WebCore::WebGPU::ComputePassEncoder&) const final;
    bool isValid(const WebCore::WebGPU::ComputePipeline&) const final;
    bool isValid(const WebCore::WebGPU::Device&) const final;
    bool isValid(const WebCore::WebGPU::ExternalTexture&) const final;
    bool isValid(const WebCore::WebGPU::PipelineLayout&) const final;
    bool isValid(const WebCore::WebGPU::PresentationContext&) const final;
    bool isValid(const WebCore::WebGPU::QuerySet&) const final;
    bool isValid(const WebCore::WebGPU::Queue&) const final;
    bool isValid(const WebCore::WebGPU::RenderBundleEncoder&) const final;
    bool isValid(const WebCore::WebGPU::RenderBundle&) const final;
    bool isValid(const WebCore::WebGPU::RenderPassEncoder&) const final;
    bool isValid(const WebCore::WebGPU::RenderPipeline&) const final;
    bool isValid(const WebCore::WebGPU::Sampler&) const final;
    bool isValid(const WebCore::WebGPU::ShaderModule&) const final;
    bool isValid(const WebCore::WebGPU::Texture&) const final;
    bool isValid(const WebCore::WebGPU::TextureView&) const final;
    bool isValid(const WebCore::WebGPU::XRBinding&) const final;
    bool isValid(const WebCore::WebGPU::XRSubImage&) const final;
    bool isValid(const WebCore::WebGPU::XRProjectionLayer&) const final;
    bool isValid(const WebCore::WebGPU::XRView&) const final;

    void abandonGPUProcess();
    void disconnectGpuProcessIfNeeded();

    // SerialFunctionDispatcher
    void dispatch(Function<void()>&&) final;
    bool isCurrent() const final;

    RefPtr<IPC::StreamClientConnection> protectedStreamConnection() const { return m_streamConnection; }

    const Ref<WebGPU::ConvertToBackingContext> m_convertToBackingContext;
    const Ref<ModelConvertToBackingContext> m_modelConvertToBackingContext;
    ThreadSafeWeakPtr<SerialFunctionDispatcher> m_dispatcher;
    WeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    RefPtr<IPC::StreamClientConnection> m_streamConnection;
    WebGPUIdentifier m_backing { WebGPUIdentifier::generate() };
    bool m_didInitialize { false };
    bool m_lost { false };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteGPUProxy)
    static bool isType(const WebCore::WebGPU::GPU& gpu) { return gpu.isRemoteGPUProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
