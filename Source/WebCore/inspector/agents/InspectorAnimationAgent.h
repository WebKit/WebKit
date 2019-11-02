/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ComputedEffectTiming.h"
#include "InspectorWebAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/Forward.h>

namespace WebCore {

class DeclarativeAnimation;
class Element;
class Event;
class KeyframeEffect;
class WebAnimation;

typedef String ErrorString;

class InspectorAnimationAgent final : public InspectorAgentBase, public Inspector::AnimationBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorAnimationAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorAnimationAgent(PageAgentContext&);
    ~InspectorAnimationAgent() override;

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // AnimationBackendDispatcherHandler
    void startTracking(ErrorString&) override;
    void stopTracking(ErrorString&) override;

    // InspectorInstrumentation
    void willApplyKeyframeEffect(Element&, KeyframeEffect&, ComputedEffectTiming);
    void didChangeWebAnimationEffect(WebAnimation&);
    void willDestroyWebAnimation(WebAnimation&);

private:
    void stopTrackingDeclarativeAnimation(DeclarativeAnimation&);

    std::unique_ptr<Inspector::AnimationFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::AnimationBackendDispatcher> m_backendDispatcher;

    struct TrackedDeclarativeAnimationData {
        String trackingAnimationId;
        ComputedEffectTiming lastComputedTiming;
    };
    HashMap<DeclarativeAnimation*, TrackedDeclarativeAnimationData> m_trackedDeclarativeAnimationData;
};

} // namespace WebCore
