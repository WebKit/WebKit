/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "InspectorAnimationAgent.h"

#include "AnimationEffect.h"
#include "AnimationEffectPhase.h"
#include "CSSAnimation.h"
#include "CSSTransition.h"
#include "DeclarativeAnimation.h"
#include "Element.h"
#include "Event.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "KeyframeEffect.h"
#include "WebAnimation.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InspectorEnvironment.h>
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>
#include <wtf/Stopwatch.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

InspectorAnimationAgent::InspectorAnimationAgent(PageAgentContext& context)
    : InspectorAgentBase("Animation"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::AnimationFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::AnimationBackendDispatcher::create(context.backendDispatcher, this))
{
}

InspectorAnimationAgent::~InspectorAnimationAgent() = default;

void InspectorAnimationAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
    ASSERT(m_instrumentingAgents.persistentInspectorAnimationAgent() != this);
    m_instrumentingAgents.setPersistentInspectorAnimationAgent(this);
}

void InspectorAnimationAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    ErrorString ignored;
    stopTracking(ignored);

    ASSERT(m_instrumentingAgents.persistentInspectorAnimationAgent() == this);
    m_instrumentingAgents.setPersistentInspectorAnimationAgent(nullptr);
}

void InspectorAnimationAgent::startTracking(ErrorString& errorString)
{
    if (m_instrumentingAgents.trackingInspectorAnimationAgent() == this) {
        errorString = "Animation domain already tracking"_s;
        return;
    }

    m_instrumentingAgents.setTrackingInspectorAnimationAgent(this);

    ASSERT(m_trackedDeclarativeAnimationData.isEmpty());

    m_frontendDispatcher->trackingStart(m_environment.executionStopwatch()->elapsedTime().seconds());
}

void InspectorAnimationAgent::stopTracking(ErrorString&)
{
    if (m_instrumentingAgents.trackingInspectorAnimationAgent() != this)
        return;

    m_instrumentingAgents.setTrackingInspectorAnimationAgent(nullptr);

    m_trackedDeclarativeAnimationData.clear();

    m_frontendDispatcher->trackingComplete(m_environment.executionStopwatch()->elapsedTime().seconds());
}

static bool isDelayed(ComputedEffectTiming& computedTiming)
{
    if (!computedTiming.localTime)
        return false;
    return computedTiming.localTime.value() < (computedTiming.endTime - computedTiming.activeDuration);
}

void InspectorAnimationAgent::willApplyKeyframeEffect(Element& target, KeyframeEffect& keyframeEffect, ComputedEffectTiming computedTiming)
{
    auto* animation = keyframeEffect.animation();
    if (!is<DeclarativeAnimation>(animation))
        return;

    auto ensureResult = m_trackedDeclarativeAnimationData.ensure(downcast<DeclarativeAnimation>(animation), [&] () -> TrackedDeclarativeAnimationData {
        return { makeString("animation:"_s, IdentifiersFactory::createIdentifier()), computedTiming };
    });
    auto& trackingData = ensureResult.iterator->value;

    Optional<Inspector::Protocol::Animation::AnimationState> animationAnimationState;

    if ((ensureResult.isNewEntry || !isDelayed(trackingData.lastComputedTiming)) && isDelayed(computedTiming))
        animationAnimationState = Inspector::Protocol::Animation::AnimationState::Delayed;
    else if (ensureResult.isNewEntry || trackingData.lastComputedTiming.phase != computedTiming.phase) {
        switch (computedTiming.phase) {
        case AnimationEffectPhase::Before:
            animationAnimationState = Inspector::Protocol::Animation::AnimationState::Ready;
            break;

        case AnimationEffectPhase::Active:
            animationAnimationState = Inspector::Protocol::Animation::AnimationState::Active;
            break;

        case AnimationEffectPhase::After:
            animationAnimationState = Inspector::Protocol::Animation::AnimationState::Done;
            break;

        case AnimationEffectPhase::Idle:
            animationAnimationState = Inspector::Protocol::Animation::AnimationState::Canceled;
            break;
        }
    } else if (trackingData.lastComputedTiming.currentIteration != computedTiming.currentIteration) {
        // Iterations are represented by sequential "active" state events.
        animationAnimationState = Inspector::Protocol::Animation::AnimationState::Active;
    }

    trackingData.lastComputedTiming = computedTiming;

    if (!animationAnimationState)
        return;

    auto event = Inspector::Protocol::Animation::TrackingUpdate::create()
        .setTrackingAnimationId(trackingData.trackingAnimationId)
        .setAnimationState(animationAnimationState.value())
        .release();

    if (ensureResult.isNewEntry) {
        if (auto* domAgent = m_instrumentingAgents.inspectorDOMAgent()) {
            if (auto nodeId = domAgent->pushNodeToFrontend(&target))
                event->setNodeId(nodeId);
        }

        if (is<CSSAnimation>(animation))
            event->setAnimationName(downcast<CSSAnimation>(*animation).animationName());
        else if (is<CSSTransition>(animation))
            event->setTransitionProperty(downcast<CSSTransition>(*animation).transitionProperty());
        else
            ASSERT_NOT_REACHED();
    }

    m_frontendDispatcher->trackingUpdate(m_environment.executionStopwatch()->elapsedTime().seconds(), WTFMove(event));
}

void InspectorAnimationAgent::didChangeWebAnimationEffect(WebAnimation& animation)
{
    if (is<DeclarativeAnimation>(animation))
        stopTrackingDeclarativeAnimation(downcast<DeclarativeAnimation>(animation));
}

void InspectorAnimationAgent::willDestroyWebAnimation(WebAnimation& animation)
{
    if (is<DeclarativeAnimation>(animation))
        stopTrackingDeclarativeAnimation(downcast<DeclarativeAnimation>(animation));
}

void InspectorAnimationAgent::stopTrackingDeclarativeAnimation(DeclarativeAnimation& animation)
{
    auto it = m_trackedDeclarativeAnimationData.find(&animation);
    if (it == m_trackedDeclarativeAnimationData.end())
        return;

    if (it->value.lastComputedTiming.phase != AnimationEffectPhase::After && it->value.lastComputedTiming.phase != AnimationEffectPhase::Idle) {
        auto event = Inspector::Protocol::Animation::TrackingUpdate::create()
            .setTrackingAnimationId(it->value.trackingAnimationId)
            .setAnimationState(Inspector::Protocol::Animation::AnimationState::Canceled)
            .release();
        m_frontendDispatcher->trackingUpdate(m_environment.executionStopwatch()->elapsedTime().seconds(), WTFMove(event));
    }

    m_trackedDeclarativeAnimationData.remove(it);
}

} // namespace WebCore
