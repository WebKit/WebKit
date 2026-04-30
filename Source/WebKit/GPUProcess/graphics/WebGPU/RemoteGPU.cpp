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
#include "RemoteGPU.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "MeshImpl.h"
#include "ModelObjectHeap.h"
#include "RemoteAdapter.h"
#include "RemoteCompositorIntegration.h"
#include "RemoteGPUMessages.h"
#include "RemoteGPUProxyMessages.h"
#include "RemoteMesh.h"
#include "RemotePresentationContext.h"
#include "RemoteRenderingBackend.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/NativeImage.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/WebGPU.h>
#include <WebCore/WebGPUAdapter.h>
#include <WebCore/WebGPUPresentationContext.h>
#include <WebCore/WebGPUPresentationContextDescriptor.h>
#include <wtf/threads/BinarySemaphore.h>

#if HAVE(WEBGPU_IMPLEMENTATION)
#include <WebCore/ProcessIdentity.h>
#include <WebCore/WebGPUCreateImpl.h>
#endif

#if ENABLE(GPU_PROCESS_MODEL)
#include "ModelTypes.h"
#include "WebKitMesh.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_streamConnection)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGPU);

RemoteGPU::RemoteGPU(WebGPUIdentifier identifier, GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_sharedPreferencesForWebProcess(gpuConnectionToWebProcess.sharedPreferencesForWebProcessValue())
    , m_workQueue(IPC::StreamConnectionWorkQueue::create("WebGPU work queue"_s))
    , m_streamConnection(WTF::move(streamConnection))
    , m_objectHeap(WebGPU::ObjectHeap::create())
    , m_modelObjectHeap(ModelObjectHeap::create())
    , m_identifier(identifier)
    , m_renderingBackend(renderingBackend)
{
    weakPtrFactory().prepareForUseOnlyOnNonMainThread();
}

RemoteGPU::~RemoteGPU() = default;

void RemoteGPU::initialize()
{
    assertIsMainRunLoop();
    protect(m_workQueue)->dispatch([protectedThis = protect(*this)]() mutable {
        protectedThis->workQueueInitialize();
    });
}

void RemoteGPU::stopListeningForIPC()
{
    assertIsMainRunLoop();
    Ref workQueue = m_workQueue;
    workQueue->dispatch([protectedThis = protect(*this)]() {
        protectedThis->workQueueUninitialize();
    });
    workQueue->stopAndWaitForCompletion();
}

void RemoteGPU::workQueueInitialize()
{
    assertIsCurrent(workQueue());
    Ref workQueue = m_workQueue;
    RefPtr streamConnection = m_streamConnection;
    streamConnection->open(*this, workQueue);
    streamConnection->startReceivingMessages(*this, Messages::RemoteGPU::messageReceiverName(), m_identifier.toUInt64());

#if HAVE(WEBGPU_IMPLEMENTATION)
    // BEWARE: This is a retain cycle.
    // this owns m_backing, but m_backing contains a callback which has a stong reference to this.
    // The retain cycle is required because callbacks need to execute even if this is disowned
    // (because the callbacks handle resource cleanup, etc.).
    // The retain cycle is broken in workQueueUninitialize().
    auto gpuProcessConnection = m_gpuConnectionToWebProcess.get();
    auto backing = WebCore::WebGPU::create([protectedThis = protect(*this)](WebCore::WebGPU::WorkItem&& workItem) {
        protect(protectedThis->m_workQueue)->dispatch(WTF::move(workItem));
    }, gpuProcessConnection ? &gpuProcessConnection->webProcessIdentity() : nullptr);
#else
    RefPtr<WebCore::WebGPU::GPU> backing;
#endif
    if (backing) {
        m_backing = backing.releaseNonNull();
        send(Messages::RemoteGPUProxy::WasCreated(true, workQueue->wakeUpSemaphore(), streamConnection->clientWaitSemaphore()));
    } else
        send(Messages::RemoteGPUProxy::WasCreated(false, { }, { }));
}

void RemoteGPU::workQueueUninitialize()
{
    assertIsCurrent(workQueue());
    RefPtr streamConnection = m_streamConnection;
    streamConnection->stopReceivingMessages(Messages::RemoteGPU::messageReceiverName(), m_identifier.toUInt64());
    streamConnection->invalidate();
    m_streamConnection = nullptr;
    protect(m_objectHeap)->clear();
    protect(m_modelObjectHeap)->clear();
    m_backing = nullptr;
}

void RemoteGPU::didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT_WITH_PAYLOAD(IPC, makeString("Received an invalid message '"_s, description(messageName), "' from WebContent process, requesting for it to be terminated."_s).utf8().data());
    callOnMainRunLoop([weakGPUConnectionToWebProcess = m_gpuConnectionToWebProcess] {
        if (RefPtr gpuConnectionToWebProcess = weakGPUConnectionToWebProcess.get())
            gpuConnectionToWebProcess->terminateWebProcess();
    });
}


