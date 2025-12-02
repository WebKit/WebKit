/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSocketProvider.h"

#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include "WebSocketChannelManager.h"
#include "WebTransportSession.h"
#include <WebCore/DocumentInlines.h>
#include <WebCore/WebTransportSessionClient.h>
#include <WebCore/WorkerGlobalScope.h>
#include <WebCore/WorkerWebTransportSession.h>

#if USE(LIBRICE)
#include "RiceBackendProxy.h"
#endif

namespace WebKit {
using namespace WebCore;

RefPtr<ThreadableWebSocketChannel> WebSocketProvider::createWebSocketChannel(Document& document, WebSocketChannelClient& client)
{
    return WebKit::WebSocketChannel::create(m_webPageProxyID, document, client);
}

WebSocketProvider::~WebSocketProvider() = default;

WebSocketProvider::WebSocketProvider(WebPageProxyIdentifier webPageProxyID)
    : m_webPageProxyID(webPageProxyID)
    , m_networkProcessConnection(WebProcess::singleton().ensureNetworkProcessConnection().connection()) { }

std::pair<RefPtr<WebCore::WebTransportSession>, Ref<WebTransportSessionPromise>> WebSocketProvider::initializeWebTransportSession(ScriptExecutionContext& context, WebTransportSessionClient& client, const URL& url, const WebCore::WebTransportOptions& options)
{
    if (RefPtr scope = dynamicDowncast<WorkerGlobalScope>(context)) {
        ASSERT(!RunLoop::isMain());
        Ref workerSession = WorkerWebTransportSession::create(context.identifier(), client);

        auto getConnection = [protectedThis = Ref { *this }] {
            Locker locker { protectedThis->m_networkProcessConnectionLock };
            return protectedThis->m_networkProcessConnection.copyRef();
        };
        Ref connection = getConnection();
        if (!connection->isValid()) {
            WorkQueue::mainSingleton().dispatchSync([protectedThis = Ref { *this }] {
                ASSERT(RunLoop::isMain());
                Locker locker { protectedThis->m_networkProcessConnectionLock };
                protectedThis->m_networkProcessConnection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
            });
            connection = getConnection();
        }

        auto [session, promise] = WebKit::WebTransportSession::initialize(WTFMove(connection), workerSession, url, options, m_webPageProxyID, scope->clientOrigin());
        workerSession->attachSession(session);
        return { WTFMove(workerSession), WTFMove(promise) };
    }

    Ref document = downcast<Document>(context);
    ASSERT(RunLoop::isMain());
    auto [session, promise] = WebKit::WebTransportSession::initialize(WebProcess::singleton().ensureNetworkProcessConnection().connection(), client, url, options, m_webPageProxyID, document->clientOrigin());
    return { WTFMove(session), WTFMove(promise) };
}

#if USE(LIBRICE)
RefPtr<WebCore::RiceBackend> WebSocketProvider::createRiceBackend(WebCore::RiceBackendClient& client)
{
    return WebKit::RiceBackendProxy::create(m_webPageProxyID, client);
}
#endif

} // namespace WebKit
