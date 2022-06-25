/*
 * Copyright (C) 2007-2010, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorAgent.h"

#include "InspectorEnvironment.h"
#include <wtf/JSONValues.h>

namespace Inspector {

InspectorAgent::InspectorAgent(AgentContext& context)
    : InspectorAgentBase("Inspector"_s)
    , m_environment(context.environment)
    , m_frontendDispatcher(makeUnique<InspectorFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(InspectorBackendDispatcher::create(context.backendDispatcher, this))
{
}

InspectorAgent::~InspectorAgent() = default;

void InspectorAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    m_pendingEvaluateTestCommands.clear();

    disable();
}

Protocol::ErrorStringOr<void> InspectorAgent::enable()
{
    m_enabled = true;

    if (m_pendingInspectData.first)
        inspect(m_pendingInspectData.first.releaseNonNull(), m_pendingInspectData.second.releaseNonNull());

    for (auto& testCommand : m_pendingEvaluateTestCommands)
        m_frontendDispatcher->evaluateForTestInFrontend(testCommand);

    m_pendingEvaluateTestCommands.clear();

    return { };
}

Protocol::ErrorStringOr<void> InspectorAgent::disable()
{
    m_enabled = false;

    return { };
}

Protocol::ErrorStringOr<void> InspectorAgent::initialized()
{
    m_environment.frontendInitialized();

    return { };
}

void InspectorAgent::inspect(Ref<Protocol::Runtime::RemoteObject>&& object, Ref<JSON::Object>&& hints)
{
    if (m_enabled) {
        m_frontendDispatcher->inspect(WTFMove(object), WTFMove(hints));
        m_pendingInspectData.first = nullptr;
        m_pendingInspectData.second = nullptr;
        return;
    }

    m_pendingInspectData.first = WTFMove(object);
    m_pendingInspectData.second = WTFMove(hints);
}

void InspectorAgent::evaluateForTestInFrontend(const String& script)
{
    if (m_enabled)
        m_frontendDispatcher->evaluateForTestInFrontend(script);
    else
        m_pendingEvaluateTestCommands.append(script);
}

} // namespace Inspector
