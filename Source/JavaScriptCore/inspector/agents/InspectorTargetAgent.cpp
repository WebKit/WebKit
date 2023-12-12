/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#include "InspectorTargetAgent.h"

#include "InspectorTarget.h"
#include <wtf/TZoneMallocInlines.h>

namespace Inspector {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorTargetAgent);

InspectorTargetAgent::InspectorTargetAgent(FrontendRouter& frontendRouter, BackendDispatcher& backendDispatcher)
    : InspectorAgentBase("Target"_s)
    , m_router(frontendRouter)
    , m_frontendDispatcher(makeUnique<TargetFrontendDispatcher>(frontendRouter))
    , m_backendDispatcher(TargetBackendDispatcher::create(backendDispatcher, this))
{
}

InspectorTargetAgent::~InspectorTargetAgent() = default;

void InspectorTargetAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
    m_isConnected = true;

    connectToTargets();
}

void InspectorTargetAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    disconnectFromTargets();

    m_isConnected = false;
    m_shouldPauseOnStart = false;
}

Protocol::ErrorStringOr<void> InspectorTargetAgent::setPauseOnStart(bool pauseOnStart)
{
    m_shouldPauseOnStart = pauseOnStart;

    return { };
}

Protocol::ErrorStringOr<void> InspectorTargetAgent::resume(const String& targetId)
{
    auto* target = m_targets.get(targetId);
    if (!target)
        return makeUnexpected("Missing target for given targetId"_s);

    if (!target->isPaused())
        return makeUnexpected("Target for given targetId is not paused"_s);

    target->resume();

    return { };
}

Protocol::ErrorStringOr<void> InspectorTargetAgent::sendMessageToTarget(const String& targetId, const String& message)
{
    InspectorTarget* target = m_targets.get(targetId);
    if (!target)
        return makeUnexpected("Missing target for given targetId"_s);

    target->sendMessageToTargetBackend(message);

    return { };
}

void InspectorTargetAgent::sendMessageFromTargetToFrontend(const String& targetId, const String& message)
{
    ASSERT_WITH_MESSAGE(m_targets.get(targetId), "Sending a message from an untracked target to the frontend.");

    m_frontendDispatcher->dispatchMessageFromTarget(targetId, message);
}

static Protocol::Target::TargetInfo::Type targetTypeToProtocolType(InspectorTargetType type)
{
    switch (type) {
    case InspectorTargetType::Page:
        return Protocol::Target::TargetInfo::Type::Page;
    case InspectorTargetType::DedicatedWorker:
        return Protocol::Target::TargetInfo::Type::Worker;
    case InspectorTargetType::ServiceWorker:
        return Protocol::Target::TargetInfo::Type::ServiceWorker;
    }

    ASSERT_NOT_REACHED();
    return Protocol::Target::TargetInfo::Type::Page;
}

static Ref<Protocol::Target::TargetInfo> buildTargetInfoObject(const InspectorTarget& target)
{
    auto result = Protocol::Target::TargetInfo::create()
        .setTargetId(target.identifier())
        .setType(targetTypeToProtocolType(target.type()))
        .release();
    if (target.isProvisional())
        result->setIsProvisional(true);
    if (target.isPaused())
        result->setIsPaused(true);
    return result;
}

void InspectorTargetAgent::targetCreated(InspectorTarget& target)
{
    auto addResult = m_targets.set(target.identifier(), &target);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    if (!m_isConnected)
        return;

    if (m_shouldPauseOnStart)
        target.pause();
    target.connect(connectionType());

    m_frontendDispatcher->targetCreated(buildTargetInfoObject(target));
}

void InspectorTargetAgent::targetDestroyed(InspectorTarget& target)
{
    m_targets.remove(target.identifier());

    if (!m_isConnected)
        return;

    m_frontendDispatcher->targetDestroyed(target.identifier());
}

void InspectorTargetAgent::didCommitProvisionalTarget(const String& oldTargetID, const String& committedTargetID)
{
    if (!m_isConnected)
        return;

    auto* target = m_targets.get(committedTargetID);
    if (!target)
        return;

    m_frontendDispatcher->didCommitProvisionalTarget(oldTargetID, committedTargetID);
}

FrontendChannel::ConnectionType InspectorTargetAgent::connectionType() const
{
    return m_router.hasLocalFrontend() ? Inspector::FrontendChannel::ConnectionType::Local : Inspector::FrontendChannel::ConnectionType::Remote;
}

void InspectorTargetAgent::connectToTargets()
{
    for (InspectorTarget* target : m_targets.values()) {
        target->connect(connectionType());
        m_frontendDispatcher->targetCreated(buildTargetInfoObject(*target));
    }
}

void InspectorTargetAgent::disconnectFromTargets()
{
    for (InspectorTarget* target : m_targets.values())
        target->disconnect();
}

} // namespace Inspector
