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
#import "LaunchServicesDatabaseObserver.h"

#import "LaunchServicesDatabaseXPCConstants.h"
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

namespace WebKit {

#if HAVE(LSDATABASECONTEXT)
static LSDatabaseContext *databaseContext()
{
    static dispatch_once_t once;
    static LSDatabaseContext *context = nullptr;
    dispatch_once(&once, ^{
        context = [NSClassFromString(@"LSDatabaseContext") sharedDatabaseContext];
    });
    return context;
}
#endif

LaunchServicesDatabaseObserver::LaunchServicesDatabaseObserver(NetworkProcess&)
{
#if HAVE(LSDATABASECONTEXT)
    if (![databaseContext() respondsToSelector:@selector(addDatabaseChangeObserver4WebKit:)])
        return;

    m_observer = [databaseContext() addDatabaseChangeObserver4WebKit:^(xpc_object_t change) {
        auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName);
        xpc_dictionary_set_value(message.get(), LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseKey, change);

        for (auto& connection : m_connections)
            xpc_connection_send_message(connection.get(), message.get());
    }];
#endif
}

const char* LaunchServicesDatabaseObserver::supplementName()
{
    return "LaunchServicesDatabaseObserverSupplement";
}

void LaunchServicesDatabaseObserver::startObserving(OSObjectPtr<xpc_connection_t> connection)
{
    m_connections.append(connection);

#if HAVE(LSDATABASECONTEXT)
    if (![databaseContext() respondsToSelector:@selector(addDatabaseChangeObserver4WebKit:)]) {
        auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName);
        xpc_connection_send_message(connection.get(), message.get());
        return;
    }

    RetainPtr<id> observer = [databaseContext() addDatabaseChangeObserver4WebKit:^(xpc_object_t change) {
        auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName);
        xpc_dictionary_set_value(message.get(), LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseKey, change);

        xpc_connection_send_message(connection.get(), message.get());
    }];

    [databaseContext() removeDatabaseChangeObserver4WebKit:observer.get()];
#else
    auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName);
    xpc_connection_send_message(connection.get(), message.get());
#endif
}

LaunchServicesDatabaseObserver::~LaunchServicesDatabaseObserver()
{
#if HAVE(LSDATABASECONTEXT)
    if (![databaseContext() respondsToSelector:@selector(removeDatabaseChangeObserver4WebKit:)])
        return;

    [databaseContext() removeDatabaseChangeObserver4WebKit:m_observer.get()];
#endif
}

const char* LaunchServicesDatabaseObserver::xpcEndpointMessageNameKey() const
{
    return xpcMessageNameKey;
}

const char* LaunchServicesDatabaseObserver::xpcEndpointMessageName() const
{
    return LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseXPCEndpointMessageName;
}

const char* LaunchServicesDatabaseObserver::xpcEndpointNameKey() const
{
    return LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseXPCEndpointNameKey;
}

void LaunchServicesDatabaseObserver::handleEvent(xpc_connection_t connection, xpc_object_t event)
{
    if (xpc_get_type(event) == XPC_TYPE_ERROR) {
        if (event != XPC_ERROR_CONNECTION_INVALID && event != XPC_ERROR_TERMINATION_IMMINENT)
            return;

        for (size_t i = 0; i < m_connections.size(); i++) {
            if (m_connections[i].get() == connection) {
                m_connections.remove(i);
                break;
            }
        }
        return;
    }
    if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
        String messageName = xpc_dictionary_get_string(event, xpcMessageNameKey);
        if (messageName != LaunchServicesDatabaseXPCConstants::xpcRequestLaunchServicesDatabaseUpdateMessageName)
            return;
        startObserving(connection);
    }
}

void LaunchServicesDatabaseObserver::initializeConnection(IPC::Connection* connection)
{
    sendEndpointToConnection(connection->xpcConnection());
}

}
