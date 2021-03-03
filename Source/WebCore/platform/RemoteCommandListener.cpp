/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "RemoteCommandListener.h"

#if PLATFORM(MAC)
#include "RemoteCommandListenerMac.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "RemoteCommandListenerIOS.h"
#endif

namespace WebCore {

static RemoteCommandListener::CreationFunction& remoteCommandListenerCreationFunction()
{
    static NeverDestroyed<RemoteCommandListener::CreationFunction> creationFunction;
    return creationFunction;
}

void RemoteCommandListener::setCreationFunction(CreationFunction&& function)
{
    remoteCommandListenerCreationFunction() = WTFMove(function);
}

void RemoteCommandListener::resetCreationFunction()
{
    remoteCommandListenerCreationFunction() = [] (RemoteCommandListenerClient& client) {
#if PLATFORM(MAC)
        return RemoteCommandListenerMac::create(client);
#elif PLATFORM(IOS_FAMILY)
        return RemoteCommandListenerIOS::create(client);
#else
        return RemoteCommandListener::create(client);
#endif
    };
}

std::unique_ptr<RemoteCommandListener> RemoteCommandListener::create(RemoteCommandListenerClient& client)
{
    if (!remoteCommandListenerCreationFunction())
        resetCreationFunction();
    return remoteCommandListenerCreationFunction()(client);
}

RemoteCommandListener::RemoteCommandListener(RemoteCommandListenerClient& client)
    : m_client(client)
{
}

RemoteCommandListener::~RemoteCommandListener() = default;


void RemoteCommandListener::scheduleSupportedCommandsUpdate()
{
    if (!m_updateCommandsTask.hasPendingTask()) {
        m_updateCommandsTask.scheduleTask([this] ()  {
            updateSupportedCommands();
        });
    }
}

void RemoteCommandListener::setSupportsSeeking(bool supports)
{
    if (m_supportsSeeking == supports)
        return;

    m_supportsSeeking = supports;
    scheduleSupportedCommandsUpdate();
}

void RemoteCommandListener::addSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    m_registeredCommands.add(command);
    scheduleSupportedCommandsUpdate();
}

void RemoteCommandListener::removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    m_registeredCommands.remove(command);
    scheduleSupportedCommandsUpdate();
}

}
