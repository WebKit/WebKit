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
#include <wtf/RunLoop.h>

namespace Inspector {

RemoteConnectionToTarget::RemoteConnectionToTarget(RemoteControllableTarget& target)
    : m_target(&target)
{
}

RemoteConnectionToTarget::~RemoteConnectionToTarget() = default;

bool RemoteConnectionToTarget::setup(bool isAutomaticInspection, bool automaticallyPause)
{
    RefPtr<RemoteControllableTarget> target;
    TargetID targetIdentifier;

    {
        Locker locker { m_targetMutex };
        target = m_target.get();
        if (!target)
            return false;
        targetIdentifier = this->targetIdentifier().value_or(0);
    }

    if (!target->remoteControlAllowed()) {
        RemoteInspector::singleton().setupFailed(targetIdentifier);
        Locker locker { m_targetMutex };
        m_target = nullptr;
    } else if (auto* inspectionTarget = dynamicDowncast<RemoteInspectionTarget>(*target)) {
        inspectionTarget->connect(*this, isAutomaticInspection, automaticallyPause);
        m_connected = true;

        RemoteInspector::singleton().updateTargetListing(targetIdentifier);
    } else if (auto* automationTarget = dynamicDowncast<RemoteAutomationTarget>(*target)) {
        automationTarget->connect(*this);
        m_connected = true;

        RemoteInspector::singleton().updateTargetListing(targetIdentifier);
    }

    return true;
}

void RemoteConnectionToTarget::sendMessageToTarget(String&& message)
{
    RefPtr<RemoteControllableTarget> target;
    {
        Locker locker { m_targetMutex };
        target = m_target.get();
    }
    if (target)
        target->dispatchMessageFromRemote(WTFMove(message));
}

void RemoteConnectionToTarget::close()
{
    RunLoop::current().dispatch([this, protectThis = Ref { *this }] {
        Locker locker { m_targetMutex };
        RefPtr target = m_target.get();
        if (!target)
            return;

        auto targetIdentifier = target->targetIdentifier();

        if (m_connected)
            target->disconnect(*this);

        m_target = nullptr;

        RemoteInspector::singleton().updateTargetListing(targetIdentifier);
    });
}

void RemoteConnectionToTarget::targetClosed()
{
    Locker locker { m_targetMutex };
    m_target = nullptr;
}

std::optional<TargetID> RemoteConnectionToTarget::targetIdentifier() const
{
    RefPtr target = m_target.get();
    return target ? std::optional<TargetID>(target->targetIdentifier()) : std::nullopt;
}

void RemoteConnectionToTarget::sendMessageToFrontend(const String& message)
{
    std::optional<TargetID> targetIdentifier;
    {
        Locker locker { m_targetMutex };
        RefPtr target = m_target.get();
        if (!target)
            return;
        targetIdentifier = target->targetIdentifier();
    }
    RemoteInspector::singleton().sendMessageToRemote(*targetIdentifier, message);
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
