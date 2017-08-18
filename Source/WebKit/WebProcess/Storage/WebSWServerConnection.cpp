/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebSWServerConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "Logging.h"
#include "StorageToWebProcessConnectionMessages.h"
#include "WebProcess.h"
#include "WebSWServerConnectionMessages.h"
#include "WebToStorageProcessConnection.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ServiceWorkerJobData.h>
#include <wtf/MainThread.h>

using namespace PAL;
using namespace WebCore;

namespace WebKit {

WebSWServerConnection::WebSWServerConnection(const SessionID& sessionID)
    : m_sessionID(sessionID)
    , m_connection(WebProcess::singleton().webToStorageProcessConnection()->connection())
{
    bool result = sendSync(Messages::StorageToWebProcessConnection::EstablishSWServerConnection(sessionID), Messages::StorageToWebProcessConnection::EstablishSWServerConnection::Reply(m_identifier));

    ASSERT_UNUSED(result, result);
}

WebSWServerConnection::WebSWServerConnection(IPC::Connection& connection, uint64_t connectionIdentifier, const SessionID& sessionID)
    : m_sessionID(sessionID)
    , m_identifier(connectionIdentifier)
    , m_connection(connection)
{
}

WebSWServerConnection::~WebSWServerConnection()
{
}

void WebSWServerConnection::disconnectedFromWebProcess()
{
    notImplemented();
}

void WebSWServerConnection::scheduleJob(const ServiceWorkerJobData& jobData)
{
    LOG(ServiceWorker, "Scheduling ServiceWorker job %" PRIu64 " in storage process", jobData.identifier);
    send(Messages::WebSWServerConnection::ScheduleStorageJob(jobData));
}

void WebSWServerConnection::scheduleStorageJob(const ServiceWorkerJobData& jobData)
{
    ASSERT(isMainThread());
    LOG(ServiceWorker, "Received ServiceWorker job %" PRIu64 " in storage process", jobData.identifier);

    // FIXME: For now, all scheduled jobs immediately reject.
    send(Messages::WebSWServerConnection::JobRejected(jobData.identifier, ExceptionData { UnknownError, ASCIILiteral("serviceWorker job scheduling is not yet implemented") }));
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
