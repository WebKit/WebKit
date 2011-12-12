/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WebConnectionToUIProcess_h
#define WebConnectionToUIProcess_h

#include "Connection.h"
#include "WebConnection.h"

namespace WebKit {

class WebProcess;

class WebConnectionToUIProcess : public WebConnection, CoreIPC::Connection::Client, CoreIPC::Connection::QueueClient {
public:
    static PassRefPtr<WebConnectionToUIProcess> create(WebProcess*, CoreIPC::Connection::Identifier, RunLoop*);

    CoreIPC::Connection* connection() { return m_connection.get(); }

    void invalidate();

private:
    WebConnectionToUIProcess(WebProcess*, CoreIPC::Connection::Identifier, RunLoop*);

    // WebConnection
    virtual void postMessage(const String&, APIObject*);

    // CoreIPC::Connection::Client
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);
    virtual void didClose(CoreIPC::Connection*);
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID);
    virtual void syncMessageSendTimedOut(CoreIPC::Connection*);
#if PLATFORM(WIN)
    virtual Vector<HWND> windowsToReceiveSentMessagesWhileWaitingForSyncReply();
#endif

    // CoreIPC::Connection::QueueClient
    virtual void didReceiveMessageOnConnectionWorkQueue(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, bool& didHandleMessage) OVERRIDE;

    WebProcess* m_process;
    RefPtr<CoreIPC::Connection> m_connection;
};

} // namespace WebKit

#endif // WebConnectionToUIProcess_h
