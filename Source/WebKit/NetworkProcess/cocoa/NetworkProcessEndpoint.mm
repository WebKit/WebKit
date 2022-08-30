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
#import "NetworkProcessEndpoint.h"

#import "Connection.h"
#import "NetworkProcessEndpointXPCConstants.h"
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

namespace WebKit {

NetworkProcessEndpoint::NetworkProcessEndpoint(NetworkProcess& networkProcess)
: m_networkProcess(WeakPtr { networkProcess })
{
}

const char* NetworkProcessEndpoint::supplementName()
{
    return "NetworkProcessEndpointSupplement";
}

NetworkProcessEndpoint::~NetworkProcessEndpoint()
{
}

void NetworkProcessEndpoint::addObserver(WeakPtr<NetworkProcessEndpointObserver> observer)
{
    m_observers.append(observer);
}

const char* NetworkProcessEndpoint::xpcEndpointMessageNameKey() const
{
    return xpcMessageNameKey;
}

const char* NetworkProcessEndpoint::xpcEndpointMessageName() const
{
    return NetworkProcessEndpointXPCConstants::xpcNetworkProcessXPCEndpointMessageName;
}

const char* NetworkProcessEndpoint::xpcEndpointNameKey() const
{
    return NetworkProcessEndpointXPCConstants::xpcNetworkProcessXPCEndpointNameKey;
}

void NetworkProcessEndpoint::didConnect(xpc_connection_t connection)
{
}

void NetworkProcessEndpoint::didCloseConnection(xpc_connection_t connection)
{
    WTFLogAlways("WebKitXPC: NetworkProcessEndpoint::didCloseConnection");
#if ENABLE(XPC_IPC)
    IPC::Connection::handleXPCDisconnect(XPCObject { connection });
#endif
}

void NetworkProcessEndpoint::handleEvent(xpc_connection_t connection, xpc_object_t event)
{
    if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
#if ENABLE(XPC_IPC)
        auto messageName = xpc_dictionary_get_string(event, xpcMessageNameKey);
        if (messageName && !strcmp(messageName, NetworkProcessEndpointXPCConstants::xpcBootstrapNetworkProcessConnectionMessageName)) {
            auto sessionID = xpc_dictionary_get_uint64(event, NetworkProcessEndpointXPCConstants::xpcBootstrapNetworkProcessConnectionSessionIDKey);
            auto processIdentifier = xpc_dictionary_get_uint64(event, NetworkProcessEndpointXPCConstants::xpcBootstrapNetworkProcessConnectionProcessIdentifierKey);
            if (!m_networkProcess)
                return;
            m_networkProcess->createXPCNetworkConnectionToWebProcess(makeObjectIdentifier<WebCore::ProcessIdentifierType>(processIdentifier), PAL::SessionID(sessionID), IPC::Connection::Identifier(0, connection));
            return;
        }

        if (IPC::Connection::handleXPCMessage(event))
            return;
#endif
        // FIXME: Lock m_observers
        for (auto& observer : m_observers) {
            callOnMainRunLoop([observer, connection = OSObjectPtr(connection), event = OSObjectPtr(event)] {
                observer->handleEvent(connection.get(), event.get());
            });
        }
    }
}

void NetworkProcessEndpoint::initializeConnection(IPC::Connection* connection)
{
    sendEndpointToConnection(connection->xpcConnection());
}

}
