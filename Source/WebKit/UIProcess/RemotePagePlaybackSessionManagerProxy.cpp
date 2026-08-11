/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RemotePagePlaybackSessionManagerProxy.h"

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "PlaybackSessionManagerProxy.h"
#include "PlaybackSessionManagerProxyMessages.h"
#include "WebProcessProxy.h"

namespace WebKit {

Ref<RemotePagePlaybackSessionManagerProxy> RemotePagePlaybackSessionManagerProxy::create(WebCore::PageIdentifier identifier, PlaybackSessionManagerProxy* manager, WebProcessProxy& process)
{
    return adoptRef(*new RemotePagePlaybackSessionManagerProxy(identifier, manager, process));
}

RemotePagePlaybackSessionManagerProxy::RemotePagePlaybackSessionManagerProxy(WebCore::PageIdentifier identifier, PlaybackSessionManagerProxy* manager, WebProcessProxy& process)
    : m_identifier(identifier)
    , m_manager(manager)
    , m_process(process)
{
    process.addMessageReceiver(Messages::PlaybackSessionManagerProxy::messageReceiverName(), m_identifier, *this);
}

RemotePagePlaybackSessionManagerProxy::~RemotePagePlaybackSessionManagerProxy()
{
    m_process->removeMessageReceiver(Messages::PlaybackSessionManagerProxy::messageReceiverName(), m_identifier);
}

void RemotePagePlaybackSessionManagerProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (RefPtr manager = m_manager.get())
        manager->didReceiveMessage(connection, decoder);
}

}

#endif
