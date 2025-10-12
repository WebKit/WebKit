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
#include "RemoteDDMesh.h"

#if ENABLE(GPUP_MODEL)

#include "DDMeshDescriptor.h"
#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "ModelObjectHeap.h"
#include "RemoteDDMeshMessages.h"
#include "StreamServerConnection.h"
#include <WebCore/DDMesh.h>
#include <WebCore/DDMeshDescriptor.h>
#include <WebCore/DDUpdateMeshDescriptor.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_OPTIONAL_CONNECTION_BASE(assertion, connection())

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteDDMesh);

RemoteDDMesh::RemoteDDMesh(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGPU& gpu, WebCore::DDModel::DDMesh& mesh, DDModel::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, DDModelIdentifier identifier)
    : m_backing(mesh)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_gpu(gpu)
{
    Ref { m_streamConnection }->startReceivingMessages(*this, Messages::RemoteDDMesh::messageReceiverName(), m_identifier.toUInt64());
}

RemoteDDMesh::~RemoteDDMesh() = default;

RefPtr<IPC::Connection> RemoteDDMesh::connection() const
{
    RefPtr connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return nullptr;
    return &connection->connection();
}

void RemoteDDMesh::stopListeningForIPC()
{
    Ref { m_streamConnection }->stopReceivingMessages(Messages::RemoteDDMesh::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteDDMesh::destruct()
{
    Ref { m_objectHeap.get() }->removeObject(m_identifier);
}

void RemoteDDMesh::setLabel(String&& label)
{
    protectedBacking()->setLabel(WTFMove(label));
}

Ref<WebCore::DDModel::DDMesh> RemoteDDMesh::protectedBacking()
{
    return m_backing;
}

void RemoteDDMesh::addMesh(const DDModel::DDMeshDescriptor& descriptor)
{
#if PLATFORM(COCOA)
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    protectedBacking()->addMesh(*convertedDescriptor);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMesh::update(const DDModel::DDUpdateMeshDescriptor& descriptor)
{
#if PLATFORM(COCOA)
    auto convertedDescriptor = m_objectHeap->convertFromBacking(descriptor);
    protectedBacking()->update(*convertedDescriptor);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMesh::render()
{
    protectedBacking()->render();
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif
