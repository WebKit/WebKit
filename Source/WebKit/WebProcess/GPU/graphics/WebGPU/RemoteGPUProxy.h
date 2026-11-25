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
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebModel {
struct ImageAsset;
struct MeshDescriptor;
}

namespace WebCore {
class NativeImage;
}

namespace WebKit {
class Mesh;
class ModelConvertToBackingContext;
class RemoteRenderingBackendProxy;
class WebPage;

namespace WebGPU {
class RemoteWebGPUInstanceProxy;
class RemoteDeviceProxy;
}

class RemoteGPUProxy final : public WebCore::WebGPUIntegration, private IPC::Connection::Client, public ThreadSafeRefCounted<RemoteGPUProxy>, SerialFunctionDispatcher {
    WTF_MAKE_TZONE_ALLOCATED(RemoteGPUProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGPUProxy);
public:
    static RefPtr<RemoteGPUProxy> create(ModelConvertToBackingContext&, WebPage&);
    static RefPtr<RemoteGPUProxy> create(ModelConvertToBackingContext&, RemoteRenderingBackendProxy&, SerialFunctionDispatcher&);

    virtual ~RemoteGPUProxy();

    RemoteGPUProxy& root() { return *this; }

    IPC::StreamClientConnection& streamClientConnection() { return *m_streamConnection; }
    // The connection of the GPU process connection that streamClientConnection() was created on.
    IPC::Connection& gpuProcessIPCConnection() const { return *m_gpuProcessIPCConnection; }

    void ref() const final { return ThreadSafeRefCounted<RemoteGPUProxy>::ref(); }
    void deref() const final { return ThreadSafeRefCounted<RemoteGPUProxy>::deref(); }
    bool isLost() const final { return m_lost; }

    void copyExternalImageToTexture(::WebGPU::Queue&, const WebCore::WebGPUExternalImageSource&, const WebCore::WebGPUImageCopyTextureTagged& destination, const ::WebGPU::Extent3D& copySize) final;
    RefPtr<WebCore::NativeImage> nativeImage(::WebGPU::Queue&, WebCore::VideoFrame&) final;
    RefPtr<::WebGPU::ExternalTexture> importExternalTexture(::WebGPU::Device&, const WebCore::WebGPUExternalTextureDescriptor&) final;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    void updateExternalTexture(::WebGPU::Device&, const ::WebGPU::ExternalTexture&, const WebCore::MediaPlayerIdentifier&) final;
#endif
    RemoteGPUIdentifier backing() const { return m_backing; }
    NODELETE RefPtr<WebKit::Mesh> createModelBacking(unsigned width, unsigned height, WebModel::ImageAsset&& diffuseTexture, WebModel::ImageAsset&& specularTexture, bool standardDynamicRange, CompletionHandler<void(Vector<MachSendRight>&&)>&&);

    // The WebGPU::Instance that the adapters are created from. Called on the dispatcher thread.
    Ref<WebGPU::RemoteWebGPUInstanceProxy> ensureInstance();
    // The devices that get the uncaptured errors that the GPU process sends. Called on the dispatcher.
    void addUncapturedErrorListener(WebGPUDeviceIdentifier, WebGPU::RemoteDeviceProxy&);
    void removeUncapturedErrorListener(WebGPUDeviceIdentifier);

    // Whether this is called on the dispatcher thread, which dispatches the replies of the
    // asynchronous messages.
    bool isOnDispatcher() const { return isCurrent(); }

private:
    friend class WebGPU::RemoteWebGPUInstanceProxy;

    RemoteGPUProxy(ModelConvertToBackingContext&, RemoteRenderingBackendProxy&, SerialFunctionDispatcher&);
    void initializeIPC(Ref<IPC::StreamClientConnection>&&, IPC::Connection& renderingBackendIPCConnection, IPC::StreamServerConnection::Handle&&);

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
    void wasCreated(bool didSucceed);
    void didGenerateUncapturedError(WebGPUDeviceIdentifier, ::WebGPU::Error&&);

    void waitUntilInitialized();

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }
    template<typename T>
    [[nodiscard]] IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return protect(root().streamClientConnection())->sendSync(std::forward<T>(message), backing());
    }
    template<typename T, typename C>
    [[nodiscard]] std::optional<IPC::StreamClientConnection::AsyncReplyID> sendWithAsyncReply(T&& message, C&& completionHandler)
    {
        return protect(root().streamClientConnection())->sendWithAsyncReply(std::forward<T>(message), std::forward<C>(completionHandler), backing());
    }

    // The GPU process destroys the instance when the GPU is released.
    void destroy() final { }

    void requestAdapter(const ::WebGPU::RequestAdapterOptions&, CompletionHandler<void(RefPtr<::WebGPU::Adapter>&&)>&&) final;

    RefPtr<::WebGPU::PresentationContext> createPresentationContext(const WebCore::WebGPUPresentationContextDescriptor&) final;

    RefPtr<WebCore::WebGPUCompositorIntegration> createCompositorIntegration() final;

    void abandonGPUProcess();
    void gpuProcessConnectionDidClose();
    void disconnectGpuProcessIfNeeded();

    // SerialFunctionDispatcher
    void dispatch(Function<void()>&&) final;
    bool isCurrent() const final;

    const Ref<ModelConvertToBackingContext> m_modelConvertToBackingContext;
    ThreadSafeWeakPtr<SerialFunctionDispatcher> m_dispatcher;
    WeakPtr<GPUProcessConnection> m_gpuProcessConnection;
    const RefPtr<IPC::StreamClientConnection> m_streamConnection;
    const RefPtr<IPC::Connection> m_gpuProcessIPCConnection;
    const Ref<RemoteRenderingBackendProxy> m_renderingBackend;
    // Weak, because the instance holds a reference to this. The adapters hold the instance.
    ThreadSafeWeakPtr<WebGPU::RemoteWebGPUInstanceProxy> m_instance;
    RemoteGPUIdentifier m_backing { RemoteGPUIdentifier::generate() };
    HashMap<WebGPUDeviceIdentifier, ThreadSafeWeakPtr<WebGPU::RemoteDeviceProxy>> m_uncapturedErrorListeners;
    bool m_didInitialize { false };
    bool m_lost { false };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteGPUProxy)
    static bool isType(const WebCore::WebGPUIntegration& gpu) { return gpu.isRemoteGPUProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
