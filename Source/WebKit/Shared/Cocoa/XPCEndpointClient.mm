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
#import "XPCEndpointClient.h"

#import <wtf/cocoa/Entitlements.h>

namespace WebKit {

void XPCEndpointClient::setEndpoint(xpc_endpoint_t endpoint)
{
    {
        LockHolder locker(m_lock);

        if (m_connection)
            return;

        m_connection = adoptOSObject(xpc_connection_create_from_endpoint(endpoint));

        xpc_connection_set_target_queue(m_connection.get(), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
        xpc_connection_set_event_handler(m_connection.get(), ^(xpc_object_t message) {
            xpc_type_t type = xpc_get_type(message);
            if (type == XPC_TYPE_ERROR) {
                if (message == XPC_ERROR_CONNECTION_INVALID || message == XPC_ERROR_TERMINATION_IMMINENT) {
                    LockHolder locker(m_lock);
                    m_connection = nullptr;
                }
                return;
            }
            if (type != XPC_TYPE_DICTIONARY)
                return;

            auto connection = xpc_dictionary_get_remote_connection(message);
            if (!connection)
                return;
            audit_token_t auditToken;
            xpc_connection_get_audit_token(connection, &auditToken);
            if (!WTF::hasEntitlement(auditToken, "com.apple.private.webkit.use-xpc-endpoint")) {
                // Uncomment before landing; this is commented out because the bots does not seem to update the entitlements on incremental builds.
                // WTFLogAlways("Audit token does not have required entitlement");
                // return;
            }
            handleEvent(message);
        });

        xpc_connection_resume(m_connection.get());
    }

    didConnect();
}

OSObjectPtr<xpc_connection_t> XPCEndpointClient::connection()
{
    LockHolder locker(m_lock);
    return m_connection;
}

}
