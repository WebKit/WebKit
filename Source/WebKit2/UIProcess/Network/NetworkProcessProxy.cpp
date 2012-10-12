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

#include "NetworkProcessCreationParameters.h"
#include "NetworkProcessMessages.h"
#include <WebCore/RunLoop.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

PassRefPtr<NetworkProcessProxy> NetworkProcessProxy::create()
{
    return adoptRef(new NetworkProcessProxy);
}

NetworkProcessProxy::NetworkProcessProxy()
{
    ProcessLauncher::LaunchOptions launchOptions;
    launchOptions.processType = ProcessLauncher::NetworkProcess;

#if PLATFORM(MAC)
    launchOptions.architecture = ProcessLauncher::LaunchOptions::MatchCurrentArchitecture;
    launchOptions.executableHeap = false;
#if HAVE(XPC)
    launchOptions.useXPC = false;
#endif
#endif

    m_processLauncher = ProcessLauncher::create(this, launchOptions);
}

NetworkProcessProxy::~NetworkProcessProxy()
{

}

void NetworkProcessProxy::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*)
{

}

void NetworkProcessProxy::didClose(CoreIPC::Connection*)
{

}

void NetworkProcessProxy::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID)
{

}

void NetworkProcessProxy::syncMessageSendTimedOut(CoreIPC::Connection*)
{

}

void NetworkProcessProxy::didFinishLaunching(ProcessLauncher*, CoreIPC::Connection::Identifier connectionIdentifier)
{
    ASSERT(!m_connection);

    if (CoreIPC::Connection::identifierIsNull(connectionIdentifier)) {
        // FIXME: Do better cleanup here.
        return;
    }

    m_connection = CoreIPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
#if PLATFORM(MAC)
    m_connection->setShouldCloseConnectionOnMachExceptions();
#endif

    m_connection->open();

    NetworkProcessCreationParameters parameters;
    platformInitializeNetworkProcess(parameters);

    // Initialize the network host process.
    m_connection->send(Messages::NetworkProcess::InitializeNetworkProcess(parameters), 0);
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
