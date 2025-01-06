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
#include "BlendingKeyframes.h"
#include "CSSAnimation.h"
#include "CSSPropertyNames.h"
#include "CSSTransition.h"
#include "CSSValue.h"
#include "ComputedStyleExtractor.h"
#include "Element.h"
#include "Event.h"
#include "FillMode.h"
#include "InspectorCSSAgent.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "JSWebAnimation.h"
#include "KeyframeEffect.h"
#include "LocalFrame.h"
#include "MutableStyleProperties.h"
#include "Page.h"
#include "PlaybackDirection.h"
#include "RenderElement.h"
#include "StyleOriginatedAnimation.h"
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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorAnimationAgent);

static std::optional<double> protocolValueForSeconds(const Seconds& seconds)
{
    if (seconds == Seconds::infinity() || seconds == Seconds::nan())
        return std::nullopt;
    return seconds.milliseconds();
}

static std::optional<Inspector::Protocol::Animation::PlaybackDirection> protocolValueForPlaybackDirection(PlaybackDirection playbackDirection)
{
    switch (playbackDirection) {
    case PlaybackDirection::Normal:
        return Inspector::Protocol::Animation::PlaybackDirection::Normal;
    case PlaybackDirection::Reverse:
        return Inspector::Protocol::Animation::PlaybackDirection::Reverse;
    case PlaybackDirection::Alternate:
        return Inspector::Protocol::Animation::PlaybackDirection::Alternate;
    case PlaybackDirection::AlternateReverse:
        return Inspector::Protocol::Animation::PlaybackDirection::AlternateReverse;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static std::optional<Inspector::Protocol::Animation::FillMode> protocolValueForFillMode(FillMode fillMode)
{
    switch (fillMode) {
    case FillMode::None:
        return Inspector::Protocol::Animation::FillMode::None;
    case FillMode::Forwards:
        return Inspector::Protocol::Animation::FillMode::Forwards;
    case FillMode::Backwards:
        return Inspector::Protocol::Animation::FillMode::Backwards;
    case FillMode::Both:
        return Inspector::Protocol::Animation::FillMode::Both;
    case FillMode::Auto:
        return Inspector::Protocol::Animation::FillMode::Auto;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static Ref<JSON::ArrayOf<Inspector::Protocol::Animation::Keyframe>> buildObjectForKeyframes(KeyframeEffect& keyframeEffect)
{
    auto keyframesPayload = JSON::ArrayOf<Inspector::Protocol::Animation::Keyframe>::create();

    const auto& blendingKeyframes = keyframeEffect.blendingKeyframes();
    const auto& parsedKeyframes = keyframeEffect.parsedKeyframes();

    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(keyframeEffect.animation())) {
        auto* target = keyframeEffect.target();
        auto* renderer = keyframeEffect.renderer();

        // Synthesize CSS style declarations for each keyframe so the frontend can display them.

        auto pseudoElementIdentifier = target->pseudoId() == PseudoId::None ? std::nullopt : std::optional(Style::PseudoElementIdentifier { target->pseudoId() });
        ComputedStyleExtractor computedStyleExtractor(target, false, pseudoElementIdentifier);

        for (size_t i = 0; i < blendingKeyframes.size(); ++i) {
            auto& blendingKeyframe = blendingKeyframes[i];

            ASSERT(blendingKeyframe.style());
            auto& style = *blendingKeyframe.style();

            auto keyframePayload = Inspector::Protocol::Animation::Keyframe::create()
                .setOffset(blendingKeyframe.offset())
                .release();

            RefPtr<TimingFunction> timingFunction;
            if (!parsedKeyframes.isEmpty())
                timingFunction = parsedKeyframes[i].timingFunction;
            if (!timingFunction)
                timingFunction = blendingKeyframe.timingFunction();
            if (!timingFunction)
                timingFunction = styleOriginatedAnimation->backingAnimation().timingFunction();
            if (timingFunction)
                keyframePayload->setEasing(timingFunction->cssText());

            StringBuilder stylePayloadBuilder;
            auto& properties = blendingKeyframe.properties();
            size_t count = properties.size();
            for (auto property : properties) {
                --count;
                WTF::switchOn(property,
                    [&] (CSSPropertyID cssPropertyId) {
                        stylePayloadBuilder.append(nameString(cssPropertyId), ": "_s);
                        if (auto value = computedStyleExtractor.valueForPropertyInStyle(style, cssPropertyId, renderer))
                            stylePayloadBuilder.append(value->cssText());
                    },
                    [&] (const AtomString& customProperty) {
                        stylePayloadBuilder.append(customProperty, ": "_s);
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
            auto keyframePayload = Inspector::Protocol::Animation::Keyframe::create()
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

static Ref<Inspector::Protocol::Animation::Effect> buildObjectForEffect(AnimationEffect& effect)
{
    auto effectPayload = Inspector::Protocol::Animation::Effect::create()
        .release();

    // FIXME: convert this to WebAnimationTime.
    if (auto delayTime = effect.delay().time()) {
        if (auto delay = protocolValueForSeconds(*delayTime))
            effectPayload->setStartDelay(*delay);
    }

    // FIXME: convert this to WebAnimationTime.
    if (auto endDelayTime = effect.endDelay().time()) {
        if (auto endDelay = protocolValueForSeconds(*endDelayTime))
            effectPayload->setEndDelay(*endDelay);
    }

    effectPayload->setIterationCount(effect.iterations() == std::numeric_limits<double>::infinity() ? -1 : effect.iterations());
    effectPayload->setIterationStart(effect.iterationStart());

    // FIXME: convert this to WebAnimationTime.
    if (auto durationTime = effect.iterationDuration().time()) {
        if (auto iterationDuration = protocolValueForSeconds(*durationTime))
            effectPayload->setIterationDuration(iterationDuration.value());
    }

    if (auto* timingFunction = effect.timingFunction())
        effectPayload->setTimingFunction(timingFunction->cssText());

    if (auto playbackDirection = protocolValueForPlaybackDirection(effect.direction()))
        effectPayload->setPlaybackDirection(playbackDirection.value());

    if (auto fillMode = protocolValueForFillMode(effect.fill()))
        effectPayload->setFillMode(fillMode.value());

    if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(effect))
        effectPayload->setKeyframes(buildObjectForKeyframes(*keyframeEffect));

    return effectPayload;
}

InspectorAnimationAgent::InspectorAnimationAgent(PageAgentContext& context)
    : InspectorAgentBase("Animation"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::AnimationFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::AnimationBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_inspectedPage(context.inspectedPage)
    , m_animationBindingTimer(*this, &InspectorAnimationAgent::animationBindingTimerFired)
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

Inspector::Protocol::ErrorStringOr<void> InspectorAnimationAgent::enable()
{
    if (m_instrumentingAgents.enabledAnimationAgent() == this)
        return makeUnexpected("Animation domain already enabled"_s);

    m_instrumentingAgents.setEnabledAnimationAgent(this);

    const auto existsInCurrentPage = [&] (ScriptExecutionContext* scriptExecutionContext) {
        // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
        RefPtr document = dynamicDowncast<Document>(scriptExecutionContext);
        return document && document->page() == &m_inspectedPage;
    };

    {
        for (auto* animation : WebAnimation::instances()) {
            if (existsInCurrentPage(animation->scriptExecutionContext()))
                bindAnimation(*animation, nullptr);
        }
    }

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorAnimationAgent::disable()
{
    m_instrumentingAgents.setEnabledAnimationAgent(nullptr);

    reset();

    return { };
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::DOM::Styleable>> InspectorAnimationAgent::requestEffectTarget(const Inspector::Protocol::Animation::AnimationId& animationId)
{
    Inspector::Protocol::ErrorString errorString;

    auto* animation = assertAnimation(errorString, animationId);
    if (!animation)
        return makeUnexpected(errorString);

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    RefPtr keyframeEffect = dynamicDowncast<KeyframeEffect>(animation->effect());
    if (!keyframeEffect)
        return makeUnexpected("Animation for given animationId does not have an effect"_s);

    auto target = keyframeEffect->targetStyleable();
    if (!target)
        return makeUnexpected("Animation for given animationId does not have a target"_s);

    return domAgent->pushStyleablePathToFrontend(errorString, *target);
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> InspectorAnimationAgent::resolveAnimation(const Inspector::Protocol::Animation::AnimationId& animationId, const String& objectGroup)
{
    Inspector::Protocol::ErrorString errorString;

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

Inspector::Protocol::ErrorStringOr<void> InspectorAnimationAgent::startTracking()
{
    if (m_instrumentingAgents.trackingAnimationAgent() == this)
        return { };

    m_instrumentingAgents.setTrackingAnimationAgent(this);

    ASSERT(m_trackedStyleOriginatedAnimationData.isEmpty());

    m_frontendDispatcher->trackingStart(m_environment.executionStopwatch().elapsedTime().seconds());

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorAnimationAgent::stopTracking()
{
    if (m_instrumentingAgents.trackingAnimationAgent() != this)
        return { };

    m_instrumentingAgents.setTrackingAnimationAgent(nullptr);

    m_trackedStyleOriginatedAnimationData.clear();

    m_frontendDispatcher->trackingComplete(m_environment.executionStopwatch().elapsedTime().seconds());

    return { };
}

static bool isDelayed(const ComputedEffectTiming& computedTiming)
{
    if (!computedTiming.localTime)
        return false;
    return computedTiming.localTime.value() < (computedTiming.endTime - computedTiming.activeDuration);
}

void InspectorAnimationAgent::willApplyKeyframeEffect(const Styleable& target, KeyframeEffect& keyframeEffect, const ComputedEffectTiming& computedTiming)
{
    auto* animation = keyframeEffect.animation();
    RefPtr styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation);
    if (!styleOriginatedAnimation)
        return;

    auto ensureResult = m_trackedStyleOriginatedAnimationData.ensure(styleOriginatedAnimation.get(), [&] () -> UniqueRef<TrackedStyleOriginatedAnimationData> {
        return makeUniqueRef<TrackedStyleOriginatedAnimationData>(TrackedStyleOriginatedAnimationData { makeString("animation:"_s, IdentifiersFactory::createIdentifier()), computedTiming });
    });
    auto& trackingData = ensureResult.iterator->value.get();

    std::optional<Inspector::Protocol::Animation::AnimationState> animationAnimationState;

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
        if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent()) {
            if (auto nodeId = domAgent->pushStyleableElementToFrontend(target))
                event->setNodeId(nodeId);
        }

        if (auto* cssAnimation = dynamicDowncast<CSSAnimation>(animation))
            event->setAnimationName(cssAnimation->animationName());
        else if (auto* cssTransition = dynamicDowncast<CSSTransition>(animation))
            event->setTransitionProperty(cssTransition->transitionProperty());
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
    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation))
        stopTrackingStyleOriginatedAnimation(*styleOriginatedAnimation);

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

    // It is not safe in all cases to resolve animation properties while still resolving styles, so binding animations
    // must be defered to prevent reentrancy into `WebCore::Document::updateStyleIfNeeded`.
    m_animationsPendingBinding.set(animation, Inspector::createScriptCallStack(JSExecState::currentState())->buildInspectorObject());
    if (!m_animationBindingTimer.isActive())
        m_animationBindingTimer.startOneShot(0_s);
}

void InspectorAnimationAgent::animationBindingTimerFired()
{
    for (auto&& [animation, backtrace] : std::exchange(m_animationsPendingBinding, { }))
        bindAnimation(animation, WTFMove(backtrace));
}

void InspectorAnimationAgent::willDestroyWebAnimation(WebAnimation& animation)
{
    if (auto* styleOriginatedAnimation = dynamicDowncast<StyleOriginatedAnimation>(animation))
        stopTrackingStyleOriginatedAnimation(*styleOriginatedAnimation);

    // The `animationId` may be empty if Animation is tracking but not enabled.
    auto animationId = findAnimationId(animation);
    if (!animationId.isEmpty())
        unbindAnimation(animationId);
}

void InspectorAnimationAgent::frameNavigated(LocalFrame& frame)
{
    if (frame.isMainFrame()) {
        reset();
        return;
    }

    Vector<String> animationIdsToRemove;
    for (auto& [animationId, animation] : m_animationIdMap) {
        if (RefPtr document = dynamicDowncast<Document>(animation->scriptExecutionContext()); document && document->frame() == &frame)
            animationIdsToRemove.append(animationId);
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

WebAnimation* InspectorAnimationAgent::assertAnimation(Inspector::Protocol::ErrorString& errorString, const String& animationId)
{
    auto* animation = m_animationIdMap.get(animationId);
    if (!animation)
        errorString = "Missing animation for given animationId"_s;
    return animation;
}

void InspectorAnimationAgent::bindAnimation(WebAnimation& animation, RefPtr<Inspector::Protocol::Console::StackTrace> backtrace)
{
    auto animationId = makeString("animation:"_s, IdentifiersFactory::createIdentifier());
    m_animationIdMap.set(animationId, &animation);

    auto animationPayload = Inspector::Protocol::Animation::Animation::create()
        .setAnimationId(animationId)
        .release();

    auto name = animation.id();
    if (!name.isEmpty())
        animationPayload->setName(name);

    if (auto* cssAnimation = dynamicDowncast<CSSAnimation>(animation))
        animationPayload->setCssAnimationName(cssAnimation->animationName());
    else if (auto* cssTransition = dynamicDowncast<CSSTransition>(animation))
        animationPayload->setCssTransitionProperty(cssTransition->transitionProperty());

    if (auto* effect = animation.effect())
        animationPayload->setEffect(buildObjectForEffect(*effect));

    if (backtrace)
        animationPayload->setStackTrace(backtrace.releaseNonNull());

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

    m_animationsPendingBinding.clear();
    if (m_animationBindingTimer.isActive())
        m_animationBindingTimer.stop();

    m_removedAnimationIds.clear();
    if (m_animationDestroyedTimer.isActive())
        m_animationDestroyedTimer.stop();
}

void InspectorAnimationAgent::stopTrackingStyleOriginatedAnimation(StyleOriginatedAnimation& animation)
{
    auto data = m_trackedStyleOriginatedAnimationData.take(&animation);
    if (!data)
        return;

    if (data->lastComputedTiming.phase != AnimationEffectPhase::After && data->lastComputedTiming.phase != AnimationEffectPhase::Idle) {
        auto event = Inspector::Protocol::Animation::TrackingUpdate::create()
            .setTrackingAnimationId(data->trackingAnimationId)
            .setAnimationState(Inspector::Protocol::Animation::AnimationState::Canceled)
            .release();
        m_frontendDispatcher->trackingUpdate(m_environment.executionStopwatch().elapsedTime().seconds(), WTFMove(event));
    }
}

} // namespace WebCore
