/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include "config.h"
#include "DisplayLinkProcessProxyClient.h"

#if HAVE(CVDISPLAYLINK)

#include "EventDispatcherMessages.h"
#include "WebProcessMessages.h"

namespace WebKit {

void DisplayLinkProcessProxyClient::setConnection(RefPtr<IPC::Connection>&& connection)
{
    Locker locker { m_connectionLock };
    m_connection = WTFMove(connection);
}

// This is called off the main thread.
void DisplayLinkProcessProxyClient::displayLinkFired(WebCore::PlatformDisplayID displayID, WebCore::DisplayUpdate displayUpdate, bool wantsFullSpeedUpdates, bool anyObserverWantsCallback)
{
    RefPtr<IPC::Connection> connection;
    {
        Locker locker { m_connectionLock };
        connection = m_connection;
    }

    if (!connection)
        return;

    if (wantsFullSpeedUpdates)
        connection->send(Messages::EventDispatcher::DisplayDidRefresh(displayID, displayUpdate, anyObserverWantsCallback), 0, { }, Thread::QOS::UserInteractive);
    else if (anyObserverWantsCallback)
        connection->send(Messages::WebProcess::DisplayDidRefresh(displayID, displayUpdate), 0, { }, Thread::QOS::UserInteractive);
}

}

#endif // HAVE(CVDISPLAYLINK)
