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

#include "config.h"
#include "DatabaseProcessProxy.h"

#include "DatabaseProcessMessages.h"
#include "WebContext.h"

#if ENABLE(DATABASE_PROCESS)

using namespace WebCore;

namespace WebKit {

PassRefPtr<DatabaseProcessProxy> DatabaseProcessProxy::create(WebContext* context)
{
    return adoptRef(new DatabaseProcessProxy(context));
}

DatabaseProcessProxy::DatabaseProcessProxy(WebContext* context)
    : m_webContext(context)
    , m_numPendingConnectionRequests(0)
{
    connect();
}

DatabaseProcessProxy::~DatabaseProcessProxy()
{
}


void DatabaseProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::DatabaseProcess;
    platformGetLaunchOptions(launchOptions);
}

void DatabaseProcessProxy::connectionWillOpen(IPC::Connection*)
{
}

void DatabaseProcessProxy::connectionWillClose(IPC::Connection*)
{
}

void DatabaseProcessProxy::getDatabaseProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply> reply)
{
    m_pendingConnectionReplies.append(reply);

    if (state() == State::Launching) {
        m_numPendingConnectionRequests++;
        return;
    }

    connection()->send(Messages::DatabaseProcess::CreateDatabaseToWebProcessConnection(), 0, IPC::DispatchMessageEvenWhenWaitingForSyncReply);
}

void DatabaseProcessProxy::didClose(IPC::Connection*)
{
}

void DatabaseProcessProxy::didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
}

void DatabaseProcessProxy::didCreateDatabaseToWebProcessConnection(const IPC::Attachment& connectionIdentifier)
{
    ASSERT(!m_pendingConnectionReplies.isEmpty());

    RefPtr<Messages::WebProcessProxy::GetDatabaseProcessConnection::DelayedReply> reply = m_pendingConnectionReplies.takeFirst();

#if OS(DARWIN)
    reply->send(IPC::Attachment(connectionIdentifier.port(), MACH_MSG_TYPE_MOVE_SEND));
#else
    notImplemented();
#endif
}

void DatabaseProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (IPC::Connection::identifierIsNull(connectionIdentifier)) {
        // FIXME: Do better cleanup here.
        return;
    }

    for (unsigned i = 0; i < m_numPendingConnectionRequests; ++i)
        connection()->send(Messages::DatabaseProcess::CreateDatabaseToWebProcessConnection(), 0);
    
    m_numPendingConnectionRequests = 0;
}

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
