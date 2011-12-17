/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "ThreadLauncher.h"

#include "RunLoop.h"

namespace WebKit {

ThreadLauncher::ThreadLauncher(Client* client)
    : m_client(client)
{
    launchThread();
}

void ThreadLauncher::launchThread()
{
    m_isLaunching = true;

    CoreIPC::Connection::Identifier connectionIdentifier = createWebThread();

    // We've finished launching the thread, message back to the main run loop.
    RunLoop::main()->dispatch(bind(&ThreadLauncher::didFinishLaunchingThread, this, connectionIdentifier));
}

void ThreadLauncher::didFinishLaunchingThread(CoreIPC::Connection::Identifier identifier)
{
    m_isLaunching = false;
    
    if (!m_client) {
        // FIXME: Dispose of the connection identifier.
        return;
    }

    m_client->didFinishLaunching(this, identifier);
}

void ThreadLauncher::invalidate()
{
    m_client = 0;
}

} // namespace WebKit