void RemoteGPU::requestAdapter(const WebGPU::RequestAdapterOptions& options, WebGPUIdentifier identifier, CompletionHandler<void(std::optional<RemoteGPURequestAdapterResponse>&&)>&& callback)
{
    assertIsCurrent(workQueue());
    RefPtr backing = m_backing;
    ASSERT(backing);

    Ref objectHeap = m_objectHeap;
    auto convertedOptions = objectHeap->convertFromBacking(options);
    ASSERT(convertedOptions);
    if (!convertedOptions) {
        callback(std::nullopt);
        return;
    }

    backing->requestAdapter(*convertedOptions, [callback = WTF::move(callback), objectHeap, streamConnection = protect(*m_streamConnection), identifier, gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get(), gpu = protect(*this)] (RefPtr<WebCore::WebGPU::Adapter>&& adapter) mutable {
        if (!adapter) {
            callback(std::nullopt);
            return;
        }

        auto remoteAdapter = RemoteAdapter::create(*gpuConnectionToWebProcess, gpu, *adapter, objectHeap, WTF::move(streamConnection), identifier);
        objectHeap->addObject(identifier, remoteAdapter);

        auto name = adapter->name();
        Ref features = adapter->features();
        Ref limits = adapter->limits();
        callback({ { WTF::move(name), WebGPU::SupportedFeatures { features->features() }, WebGPU::SupportedLimits {
            limits->maxTextureDimension1D(),
            limits->maxTextureDimension2D(),
            limits->maxTextureDimension3D(),
            limits->maxTextureArrayLayers(),
            limits->maxBindGroups(),
            limits->maxBindGroupsPlusVertexBuffers(),
            limits->maxBindingsPerBindGroup(),
            limits->maxDynamicUniformBuffersPerPipelineLayout(),
            limits->maxDynamicStorageBuffersPerPipelineLayout(),
            limits->maxSampledTexturesPerShaderStage(),
            limits->maxSamplersPerShaderStage(),
            limits->maxStorageBuffersPerShaderStage(),
            limits->maxStorageTexturesPerShaderStage(),
            limits->maxUniformBuffersPerShaderStage(),
            limits->maxUniformBufferBindingSize(),
            limits->maxStorageBufferBindingSize(),
            limits->minUniformBufferOffsetAlignment(),
            limits->minStorageBufferOffsetAlignment(),
            limits->maxVertexBuffers(),
            limits->maxBufferSize(),
            limits->maxVertexAttributes(),
            limits->maxVertexBufferArrayStride(),
            limits->maxInterStageShaderVariables(),
            limits->maxColorAttachments(),
            limits->maxColorAttachmentBytesPerSample(),
            limits->maxComputeWorkgroupStorageSize(),
            limits->maxComputeInvocationsPerWorkgroup(),
            limits->maxComputeWorkgroupSizeX(),
            limits->maxComputeWorkgroupSizeY(),
            limits->maxComputeWorkgroupSizeZ(),
            limits->maxComputeWorkgroupsPerDimension(),
            limits->maxStorageBuffersInFragmentStage(),
            limits->maxStorageTexturesInFragmentStage(),
            limits->maxStorageBuffersInVertexStage(),
            limits->maxStorageTexturesInVertexStage(),
        }, adapter->isFallbackAdapter() } });
    });
}

