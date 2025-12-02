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

#include "DDModelIdentifier.h"
#include "RemoteGPU.h"
#include "StreamMessageReceiver.h"
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore::DDModel {
class DDMesh;
struct DDMaterialDescriptor;
struct DDMeshDescriptor;
struct DDTextureDescriptor;
struct DDUpdateMaterialDescriptor;
struct DDUpdateMeshDescriptor;
struct DDUpdateTextureDescriptor;
}

namespace IPC {
class Connection;
class StreamServerConnection;
}

namespace WebKit {

class GPUConnectionToWebProcess;

namespace DDModel {
class ObjectHeap;
}

class RemoteDDMesh final : public IPC::StreamMessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteDDMesh);
public:
    static Ref<RemoteDDMesh> create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, WebCore::DDModel::DDMesh& mesh, DDModel::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, DDModelIdentifier identifier)
    {
        return adoptRef(*new RemoteDDMesh(gpuConnectionToWebProcess, gpu, mesh, objectHeap, WTFMove(streamConnection), identifier));
    }

    virtual ~RemoteDDMesh();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_gpu->sharedPreferencesForWebProcess(); }

    void stopListeningForIPC();

private:
    friend class DDModel::ObjectHeap;

    RemoteDDMesh(GPUConnectionToWebProcess&, RemoteGPU&, WebCore::DDModel::DDMesh&, DDModel::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, DDModelIdentifier);

    RemoteDDMesh(const RemoteDDMesh&) = delete;
    RemoteDDMesh(RemoteDDMesh&&) = delete;
    RemoteDDMesh& operator=(const RemoteDDMesh&) = delete;
    RemoteDDMesh& operator=(RemoteDDMesh&&) = delete;

    WebCore::DDModel::DDMesh& backing() { return m_backing; }

    RefPtr<IPC::Connection> connection() const;

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void destruct();

    void setLabel(String&&);
    void update(const WebCore::DDModel::DDUpdateMeshDescriptor&);
    void updateTexture(const WebCore::DDModel::DDUpdateTextureDescriptor&);
    void updateMaterial(const WebCore::DDModel::DDUpdateMaterialDescriptor&);
    void updateTransform(const WebCore::DDModel::DDFloat4x4& transform);
    void setCameraDistance(float);
    void play(bool);

    void render();

    const Ref<WebCore::DDModel::DDMesh> m_backing;
    WeakRef<DDModel::ObjectHeap> m_objectHeap;
    const Ref<IPC::StreamServerConnection> m_streamConnection;
    DDModelIdentifier m_identifier;
    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    WeakRef<RemoteGPU> m_gpu;
};

} // namespace WebKit

#endif
