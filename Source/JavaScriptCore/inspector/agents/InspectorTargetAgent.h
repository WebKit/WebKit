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

#pragma once

#include "InspectorAgentBase.h"
#include "InspectorBackendDispatchers.h"
#include "InspectorFrontendDispatchers.h"
#include <wtf/Forward.h>

namespace Inspector {

class InspectorTarget;

typedef String ErrorString;

class JS_EXPORT_PRIVATE InspectorTargetAgent : public InspectorAgentBase, public TargetBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorTargetAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorTargetAgent(FrontendRouter&, BackendDispatcher&);
    virtual ~InspectorTargetAgent() = default;

    void didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*) final;
    void willDestroyFrontendAndBackend(DisconnectReason) final;

    virtual FrontendChannel& frontendChannel() = 0;

    // TargetBackendDispatcherHandler
    void exists(ErrorString&) final;
    void sendMessageToTarget(ErrorString&, const String& targetId, const String& message) final;

    // Target lifecycle.
    void targetCreated(InspectorTarget&);
    void targetDestroyed(InspectorTarget&);

    // Target messages.
    void sendMessageFromTargetToFrontend(const String& targetId, const String& message);

private:
    void connectToTargets();
    void disconnectFromTargets();

    std::unique_ptr<TargetFrontendDispatcher> m_frontendDispatcher;
    Ref<TargetBackendDispatcher> m_backendDispatcher;
    HashMap<String, InspectorTarget*> m_targets;
    bool m_isConnected { false };
};

} // namespace Inspector
