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
#include "CSSPropertyNames.h"
#include "CSSTransition.h"
#include "CSSValue.h"
#include "ComputedStyleExtractor.h"
#include "DeclarativeAnimation.h"
#include "Element.h"
#include "Event.h"
#include "FillMode.h"
#include "Frame.h"
#include "InspectorCSSAgent.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "JSWebAnimation.h"
#include "KeyframeEffect.h"
#include "KeyframeList.h"
#include "Page.h"
#include "PlaybackDirection.h"
#include "RenderElement.h"
#include "Styleable.h"
#include "TimingFunction.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorEnvironment.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>
#include <wtf/Stopwatch.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

static std::optional<double> protocolValueForSeconds(const Seconds& seconds)
{
    if (seconds == Seconds::infinity() || seconds == Seconds::nan())
        return std::nullopt;
    return seconds.milliseconds();
}

static std::optional<Protocol::Animation::PlaybackDirection> protocolValueForPlaybackDirection(PlaybackDirection playbackDirection)
{
    switch (playbackDirection) {
    case PlaybackDirection::Normal:
        return Protocol::Animation::PlaybackDirection::Normal;
    case PlaybackDirection::Reverse:
        return Protocol::Animation::PlaybackDirection::Reverse;
    case PlaybackDirection::Alternate:
        return Protocol::Animation::PlaybackDirection::Alternate;
    case PlaybackDirection::AlternateReverse:
        return Protocol::Animation::PlaybackDirection::AlternateReverse;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static std::optional<Protocol::Animation::FillMode> protocolValueForFillMode(FillMode fillMode)
{
    switch (fillMode) {
    case FillMode::None:
        return Protocol::Animation::FillMode::None;
    case FillMode::Forwards:
        return Protocol::Animation::FillMode::Forwards;
    case FillMode::Backwards:
        return Protocol::Animation::FillMode::Backwards;
    case FillMode::Both:
        return Protocol::Animation::FillMode::Both;
    case FillMode::Auto:
        return Protocol::Animation::FillMode::Auto;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static Ref<JSON::ArrayOf<Protocol::Animation::Keyframe>> buildObjectForKeyframes(KeyframeEffect& keyframeEffect)
{
    auto keyframesPayload = JSON::ArrayOf<Protocol::Animation::Keyframe>::create();

    const auto& blendingKeyframes = keyframeEffect.blendingKeyframes();
    const auto& parsedKeyframes = keyframeEffect.parsedKeyframes();

    if (is<DeclarativeAnimation>(keyframeEffect.animation())) {
        auto& declarativeAnimation = downcast<DeclarativeAnimation>(*keyframeEffect.animation());

        auto* target = keyframeEffect.target();
        auto* renderer = keyframeEffect.renderer();

        // Synthesize CSS style declarations for each keyframe so the frontend can display them.
        ComputedStyleExtractor computedStyleExtractor(target, false, target->pseudoId());

        for (size_t i = 0; i < blendingKeyframes.size(); ++i) {
            auto& blendingKeyframe = blendingKeyframes[i];

            ASSERT(blendingKeyframe.style());
            auto& style = *blendingKeyframe.style();

            auto keyframePayload = Protocol::Animation::Keyframe::create()
                .setOffset(blendingKeyframe.key())
                .release();

            RefPtr<TimingFunction> timingFunction;
            if (!parsedKeyframes.isEmpty())
                timingFunction = parsedKeyframes[i].timingFunction;
            if (!timingFunction)
                timingFunction = blendingKeyframe.timingFunction();
            if (!timingFunction)
                timingFunction = declarativeAnimation.backingAnimation().timingFunction();
            if (timingFunction)
                keyframePayload->setEasing(timingFunction->cssText());

            StringBuilder stylePayloadBuilder;
            auto& properties = blendingKeyframe.properties();
            size_t count = properties.size();
            for (auto property : properties) {
                --count;
                WTF::switchOn(property,
                    [&] (CSSPropertyID cssPropertyId) {
                        stylePayloadBuilder.append(nameString(cssPropertyId), ": ");
                        if (auto value = computedStyleExtractor.valueForPropertyInStyle(style, cssPropertyId, renderer))
                            stylePayloadBuilder.append(value->cssText());
                    },
                    [&] (const AtomString& customProperty) {
                        stylePayloadBuilder.append(customProperty, ": ");
                        if (auto value = computedStyleExtractor.customPropertyValue(customProperty))
                            stylePayloadBuilder.append(value->cssText());
                    }
                );
                stylePayloadBuilder.append(';');
                if (count > 0)
                    stylePayloadBuilder.append(' ');
            }
            if (!stylePayloadBuilder.isEmpty())
                keyframePayload->setStyle(stylePayloadBuilder.toString());

            keyframesPayload->addItem(WTFMove(keyframePayload));
        }
    } else {
        for (const auto& parsedKeyframe : parsedKeyframes) {
            auto keyframePayload = Protocol::Animation::Keyframe::create()
                .setOffset(parsedKeyframe.computedOffset)
                .release();

            if (!parsedKeyframe.easing.isEmpty())
                keyframePayload->setEasing(parsedKeyframe.easing);
            else if (const auto& timingFunction = parsedKeyframe.timingFunction)
                keyframePayload->setEasing(timingFunction->cssText());

            if (!parsedKeyframe.style->isEmpty())
                keyframePayload->setStyle(parsedKeyframe.style->asText());

            keyframesPayload->addItem(WTFMove(keyframePayload));
        }
    }

    return keyframesPayload;
}

static Ref<Protocol::Animation::Effect> buildObjectForEffect(AnimationEffect& effect)
{
    auto effectPayload = Protocol::Animation::Effect::create()
        .release();

    if (auto startDelay = protocolValueForSeconds(effect.delay()))
        effectPayload->setStartDelay(startDelay.value());

    if (auto endDelay = protocolValueForSeconds(effect.endDelay()))
        effectPayload->setEndDelay(endDelay.value());

    effectPayload->setIterationCount(effect.iterations() == std::numeric_limits<double>::infinity() ? -1 : effect.iterations());
    effectPayload->setIterationStart(effect.iterationStart());

    if (auto iterationDuration = protocolValueForSeconds(effect.iterationDuration()))
        effectPayload->setIterationDuration(iterationDuration.value());

    if (auto* timingFunction = effect.timingFunction())
        effectPayload->setTimingFunction(timingFunction->cssText());

    if (auto playbackDirection = protocolValueForPlaybackDirection(effect.direction()))
        effectPayload->setPlaybackDirection(playbackDirection.value());

    if (auto fillMode = protocolValueForFillMode(effect.fill()))
        effectPayload->setFillMode(fillMode.value());

    if (is<KeyframeEffect>(effect))
        effectPayload->setKeyframes(buildObjectForKeyframes(downcast<KeyframeEffect>(effect)));

    return effectPayload;
}

InspectorAnimationAgent::InspectorAnimationAgent(PageAgentContext& context)
    : InspectorAgentBase("Animation"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::AnimationFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::AnimationBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_inspectedPage(context.inspectedPage)
    , m_animationDestroyedTimer(*this, &InspectorAnimationAgent::animationDestroyedTimerFired)
{
}

InspectorAnimationAgent::~InspectorAnimationAgent() = default;

void InspectorAnimationAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
    ASSERT(m_instrumentingAgents.persistentAnimationAgent() != this);
    m_instrumentingAgents.setPersistentAnimationAgent(this);
}

void InspectorAnimationAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    stopTracking();
    disable();

    ASSERT(m_instrumentingAgents.persistentAnimationAgent() == this);
    m_instrumentingAgents.setPersistentAnimationAgent(nullptr);
}

Protocol::ErrorStringOr<void> InspectorAnimationAgent::enable()
{
    if (m_instrumentingAgents.enabledAnimationAgent() == this)
        return makeUnexpected("Animation domain already enabled"_s);

    m_instrumentingAgents.setEnabledAnimationAgent(this);

    const auto existsInCurrentPage = [&] (ScriptExecutionContext* scriptExecutionContext) {
        if (!is<Document>(scriptExecutionContext))
            return false;

        // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
        auto* document = downcast<Document>(scriptExecutionContext);
        return document->page() == &m_inspectedPage;
    };

    {
        for (auto* animation : WebAnimation::instances()) {
            if (existsInCurrentPage(animation->scriptExecutionContext()))
                bindAnimation(*animation, false);
        }
    }

    return { };
}

Protocol::ErrorStringOr<void> InspectorAnimationAgent::disable()
{
    m_instrumentingAgents.setEnabledAnimationAgent(nullptr);

    reset();

    return { };
}

Protocol::ErrorStringOr<Ref<Protocol::DOM::Styleable>> InspectorAnimationAgent::requestEffectTarget(const Protocol::Animation::AnimationId& animationId)
{
    Protocol::ErrorString errorString;

    auto* animation = assertAnimation(errorString, animationId);
    if (!animation)
        return makeUnexpected(errorString);

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto* effect = animation->effect();
    if (!is<KeyframeEffect>(effect))
        return makeUnexpected("Animation for given animationId does not have an effect"_s);

    auto& keyframeEffect = downcast<KeyframeEffect>(*effect);

    auto target = keyframeEffect.targetStyleable();
    if (!target)
        return makeUnexpected("Animation for given animationId does not have a target"_s);

    return domAgent->pushStyleablePathToFrontend(errorString, *target);
}

Protocol::ErrorStringOr<Ref<Protocol::Runtime::RemoteObject>> InspectorAnimationAgent::resolveAnimation(const Protocol::Animation::AnimationId& animationId, const String& objectGroup)
{
    Protocol::ErrorString errorString;

    auto* animation = assertAnimation(errorString, animationId);
    if (!animation)
        return makeUnexpected(errorString);

    auto* state = animation->scriptExecutionContext()->globalObject();
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(state);
    ASSERT(!injectedScript.hasNoValue());

    JSC::JSValue value;
    {
        JSC::JSLockHolder lock(state);

        auto* globalObject = deprecatedGlobalObjectForPrototype(state);
        value = toJS(state, globalObject, animation);
    }

    if (!value) {
        ASSERT_NOT_REACHED();
        return makeUnexpected("Internal error: unknown Animation for given animationId"_s);
    }

    auto object = injectedScript.wrapObject(value, objectGroup);
    if (!object)
        return makeUnexpected("Internal error: unable to cast Animation"_s);

    return object.releaseNonNull();
}

Protocol::ErrorStringOr<void> InspectorAnimationAgent::startTracking()
{
    if (m_instrumentingAgents.trackingAnimationAgent() == this)
        return { };

    m_instrumentingAgents.setTrackingAnimationAgent(this);

    ASSERT(m_trackedDeclarativeAnimationData.isEmpty());

    m_frontendDispatcher->trackingStart(m_environment.executionStopwatch().elapsedTime().seconds());

    return { };
}

Protocol::ErrorStringOr<void> InspectorAnimationAgent::stopTracking()
{
    if (m_instrumentingAgents.trackingAnimationAgent() != this)
        return { };

    m_instrumentingAgents.setTrackingAnimationAgent(nullptr);

    m_trackedDeclarativeAnimationData.clear();

    m_frontendDispatcher->trackingComplete(m_environment.executionStopwatch().elapsedTime().seconds());

    return { };
}

static bool isDelayed(ComputedEffectTiming& computedTiming)
{
    if (!computedTiming.localTime)
        return false;
    return computedTiming.localTime.value() < (computedTiming.endTime - computedTiming.activeDuration);
}

void InspectorAnimationAgent::willApplyKeyframeEffect(const Styleable& target, KeyframeEffect& keyframeEffect, ComputedEffectTiming computedTiming)
{
    auto* animation = keyframeEffect.animation();
    if (!is<DeclarativeAnimation>(animation))
        return;

    auto ensureResult = m_trackedDeclarativeAnimationData.ensure(downcast<DeclarativeAnimation>(animation), [&] () -> UniqueRef<TrackedDeclarativeAnimationData> {
        return makeUniqueRef<TrackedDeclarativeAnimationData>(TrackedDeclarativeAnimationData { makeString("animation:"_s, IdentifiersFactory::createIdentifier()), computedTiming });
    });
    auto& trackingData = ensureResult.iterator->value.get();

    std::optional<Protocol::Animation::AnimationState> animationAnimationState;

    if ((ensureResult.isNewEntry || !isDelayed(trackingData.lastComputedTiming)) && isDelayed(computedTiming))
        animationAnimationState = Protocol::Animation::AnimationState::Delayed;
    else if (ensureResult.isNewEntry || trackingData.lastComputedTiming.phase != computedTiming.phase) {
        switch (computedTiming.phase) {
        case AnimationEffectPhase::Before:
            animationAnimationState = Protocol::Animation::AnimationState::Ready;
            break;

        case AnimationEffectPhase::Active:
            animationAnimationState = Protocol::Animation::AnimationState::Active;
            break;

        case AnimationEffectPhase::After:
            animationAnimationState = Protocol::Animation::AnimationState::Done;
            break;

        case AnimationEffectPhase::Idle:
            animationAnimationState = Protocol::Animation::AnimationState::Canceled;
            break;
        }
    } else if (trackingData.lastComputedTiming.currentIteration != computedTiming.currentIteration) {
        // Iterations are represented by sequential "active" state events.
        animationAnimationState = Protocol::Animation::AnimationState::Active;
    }

    trackingData.lastComputedTiming = computedTiming;

    if (!animationAnimationState)
        return;

    auto event = Protocol::Animation::TrackingUpdate::create()
        .setTrackingAnimationId(trackingData.trackingAnimationId)
        .setAnimationState(animationAnimationState.value())
        .release();

    if (ensureResult.isNewEntry) {
        if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent()) {
            if (auto nodeId = domAgent->pushStyleableElementToFrontend(target))
                event->setNodeId(nodeId);
        }

        if (is<CSSAnimation>(animation))
            event->setAnimationName(downcast<CSSAnimation>(*animation).animationName());
        else if (is<CSSTransition>(animation))
            event->setTransitionProperty(downcast<CSSTransition>(*animation).transitionProperty());
        else
            ASSERT_NOT_REACHED();
    }

    m_frontendDispatcher->trackingUpdate(m_environment.executionStopwatch().elapsedTime().seconds(), WTFMove(event));
}

void InspectorAnimationAgent::didChangeWebAnimationName(WebAnimation& animation)
{
    // The `animationId` may be empty if Animation is tracking but not enabled.
    auto animationId = findAnimationId(animation);
    if (animationId.isEmpty())
        return;

    m_frontendDispatcher->nameChanged(animationId, animation.id());
}

void InspectorAnimationAgent::didSetWebAnimationEffect(WebAnimation& animation)
{
    if (is<DeclarativeAnimation>(animation))
        stopTrackingDeclarativeAnimation(downcast<DeclarativeAnimation>(animation));

    didChangeWebAnimationEffectTiming(animation);
    didChangeWebAnimationEffectTarget(animation);
}

void InspectorAnimationAgent::didChangeWebAnimationEffectTiming(WebAnimation& animation)
{
    // The `animationId` may be empty if Animation is tracking but not enabled.
    auto animationId = findAnimationId(animation);
    if (animationId.isEmpty())
        return;

    if (auto* effect = animation.effect())
        m_frontendDispatcher->effectChanged(animationId, buildObjectForEffect(*effect));
    else
        m_frontendDispatcher->effectChanged(animationId, nullptr);
}

void InspectorAnimationAgent::didChangeWebAnimationEffectTarget(WebAnimation& animation)
{
    // The `animationId` may be empty if Animation is tracking but not enabled.
    auto animationId = findAnimationId(animation);
    if (animationId.isEmpty())
        return;

    m_frontendDispatcher->targetChanged(animationId);
}

void InspectorAnimationAgent::didCreateWebAnimation(WebAnimation& animation)
{
    if (!findAnimationId(animation).isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

    bindAnimation(animation, true);
}

void InspectorAnimationAgent::willDestroyWebAnimation(WebAnimation& animation)
{
    if (is<DeclarativeAnimation>(animation))
        stopTrackingDeclarativeAnimation(downcast<DeclarativeAnimation>(animation));

    // The `animationId` may be empty if Animation is tracking but not enabled.
    auto animationId = findAnimationId(animation);
    if (!animationId.isEmpty())
        unbindAnimation(animationId);
}

void InspectorAnimationAgent::frameNavigated(Frame& frame)
{
    if (frame.isMainFrame()) {
        reset();
        return;
    }

    Vector<String> animationIdsToRemove;
    for (auto& [animationId, animation] : m_animationIdMap) {
        if (auto* scriptExecutionContext = animation->scriptExecutionContext()) {
            if (is<Document>(scriptExecutionContext) && downcast<Document>(*scriptExecutionContext).frame() == &frame)
                animationIdsToRemove.append(animationId);
        }
    }
    for (const auto& animationId : animationIdsToRemove)
        unbindAnimation(animationId);
}

String InspectorAnimationAgent::findAnimationId(WebAnimation& animation)
{
    for (auto& [animationId, existingAnimation] : m_animationIdMap) {
        if (existingAnimation == &animation)
            return animationId;
    }
    return nullString();
}

WebAnimation* InspectorAnimationAgent::assertAnimation(Protocol::ErrorString& errorString, const String& animationId)
{
    auto* animation = m_animationIdMap.get(animationId);
    if (!animation)
        errorString = "Missing animation for given animationId"_s;
    return animation;
}

void InspectorAnimationAgent::bindAnimation(WebAnimation& animation, bool captureBacktrace)
{
    auto animationId = makeString("animation:" + IdentifiersFactory::createIdentifier());
    m_animationIdMap.set(animationId, &animation);

    auto animationPayload = Protocol::Animation::Animation::create()
        .setAnimationId(animationId)
        .release();

    auto name = animation.id();
    if (!name.isEmpty())
        animationPayload->setName(name);

    if (is<CSSAnimation>(animation))
        animationPayload->setCssAnimationName(downcast<CSSAnimation>(animation).animationName());
    else if (is<CSSTransition>(animation))
        animationPayload->setCssTransitionProperty(downcast<CSSTransition>(animation).transitionProperty());

    if (auto* effect = animation.effect())
        animationPayload->setEffect(buildObjectForEffect(*effect));

    if (captureBacktrace) {
        auto stackTrace = Inspector::createScriptCallStack(JSExecState::currentState());
        animationPayload->setStackTrace(stackTrace->buildInspectorObject());
    }

    m_frontendDispatcher->animationCreated(WTFMove(animationPayload));
}

void InspectorAnimationAgent::unbindAnimation(const String& animationId)
{
    m_animationIdMap.remove(animationId);

    // This can be called in response to GC. Due to the single-process model used in WebKit1, the
    // event must be dispatched from a timer to prevent the frontend from making JS allocations
    // while the GC is still active.
    m_removedAnimationIds.append(animationId);

    if (!m_animationDestroyedTimer.isActive())
        m_animationDestroyedTimer.startOneShot(0_s);
}

void InspectorAnimationAgent::animationDestroyedTimerFired()
{
    if (!m_removedAnimationIds.size())
        return;

    for (auto& identifier : m_removedAnimationIds)
        m_frontendDispatcher->animationDestroyed(identifier);

    m_removedAnimationIds.clear();
}

void InspectorAnimationAgent::reset()
{
    m_animationIdMap.clear();

    m_removedAnimationIds.clear();

    if (m_animationDestroyedTimer.isActive())
        m_animationDestroyedTimer.stop();
}

void InspectorAnimationAgent::stopTrackingDeclarativeAnimation(DeclarativeAnimation& animation)
{
    auto data = m_trackedDeclarativeAnimationData.take(&animation);
    if (!data)
        return;

    if (data->lastComputedTiming.phase != AnimationEffectPhase::After && data->lastComputedTiming.phase != AnimationEffectPhase::Idle) {
        auto event = Protocol::Animation::TrackingUpdate::create()
            .setTrackingAnimationId(data->trackingAnimationId)
            .setAnimationState(Protocol::Animation::AnimationState::Canceled)
            .release();
        m_frontendDispatcher->trackingUpdate(m_environment.executionStopwatch().elapsedTime().seconds(), WTFMove(event));
    }
}

} // namespace WebCore
