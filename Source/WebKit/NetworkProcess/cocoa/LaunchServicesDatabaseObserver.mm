/*
 * Copyright (C) 2020, 2022 Apple Inc. All rights reserved.
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
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/darwin/XPCExtras.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LaunchServicesDatabaseObserver);

LaunchServicesDatabaseObserver::LaunchServicesDatabaseObserver(NetworkProcess&)
{
#if HAVE(LSDATABASECONTEXT) && !HAVE(SYSTEM_CONTENT_LS_DATABASE)
    m_observer = [LSDatabaseContext.sharedDatabaseContext addDatabaseChangeObserver4WebKit:^(xpc_object_t change) {
        // FIXME: This is a false positive. <rdar://164843889>
        SUPPRESS_RETAINPTR_CTOR_ADOPT auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName);
        xpc_dictionary_set_value(message.get(), LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseKey, change);

        Locker locker { m_connectionsLock };
        for (auto& connection : m_connections) {
            RELEASE_ASSERT(xpc_get_type(connection.get()) == XPC_TYPE_CONNECTION);
            xpc_connection_send_message(connection.get(), message.get());
        }
    }];
#endif
}

ASCIILiteral LaunchServicesDatabaseObserver::supplementName()
{
    return "LaunchServicesDatabaseObserverSupplement"_s;
}

void LaunchServicesDatabaseObserver::startObserving(OSObjectPtr<xpc_connection_t> connection)
{
    {
        Locker locker { m_connectionsLock };
        m_connections.append(connection);
    }

#if HAVE(SYSTEM_CONTENT_LS_DATABASE)
    [LSDatabaseContext.sharedDatabaseContext getSystemContentDatabaseObject4WebKit:makeBlockPtr([connection = connection] (xpc_object_t _Nullable object, NSError * _Nullable error) {
        if (!object)
            return;
        // FIXME: This is a false positive. <rdar://164843889>
        SUPPRESS_RETAINPTR_CTOR_ADOPT auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName);
        xpc_dictionary_set_value(message.get(), LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseKey, object);

        xpc_connection_send_message(connection.get(), message.get());

    }).get()];
#elif HAVE(LSDATABASECONTEXT)
    RetainPtr<id> observer = [LSDatabaseContext.sharedDatabaseContext addDatabaseChangeObserver4WebKit:^(xpc_object_t change) {
        // FIXME: This is a false positive. <rdar://164843889>
        SUPPRESS_RETAINPTR_CTOR_ADOPT auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName);
        xpc_dictionary_set_value(message.get(), LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseKey, change);

        xpc_connection_send_message(connection.get(), message.get());
    }];

    [LSDatabaseContext.sharedDatabaseContext removeDatabaseChangeObserver4WebKit:observer.get()];
#else
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName);
    xpc_connection_send_message(connection.get(), message.get());
#endif
}

LaunchServicesDatabaseObserver::~LaunchServicesDatabaseObserver()
{
#if HAVE(LSDATABASECONTEXT) && !HAVE(SYSTEM_CONTENT_LS_DATABASE)
    [LSDatabaseContext.sharedDatabaseContext removeDatabaseChangeObserver4WebKit:m_observer.get()];
#endif
}

ASCIILiteral LaunchServicesDatabaseObserver::xpcEndpointMessageNameKey() const
{
    return xpcMessageNameKey;
}

ASCIILiteral LaunchServicesDatabaseObserver::xpcEndpointMessageName() const
{
    return LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseXPCEndpointMessageName;
}

ASCIILiteral LaunchServicesDatabaseObserver::xpcEndpointNameKey() const
{
    return LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseXPCEndpointNameKey;
}

void LaunchServicesDatabaseObserver::handleEvent(xpc_connection_t connection, xpc_object_t event)
{
    if (xpc_get_type(event) == XPC_TYPE_ERROR) {
        if (event != XPC_ERROR_CONNECTION_INVALID && event != XPC_ERROR_TERMINATION_IMMINENT)
            return;

        Locker locker { m_connectionsLock };
        for (size_t i = 0; i < m_connections.size(); i++) {
            if (m_connections[i].get() == connection) {
                m_connections.removeAt(i);
                break;
            }
        }
        return;
    }
    if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
        String messageName = xpcDictionaryGetString(event, xpcMessageNameKey);
        if (messageName != LaunchServicesDatabaseXPCConstants::xpcRequestLaunchServicesDatabaseUpdateMessageName)
            return;
        startObserving(connection);
    }
}

void LaunchServicesDatabaseObserver::initializeConnection(IPC::Connection* connection)
{
    sendEndpointToConnection(protect(connection->xpcConnection()).get());
}

}