void RemoteGPU::createPresentationContext(const WebGPU::PresentationContextDescriptor& descriptor, WebGPUIdentifier identifier)
{
    assertIsCurrent(workQueue());
    RefPtr backing = m_backing;
    ASSERT(backing);

    Ref objectHeap = m_objectHeap;
    auto convertedDescriptor = objectHeap->convertFromBacking(descriptor);
    MESSAGE_CHECK(convertedDescriptor);

    auto presentationContext = backing->createPresentationContext(*convertedDescriptor);
    MESSAGE_CHECK(presentationContext);
    auto remotePresentationContext = RemotePresentationContext::create(*m_gpuConnectionToWebProcess.get(), *this, *presentationContext, objectHeap, *m_streamConnection, identifier);
    objectHeap->addObject(identifier, remotePresentationContext);
}

RefPtr<GPUConnectionToWebProcess> RemoteGPU::gpuConnectionToWebProcess() const
{
    return m_gpuConnectionToWebProcess.get();
}

void RemoteGPU::createCompositorIntegration(WebGPUIdentifier identifier)
{
    assertIsCurrent(workQueue());
    RefPtr backing = m_backing;
    ASSERT(backing);

    auto compositorIntegration = backing->createCompositorIntegration();
    MESSAGE_CHECK(compositorIntegration);

    Ref objectHeap = m_objectHeap;
    auto remoteCompositorIntegration = RemoteCompositorIntegration::create(*compositorIntegration, objectHeap, *m_streamConnection, *this, identifier);
    objectHeap->addObject(identifier, remoteCompositorIntegration);
}

void RemoteGPU::paintNativeImageToImageBuffer(WebCore::NativeImage& nativeImage, WebCore::RenderingResourceIdentifier imageBufferIdentifier)
{
    assertIsCurrent(workQueue());
    BinarySemaphore semaphore;

    RefPtr gpu = m_backing.get();
    Ref renderingBackend = m_renderingBackend;
    Ref protectedNativeImage = nativeImage;
    renderingBackend->dispatch([&]() mutable {
        if (auto imageBuffer = renderingBackend->imageBuffer(imageBufferIdentifier)) {
            gpu->paintToCanvas(protectedNativeImage, imageBuffer->backendSize(), imageBuffer->context());
            imageBuffer->flushDrawingContext();
        }
        semaphore.signal();
    });
    semaphore.wait();
}


#if ENABLE(GPU_PROCESS_MODEL)
Vector<UniqueRef<WebCore::IOSurface>> RemoteGPU::createRenderBuffers(unsigned width, unsigned height, const WebCore::ProcessIdentity& processIdentity)
{
    const auto colorFormat = WebCore::IOSurface::Format::RGBA16F;
    const auto colorSpace = WebCore::DestinationColorSpace::ExtendedLinearDisplayP3();

    Vector<UniqueRef<WebCore::IOSurface>> ioSurfaces;

    constexpr auto surfaceCount = 2;
    for (auto i = 0; i < surfaceCount; ++i) {
        if (auto buffer = WebCore::IOSurface::create(nullptr, WebCore::IntSize(width, height), colorSpace, WebCore::IOSurface::Name::WebGPU, colorFormat)) {
            buffer->setOwnershipIdentity(processIdentity);
            ioSurfaces.append(makeUniqueRefFromNonNullUniquePtr(WTF::move(buffer)));
        }
    }

    return ioSurfaces;
}
#endif

