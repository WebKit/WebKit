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

#include "config.h"
#include "GPUConnectionToModelProcess.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MODEL_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "Logging.h"
#include "RemoteRenderingBackend.h"
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, m_connection.get(), completion)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUConnectionToModelProcess);

Ref<GPUConnectionToModelProcess> GPUConnectionToModelProcess::create(GPUProcess& gpuProcess, WebCore::ProcessIdentifier webProcessIdentifier, IPC::Connection::Handle&& connectionHandle)
{
    return adoptRef(*new GPUConnectionToModelProcess(gpuProcess, webProcessIdentifier, WTF::move(connectionHandle)));
}

GPUConnectionToModelProcess::GPUConnectionToModelProcess(GPUProcess& gpuProcess, WebCore::ProcessIdentifier webProcessIdentifier, IPC::Connection::Handle&& connectionHandle)
    : m_connection(IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTF::move(connectionHandle) }))
    , m_gpuProcess(gpuProcess)
    , m_webProcessIdentifier(webProcessIdentifier)
{
    RELEASE_ASSERT(RunLoop::isMain());
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open(*this);
}

GPUConnectionToModelProcess::~GPUConnectionToModelProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());
    m_connection->invalidate();
}

void GPUConnectionToModelProcess::didClose(IPC::Connection&)
{
    Ref protectedThis { *this };
    m_gpuProcess->removeGPUConnectionToModelProcess(*this);
}

void GPUConnectionToModelProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT(IPC, "Received an invalid message '%" PUBLIC_LOG_STRING "' from the Model process.", description(messageName).characters());
    Ref protectedThis { *this };
    m_connection->invalidate();
    m_gpuProcess->removeGPUConnectionToModelProcess(*this);
}

#if ENABLE(SPATIAL_PORTAL) && HAVE(IOSURFACE)
void GPUConnectionToModelProcess::frontBufferForExternalRead(RemoteRenderingBackendIdentifier renderingBackendIdentifier, ImageBufferSetIdentifier bufferSetIdentifier, WTF::UUID externalReadToken, Vector<uint64_t>&& alreadyMappedBuffers, CompletionHandler<void(std::optional<MachSendRight>&&, uint64_t, bool)>&& completionHandler)
{
    RefPtr webProcessConnection = m_gpuProcess->webProcessConnection(m_webProcessIdentifier);
    if (!webProcessConnection)
        return completionHandler(std::nullopt, 0, false);

    const auto& preferences = webProcessConnection->sharedPreferencesForWebProcessValue();
    MESSAGE_CHECK_COMPLETION(preferences.modelElementEnabled && preferences.modelProcessEnabled && preferences.spatialPortalEnabled, completionHandler(std::nullopt, 0, false));

    RefPtr renderingBackend = webProcessConnection->remoteRenderingBackend(renderingBackendIdentifier);
    if (!renderingBackend)
        return completionHandler(std::nullopt, 0, false);

    renderingBackend->frontBufferForExternalRead(bufferSetIdentifier, externalReadToken, WTF::move(alreadyMappedBuffers), WTF::move(completionHandler));
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK_COMPLETION

#endif // ENABLE(GPU_PROCESS) && ENABLE(MODEL_PROCESS)
