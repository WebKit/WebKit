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
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"

namespace WebKit {

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
        entries.uncheckedAppend(std::pair<SimulatedInputSource&, SimulatedInputSourceState> { inputSource.get(), SimulatedInputSourceState::emptyState() });

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

    transitionBetweenKeyFrames(m_keyframes[m_keyframeIndex - 1], m_keyframes[m_keyframeIndex], [this, protectedThis = makeRef(*this)](std::optional<AutomationCommandError> error) {
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
    SimulatedInputSource& inputSource = postStateEntry.first;

    transitionInputSourceToState(inputSource, postStateEntry.second, [this, protectedThis = makeRef(*this)](std::optional<AutomationCommandError> error) {
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

    transitionToNextInputSourceState();
}

void SimulatedInputDispatcher::transitionInputSourceToState(SimulatedInputSource& inputSource, const SimulatedInputSourceState& newState, AutomationCompletionHandler&& completionHandler)
{
    // Make cases and conditionals more readable by aliasing pre/post states as 'a' and 'b'.
    SimulatedInputSourceState a = inputSource.state;
    SimulatedInputSourceState b = newState;

    AutomationCompletionHandler eventDispatchFinished = [&inputSource, &newState, completionHandler = WTFMove(completionHandler)](std::optional<AutomationCommandError> error) {
        if (error) {
            completionHandler(error);
            return;
        }

        inputSource.state = newState;
        completionHandler(std::nullopt);
    };

    switch (inputSource.type) {
    case SimulatedInputSource::Type::Null:
        // The maximum duration is handled at the keyframe level by m_keyFrameTransitionDurationTimer.
        eventDispatchFinished(std::nullopt);
        break;
    case SimulatedInputSource::Type::Mouse: {
        // The "dispatch a pointer{Down,Up,Move} action" algorithms (§17.4 Dispatching Actions).
        if (!a.pressedMouseButton && b.pressedMouseButton)
            m_client.simulateMouseInteraction(m_page, MouseInteraction::Down, b.pressedMouseButton.value(), b.location.value(), WTFMove(eventDispatchFinished));
        else if (a.pressedMouseButton && !b.pressedMouseButton)
            m_client.simulateMouseInteraction(m_page, MouseInteraction::Up, a.pressedMouseButton.value(), b.location.value(), WTFMove(eventDispatchFinished));
        else if (a.location != b.location) {
            // FIXME: This does not interpolate mousemoves per the "perform a pointer move" algorithm (§17.4 Dispatching Actions).
            m_client.simulateMouseInteraction(m_page, MouseInteraction::Move, b.pressedMouseButton.value_or(MouseButton::NoButton), b.location.value(), WTFMove(eventDispatchFinished));
        } else
            eventDispatchFinished(std::nullopt);
        break;
    }
    case SimulatedInputSource::Type::Keyboard:
        // The "dispatch a key{Down,Up} action" algorithms (§17.4 Dispatching Actions).
        if ((!a.pressedCharKey && b.pressedCharKey) || (!a.pressedVirtualKey && b.pressedVirtualKey))
            m_client.simulateKeyboardInteraction(m_page, KeyboardInteraction::KeyPress, b.pressedVirtualKey, b.pressedCharKey, WTFMove(eventDispatchFinished));
        else if ((a.pressedCharKey && !b.pressedCharKey) || (a.pressedVirtualKey && !b.pressedVirtualKey))
            m_client.simulateKeyboardInteraction(m_page, KeyboardInteraction::KeyRelease, a.pressedVirtualKey, a.pressedCharKey, WTFMove(eventDispatchFinished));
        else
            eventDispatchFinished(std::nullopt);
        break;
    case SimulatedInputSource::Type::Touch:
        // Not supported yet.
        ASSERT_NOT_REACHED();
        eventDispatchFinished(AUTOMATION_COMMAND_ERROR_WITH_NAME(NotImplemented));
        break;
    }
}

void SimulatedInputDispatcher::run(Vector<SimulatedInputKeyFrame>&& keyFrames, HashSet<Ref<SimulatedInputSource>>& inputSources, AutomationCompletionHandler&& completionHandler)
{
    ASSERT(!isActive());
    if (isActive()) {
        completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
        return;
    }

    m_runCompletionHandler = WTFMove(completionHandler);
    for (const Ref<SimulatedInputSource>& inputSource : inputSources)
        m_inputSources.add(inputSource.copyRef());

    // The "dispatch actions" algorithm (§17.4 Dispatching Actions).

    m_keyframes.reserveCapacity(keyFrames.size() + 1);
    m_keyframes.append(SimulatedInputKeyFrame::keyFrameFromStateOfInputSources(m_inputSources));
    m_keyframes.appendVector(WTFMove(keyFrames));

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

    auto finish = std::exchange(m_runCompletionHandler, nullptr);
    m_keyframes.clear();
    m_inputSources.clear();
    m_keyframeIndex = 0;
    m_inputSourceStateIndex = 0;

    finish(error);
}

} // namespace Webkit