#if ENABLE(GPU_PROCESS_MODEL)
static RefPtr<WebKit::Mesh> createModelBackingInternal(unsigned width, unsigned height, const WebModel::ImageAsset& diffuseTexture, const WebModel::ImageAsset& specularTexture, const WebCore::ProcessIdentity& processIdentity, CompletionHandler<void(Vector<MachSendRight>&&)>&& callback)
{
    auto ioSurfaceVector = RemoteGPU::createRenderBuffers(width, height, processIdentity);
    Vector<RetainPtr<IOSurfaceRef>> ioSurfaces;
    for (auto& ioSurface : ioSurfaceVector)
        ioSurfaces.append(ioSurface->surface());

    WebModelCreateMeshDescriptor backingDescriptor {
        .width = width,
        .height = height,
        .ioSurfaces = WTF::move(ioSurfaces),
        .diffuseTexture = diffuseTexture,
        .specularTexture = specularTexture,
        .processIdentity = &processIdentity
    };

    auto mesh = WebKit::MeshImpl::create(WebMesh::create(backingDescriptor), WTF::move(ioSurfaceVector));
    callback(mesh->ioSurfaceHandles());
    return mesh;
}
#endif

void RemoteGPU::createModelBacking(unsigned width, unsigned height, const WebModel::ImageAsset& diffuseTexture, const WebModel::ImageAsset& specularTexture, WebModelIdentifier identifier, CompletionHandler<void(Vector<MachSendRight>&&)>&& callback)
{
#if ENABLE(GPU_PROCESS_MODEL)
    assertIsCurrent(workQueue());

    constexpr auto max2dTextureSize = 16384;
    MESSAGE_CHECK(width <= max2dTextureSize && height <= max2dTextureSize);
    auto& inputSpecularTexture = specularTexture;
    auto& inputDiffuseTexture = diffuseTexture;
    {
#define loadData(...) { }
        WEBMODEL_WEB_MODEL_PLAYER_DECLARE_DIFFUSE_AND_SPECULAR_TEXTURES
#undef loadData
#define equalIgnoringDataContents(a, b, dataSize) \
            a.data.size() == dataSize && \
            a.width == b.width && \
            a.height == b.height && \
            a.depth == b.depth && \
            a.textureType == b.textureType && \
            a.pixelFormat == b.pixelFormat && \
            a.mipmapLevelCount == b.mipmapLevelCount && \
            a.arrayLength == b.arrayLength && \
            a.textureUsage == b.textureUsage

        MESSAGE_CHECK(equalIgnoringDataContents(inputDiffuseTexture, diffuseTexture, 49152));
        MESSAGE_CHECK(equalIgnoringDataContents(inputSpecularTexture, specularTexture, 1048572));
#undef equalIgnoringDataContents
    }
    Ref objectHeap = m_modelObjectHeap.get();

    auto gpuProcessConnection = m_gpuConnectionToWebProcess.get();
    MESSAGE_CHECK(gpuProcessConnection);

    auto mesh = createModelBackingInternal(width, height, diffuseTexture, specularTexture, gpuProcessConnection->webProcessIdentity(), WTF::move(callback));
    auto remoteMesh = RemoteMesh::create(*m_gpuConnectionToWebProcess.get(), *this, *mesh, objectHeap, protect(*m_streamConnection), identifier);
    objectHeap->addObject(identifier, remoteMesh);
#else
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(diffuseTexture);
    UNUSED_PARAM(specularTexture);
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(callback);
#endif
}

void RemoteGPU::isValid(WebGPUIdentifier identifier, CompletionHandler<void(bool, bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    RefPtr gpu = static_cast<WebCore::WebGPU::GPU*>(m_backing.get());
    if (!gpu) {
        completionHandler(false, false);
        return;
    }

    auto result = protect(m_objectHeap)->objectExistsAndValid(*gpu, identifier);
    completionHandler(result.valid, result.exists);
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
