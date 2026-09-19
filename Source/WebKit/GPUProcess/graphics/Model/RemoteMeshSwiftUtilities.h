/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(GPU_PROCESS_MODEL)

#include "GPUConnectionToWebProcess.h"
#include "Mesh.h"
#include "ModelObjectHeap.h"
#include "ModelTypes.h"
#include "RemoteGPU.h"
#include "RemoteMeshMessages.h"
#include "SharedPreferencesForWebProcess.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamServerConnection.h"
#include "WebModelIdentifier.h"
#include <WebCore/NativeImage.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/MachSendRight.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

// The type-system workarounds RemoteMesh.swift needs. Everything here is an adapter for
// something Swift cannot currently express; none of it makes decisions of its own.
namespace WebKit {

// Swift cannot spell either of these; declare the aliases the Swift receiver names instead.
using WeakPtrModelObjectHeap = WeakPtr<ModelObjectHeap>;
using OptionalSharedPreferencesForWebProcess = std::optional<SharedPreferencesForWebProcess>;

// Registration takes the forwarder as a StreamMessageReceiver, and Swift will not upcast an
// imported C++ reference type to a base that is not itself a foreign reference type.
inline void startReceivingMeshMessages(IPC::StreamServerConnection& streamConnection, Ref<RemoteMeshMessageForwarder> forwarder, WebModelIdentifier identifier)
{
    streamConnection.startReceivingMessages(forwarder.get(), Messages::RemoteMesh::messageReceiverName(), identifier.toUInt64());
}

inline void stopReceivingMeshMessages(IPC::StreamServerConnection& streamConnection, WebModelIdentifier identifier)
{
    streamConnection.stopReceivingMessages(Messages::RemoteMesh::messageReceiverName(), identifier.toUInt64());
}

// Mesh::render() takes a WTF::Function, which Swift has no way to construct - only
// WTF::CompletionHandler has the constructor that bridges a Swift closure - and the result has
// to hop back onto the GPU work queue, which needs a second one.
inline void renderMesh(Mesh& mesh, RemoteGPU& gpu, uint32_t textureIndex, CompletionHandlers::RemoteMesh::RenderCompletionHandler& completionHandler)
{
    Ref workQueue = gpu.workQueue();
    mesh.render(textureIndex, [workQueue = WTF::move(workQueue), completionHandler = protect(completionHandler)] (bool result) mutable {
        protect(workQueue)->dispatch([result, completionHandler = WTF::move(completionHandler)] mutable {
            (*completionHandler.get())(result);
        });
    });
}

// Allocating the replacement buffers yields a Vector<UniqueRef<WebCore::IOSurface>>, which is
// not representable in Swift.
inline Vector<MachSendRight> resizeMeshRenderBuffers(Mesh& mesh, RemoteGPU& gpu, uint32_t width, uint32_t height, bool standardDynamicRange)
{
    RefPtr gpuProcessConnection = gpu.gpuConnectionToWebProcess();
    if (!gpuProcessConnection)
        return { };

    auto renderBuffers = RemoteGPU::createRenderBuffers(width, height, gpuProcessConnection->webProcessIdentity(), standardDynamicRange);
    WebModel::ResizeMeshDescriptor descriptor { width, height, WTF::move(renderBuffers) };
    mesh.updateRenderBuffers(WTF::move(descriptor));
    return mesh.ioSurfaceHandles();
}

// WebCore::NativeImage is not a foreign reference type, so Swift cannot hold the RefPtr that
// Mesh::getCurrentFrameAsNativeImage() returns.
inline void paintMeshCurrentFrameToImageBuffer(Mesh& mesh, RemoteGPU& gpu, WebCore::RenderingResourceIdentifier imageBufferIdentifier, uint32_t bufferIndex)
{
    if (RefPtr nativeImage { mesh.getCurrentFrameAsNativeImage(bufferIndex) })
        gpu.paintNativeImageToImageBuffer(*nativeImage, imageBufferIdentifier);
}

// Mesh::processRemovals() takes its completion handler by rvalue reference, but Swift can only
// hold one inside a RefCountable. The removal lists are taken by value because Swift has no
// spelling for a call that moves into more than one rvalue reference parameter.
inline void meshProcessRemovals(Mesh& mesh, WebModel::VectorTypedResourceId meshRemovals, WebModel::VectorTypedResourceId materialRemovals, WebModel::VectorTypedResourceId textureRemovals, CompletionHandlers::RemoteMesh::ProcessRemovalsCompletionHandler& completionHandler)
{
    mesh.processRemovals(WTF::move(meshRemovals), WTF::move(materialRemovals), WTF::move(textureRemovals), WTF::move(*completionHandler));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS_MODEL)
