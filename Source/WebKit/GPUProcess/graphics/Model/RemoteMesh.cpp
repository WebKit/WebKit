/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RemoteMesh.h"

#if ENABLE(GPU_PROCESS_MODEL)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "MeshImpl.h"
#include "ModelObjectHeap.h"
#include "ModelTypes.h"
#include "RemoteMeshMessages.h"
#include "StreamServerConnection.h"
#include <limits>
#include <tuple>
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_OPTIONAL_CONNECTION_BASE(assertion, connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection(), completion)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMesh);

RemoteMesh::RemoteMesh(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, WebKit::Mesh& mesh, ModelObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebModelIdentifier identifier, bool standardDynamicRange)
    : m_backing(mesh)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_identifier(identifier)
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_gpu(gpu)
    , m_standardDynamicRange(standardDynamicRange)
{
    protect(m_streamConnection)->startReceivingMessages(*this, Messages::RemoteMesh::messageReceiverName(), m_identifier.toUInt64());
}

RemoteMesh::~RemoteMesh() = default;

RefPtr<IPC::Connection> RemoteMesh::connection() const
{
    RefPtr connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return nullptr;
    return &connection->connection();
}

void RemoteMesh::stopListeningForIPC()
{
    protect(m_streamConnection)->stopReceivingMessages(Messages::RemoteMesh::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteMesh::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteMesh::setLabel(String&& label)
{
    m_backing->setLabel(WTF::move(label));
}

<<<<<<< HEAD
void RemoteMesh::updateModel(WebCore::NodeIdentifier nodeIdentifier, Vector<WebModel::UpdateMeshDescriptor>&& descriptor)
{
    m_backing->updateModel(nodeIdentifier, WTF::move(descriptor));
}

void RemoteMesh::removeModel(WebCore::NodeIdentifier nodeIdentifier)
{
    m_backing->removeModel(nodeIdentifier);
=======
static int64_t vertexFormatByteSize(WebCore::WebGPU::VertexFormat format)
{
    switch (format) {
    case WebCore::WebGPU::VertexFormat::Uint8:
    case WebCore::WebGPU::VertexFormat::Sint8:
    case WebCore::WebGPU::VertexFormat::Unorm8:
    case WebCore::WebGPU::VertexFormat::Snorm8:
        return 1;
    case WebCore::WebGPU::VertexFormat::Uint8x2:
    case WebCore::WebGPU::VertexFormat::Sint8x2:
    case WebCore::WebGPU::VertexFormat::Unorm8x2:
    case WebCore::WebGPU::VertexFormat::Snorm8x2:
    case WebCore::WebGPU::VertexFormat::Uint16:
    case WebCore::WebGPU::VertexFormat::Sint16:
    case WebCore::WebGPU::VertexFormat::Unorm16:
    case WebCore::WebGPU::VertexFormat::Snorm16:
    case WebCore::WebGPU::VertexFormat::Float16:
        return 2;
    case WebCore::WebGPU::VertexFormat::Uint8x4:
    case WebCore::WebGPU::VertexFormat::Sint8x4:
    case WebCore::WebGPU::VertexFormat::Unorm8x4:
    case WebCore::WebGPU::VertexFormat::Snorm8x4:
    case WebCore::WebGPU::VertexFormat::Uint16x2:
    case WebCore::WebGPU::VertexFormat::Sint16x2:
    case WebCore::WebGPU::VertexFormat::Unorm16x2:
    case WebCore::WebGPU::VertexFormat::Snorm16x2:
    case WebCore::WebGPU::VertexFormat::Float16x2:
    case WebCore::WebGPU::VertexFormat::Float32:
    case WebCore::WebGPU::VertexFormat::Uint32:
    case WebCore::WebGPU::VertexFormat::Sint32:
    case WebCore::WebGPU::VertexFormat::Unorm1010102:
    case WebCore::WebGPU::VertexFormat::Unorm8x4Bgra:
        return 4;
    case WebCore::WebGPU::VertexFormat::Uint16x4:
    case WebCore::WebGPU::VertexFormat::Sint16x4:
    case WebCore::WebGPU::VertexFormat::Unorm16x4:
    case WebCore::WebGPU::VertexFormat::Snorm16x4:
    case WebCore::WebGPU::VertexFormat::Float16x4:
    case WebCore::WebGPU::VertexFormat::Float32x2:
    case WebCore::WebGPU::VertexFormat::Uint32x2:
    case WebCore::WebGPU::VertexFormat::Sint32x2:
        return 8;
    case WebCore::WebGPU::VertexFormat::Float32x3:
    case WebCore::WebGPU::VertexFormat::Uint32x3:
    case WebCore::WebGPU::VertexFormat::Sint32x3:
        return 12;
    case WebCore::WebGPU::VertexFormat::Float32x4:
    case WebCore::WebGPU::VertexFormat::Uint32x4:
    case WebCore::WebGPU::VertexFormat::Sint32x4:
        return 16;
    }
}

static int64_t deformedAttributeByteSize(WebModel::VertexSemantic semantic)
{
    switch (semantic) {
    case WebModel::VertexSemantic::Position:
    case WebModel::VertexSemantic::Normal:
    case WebModel::VertexSemantic::Tangent:
    case WebModel::VertexSemantic::Bitangent:
        return 12;
    case WebModel::VertexSemantic::UV0:
    case WebModel::VertexSemantic::UV1:
    case WebModel::VertexSemantic::UV2:
    case WebModel::VertexSemantic::UV3:
    case WebModel::VertexSemantic::UV4:
    case WebModel::VertexSemantic::UV5:
    case WebModel::VertexSemantic::UV6:
    case WebModel::VertexSemantic::UV7:
        return 8;
    case WebModel::VertexSemantic::Color:
    case WebModel::VertexSemantic::Unspecified:
        return 0;
    }
}

static int64_t indexByteSize(WebModel::IndexType indexType)
{
    switch (indexType) {
    case WebModel::IndexType::UInt16:
        return 2;
    case WebModel::IndexType::UInt32:
        return 4;
    }
}

static bool isSameDescriptor(const WebModel::MeshDescriptor& a, const WebModel::MeshDescriptor& b)
{
    if (std::tie(a.vertexBufferCount, a.vertexCapacity, a.indexCapacity, a.indexType) != std::tie(b.vertexBufferCount, b.vertexCapacity, b.indexCapacity, b.indexType))
        return false;

    if (a.vertexLayouts.size() != b.vertexLayouts.size() || a.vertexAttributes.size() != b.vertexAttributes.size())
        return false;

    for (size_t i = 0; i < a.vertexLayouts.size(); ++i) {
        auto& x = a.vertexLayouts[i];
        auto& y = b.vertexLayouts[i];
        if (std::tie(x.bufferIndex, x.bufferOffset, x.bufferStride, x.stepFunction, x.stepRate) != std::tie(y.bufferIndex, y.bufferOffset, y.bufferStride, y.stepFunction, y.stepRate))
            return false;
    }

    for (size_t i = 0; i < a.vertexAttributes.size(); ++i) {
        auto& x = a.vertexAttributes[i];
        auto& y = b.vertexAttributes[i];
        if (std::tie(x.semantic, x.format, x.layoutIndex, x.offset) != std::tie(y.semantic, y.format, y.layoutIndex, y.offset))
            return false;
    }

    return true;
}

static long declaredBufferCount(const WebModel::MeshDescriptor& descriptor)
{
    long count = 0;
    for (auto& layout : descriptor.vertexLayouts)
        count = std::max(count, layout.bufferIndex + 1);
    return count;
}

static bool isValidDescriptor(const WebModel::MeshDescriptor& descriptor, bool deformed)
{
    if (descriptor.vertexBufferCount <= 0 || descriptor.vertexCapacity < 0 || descriptor.indexCapacity < 0)
        return false;

    for (auto& layout : descriptor.vertexLayouts) {
        if (layout.bufferIndex < 0 || layout.bufferIndex >= descriptor.vertexBufferCount)
            return false;
        if (layout.bufferOffset < 0 || layout.bufferStride < 0 || layout.stepRate < 0)
            return false;
    }

    for (auto& attribute : descriptor.vertexAttributes) {
        if (attribute.layoutIndex < 0 || static_cast<uint64_t>(attribute.layoutIndex) >= descriptor.vertexLayouts.size())
            return false;
        if (attribute.offset < 0)
            return false;

        auto& layout = descriptor.vertexLayouts[attribute.layoutIndex];

        auto attributeByteSize = vertexFormatByteSize(attribute.format);
        if (deformed)
            attributeByteSize = std::max(attributeByteSize, deformedAttributeByteSize(attribute.semantic));

        if (descriptor.vertexCapacity && attributeByteSize > layout.bufferStride)
            return false;

        CheckedInt64 vertexBufferByteCount = CheckedInt64(layout.bufferOffset) + attribute.offset + CheckedInt64(descriptor.vertexCapacity) * layout.bufferStride;
        if (vertexBufferByteCount.hasOverflowed())
            return false;
    }

    return true;
}

static bool isValidGeometry(const WebModel::MeshDescriptor& descriptor, const WebModel::UpdateMeshDescriptor& update)
{
    if (!isValidDescriptor(descriptor, !!update.deformationData))
        return false;

    // One vertex buffer is created per declared buffer; data for a buffer that was never declared
    // has nowhere to go.
    if (update.vertexData.size() > static_cast<uint64_t>(declaredBufferCount(descriptor)))
        return false;

    if (!descriptor.vertexCapacity) {
        for (auto& part : update.parts) {
            if (part.indexCount)
                return false;
        }
    }

    CheckedInt64 indexBufferByteCount = CheckedInt64(descriptor.indexCapacity) * indexByteSize(descriptor.indexType);
    if (indexBufferByteCount.hasOverflowed())
        return false;

    for (auto& part : update.parts) {
        CheckedInt64 partByteCount = CheckedInt64(part.indexOffset) + CheckedInt64(part.indexCount) * indexByteSize(descriptor.indexType);
        if (partByteCount.hasOverflowed() || partByteCount.value() > indexBufferByteCount.value())
            return false;
    }

    return true;
}

void RemoteMesh::update(Vector<WebModel::UpdateMeshDescriptor>&& descriptor, CompletionHandler<void(bool)>&& completionHandler)
{
    // WKBridgeDataUpdateTypeInitial, the enum is Objective-C only, see ModelTypes.h.
    constexpr uint8_t initialUpdateType = 0;

    HashMap<int64_t, const WebModel::MeshDescriptor*, IntHash<int64_t>, WTF::SignedWithZeroKeyHashTraits<int64_t>> newDescriptors;
    for (auto& update : descriptor) {
        auto identifier = update.identifier.hashValue;
        MESSAGE_CHECK_COMPLETION(identifier != std::numeric_limits<int64_t>::min() && identifier != std::numeric_limits<int64_t>::max(), completionHandler(false));

        const WebModel::MeshDescriptor* resourceDescriptor = newDescriptors.get(identifier);
        if (!resourceDescriptor) {
            auto existing = m_meshDescriptors.find(identifier);
            if (existing != m_meshDescriptors.end())
                resourceDescriptor = &existing->value;
        }

        bool declaresDescriptor = !!update.descriptor.vertexBufferCount;
        if (!resourceDescriptor) {
            MESSAGE_CHECK_COMPLETION(declaresDescriptor, completionHandler(false));
            MESSAGE_CHECK_COMPLETION(isValidGeometry(update.descriptor, update), completionHandler(false));
            newDescriptors.set(identifier, &update.descriptor);
            continue;
        }

        MESSAGE_CHECK_COMPLETION(update.updateType != initialUpdateType || (declaresDescriptor && isSameDescriptor(*resourceDescriptor, update.descriptor)), completionHandler(false));
        MESSAGE_CHECK_COMPLETION(!declaresDescriptor || isValidDescriptor(update.descriptor, !!update.deformationData), completionHandler(false));
        MESSAGE_CHECK_COMPLETION(isValidGeometry(*resourceDescriptor, update), completionHandler(false));
    }

    for (auto& newDescriptor : newDescriptors)
        m_meshDescriptors.add(newDescriptor.key, *newDescriptor.value);

    m_backing->update(WTF::move(descriptor));
    completionHandler(true);
>>>>>>> 412ea8abf06b (Unvalidated MeshDescriptor bufferStride in RemoteMesh::Update drives out-of-bounds AGX vertex-fetch read in the GPUP)
}

void RemoteMesh::render(uint32_t textureIndex, CompletionHandler<void(bool)>&& completionHandler)
{
    Ref workQueue = m_gpu->workQueue();
    m_backing->render(textureIndex, [workQueue = WTF::move(workQueue), completionHandler = WTF::move(completionHandler)] (bool result) mutable {
        protect(workQueue)->dispatch([result, completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler(result);
        });
    });
}

void RemoteMesh::updateTexture(Vector<WebModel::UpdateTextureDescriptor>&& descriptor, CompletionHandler<void(bool)>&& completionHandler)
{
    m_backing->updateTexture(WTF::move(descriptor));
    completionHandler(true);
}

void RemoteMesh::updateMaterial(Vector<WebModel::UpdateMaterialDescriptor>&& descriptor, CompletionHandler<void(bool)>&& completionHandler)
{
    m_backing->updateMaterial(WTF::move(descriptor));
    completionHandler(true);
}

void RemoteMesh::updateTransform(const WebModel::Float4x4& transform)
{
    m_backing->setEntityTransform(transform);
}

#if ENABLE(SPATIAL_PORTAL)
void RemoteMesh::updatePortalChildTransform(WebCore::NodeIdentifier nodeIdentifier, const WebModel::Float4x4& transform)
{
    m_backing->setPortalChildTransform(nodeIdentifier, transform);
}
#endif

void RemoteMesh::setFOV(float fovY)
{
    m_backing->setFOV(fovY);
}

void RemoteMesh::play(bool playing)
{
    m_backing->play(playing);
}

void RemoteMesh::setEnvironmentMap(WebModel::UpdateTextureDescriptor&& imageAsset)
{
    m_backing->setEnvironmentMap(WTF::move(imageAsset));
}

#if ENABLE(SPATIAL_PORTAL)
void RemoteMesh::setDefaultEnvironmentMap()
{
    m_backing->setDefaultEnvironmentMap();
}

void RemoteMesh::removeEnvironmentMap()
{
    m_backing->removeEnvironmentMap();
}
#endif

void RemoteMesh::updateContentsHeadroom(float headroom)
{
#if HAVE(SUPPORT_HDR_DISPLAY)
    m_backing->updateContentsHeadroom(headroom);
#else
    UNUSED_PARAM(headroom);
#endif
}

void RemoteMesh::updateRenderBuffers(unsigned width, unsigned height, CompletionHandler<void(Vector<MachSendRight>&&)>&& completionHandler)
{
    auto gpuProcessConnection = m_gpuConnectionToWebProcess.get();
    if (!gpuProcessConnection) {
        completionHandler({ });
        return;
    }

    auto renderBuffers = RemoteGPU::createRenderBuffers(width, height, gpuProcessConnection->webProcessIdentity(), m_standardDynamicRange);
    WebModel::ResizeMeshDescriptor descriptor { width, height, WTF::move(renderBuffers) };
    m_backing->updateRenderBuffers(WTF::move(descriptor));
    completionHandler(m_backing->ioSurfaceHandles());
}

void RemoteMesh::processRemovals(Vector<WebModel::TypedResourceId>&& meshRemovals, Vector<WebModel::TypedResourceId>&& materialRemovals, Vector<WebModel::TypedResourceId>&& textureRemovals, CompletionHandler<void(bool)>&& completionHandler)
{
    auto removedIdentifiers = meshRemovals.map([](auto& removal) {
        return removal.hashValue;
    });
    m_backing->processRemovals(WTF::move(meshRemovals), WTF::move(materialRemovals), WTF::move(textureRemovals), [protectedThis = Ref { *this }, removedIdentifiers = WTF::move(removedIdentifiers), completionHandler = WTF::move(completionHandler)] (bool success) mutable {
        if (success) {
            for (auto identifier : removedIdentifiers)
                protectedThis->m_meshDescriptors.remove(identifier);
        }
        completionHandler(success);
    });
}

void RemoteMesh::paintCurrentFrameToImageBuffer(WebCore::RenderingResourceIdentifier imageBufferIdentifier, uint32_t bufferIndex, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr nativeImage { m_backing->getCurrentFrameAsNativeImage(bufferIndex) })
        m_gpu->paintNativeImageToImageBuffer(*nativeImage, imageBufferIdentifier);
    completionHandler();
}

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_COMPLETION

#endif
