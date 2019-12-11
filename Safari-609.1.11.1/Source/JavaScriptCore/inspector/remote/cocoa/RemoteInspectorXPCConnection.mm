/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteInspectorXPCConnection.h"

#if ENABLE(REMOTE_INSPECTOR)

#import <Foundation/Foundation.h>
#import <mutex>
#import <wtf/Assertions.h>
#import <wtf/Lock.h>
#import <wtf/Ref.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/cocoa/CFXPCBridgeSPI.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/spi/darwin/XPCSPI.h>

namespace Inspector {

// Constants private to this file for message serialization on both ends.
#define RemoteInspectorXPCConnectionMessageNameKey @"messageName"
#define RemoteInspectorXPCConnectionUserInfoKey @"userInfo"
#define RemoteInspectorXPCConnectionSerializedMessageKey "msgData"

RemoteInspectorXPCConnection::RemoteInspectorXPCConnection(xpc_connection_t connection, dispatch_queue_t queue, Client* client)
    : m_connection(connection)
    , m_queue(queue)
    , m_client(client)
{
    dispatch_retain(m_queue);

    xpc_retain(m_connection);
    xpc_connection_set_target_queue(m_connection, m_queue);
    xpc_connection_set_event_handler(m_connection, ^(xpc_object_t object) {
        handleEvent(object);
    });

    // Balanced by deref when the xpc_connection receives XPC_ERROR_CONNECTION_INVALID.
    ref();

    xpc_connection_resume(m_connection);
}

RemoteInspectorXPCConnection::~RemoteInspectorXPCConnection()
{
    ASSERT(!m_client);
    ASSERT(!m_connection);
    ASSERT(m_closed);
}

void RemoteInspectorXPCConnection::close()
{
    LockHolder lock(m_mutex);
    closeFromMessage();
}

void RemoteInspectorXPCConnection::closeFromMessage()
{
    m_closed = true;
    m_client = nullptr;

    dispatch_async(m_queue, ^{
        LockHolder lock(m_mutex);
        // This will trigger one last XPC_ERROR_CONNECTION_INVALID event on the queue and deref us.
        closeOnQueue();
    });
}

void RemoteInspectorXPCConnection::closeOnQueue()
{
    if (m_connection) {
        xpc_connection_cancel(m_connection);
        xpc_release(m_connection);
        m_connection = nullptr;
    }

    if (m_queue) {
        dispatch_release(m_queue);
        m_queue = nullptr;
    }
}

NSDictionary *RemoteInspectorXPCConnection::deserializeMessage(xpc_object_t object)
{
    if (xpc_get_type(object) != XPC_TYPE_DICTIONARY)
        return nil;

    xpc_object_t xpcDictionary = xpc_dictionary_get_value(object, RemoteInspectorXPCConnectionSerializedMessageKey);
    if (!xpcDictionary || xpc_get_type(xpcDictionary) != XPC_TYPE_DICTIONARY) {
        LockHolder lock(m_mutex);
        if (m_client)
            m_client->xpcConnectionUnhandledMessage(this, object);
        return nil;
    }

    auto dictionary = _CFXPCCreateCFObjectFromXPCMessage(xpcDictionary);
    ASSERT_WITH_MESSAGE(dictionary, "Unable to deserialize xpc message");
    ASSERT(CFGetTypeID(dictionary) == CFDictionaryGetTypeID());
    return CFBridgingRelease(dictionary);
}

void RemoteInspectorXPCConnection::handleEvent(xpc_object_t object)
{
    if (xpc_get_type(object) == XPC_TYPE_ERROR) {
        {
            LockHolder lock(m_mutex);
            if (m_client)
                m_client->xpcConnectionFailed(this);

            m_closed = true;
            m_client = nullptr;
            closeOnQueue();
        }

        if (object == XPC_ERROR_CONNECTION_INVALID) {
            // This is the last event we will ever receive from the connection.
            // This balances the ref() in the constructor.
            deref();
        }
        return;
    }

#if PLATFORM(MAC)
    if (!m_validated) {
        audit_token_t token;
        xpc_connection_get_audit_token(m_connection, &token);
        if (!WTF::hasEntitlement(token, "com.apple.private.webinspector.webinspectord")) {
            LockHolder lock(m_mutex);
            // This will trigger one last XPC_ERROR_CONNECTION_INVALID event on the queue and deref us.
            closeOnQueue();
            return;
        }
        m_validated = true;
    }
#endif

    NSDictionary *dictionary = deserializeMessage(object);
    if (![dictionary isKindOfClass:[NSDictionary class]])
        return;

    NSString *message = dictionary[RemoteInspectorXPCConnectionMessageNameKey];
    if (![message isKindOfClass:[NSString class]])
        return;

    NSDictionary *userInfo = dictionary[RemoteInspectorXPCConnectionUserInfoKey];
    if (userInfo && ![userInfo isKindOfClass:[NSDictionary class]])
        return;

    LockHolder lock(m_mutex);
    if (m_client)
        m_client->xpcConnectionReceivedMessage(this, message, userInfo);
}

void RemoteInspectorXPCConnection::sendMessage(NSString *messageName, NSDictionary *userInfo)
{
    ASSERT(!m_closed);
    if (m_closed)
        return;

    RetainPtr<NSMutableDictionary> dictionary = adoptNS([[NSMutableDictionary alloc] init]);
    [dictionary setObject:messageName forKey:RemoteInspectorXPCConnectionMessageNameKey];
    if (userInfo)
        [dictionary setObject:userInfo forKey:RemoteInspectorXPCConnectionUserInfoKey];

    xpc_object_t xpcDictionary = _CFXPCCreateXPCMessageWithCFObject((__bridge CFDictionaryRef)dictionary.get());
    ASSERT_WITH_MESSAGE(xpcDictionary && xpc_get_type(xpcDictionary) == XPC_TYPE_DICTIONARY, "Unable to serialize xpc message");
    if (!xpcDictionary)
        return;

    xpc_object_t msg = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_value(msg, RemoteInspectorXPCConnectionSerializedMessageKey, xpcDictionary);
    xpc_release(xpcDictionary);

    xpc_connection_send_message(m_connection, msg);

    xpc_release(msg);
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
