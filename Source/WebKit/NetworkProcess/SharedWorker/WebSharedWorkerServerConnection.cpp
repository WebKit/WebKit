/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebSharedWorkerServerConnection.h"

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSession.h"
#include "WebCoreArgumentCoders.h"
#include "WebSharedWorker.h"
#include "WebSharedWorkerObjectConnectionMessages.h"
#include "WebSharedWorkerServer.h"
#include <WebCore/WorkerFetchResult.h>

namespace WebKit {

#define CONNECTION_MESSAGE_CHECK(assertion) CONNECTION_MESSAGE_CHECK_COMPLETION(assertion, (void)0)
#define CONNECTION_MESSAGE_CHECK_COMPLETION(assertion, completion) do { \
    ASSERT(assertion); \
    if (UNLIKELY(!(assertion))) { \
        m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::TerminateWebProcess(m_webProcessIdentifier), 0); \
        { completion; } \
        return; \
    } \
} while (0)

#define CONNECTION_RELEASE_LOG(fmt, ...) RELEASE_LOG(SharedWorker, "%p - [webProcessIdentifier=%" PRIu64 "] WebSharedWorkerServerConnection::" fmt, this, m_webProcessIdentifier.toUInt64(), ##__VA_ARGS__)
#define CONNECTION_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(SharedWorker, "%p - [webProcessIdentifier=%" PRIu64 "] WebSharedWorkerServerConnection::" fmt, this, m_webProcessIdentifier.toUInt64(), ##__VA_ARGS__)

WebSharedWorkerServerConnection::WebSharedWorkerServerConnection(NetworkProcess& networkProcess, WebSharedWorkerServer& server, IPC::Connection& connection, WebCore::ProcessIdentifier webProcessIdentifier)
    : m_contentConnection(connection)
    , m_networkProcess(networkProcess)
    , m_server(server)
    , m_webProcessIdentifier(webProcessIdentifier)
{
    CONNECTION_RELEASE_LOG("WebSharedWorkerServerConnection:");
}

WebSharedWorkerServerConnection::~WebSharedWorkerServerConnection()
{
    CONNECTION_RELEASE_LOG("~WebSharedWorkerServerConnection:");
}

IPC::Connection* WebSharedWorkerServerConnection::messageSenderConnection() const
{
    return m_contentConnection.ptr();
}

PAL::SessionID WebSharedWorkerServerConnection::sessionID()
{
    return server().sessionID();
}

NetworkSession* WebSharedWorkerServerConnection::session()
{
    return m_networkProcess->networkSession(sessionID());
}

void WebSharedWorkerServerConnection::requestSharedWorker(WebCore::SharedWorkerKey&& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, WebCore::TransferredMessagePort&& port, WebCore::WorkerOptions&& workerOptions)
{
    CONNECTION_MESSAGE_CHECK(sharedWorkerObjectIdentifier.processIdentifier() == m_webProcessIdentifier);
    CONNECTION_MESSAGE_CHECK(sharedWorkerKey.name == workerOptions.name);
    CONNECTION_RELEASE_LOG("requestSharedWorker: sharedWorkerObjectIdentifier=%{public}s", sharedWorkerObjectIdentifier.toString().utf8().data());
    if (auto* session = this->session())
        session->ensureSharedWorkerServer().requestSharedWorker(WTFMove(sharedWorkerKey), sharedWorkerObjectIdentifier, WTFMove(port), WTFMove(workerOptions));
}

void WebSharedWorkerServerConnection::sharedWorkerObjectIsGoingAway(WebCore::SharedWorkerKey&& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    CONNECTION_MESSAGE_CHECK(sharedWorkerObjectIdentifier.processIdentifier() == m_webProcessIdentifier);
    CONNECTION_RELEASE_LOG("sharedWorkerObjectIsGoingAway: sharedWorkerObjectIdentifier=%{public}s", sharedWorkerObjectIdentifier.toString().utf8().data());
    if (auto* session = this->session())
        session->ensureSharedWorkerServer().sharedWorkerObjectIsGoingAway(sharedWorkerKey, sharedWorkerObjectIdentifier);
}

void WebSharedWorkerServerConnection::fetchScriptInClient(const WebSharedWorker& sharedWorker, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, CompletionHandler<void(WebCore::WorkerFetchResult&&)>&& completionHandler)
{
    CONNECTION_RELEASE_LOG("fetchScriptInClient: sharedWorkerObjectIdentifier=%{public}s", sharedWorkerObjectIdentifier.toString().utf8().data());
    sendWithAsyncReply(Messages::WebSharedWorkerObjectConnection::FetchScriptInClient { sharedWorker.url(), sharedWorkerObjectIdentifier, sharedWorker.workerOptions() }, WTFMove(completionHandler));
}

void WebSharedWorkerServerConnection::notifyWorkerObjectOfLoadCompletion(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, const WebCore::ResourceError& error)
{
    CONNECTION_RELEASE_LOG("notifyWorkerObjectOfLoadCompletion: sharedWorkerObjectIdentifier=%{public}s, success=%d", sharedWorkerObjectIdentifier.toString().utf8().data(), error.isNull());
    send(Messages::WebSharedWorkerObjectConnection::NotifyWorkerObjectOfLoadCompletion { sharedWorkerObjectIdentifier, error });
}

void WebSharedWorkerServerConnection::postExceptionToWorkerObject(WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    CONNECTION_RELEASE_LOG_ERROR("postExceptionToWorkerObject: sharedWorkerObjectIdentifier=%{public}s", sharedWorkerObjectIdentifier.toString().utf8().data());
    send(Messages::WebSharedWorkerObjectConnection::PostExceptionToWorkerObject { sharedWorkerObjectIdentifier, errorMessage, lineNumber, columnNumber, sourceURL });
}

#undef CONNECTION_RELEASE_LOG
#undef CONNECTION_RELEASE_LOG_ERROR
#undef CONNECTION_MESSAGE_CHECK
#undef CONNECTION_MESSAGE_CHECK_COMPLETION

} // namespace WebKit
