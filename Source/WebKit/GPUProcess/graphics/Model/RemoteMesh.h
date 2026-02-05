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

#pragma once

#if ENABLE(GPU_PROCESS_MODEL)

#include "RemoteGPU.h"
#include "StreamMessageReceiver.h"
#include "WebModelIdentifier.h"
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class Mesh;
}
namespace WebModel {
struct Float4x4;
struct MaterialDescriptor;
struct MeshDescriptor;
struct TextureDescriptor;
struct UpdateMaterialDescriptor;
struct UpdateMeshDescriptor;
struct UpdateTextureDescriptor;
}

namespace IPC {
class Connection;
class StreamServerConnection;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class ObjectHeap;

class RemoteMesh final : public IPC::StreamMessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMesh);
public:
    static Ref<RemoteMesh> create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, WebCore::Mesh& mesh, ModelObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebModelIdentifier identifier)
    {
        return adoptRef(*new RemoteMesh(gpuConnectionToWebProcess, gpu, mesh, objectHeap, WTF::move(streamConnection), identifier));
    }

    virtual ~RemoteMesh();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_gpu->sharedPreferencesForWebProcess(); }

    void stopListeningForIPC();

private:
    friend class ModelObjectHeap;

    RemoteMesh(GPUConnectionToWebProcess&, RemoteGPU&, WebCore::Mesh&, ModelObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebModelIdentifier);

    RemoteMesh(const RemoteMesh&) = delete;
    RemoteMesh(RemoteMesh&&) = delete;
    RemoteMesh& operator=(const RemoteMesh&) = delete;
    RemoteMesh& operator=(RemoteMesh&&) = delete;

    WebCore::Mesh& backing() { return m_backing; }

    RefPtr<IPC::Connection> connection() const;

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void destruct();

    void setLabel(String&&);
    void update(const WebModel::UpdateMeshDescriptor&);
    void updateTexture(const WebModel::UpdateTextureDescriptor&);
    void updateMaterial(const WebModel::UpdateMaterialDescriptor&);
    void updateTransform(const WebModel::Float4x4& transform);
    void setCameraDistance(float);
    void play(bool);
    void setEnvironmentMap(const WebModel::ImageAsset&);

    void render();

    const Ref<WebCore::Mesh> m_backing;
    WeakRef<ModelObjectHeap> m_objectHeap;
    const Ref<IPC::StreamServerConnection> m_streamConnection;
    const WebModelIdentifier m_identifier;
    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    WeakRef<RemoteGPU> m_gpu;
};

} // namespace WebKit

#endif
