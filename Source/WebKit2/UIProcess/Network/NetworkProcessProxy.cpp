/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "NetworkProcessProxy.h"

#include "DownloadProxyMessages.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include "WebContext.h"
#include "WebProcessMessages.h"
#include <WebCore/RunLoop.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

PassRefPtr<NetworkProcessProxy> NetworkProcessProxy::create(WebContext* webContext)
{
    return adoptRef(new NetworkProcessProxy(webContext));
}

NetworkProcessProxy::NetworkProcessProxy(WebContext* webContext)
    : m_webContext(webContext)
    , m_numPendingConnectionRequests(0)
{
    connect();
}

void NetworkProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::NetworkProcess;

#if PLATFORM(MAC)
    launchOptions.architecture = ProcessLauncher::LaunchOptions::MatchCurrentArchitecture;
    launchOptions.executableHeap = false;
#if HAVE(XPC)
    launchOptions.useXPC = false;
#endif
#endif
}

NetworkProcessProxy::~NetworkProcessProxy()
{
}

void NetworkProcessProxy::getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply)
{
    m_pendingConnectionReplies.append(reply);

    if (isLaunching()) {
        m_numPendingConnectionRequests++;
        return;
    }

    connection()->send(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess(), 0, CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply);
}

DownloadProxy* NetworkProcessProxy::createDownloadProxy()
{
    if (!m_downloadProxyMap)
        m_downloadProxyMap = adoptPtr(new DownloadProxyMap(m_messageReceiverMap));

    return m_downloadProxyMap->createDownloadProxy(m_webContext);
}

void NetworkProcessProxy::networkProcessCrashedOrFailedToLaunch()
{
    // The network process must have crashed or exited, send any pending sync replies we might have.
    while (!m_pendingConnectionReplies.isEmpty()) {
        RefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply = m_pendingConnectionReplies.takeFirst();

#if PLATFORM(MAC)
        reply->send(CoreIPC::Attachment(0, MACH_MSG_TYPE_MOVE_SEND));
#else
        notImplemented();
#endif
    }

    // Tell the network process manager to forget about this network process proxy. This may cause us to be deleted.
    m_webContext->removeNetworkProcessProxy(this);
}

void NetworkProcessProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    if (m_messageReceiverMap.dispatchMessage(connection, messageID, decoder))
        return;

    didReceiveNetworkProcessProxyMessage(connection, messageID, decoder);
}

void NetworkProcessProxy::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder, OwnPtr<CoreIPC::MessageEncoder>& replyEncoder)
{
    if (m_messageReceiverMap.dispatchSyncMessage(connection, messageID, decoder, replyEncoder))
        return;

    ASSERT_NOT_REACHED();
}

void NetworkProcessProxy::didClose(CoreIPC::Connection*)
{
    if (m_downloadProxyMap)
        m_downloadProxyMap->processDidClose();

    // This may cause us to be deleted.
    networkProcessCrashedOrFailedToLaunch();
}

void NetworkProcessProxy::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference, CoreIPC::StringReference)
{
}

void NetworkProcessProxy::didCreateNetworkConnectionToWebProcess(const CoreIPC::Attachment& connectionIdentifier)
{
    ASSERT(!m_pendingConnectionReplies.isEmpty());

    // Grab the first pending connection reply.
    RefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply> reply = m_pendingConnectionReplies.takeFirst();

#if PLATFORM(MAC)
    reply->send(CoreIPC::Attachment(connectionIdentifier.port(), MACH_MSG_TYPE_MOVE_SEND));
#else
    notImplemented();
#endif
}

void NetworkProcessProxy::didFinishLaunching(ProcessLauncher* launcher, CoreIPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (CoreIPC::Connection::identifierIsNull(connectionIdentifier)) {
        // FIXME: Do better cleanup here.
        return;
    }

    for (unsigned i = 0; i < m_numPendingConnectionRequests; ++i)
        connection()->send(Messages::NetworkProcess::CreateNetworkConnectionToWebProcess(), 0);
    
    m_numPendingConnectionRequests = 0;

#if PLATFORM(MAC)
    if (WebContext::applicationIsOccluded())
        connection()->send(Messages::NetworkProcess::SetApplicationIsOccluded(true), 0);
#endif
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
