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

#include "AutomationProtocolObjects.h"
#include "Logging.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"

namespace WebKit {

SimulatedInputSourceState SimulatedInputSourceState::emptyStateForSourceType(SimulatedInputSourceType type)
{
    SimulatedInputSourceState result { };
    switch (type) {
    case SimulatedInputSourceType::Null:
    case SimulatedInputSourceType::Keyboard:
        break;
    case SimulatedInputSourceType::Mouse:
    case SimulatedInputSourceType::Touch:
        result.location = WebCore::IntPoint();
    }

    return result;
}


SimulatedInputKeyFrame::SimulatedInputKeyFrame(Vector<StateEntry>&& entries)
    : states(WTFMove(entries))
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

SimulatedInputKeyFrame SimulatedInputKeyFrame::keyFrameFromStateOfInputSources(HashSet<Ref<SimulatedInputSource>>& inputSources)
{
    // The client of this class is required to intern SimulatedInputSource instances if the last state
    // from the previous command should be used as the inital state for the next command. This is the
    // case for Perform Actions and Release Actions, but not Element Click or Element Send Keys.
    Vector<SimulatedInputKeyFrame::StateEntry> entries;
    entries.reserveCapacity(inputSources.size());

    for (auto& inputSource : inputSources)
        entries.uncheckedAppend(std::pair<SimulatedInputSource&, SimulatedInputSourceState> { inputSource.get(), inputSource->state });

    return SimulatedInputKeyFrame(WTFMove(entries));
}

SimulatedInputKeyFrame SimulatedInputKeyFrame::keyFrameToResetInputSources(HashSet<Ref<SimulatedInputSource>>& inputSources)
{
    Vector<SimulatedInputKeyFrame::StateEntry> entries;
    entries.reserveCapacity(inputSources.size());

    for (auto& inputSource : inputSources)
        entries.uncheckedAppend(std::pair<SimulatedInputSource&, SimulatedInputSourceState> { inputSource.get(), SimulatedInputSourceState::emptyStateForSourceType(inputSource->type) });

    return SimulatedInputKeyFrame(WTFMove(entries));
}
    
SimulatedInputDispatcher::SimulatedInputDispatcher(WebPageProxy& page, SimulatedInputDispatcher::Client& client)
    : m_page(page)
    , m_client(client)
    , m_keyFrameTransitionDurationTimer(RunLoop::current(), this, &SimulatedInputDispatcher::keyFrameTransitionDurationTimerFired)
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
        finish(WTF::nullopt);
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
        finishDispatching(WTF::nullopt);
        return;
    }

    transitionBetweenKeyFrames(m_keyframes[m_keyframeIndex - 1], m_keyframes[m_keyframeIndex], [this, protectedThis = makeRef(*this)](Optional<AutomationCommandError> error) {
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
        finish(WTF::nullopt);
        return;
    }

    // In this case, transitions are done but we need to wait for the tick timer.
    if (m_inputSourceStateIndex == m_keyframes[m_keyframeIndex].states.size())
        return;

    auto& nextKeyFrame = m_keyframes[m_keyframeIndex];
    auto& postStateEntry = nextKeyFrame.states[m_inputSourceStateIndex];
    SimulatedInputSource& inputSource = postStateEntry.first;

    transitionInputSourceToState(inputSource, postStateEntry.second, [this, protectedThis = makeRef(*this)](Optional<AutomationCommandError> error) {
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
    m_keyFrameTransitionCompletionHandler = WTFMove(completionHandler);
    m_keyFrameTransitionDurationTimer.startOneShot(b.maximumDuration());

    LOG(Automation, "SimulatedInputDispatcher[%p]: started transition between keyframes: %d --> %d", this, m_keyframeIndex - 1, m_keyframeIndex);
    LOG(Automation, "SimulatedInputDispatcher[%p]: timer started to ensure minimum duration of %.2f seconds for transition %d --> %d", this, b.maximumDuration().value(), m_keyframeIndex - 1, m_keyframeIndex);

    transitionToNextInputSourceState();
}

void SimulatedInputDispatcher::resolveLocation(const WebCore::IntPoint& currentLocation, Optional<WebCore::IntPoint> location, MouseMoveOrigin origin, Optional<String> nodeHandle, Function<void (Optional<WebCore::IntPoint>, Optional<AutomationCommandError>)>&& completionHandler)
{
    if (!location) {
        completionHandler(currentLocation, WTF::nullopt);
        return;
    }

    switch (origin) {
    case MouseMoveOrigin::Viewport:
        completionHandler(location.value(), WTF::nullopt);
        break;
    case MouseMoveOrigin::Pointer: {
        WebCore::IntPoint destination(currentLocation);
        destination.moveBy(location.value());
        completionHandler(destination, WTF::nullopt);
        break;
    }
    case MouseMoveOrigin::Element: {
        m_client.viewportInViewCenterPointOfElement(m_page, m_frameID.value(), nodeHandle.value(), [destination = location.value(), completionHandler = WTFMove(completionHandler)](Optional<WebCore::IntPoint> inViewCenterPoint, Optional<AutomationCommandError> error) mutable {
            if (error) {
                completionHandler(WTF::nullopt, error);
                return;
            }

            ASSERT(inViewCenterPoint);
            destination.moveBy(inViewCenterPoint.value());
            completionHandler(destination, WTF::nullopt);
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

    AutomationCompletionHandler eventDispatchFinished = [this, &inputSource, &newState, completionHandler = WTFMove(completionHandler)](Optional<AutomationCommandError> error) mutable {
        if (error) {
            completionHandler(error);
            return;
        }

#if !LOG_DISABLED
        LOG(Automation, "SimulatedInputDispatcher[%p]: transition finished between input source states: %d.%d --> [%d.%d]", this, m_keyframeIndex - 1, m_inputSourceStateIndex, m_keyframeIndex, m_inputSourceStateIndex);
#else
        UNUSED_PARAM(this);
#endif

        inputSource.state = newState;
        completionHandler(WTF::nullopt);
    };

    switch (inputSource.type) {
    case SimulatedInputSourceType::Null:
        // The maximum duration is handled at the keyframe level by m_keyFrameTransitionDurationTimer.
        eventDispatchFinished(WTF::nullopt);
        break;
    case SimulatedInputSourceType::Mouse: {
        resolveLocation(a.location.value_or(WebCore::IntPoint()), b.location, b.origin.value_or(MouseMoveOrigin::Viewport), b.nodeHandle, [this, &a, &b, eventDispatchFinished = WTFMove(eventDispatchFinished)](Optional<WebCore::IntPoint> location, Optional<AutomationCommandError> error) mutable {
            if (error) {
                eventDispatchFinished(error);
                return;
            }
            RELEASE_ASSERT(location);
            b.location = location;
            // The "dispatch a pointer{Down,Up,Move} action" algorithms (§17.4 Dispatching Actions).
            if (!a.pressedMouseButton && b.pressedMouseButton) {
#if !LOG_DISABLED
                String mouseButtonName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(b.pressedMouseButton.value());
                LOG(Automation, "SimulatedInputDispatcher[%p]: simulating MouseDown[button=%s] for transition to %d.%d", this, mouseButtonName.utf8().data(), m_keyframeIndex, m_inputSourceStateIndex);
#endif
                m_client.simulateMouseInteraction(m_page, MouseInteraction::Down, b.pressedMouseButton.value(), b.location.value(), WTFMove(eventDispatchFinished));
            } else if (a.pressedMouseButton && !b.pressedMouseButton) {
#if !LOG_DISABLED
                String mouseButtonName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(a.pressedMouseButton.value());
                LOG(Automation, "SimulatedInputDispatcher[%p]: simulating MouseUp[button=%s] for transition to %d.%d", this, mouseButtonName.utf8().data(), m_keyframeIndex, m_inputSourceStateIndex);
#endif
                m_client.simulateMouseInteraction(m_page, MouseInteraction::Up, a.pressedMouseButton.value(), b.location.value(), WTFMove(eventDispatchFinished));
            } else if (a.location != b.location) {
                LOG(Automation, "SimulatedInputDispatcher[%p]: simulating MouseMove for transition to %d.%d", this, m_keyframeIndex, m_inputSourceStateIndex);
                // FIXME: This does not interpolate mousemoves per the "perform a pointer move" algorithm (§17.4 Dispatching Actions).
                m_client.simulateMouseInteraction(m_page, MouseInteraction::Move, b.pressedMouseButton.value_or(MouseButton::NoButton), b.location.value(), WTFMove(eventDispatchFinished));
            } else
                eventDispatchFinished(WTF::nullopt);
        });
        break;
    }
    case SimulatedInputSourceType::Keyboard:
        // The "dispatch a key{Down,Up} action" algorithms (§17.4 Dispatching Actions).
        if (!a.pressedCharKey && b.pressedCharKey) {
            LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyPress[key=%c] for transition to %d.%d", this, b.pressedCharKey.value(), m_keyframeIndex, m_inputSourceStateIndex);
            m_client.simulateKeyboardInteraction(m_page, KeyboardInteraction::KeyPress, b.pressedCharKey.value(), WTFMove(eventDispatchFinished));
        } else if (a.pressedCharKey && !b.pressedCharKey) {
            LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyRelease[key=%c] for transition to %d.%d", this, a.pressedCharKey.value(), m_keyframeIndex, m_inputSourceStateIndex);
            m_client.simulateKeyboardInteraction(m_page, KeyboardInteraction::KeyRelease, a.pressedCharKey.value(), WTFMove(eventDispatchFinished));
        } else if (a.pressedVirtualKeys != b.pressedVirtualKeys) {
            bool simulatedAnInteraction = false;
            for (VirtualKey key : b.pressedVirtualKeys) {
                if (!a.pressedVirtualKeys.contains(key)) {
                    ASSERT_WITH_MESSAGE(!simulatedAnInteraction, "Only one VirtualKey may differ at a time between two input source states.");
                    if (simulatedAnInteraction)
                        continue;
                    simulatedAnInteraction = true;
#if !LOG_DISABLED
                    String virtualKeyName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(key);
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyPress[key=%s] for transition to %d.%d", this, virtualKeyName.utf8().data(), m_keyframeIndex, m_inputSourceStateIndex);
#endif
                    m_client.simulateKeyboardInteraction(m_page, KeyboardInteraction::KeyPress, key, WTFMove(eventDispatchFinished));
                }
            }

            for (VirtualKey key : a.pressedVirtualKeys) {
                if (!b.pressedVirtualKeys.contains(key)) {
                    ASSERT_WITH_MESSAGE(!simulatedAnInteraction, "Only one VirtualKey may differ at a time between two input source states.");
                    if (simulatedAnInteraction)
                        continue;
                    simulatedAnInteraction = true;
#if !LOG_DISABLED
                    String virtualKeyName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(key);
                    LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyRelease[key=%s] for transition to %d.%d", this, virtualKeyName.utf8().data(), m_keyframeIndex, m_inputSourceStateIndex);
#endif
                    m_client.simulateKeyboardInteraction(m_page, KeyboardInteraction::KeyRelease, key, WTFMove(eventDispatchFinished));
                }
            }
        } else
            eventDispatchFinished(WTF::nullopt);
        break;
    case SimulatedInputSourceType::Touch:
        // Not supported yet.
        ASSERT_NOT_REACHED();
        eventDispatchFinished(AUTOMATION_COMMAND_ERROR_WITH_NAME(NotImplemented));
        break;
    }
}

void SimulatedInputDispatcher::run(uint64_t frameID, Vector<SimulatedInputKeyFrame>&& keyFrames, HashSet<Ref<SimulatedInputSource>>& inputSources, AutomationCompletionHandler&& completionHandler)
{
    ASSERT(!isActive());
    if (isActive()) {
        completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
        return;
    }

    m_frameID = frameID;
    m_runCompletionHandler = WTFMove(completionHandler);
    for (const Ref<SimulatedInputSource>& inputSource : inputSources)
        m_inputSources.add(inputSource.copyRef());

    // The "dispatch actions" algorithm (§17.4 Dispatching Actions).

    m_keyframes.reserveCapacity(keyFrames.size() + 1);
    m_keyframes.append(SimulatedInputKeyFrame::keyFrameFromStateOfInputSources(m_inputSources));
    m_keyframes.appendVector(WTFMove(keyFrames));

    LOG(Automation, "SimulatedInputDispatcher[%p]: starting input simulation using %d keyframes", this, m_keyframeIndex);

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

void SimulatedInputDispatcher::finishDispatching(Optional<AutomationCommandError> error)
{
    m_keyFrameTransitionDurationTimer.stop();

    LOG(Automation, "SimulatedInputDispatcher[%p]: finished all input simulation at [%u.%u]", this, m_keyframeIndex, m_inputSourceStateIndex);

    auto finish = std::exchange(m_runCompletionHandler, nullptr);
    m_frameID = WTF::nullopt;
    m_keyframes.clear();
    m_inputSources.clear();
    m_keyframeIndex = 0;
    m_inputSourceStateIndex = 0;

    finish(error);
}

} // namespace Webkit
