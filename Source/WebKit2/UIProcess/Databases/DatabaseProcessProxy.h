/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DatabaseProcessProxy_h
#define DatabaseProcessProxy_h

#if ENABLE(DATABASE_PROCESS)

#include "ChildProcessProxy.h"
#include "ProcessLauncher.h"
#include "WebProcessProxyMessages.h"
#include <wtf/Deque.h>

namespace WebKit {

class WebContext;

class DatabaseProcessProxy : public ChildProcessProxy {
public:
    static PassRefPtr<DatabaseProcessProxy> create(WebContext*);
    ~DatabaseProcessProxy();

    void getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply>);

private:
    DatabaseProcessProxy(WebContext*);

    // ChildProcessProxy
    virtual void getLaunchOptions(ProcessLauncher::LaunchOptions&) override;
    virtual void connectionWillOpen(IPC::Connection*) override;
    virtual void connectionWillClose(IPC::Connection*) override;

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didClose(IPC::Connection*) override;
    virtual void didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    void didReceiveDatabaseProcessProxyMessage(IPC::Connection*, IPC::MessageDecoder&);

    // Message handlers
    void didCreateDatabaseToWebProcessConnection(const IPC::Attachment&);

    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    void platformGetLaunchOptions(ProcessLauncher::LaunchOptions&);

    WebContext* m_webContext;

    unsigned m_numPendingConnectionRequests;
    Deque<RefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply>> m_pendingConnectionReplies;
};

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)

#endif // DatabaseProcessProxy_h
