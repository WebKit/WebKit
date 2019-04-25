/*
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteConnectionToTarget.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteAutomationTarget.h"
#include "RemoteInspectionTarget.h"
#include "RemoteInspector.h"

namespace Inspector {

RemoteConnectionToTarget::RemoteConnectionToTarget(RemoteControllableTarget& target)
    : m_target(&target)
{
}

RemoteConnectionToTarget::~RemoteConnectionToTarget()
{
}

bool RemoteConnectionToTarget::setup(bool isAutomaticInspection, bool automaticallyPause)
{
    LockHolder lock(m_targetMutex);
    if (!m_target)
        return false;

    auto targetIdentifier = this->targetIdentifier().valueOr(0);

    if (!m_target || !m_target->remoteControlAllowed()) {
        RemoteInspector::singleton().setupFailed(targetIdentifier);
        m_target = nullptr;
    } else if (is<RemoteInspectionTarget>(m_target)) {
        auto target = downcast<RemoteInspectionTarget>(m_target);
        target->connect(*this, isAutomaticInspection, automaticallyPause);
        m_connected = true;

        RemoteInspector::singleton().updateTargetListing(targetIdentifier);
    } else if (is<RemoteAutomationTarget>(m_target)) {
        auto target = downcast<RemoteAutomationTarget>(m_target);
        target->connect(*this);
        m_connected = true;

        RemoteInspector::singleton().updateTargetListing(targetIdentifier);
    }

    return true;
}

void RemoteConnectionToTarget::sendMessageToTarget(const String& message)
{
    RemoteControllableTarget* target = nullptr;
    {
        LockHolder lock(m_targetMutex);
        if (!m_target)
            return;
        target = m_target;
    }

    target->dispatchMessageFromRemote(message);
}

void RemoteConnectionToTarget::close()
{
    LockHolder lock(m_targetMutex);
    if (!m_target)
        return;

    auto targetIdentifier = m_target->targetIdentifier();

    if (m_connected)
        m_target->disconnect(*this);

    m_target = nullptr;

    RemoteInspector::singleton().updateTargetListing(targetIdentifier);
}

void RemoteConnectionToTarget::targetClosed()
{
    LockHolder lock(m_targetMutex);
    m_target = nullptr;
}

Optional<TargetID> RemoteConnectionToTarget::targetIdentifier() const
{
    return m_target ? Optional<TargetID>(m_target->targetIdentifier()) : WTF::nullopt;
}

void RemoteConnectionToTarget::sendMessageToFrontend(const String& message)
{
    if (!m_target)
        return;

    RemoteInspector::singleton().sendMessageToRemote(m_target->targetIdentifier(), message);
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
