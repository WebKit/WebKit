/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkProcessEndpointClient.h"

#import "Connection.h"
#import "XPCEndpoint.h"
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

NetworkProcessEndpointClient& NetworkProcessEndpointClient::singleton()
{
    static NeverDestroyed<NetworkProcessEndpointClient> manager;
    return manager.get();
}

void NetworkProcessEndpointClient::handleEvent(xpc_object_t message)
{
#if ENABLE(XPC_IPC)
    if (IPC::Connection::handleXPCMessage(message))
        return;
#endif

    for (auto& observer : m_observers)
        observer->handleEvent(message);
}

void NetworkProcessEndpointClient::didConnect()
{
    for (auto& observer : m_observers)
        observer->didConnect(connection().get());
}

void NetworkProcessEndpointClient::didCloseConnection(xpc_connection_t connection)
{
    WTFLogAlways("WebKitXPC: NetworkProcessEndpointClient::didCloseConnection");
#if ENABLE(XPC_IPC)
    IPC::Connection::handleXPCDisconnect(XPCObject { connection });
#endif
}

void NetworkProcessEndpointClient::addObserver(WeakPtr<NetworkProcessEndpointObserver> observer)
{
    m_observers.append(observer);
}

}
