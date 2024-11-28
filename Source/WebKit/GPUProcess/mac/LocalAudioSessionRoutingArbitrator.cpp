/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "LocalAudioSessionRoutingArbitrator.h"

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "GPUProcessConnectionMessages.h"
#include "Logging.h"
#include <wtf/LoggerHelper.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(LocalAudioSessionRoutingArbitrator);

std::unique_ptr<LocalAudioSessionRoutingArbitrator> LocalAudioSessionRoutingArbitrator::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
{
    return makeUnique<LocalAudioSessionRoutingArbitrator>(gpuConnectionToWebProcess);
}

LocalAudioSessionRoutingArbitrator::LocalAudioSessionRoutingArbitrator(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_connectionToWebProcess(gpuConnectionToWebProcess)
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
{
}

LocalAudioSessionRoutingArbitrator::~LocalAudioSessionRoutingArbitrator() = default;

void LocalAudioSessionRoutingArbitrator::processDidTerminate()
{
    leaveRoutingAbritration();
}

void LocalAudioSessionRoutingArbitrator::beginRoutingArbitrationWithCategory(AudioSession::CategoryType category, CompletionHandler<void(RoutingArbitrationError, DefaultRouteChanged)>&& callback)
{
    ALWAYS_LOG(LOGIDENTIFIER, category);
    RefPtr connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->protectedConnection()->sendWithAsyncReply(Messages::GPUProcessConnection::BeginRoutingArbitrationWithCategory(category), WTFMove(callback), 0);
}

void LocalAudioSessionRoutingArbitrator::leaveRoutingAbritration()
{
    RefPtr connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    connection->protectedConnection()->send(Messages::GPUProcessConnection::EndRoutingArbitration(), 0);
}

Logger& LocalAudioSessionRoutingArbitrator::logger()
{
    return m_connectionToWebProcess.get()->logger();
};

WTFLogChannel& LocalAudioSessionRoutingArbitrator::logChannel() const
{
    return WebKit2LogMedia;
}

bool LocalAudioSessionRoutingArbitrator::canLog() const
{
    if (RefPtr connection = m_connectionToWebProcess.get())
        return connection->isAlwaysOnLoggingAllowed();
    return false;
}

}

#endif
