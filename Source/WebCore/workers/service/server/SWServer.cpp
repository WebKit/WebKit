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
#include "SWServer.h"

#if ENABLE(SERVICE_WORKER)

#include "ExceptionCode.h"
#include "ExceptionData.h"
#include "Logging.h"
#include "ServiceWorkerJobData.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

SWServer::Connection::Connection(SWServer& server)
    : m_server(server)
{
    m_server.registerConnection(*this);
}

SWServer::Connection::~Connection()
{
    m_server.unregisterConnection(*this);
}


SWServer::~SWServer()
{
    RELEASE_ASSERT(m_connections.isEmpty());
}

void SWServer::Connection::scheduleJobInServer(const ServiceWorkerJobData& jobData)
{
    LOG(ServiceWorker, "Scheduling ServiceWorker job %" PRIu64 " in server", jobData.identifier);
    m_server.scheduleJob(*this, jobData);
}

void SWServer::scheduleJob(Connection& connection, const ServiceWorkerJobData& jobData)
{
    // FIXME: For now, all scheduled jobs immediately reject.
    connection.rejectJobInClient(jobData.identifier, ExceptionData { UnknownError, ASCIILiteral("serviceWorker job scheduling is not yet implemented") });
}

void SWServer::registerConnection(Connection& connection)
{
    auto result = m_connections.add(&connection);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void SWServer::unregisterConnection(Connection& connection)
{
    ASSERT(m_connections.contains(&connection));
    m_connections.remove(&connection);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
