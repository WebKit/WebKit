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
#include "RemoteMediaSessionClientProxy.h"

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)

#include "MessageSenderInlines.h"
#include "RemoteMediaSessionManagerMessages.h"
#include "RemoteMediaSessionManagerProxy.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaSessionClientProxy);

RemoteMediaSessionClientProxy::RemoteMediaSessionClientProxy(const RemoteMediaSessionState& state, RemoteMediaSessionManagerProxy& manager)
    : PlatformMediaSessionClient()
    , m_manager(manager)
    , m_state(state)
#if !RELEASE_LOG_DISABLED
    , m_logger(manager.process()->logger())
#endif
{
}

RemoteMediaSessionClientProxy::~RemoteMediaSessionClientProxy() = default;

void RemoteMediaSessionClientProxy::resumeAutoplaying()
{
    if (RefPtr manager = m_manager.get())
        manager->send(Messages::RemoteMediaSessionManager::ClientShouldResumeAutoplaying(sessionIdentifier()));
}

void RemoteMediaSessionClientProxy::mayResumePlayback(bool shouldResume)
{
    if (RefPtr manager = m_manager.get())
        manager->send(Messages::RemoteMediaSessionManager::ClientMayResumePlayback(sessionIdentifier(), shouldResume));
}

void RemoteMediaSessionClientProxy::suspendPlayback()
{
    if (RefPtr manager = m_manager.get())
        manager->send(Messages::RemoteMediaSessionManager::ClientShouldSuspendPlayback(sessionIdentifier()));
}

void RemoteMediaSessionClientProxy::didReceiveRemoteControlCommand(WebCore::PlatformMediaSessionRemoteControlCommandType command, const WebCore::PlatformMediaSessionRemoteCommandArgument& argument)
{
    ASSERT_NOT_REACHED();
}

bool RemoteMediaSessionClientProxy::shouldOverrideBackgroundPlaybackRestriction(WebCore::PlatformMediaSessionInterruptionType) const
{
    // FIXME: Sync XPC?
    notImplemented();

    return false;
}

void RemoteMediaSessionClientProxy::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    if (RefPtr manager = m_manager.get())
        manager->send(Messages::RemoteMediaSessionManager::ClientSetShouldPlayToPlaybackTarget(sessionIdentifier(), shouldPlay));
}

RefPtr<WebCore::MediaSessionManagerInterface> RemoteMediaSessionClientProxy::sessionManager() const
{
    if (RefPtr manager = m_manager.get())
        return manager;

    return nullptr;
}

} // namespace WebKit

#endif // ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
