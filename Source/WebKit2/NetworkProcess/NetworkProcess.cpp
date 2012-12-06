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
#include "NetworkProcess.h"

#if ENABLE(NETWORK_PROCESS)

#include "ArgumentCoders.h"
#include "Attachment.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessProxyMessages.h"
#include "RemoteNetworkingContext.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/RunLoop.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

NetworkProcess& NetworkProcess::shared()
{
    DEFINE_STATIC_LOCAL(NetworkProcess, networkProcess, ());
    return networkProcess;
}

NetworkProcess::NetworkProcess()
{
}

NetworkProcess::~NetworkProcess()
{
}

void NetworkProcess::initialize(CoreIPC::Connection::Identifier serverIdentifier, WebCore::RunLoop* runLoop)
{
    ASSERT(!m_uiConnection);

    m_uiConnection = CoreIPC::Connection::createClientConnection(serverIdentifier, this, runLoop);
    m_uiConnection->setDidCloseOnConnectionWorkQueueCallback(didCloseOnConnectionWorkQueue);
    m_uiConnection->open();
}

void NetworkProcess::removeNetworkConnectionToWebProcess(NetworkConnectionToWebProcess* connection)
{
    size_t vectorIndex = m_webProcessConnections.find(connection);
    ASSERT(vectorIndex != notFound);

    m_webProcessConnections.remove(vectorIndex);
}

bool NetworkProcess::shouldTerminate()
{
    return true;
}

void NetworkProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    didReceiveNetworkProcessMessage(connection, messageID, decoder);
}

void NetworkProcess::didClose(CoreIPC::Connection*)
{
    // The UIProcess just crashed.
    RunLoop::current()->stop();
}

void NetworkProcess::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference, CoreIPC::StringReference)
{
    RunLoop::current()->stop();
}

void NetworkProcess::initializeNetworkProcess(const NetworkProcessCreationParameters& parameters)
{
    platformInitialize(parameters);

#if PLATFORM(MAC) || USE(CFNETWORK)
    RemoteNetworkingContext::setPrivateBrowsingStorageSessionIdentifierBase(parameters.uiProcessBundleIdentifier);
#endif
}

void NetworkProcess::createNetworkConnectionToWebProcess()
{
#if PLATFORM(MAC)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    RefPtr<NetworkConnectionToWebProcess> connection = NetworkConnectionToWebProcess::create(CoreIPC::Connection::Identifier(listeningPort));
    m_webProcessConnections.append(connection.release());

    CoreIPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    m_uiConnection->send(Messages::NetworkProcessProxy::DidCreateNetworkConnectionToWebProcess(clientPort), 0);
#else
    notImplemented();
#endif
}

void NetworkProcess::ensurePrivateBrowsingSession()
{
    RemoteNetworkingContext::ensurePrivateBrowsingSession();
}

void NetworkProcess::destroyPrivateBrowsingSession()
{
    RemoteNetworkingContext::destroyPrivateBrowsingSession();
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
