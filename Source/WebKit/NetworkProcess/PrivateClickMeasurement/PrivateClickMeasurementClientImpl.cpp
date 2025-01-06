/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PrivateClickMeasurementClientImpl.h"

#include "NetworkProcess.h"
#include "NetworkSession.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::PCM {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ClientImpl);

ClientImpl::ClientImpl(NetworkSession& session, NetworkProcess& networkProcess)
    : m_networkSession(session)
    , m_networkProcess(networkProcess) { }

void ClientImpl::broadcastConsoleMessage(JSC::MessageLevel messageLevel, const String& message)
{
    if (!featureEnabled())
        return;

    m_networkProcess->broadcastConsoleMessage(m_networkSession->sessionID(), MessageSource::PrivateClickMeasurement, messageLevel, message);
}

bool ClientImpl::featureEnabled() const
{
    return m_networkSession && m_networkProcess->privateClickMeasurementEnabled();
}

bool ClientImpl::debugModeEnabled() const
{
    return m_networkSession && m_networkSession->privateClickMeasurementDebugModeEnabled();
}

bool ClientImpl::usesEphemeralDataStore() const
{
    return m_networkSession && m_networkSession->sessionID().isEphemeral();
}

} // namespace WebKit::PCM
