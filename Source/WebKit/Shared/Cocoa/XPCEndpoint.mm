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
#import "XPCEndpoint.h"

#import <wtf/cocoa/Entitlements.h>

namespace WebKit {

XPCEndpoint::XPCEndpoint()
{
    m_connection = adoptOSObject(xpc_connection_create(nullptr, nullptr));
    m_endpoint = adoptOSObject(xpc_endpoint_create(m_connection.get()));

    xpc_connection_set_target_queue(m_connection.get(), dispatch_get_main_queue());
    xpc_connection_set_event_handler(m_connection.get(), ^(xpc_object_t message) {
        xpc_type_t type = xpc_get_type(message);

        if (type == XPC_TYPE_CONNECTION) {
            OSObjectPtr<xpc_connection_t> connection = message;
            auto pid = xpc_connection_get_pid(connection.get());

            if (pid != getpid() && !WTF::hasEntitlement(connection.get(), "com.apple.private.webkit.use-xpc-endpoint")) {
                WTFLogAlways("Audit token does not have required entitlement com.apple.private.webkit.use-xpc-endpoint");
                return;
            }
            xpc_connection_set_target_queue(connection.get(), dispatch_get_main_queue());
            xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t event) {
                handleEvent(connection.get(), event);
            });
            xpc_connection_resume(connection.get());
        }
    });
    xpc_connection_resume(m_connection.get());
}

void XPCEndpoint::sendEndpointToConnection(xpc_connection_t connection)
{
    if (!connection)
        return;

    auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(message.get(), xpcEndpointMessageNameKey(), xpcEndpointMessageName());
    xpc_dictionary_set_value(message.get(), xpcEndpointNameKey(), m_endpoint.get());

    xpc_connection_send_message(connection, message.get());
}

OSObjectPtr<xpc_endpoint_t> XPCEndpoint::endpoint() const
{
    return m_endpoint;
}

}
