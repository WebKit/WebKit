/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

namespace Inspector {

InspectorTargetAgent::InspectorTargetAgent(FrontendRouter& frontendRouter, BackendDispatcher& backendDispatcher)
    : InspectorAgentBase("Target"_s)
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
}

void InspectorTargetAgent::exists(ErrorString&)
{
    // Intentionally do nothing to return success.
    // FIXME: Remove this when the local inspector has switched over to the modern path.
}

void InspectorTargetAgent::sendMessageToTarget(ErrorString& errorString, const String& targetId, const String& message)
{
    InspectorTarget* target = m_targets.get(targetId);
    if (!target) {
        errorString = "Missing target for given targetId"_s;
        return;
    }

    target->sendMessageToTargetBackend(message);
}

void InspectorTargetAgent::sendMessageFromTargetToFrontend(const String& targetId, const String& message)
{
    ASSERT_WITH_MESSAGE(m_targets.get(targetId), "Sending a message from an untracked target to the frontend.");

    m_frontendDispatcher->dispatchMessageFromTarget(targetId, message);
}

static Protocol::Target::TargetInfo::Type targetTypeToProtocolType(InspectorTargetType type)
{
    switch (type) {
    case InspectorTargetType::JavaScriptContext:
        return Protocol::Target::TargetInfo::Type::JavaScript;
    case InspectorTargetType::Page:
        return Protocol::Target::TargetInfo::Type::Page;
    case InspectorTargetType::DedicatedWorker:
        return Protocol::Target::TargetInfo::Type::Worker;
    case InspectorTargetType::ServiceWorker:
        return Protocol::Target::TargetInfo::Type::ServiceWorker;
    }

    ASSERT_NOT_REACHED();
    return Protocol::Target::TargetInfo::Type::JavaScript;
}

static Ref<Protocol::Target::TargetInfo> buildTargetInfoObject(const InspectorTarget& target)
{
    return Protocol::Target::TargetInfo::create()
        .setTargetId(target.identifier())
        .setType(targetTypeToProtocolType(target.type()))
        .release();
}

void InspectorTargetAgent::targetCreated(InspectorTarget& target)
{
    auto addResult = m_targets.set(target.identifier(), &target);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    if (!m_isConnected)
        return;

    target.connect(frontendChannel());

    m_frontendDispatcher->targetCreated(buildTargetInfoObject(target));
}

void InspectorTargetAgent::targetDestroyed(InspectorTarget& target)
{
    m_targets.remove(target.identifier());

    if (!m_isConnected)
        return;

    target.disconnect(frontendChannel());

    m_frontendDispatcher->targetDestroyed(target.identifier());
}

void InspectorTargetAgent::connectToTargets()
{
    auto& channel = frontendChannel();

    for (InspectorTarget* target : m_targets.values()) {
        target->connect(channel);
        m_frontendDispatcher->targetCreated(buildTargetInfoObject(*target));
    }
}

void InspectorTargetAgent::disconnectFromTargets()
{
    auto& channel = frontendChannel();

    for (InspectorTarget* target : m_targets.values())
        target->disconnect(channel);
}

} // namespace Inspector
