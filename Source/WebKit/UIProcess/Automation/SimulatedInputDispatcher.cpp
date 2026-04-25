/*
 * Copyright (C) 2018, 2019 Apple Inc. All rights reserved.
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
#include "SimulatedInputDispatcher.h"

#if ENABLE(WEBDRIVER_ACTIONS_API)

#include "AutomationProtocolObjects.h"
#include "Logging.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebPageProxy.h"
#include <WebCore/PointerEventTypeNames.h>

#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
#include <wtf/text/TextBreakIterator.h>
#endif

namespace WebKit {

SimulatedInputSourceState SimulatedInputSourceState::emptyStateForSourceType(SimulatedInputSourceType type)
{
    SimulatedInputSourceState result { };
    switch (type) {
    case SimulatedInputSourceType::Null:
    case SimulatedInputSourceType::Keyboard:
        break;
    case SimulatedInputSourceType::Wheel:
        result.scrollDelta = WebCore::IntSize();
        [[fallthrough]];
    case SimulatedInputSourceType::Mouse:
    case SimulatedInputSourceType::Touch:
    case SimulatedInputSourceType::Pen:
        result.location = WebCore::IntPoint();
    }

    return result;
}


SimulatedInputKeyFrame::SimulatedInputKeyFrame(Vector<StateEntry>&& entries)
    : states(WTF::move(entries))
{
}

Seconds SimulatedInputKeyFrame::maximumDuration() const
{
    // The "compute the tick duration" algorithm (§17.4 Dispatching Actions).
    Seconds result;
    for (auto& entry : states)
        result = std::max(result, entry.second.duration.value_or(Seconds(0)));
    
    return result;
}

SimulatedInputKeyFrame SimulatedInputKeyFrame::keyFrameFromStateOfInputSources(const HashMap<String, Ref<SimulatedInputSource>>& inputSources)
{
    // The client of this class is required to intern SimulatedInputSource instances if the last state
    // from the previous command should be used as the inital state for the next command. This is the
    // case for Perform Actions and Release Actions, but not Element Click or Element Send Keys.
    auto& values = inputSources.values();
    auto entries = WTF::map(values, [](auto& inputSource) {
        return std::pair<SimulatedInputSource&, SimulatedInputSourceState> { inputSource.get(), inputSource->state };
    });

    return SimulatedInputKeyFrame(WTF::move(entries));
}

SimulatedInputKeyFrame SimulatedInputKeyFrame::keyFrameToResetInputSources(const HashMap<String, Ref<SimulatedInputSource>>& inputSources)
{
    auto& values = inputSources.values();
    auto entries = WTF::map(values, [](auto& inputSource) {
        return std::pair<SimulatedInputSource&, SimulatedInputSourceState> { inputSource.get(), SimulatedInputSourceState::emptyStateForSourceType(inputSource->type) };
    });

    return SimulatedInputKeyFrame(WTF::move(entries));
}
    
SimulatedInputDispatcher::SimulatedInputDispatcher(WebPageProxy& page, SimulatedInputDispatcher::Client& client)
    : m_page(page)
    , m_client(client)
    , m_keyFrameTransitionDurationTimer(RunLoop::currentSingleton(), "SimulatedInputDispatcher::KeyFrameTransitionDurationTimer"_s, this, &SimulatedInputDispatcher::keyFrameTransitionDurationTimerFired)
{
}

SimulatedInputDispatcher::~SimulatedInputDispatcher()
{
    ASSERT(!m_runCompletionHandler);
    ASSERT(!m_keyFrameTransitionDurationTimer.isActive());
}

bool SimulatedInputDispatcher::isActive() const
{
    return !!m_runCompletionHandler;
}

void SimulatedInputDispatcher::keyFrameTransitionDurationTimerFired()
{
    ASSERT(m_keyFrameTransitionCompletionHandler);

    m_keyFrameTransitionDurationTimer.stop();

    LOG(Automation, "SimulatedInputDispatcher[%p]: timer finished for transition between keyframes: %d --> %d", this, m_keyframeIndex - 1, m_keyframeIndex);

    if (isKeyFrameTransitionComplete()) {
        auto finish = std::exchange(m_keyFrameTransitionCompletionHandler, nullptr);
        finish(std::nullopt);
    }
}

bool SimulatedInputDispatcher::isKeyFrameTransitionComplete() const
{
    ASSERT(m_keyframeIndex < m_keyframes.size());

    if (m_inputSourceStateIndex < m_keyframes[m_keyframeIndex].states.size())
        return false;

    if (m_keyFrameTransitionDurationTimer.isActive())
        return false;

    return true;
}

void SimulatedInputDispatcher::transitionToNextKeyFrame()
{
    ++m_keyframeIndex;
    if (m_keyframeIndex == m_keyframes.size()) {
        finishDispatching(std::nullopt);
        return;
    }

    transitionBetweenKeyFrames(m_keyframes[m_keyframeIndex - 1], m_keyframes[m_keyframeIndex], [this, protectedThis = Ref { *this }](std::optional<AutomationCommandError> error) {
        if (error) {
            finishDispatching(error);
            return;
        }

        transitionToNextKeyFrame();
    });
}

void SimulatedInputDispatcher::transitionToNextInputSourceState()
{
    if (isKeyFrameTransitionComplete()) {
        auto finish = std::exchange(m_keyFrameTransitionCompletionHandler, nullptr);
        finish(std::nullopt);
        return;
    }

    // In this case, transitions are done but we need to wait for the tick timer.
    if (m_inputSourceStateIndex == m_keyframes[m_keyframeIndex].states.size())
        return;

    auto& nextKeyFrame = m_keyframes[m_keyframeIndex];
    auto& postStateEntry = nextKeyFrame.states[m_inputSourceStateIndex];
    Ref inputSource = postStateEntry.first;

    transitionInputSourceToState(inputSource, postStateEntry.second, [this, protectedThis = Ref { *this }](std::optional<AutomationCommandError> error) {
        if (error) {
            auto finish = std::exchange(m_keyFrameTransitionCompletionHandler, nullptr);
            finish(error);
            return;
        }

        // Perform state transitions in the order specified by the currentKeyFrame.
        ++m_inputSourceStateIndex;

        transitionToNextInputSourceState();
    });
}

void SimulatedInputDispatcher::transitionBetweenKeyFrames(const SimulatedInputKeyFrame& a, const SimulatedInputKeyFrame& b, AutomationCompletionHandler&& completionHandler)
{
    m_inputSourceStateIndex = 0;

    // The "dispatch tick actions" algorithm (§17.4 Dispatching Actions).
    m_keyFrameTransitionCompletionHandler = WTF::move(completionHandler);
    m_keyFrameTransitionDurationTimer.startOneShot(b.maximumDuration());

    LOG(Automation, "SimulatedInputDispatcher[%p]: started transition between keyframes: %d --> %d", this, m_keyframeIndex - 1, m_keyframeIndex);
    LOG(Automation, "SimulatedInputDispatcher[%p]: timer started to ensure minimum duration of %.2f seconds for transition %d --> %d", this, b.maximumDuration().value(), m_keyframeIndex - 1, m_keyframeIndex);

    transitionToNextInputSourceState();
}

void SimulatedInputDispatcher::resolveLocation(const WebCore::IntPoint& currentLocation, std::optional<WebCore::IntPoint> location, MouseMoveOrigin origin, std::optional<String> nodeHandle, Function<void (std::optional<WebCore::IntPoint>, std::optional<AutomationCommandError>)>&& completionHandler)
{
    if (!location) {
        completionHandler(currentLocation, std::nullopt);
        return;
    }

    switch (origin) {
    case MouseMoveOrigin::Viewport:
        completionHandler(location.value(), std::nullopt);
        break;
    case MouseMoveOrigin::Pointer: {
        WebCore::IntPoint destination(currentLocation);
        destination.moveBy(location.value());
        completionHandler(destination, std::nullopt);
        break;
    }
    case MouseMoveOrigin::Element: {
        m_client.viewportInViewCenterPointOfElement(protect(m_page), m_frameID, nodeHandle.value(), [destination = location.value(), completionHandler = WTF::move(completionHandler)](std::optional<WebCore::IntPoint> inViewCenterPoint, std::optional<AutomationCommandError> error) mutable {
            if (error) {
                completionHandler(std::nullopt, error);
                return;
            }

            if (!inViewCenterPoint) {
                completionHandler(std::nullopt, AUTOMATION_COMMAND_ERROR_WITH_NAME(ElementNotInteractable));
                return;
            }

            destination.moveBy(inViewCenterPoint.value());
            completionHandler(destination, std::nullopt);
        });
        break;
    }
    }
}

void SimulatedInputDispatcher::transitionInputSourceToState(SimulatedInputSource& inputSource, SimulatedInputSourceState& newState, AutomationCompletionHandler&& completionHandler)
{
    // Make cases and conditionals more readable by aliasing pre/post states as 'a' and 'b'.
    SimulatedInputSourceState& a = inputSource.state;
    SimulatedInputSourceState& b = newState;

    LOG(Automation, "SimulatedInputDispatcher[%p]: transition started between input source states: [%d.%d] --> %d.%d", this, m_keyframeIndex - 1, m_inputSourceStateIndex, m_keyframeIndex, m_inputSourceStateIndex);

    AutomationCompletionHandler eventDispatchFinished = [this, protectedThis = Ref { *this }, inputSource = Ref { inputSource }, &newState, completionHandler = WTF::move(completionHandler)](std::optional<AutomationCommandError> error) mutable {
        if (error) {
            completionHandler(error);
            return;
        }

#if !LOG_DISABLED
        LOG(Automation, "SimulatedInputDispatcher[%p]: transition finished between input source states: %d.%d --> [%d.%d]", this, m_keyframeIndex - 1, m_inputSourceStateIndex, m_keyframeIndex, m_inputSourceStateIndex);
#else
        UNUSED_PARAM(this);
#endif

        inputSource->state = newState;
        completionHandler(std::nullopt);
    };

    switch (inputSource.type) {
    case SimulatedInputSourceType::Null:
        // The maximum duration is handled at the keyframe level by m_keyFrameTransitionDurationTimer.
        eventDispatchFinished(std::nullopt);
        break;
    case SimulatedInputSourceType::Mouse:
    case SimulatedInputSourceType::Pen: {
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        RELEASE_ASSERT_NOT_REACHED();
#else
        resolveLocation(valueOrDefault(a.location), b.location, b.origin.value_or(MouseMoveOrigin::Pointer), b.nodeHandle, [this, protectedThis = Ref { *this }, &a, &b, inputSource = inputSource.type, eventDispatchFinished = WTF::move(eventDispatchFinished)](std::optional<WebCore::IntPoint> location, std::optional<AutomationCommandError> error) mutable {
            if (error) {
                eventDispatchFinished(error);
                return;
            }

            if (!location) {
                eventDispatchFinished(AUTOMATION_COMMAND_ERROR_WITH_NAME(ElementNotInteractable));
                return;
            }

            const String& pointerType = inputSource == SimulatedInputSourceType::Mouse ? WebCore::mousePointerEventType() : WebCore::penPointerEventType();

            b.location = location;
            // The "dispatch a pointer{Down,Up,Move} action" algorithms (§17.4 Dispatching Actions).
            if (b.mouseInteraction) {
                const auto stateTransitionIsNoop = [&a, &b] {
                    return std::tie(a.location, a.mouseInteraction, a.pressedMouseButton) == std::tie(b.location, b.mouseInteraction, b.pressedMouseButton);
                }();

                if (!stateTransitionIsNoop) {
#if !LOG_DISABLED
                    String interactionName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(b.mouseInteraction.value());
                    String mouseButtonName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(b.pressedMouseButton.value_or(MouseButton::None));
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating %s[button=%s] @ (%d, %d) for transition to %d.%d", this, interactionName.utf8().data(), mouseButtonName.utf8().data(), b.location.value().x(), b.location.value().y(), m_keyframeIndex, m_inputSourceStateIndex);
#endif
                    m_client.simulateMouseInteraction(protect(m_page), b.mouseInteraction.value(), b.pressedMouseButton.value_or(MouseButton::None), b.location.value(), pointerType, WTF::move(eventDispatchFinished));
                } else
                    eventDispatchFinished({ });
            } else
                eventDispatchFinished(std::nullopt);
        });
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        break;
    }
    case SimulatedInputSourceType::Touch: {
#if !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        RELEASE_ASSERT_NOT_REACHED();
#else
        resolveLocation(valueOrDefault(a.location), b.location, b.origin.value_or(MouseMoveOrigin::Viewport), b.nodeHandle, [this, protectedThis = Ref { *this }, &a, &b, eventDispatchFinished = WTF::move(eventDispatchFinished)](std::optional<WebCore::IntPoint> location, std::optional<AutomationCommandError> error) mutable {
            if (error) {
                eventDispatchFinished(error);
                return;
            }

            if (!location) {
                eventDispatchFinished(AUTOMATION_COMMAND_ERROR_WITH_NAME(ElementNotInteractable));
                return;
            }

            b.location = location;
            // The "dispatch a pointer{Down,Up,Move} action" algorithms (§17.4 Dispatching Actions).
            if (!a.pressedMouseButton && b.pressedMouseButton) {
                LOG(Automation, "SimulatedInputDispatcher[%p]: simulating TouchDown @ (%d, %d) for transition to %d.%d", this, b.location.value().x(), b.location.value().y(), m_keyframeIndex, m_inputSourceStateIndex);
                m_client.simulateTouchInteraction(protect(m_page), TouchInteraction::TouchDown, b.location.value(), std::nullopt, WTF::move(eventDispatchFinished));
            } else if (a.pressedMouseButton && !b.pressedMouseButton) {
                LOG(Automation, "SimulatedInputDispatcher[%p]: simulating LiftUp @ (%d, %d) for transition to %d.%d", this, b.location.value().x(), b.location.value().y(), m_keyframeIndex, m_inputSourceStateIndex);
                m_client.simulateTouchInteraction(protect(m_page), TouchInteraction::LiftUp, b.location.value(), std::nullopt, WTF::move(eventDispatchFinished));
            } else if (a.location != b.location) {
                LOG(Automation, "SimulatedInputDispatcher[%p]: simulating MoveTo from (%d, %d) to (%d, %d) for transition to %d.%d", this, a.location.value().x(), a.location.value().y(), b.location.value().x(), b.location.value().y(), m_keyframeIndex, m_inputSourceStateIndex);
                m_client.simulateTouchInteraction(protect(m_page), TouchInteraction::MoveTo, b.location.value(), a.duration.value_or(0_s), WTF::move(eventDispatchFinished));
            } else
                eventDispatchFinished(std::nullopt);
        });
#endif // !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        break;
    }
    case SimulatedInputSourceType::Keyboard: {
#if !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        RELEASE_ASSERT_NOT_REACHED();
#else
        auto comparePressedCharKeys = [](const auto& a, const auto& b) {
            if (a.size() != b.size())
                return false;
            for (const auto& charKey : a) {
                if (!b.contains(charKey))
                    return false;
            }
            return true;
        };

        // The "dispatch a key{Down,Up} action" algorithms (§17.4 Dispatching Actions).
        if (!comparePressedCharKeys(a.pressedCharKeys, b.pressedCharKeys)) {
            bool simulatedAnInteraction = false;
            for (auto charKey : b.pressedCharKeys) {
                if (!a.pressedCharKeys.contains(charKey)) {
#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
                    ASSERT_WITH_MESSAGE(WTF::numGraphemeClusters(charKey) <= 1, "A CharKey must either be a single unicode code point, a single grapheme cluster, or null.");
#endif
                    ASSERT_WITH_MESSAGE(!simulatedAnInteraction, "Only one CharKey may differ at a time between two input source states.");
                    if (simulatedAnInteraction)
                        continue;
                    simulatedAnInteraction = true;

#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyPress[key=%s] for transition to %d.%d", this, charKey.utf8().data(), m_keyframeIndex, m_inputSourceStateIndex);
#else
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyPress[key=%c] for transition to %d.%d", this, charKey, m_keyframeIndex, m_inputSourceStateIndex);
#endif
                    m_client.simulateKeyboardInteraction(protect(m_page), KeyboardInteraction::KeyPress, charKey, WTF::move(eventDispatchFinished));
                }
            }

            for (auto charKey : copyToVector(a.pressedCharKeys)) {
                if (!b.pressedCharKeys.contains(charKey)) {
#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
                    ASSERT_WITH_MESSAGE(WTF::numGraphemeClusters(charKey) <= 1, "A CharKey must either be a single unicode code point, a single grapheme cluster, or null.");
#endif
                    ASSERT_WITH_MESSAGE(!simulatedAnInteraction, "Only one CharKey may differ at a time between two input source states.");
                    if (simulatedAnInteraction)
                        continue;
                    simulatedAnInteraction = true;
#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyRelease[key=%s] for transition to %d.%d", this, charKey.utf8().data(), m_keyframeIndex, m_inputSourceStateIndex);
#else
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyRelease[key=%c] for transition to %d.%d", this, charKey, m_keyframeIndex, m_inputSourceStateIndex);
#endif
                    m_client.simulateKeyboardInteraction(protect(m_page), KeyboardInteraction::KeyRelease, charKey, WTF::move(eventDispatchFinished));
                }
            }
        } else if (a.pressedVirtualKeys != b.pressedVirtualKeys) {
            bool simulatedAnInteraction = false;
            for (const auto& iter : b.pressedVirtualKeys) {
                if (!a.pressedVirtualKeys.contains(iter.key)) {
                    ASSERT_WITH_MESSAGE(!simulatedAnInteraction, "Only one VirtualKey may differ at a time between two input source states.");
                    if (simulatedAnInteraction)
                        continue;
                    simulatedAnInteraction = true;
#if !LOG_DISABLED
                    String virtualKeyName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(iter.value);
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyPress[key=%s] for transition to %d.%d", this, virtualKeyName.utf8().data(), m_keyframeIndex, m_inputSourceStateIndex);
#endif
                    m_client.simulateKeyboardInteraction(protect(m_page), KeyboardInteraction::KeyPress, iter.value, WTF::move(eventDispatchFinished));
                }
            }

            for (const auto& iter : copyToVector(a.pressedVirtualKeys)) {
                if (!b.pressedVirtualKeys.contains(iter.key)) {
                    ASSERT_WITH_MESSAGE(!simulatedAnInteraction, "Only one VirtualKey may differ at a time between two input source states.");
                    if (simulatedAnInteraction)
                        continue;
                    simulatedAnInteraction = true;
#if !LOG_DISABLED
                    String virtualKeyName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(iter.value);
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyRelease[key=%s] for transition to %d.%d", this, virtualKeyName.utf8().data(), m_keyframeIndex, m_inputSourceStateIndex);
#endif
                    m_client.simulateKeyboardInteraction(protect(m_page), KeyboardInteraction::KeyRelease, iter.value, WTF::move(eventDispatchFinished));
                }
            }
        } else
            eventDispatchFinished(std::nullopt);
#endif // !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        break;
    }
    case SimulatedInputSourceType::Wheel:
#if !ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
        RELEASE_ASSERT_NOT_REACHED();
#else
        resolveLocation(valueOrDefault(a.location), b.location, b.origin.value_or(MouseMoveOrigin::Viewport), b.nodeHandle, [this, protectedThis = Ref { *this }, &a, &b, eventDispatchFinished = WTF::move(eventDispatchFinished)](std::optional<WebCore::IntPoint> location, std::optional<AutomationCommandError> error) mutable {
            if (error) {
                eventDispatchFinished(error);
                return;
            }

            if (!location) {
                eventDispatchFinished(AUTOMATION_COMMAND_ERROR_WITH_NAME(ElementNotInteractable));
                return;
            }

            b.location = location;

            auto aScrollDelta = a.scrollDelta.value_or(WebCore::IntSize());
            auto bScrollDelta = b.scrollDelta.value_or(WebCore::IntSize());
            auto usedScrollDelta = bScrollDelta;
            if (!aScrollDelta.isZero())
                usedScrollDelta.contract(aScrollDelta.width(), aScrollDelta.height());

            if (!usedScrollDelta.isZero()) {
                LOG(Automation, "SimulatedInputDispatcher[%p]: simulating Wheel from (%d, %d) to (%d, %d) for transition to %d.%d", this, aScrollDelta.width(), aScrollDelta.height(), bScrollDelta.width(), bScrollDelta.height(), m_keyframeIndex, m_inputSourceStateIndex);
                // FIXME: This does not interpolate mouse scrolls per the "perform a scroll" algorithm (§15.4.4 Wheel actions).
                m_client.simulateWheelInteraction(protect(m_page), b.location.value(), usedScrollDelta, WTF::move(eventDispatchFinished));
            } else
                eventDispatchFinished(std::nullopt);
        });
#endif // !ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
        break;
    }
}

void SimulatedInputDispatcher::run(std::optional<WebCore::FrameIdentifier> frameID, Vector<SimulatedInputKeyFrame>&& keyFrames, const HashMap<String, Ref<SimulatedInputSource>>& inputSources, AutomationCompletionHandler&& completionHandler)
{
    ASSERT(!isActive());
    if (isActive()) {
        completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
        return;
    }

    m_frameID = frameID;
    m_runCompletionHandler = WTF::move(completionHandler);

    // The "dispatch actions" algorithm (§17.4 Dispatching Actions).
    m_keyframes.reserveCapacity(keyFrames.size() + 1);
    m_keyframes.append(SimulatedInputKeyFrame::keyFrameFromStateOfInputSources(inputSources));
    m_keyframes.appendVector(WTF::move(keyFrames));

    LOG(Automation, "SimulatedInputDispatcher[%p]: starting input simulation using %zu keyframes", this, m_keyframes.size());

    transitionToNextKeyFrame();
}

void SimulatedInputDispatcher::cancel()
{
    // If we were waiting for m_client to finish an interaction and the interaction had an error,
    // then the rest of the async chain will have been torn down. If we are just waiting on a
    // dispatch timer, then this will cancel the timer and clear

    if (isActive())
        finishDispatching(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
}

void SimulatedInputDispatcher::finishDispatching(std::optional<AutomationCommandError> error)
{
    m_keyFrameTransitionDurationTimer.stop();

    LOG(Automation, "SimulatedInputDispatcher[%p]: finished all input simulation at [%u.%u]", this, m_keyframeIndex, m_inputSourceStateIndex);

    auto finish = std::exchange(m_runCompletionHandler, nullptr);
    m_frameID = std::nullopt;
    m_keyframes.clear();
    m_keyframeIndex = 0;
    m_inputSourceStateIndex = 0;
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    // It is unclear if this is desirable; see
    // https://github.com/w3c/webdriver/issues/1772 for ongoing discussion:
    m_client.clearDoubleClicks();
#endif
    finish(error);
}

} // namespace Webkit

#endif // ENABLE(WEBDRIVER_ACTIONS_API)
