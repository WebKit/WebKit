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
#include "DatabaseProcess.h"

#include "DatabaseProcessProxyMessages.h"
#include "DatabaseToWebProcessConnection.h"

#if ENABLE(DATABASE_PROCESS)

using namespace WebCore;

namespace WebKit {

DatabaseProcess& DatabaseProcess::shared()
{
    DEFINE_STATIC_LOCAL(DatabaseProcess, databaseProcess, ());
    return databaseProcess;
}

DatabaseProcess::DatabaseProcess()
{
}

DatabaseProcess::~DatabaseProcess()
{
}

void DatabaseProcess::initializeConnection(CoreIPC::Connection* connection)
{
    ChildProcess::initializeConnection(connection);
}

bool DatabaseProcess::shouldTerminate()
{
    return true;
}

void DatabaseProcess::didClose(CoreIPC::Connection*)
{
    RunLoop::current()->stop();
}

void DatabaseProcess::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference, CoreIPC::StringReference)
{
    RunLoop::current()->stop();
}

void DatabaseProcess::createDatabaseToWebProcessConnection()
{
#if PLATFORM(MAC)
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Create a listening connection.
    RefPtr<DatabaseToWebProcessConnection> connection = DatabaseToWebProcessConnection::create(CoreIPC::Connection::Identifier(listeningPort));
    m_databaseToWebProcessConnections.append(connection.release());

    CoreIPC::Attachment clientPort(listeningPort, MACH_MSG_TYPE_MAKE_SEND);
    parentProcessConnection()->send(Messages::DatabaseProcessProxy::DidCreateDatabaseToWebProcessConnection(clientPort), 0);
#else
    notImplemented();
#endif
}

#if !PLATFORM(MAC)
void DatabaseProcess::initializeProcess(const ChildProcessInitializationParameters&)
{
}

void DatabaseProcess::initializeProcessName(const ChildProcessInitializationParameters&)
{
}

void DatabaseProcess::initializeSandbox(const ChildProcessInitializationParameters&, SandboxInitializationParameters&)
{
}
#endif

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
