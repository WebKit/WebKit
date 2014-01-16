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

#if ENABLE(REMOTE_INSPECTOR)

#ifndef RemoteInspectorXPCConnection_h
#define RemoteInspectorXPCConnection_h

#import <dispatch/dispatch.h>
#import <wtf/Noncopyable.h>
#import <xpc/xpc.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;

namespace Inspector {

class RemoteInspectorXPCConnection {
    WTF_MAKE_NONCOPYABLE(RemoteInspectorXPCConnection);

public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void xpcConnectionReceivedMessage(RemoteInspectorXPCConnection*, NSString *messageName, NSDictionary *userInfo) = 0;
        virtual void xpcConnectionFailed(RemoteInspectorXPCConnection*) = 0;
        virtual void xpcConnectionUnhandledMessage(RemoteInspectorXPCConnection*, xpc_object_t) = 0;
    };

    RemoteInspectorXPCConnection(xpc_connection_t, Client*);
    virtual ~RemoteInspectorXPCConnection();

    void close();
    void sendMessage(NSString *messageName, NSDictionary *userInfo);

private:
    NSDictionary *deserializeMessage(xpc_object_t);
    void handleEvent(xpc_object_t);

    xpc_connection_t m_connection;
    dispatch_queue_t m_queue;
    Client* m_client;
};

} // namespace Inspector

#endif // RemoteInspectorXPCConnection_h

#endif // ENABLE(REMOTE_INSPECTOR)
