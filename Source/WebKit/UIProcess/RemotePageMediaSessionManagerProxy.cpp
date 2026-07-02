/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RemotePageMediaSessionManagerProxy.h"

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "RemoteMediaSessionManagerProxy.h"
#include "RemoteMediaSessionManagerProxyMessages.h"
#include "WebProcessProxy.h"

namespace WebKit {

Ref<RemotePageMediaSessionManagerProxy> RemotePageMediaSessionManagerProxy::create(WebCore::PageIdentifier pageID, WebProcessProxy& process, RemoteMediaSessionManagerProxy* manager)
{
    return adoptRef(*new RemotePageMediaSessionManagerProxy(pageID, process, manager));
}

RemotePageMediaSessionManagerProxy::RemotePageMediaSessionManagerProxy(WebCore::PageIdentifier pageID, WebProcessProxy& process, RemoteMediaSessionManagerProxy* manager)
    : m_identifier(pageID)
    , m_process(process)
    , m_manager(manager)
{
    process.addMessageReceiver(Messages::RemoteMediaSessionManagerProxy::messageReceiverName(), m_identifier, *this);
}

RemotePageMediaSessionManagerProxy::~RemotePageMediaSessionManagerProxy()
{
    m_process->removeMessageReceiver(Messages::RemoteMediaSessionManagerProxy::messageReceiverName(), m_identifier);
}

void RemotePageMediaSessionManagerProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (RefPtr manager = m_manager.get())
        manager->didReceiveMessage(connection, decoder);
}

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
