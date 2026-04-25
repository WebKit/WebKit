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
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_OPTIONAL_CONNECTION_BASE(assertion, connection())

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMesh);

RemoteMesh::RemoteMesh(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, WebKit::Mesh& mesh, ModelObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebModelIdentifier identifier)
    : m_backing(mesh)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_identifier(identifier)
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_gpu(gpu)
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

void RemoteMesh::update(Vector<WebModel::UpdateMeshDescriptor>&& descriptor, CompletionHandler<void(bool)>&& completionHandler)
{
    m_backing->update(WTF::move(descriptor));
    completionHandler(true);
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

void RemoteMesh::setFOV(float fovY)
{
    m_backing->setFOV(fovY);
}

void RemoteMesh::setBackgroundColor(const WebModel::Float3& color)
{
    m_backing->setBackgroundColor(color);
}

void RemoteMesh::play(bool playing)
{
    m_backing->play(playing);
}

void RemoteMesh::setEnvironmentMap(const WebModel::UpdateTextureDescriptor& imageAsset)
{
    m_backing->setEnvironmentMap(imageAsset);
}

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

    auto renderBuffers = RemoteGPU::createRenderBuffers(width, height, gpuProcessConnection->webProcessIdentity());
    WebModel::ResizeMeshDescriptor descriptor { width, height, WTF::move(renderBuffers) };
    m_backing->updateRenderBuffers(WTF::move(descriptor));
    completionHandler(m_backing->ioSurfaceHandles());
}

void RemoteMesh::processRemovals(Vector<WebModel::TypedResourceId>&& meshRemovals, Vector<WebModel::TypedResourceId>&& materialRemovals, Vector<WebModel::TypedResourceId>&& textureRemovals, CompletionHandler<void(bool)>&& completionHandler)
{
    m_backing->processRemovals(WTF::move(meshRemovals), WTF::move(materialRemovals), WTF::move(textureRemovals), WTF::move(completionHandler));
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif
