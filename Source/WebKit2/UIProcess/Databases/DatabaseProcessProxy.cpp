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

void DatabaseProcessProxy::connectionWillOpen(CoreIPC::Connection*)
{
}

void DatabaseProcessProxy::connectionWillClose(CoreIPC::Connection*)
{
}

void DatabaseProcessProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageDecoder& decoder)
{
    if (dispatchMessage(connection, decoder))
        return;

    if (m_webContext->dispatchMessage(connection, decoder))
        return;

    // FIXME: Call through to a new didReceiveDatabaseProcessProxyMessage method when messages actually exist.
}

void DatabaseProcessProxy::didClose(CoreIPC::Connection*)
{
}

void DatabaseProcessProxy::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference messageReceiverName, CoreIPC::StringReference messageName)
{
}

void DatabaseProcessProxy::didFinishLaunching(ProcessLauncher* launcher, CoreIPC::Connection::Identifier connectionIdentifier)
{
    ChildProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (CoreIPC::Connection::identifierIsNull(connectionIdentifier)) {
        // FIXME: Do better cleanup here.
        return;
    }
}

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
