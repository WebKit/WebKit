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

#include <wtf/text/TextStream.h>

#if ENABLE(WEBDRIVER_ACTIONS_API)

#include "AutomationProtocolObjects.h"
#include "Logging.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebPageProxy.h"
#include <WebCore/PointerEventTypeNames.h>
#include <wtf/Borrow.h>

#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
#include <wtf/text/TextBreakIterator.h>
#endif

namespace WebKit {

// https://w3c.github.io/webdriver/#dfn-normalised-key-value
NODELETE VirtualKey normalizedVirtualKey(VirtualKey key)
{
    switch (key) {
    case VirtualKey::ControlRight:
        return VirtualKey::Control;
    case VirtualKey::ShiftRight:
        return VirtualKey::Shift;
    case VirtualKey::AlternateRight:
        return VirtualKey::Alternate;
    case VirtualKey::MetaRight:
        return VirtualKey::Meta;
    case VirtualKey::CommandRight:
        return VirtualKey::Command;
    case VirtualKey::DownArrowRight:
        return VirtualKey::DownArrow;
    case VirtualKey::UpArrowRight:
        return VirtualKey::UpArrow;
    case VirtualKey::LeftArrowRight:
        return VirtualKey::LeftArrow;
    case VirtualKey::RightArrowRight:
        return VirtualKey::RightArrow;
    case VirtualKey::PageUpRight:
        return VirtualKey::PageUp;
    case VirtualKey::PageDownRight:
        return VirtualKey::PageDown;
    case VirtualKey::EndRight:
        return VirtualKey::End;
    case VirtualKey::HomeRight:
        return VirtualKey::Home;
    case VirtualKey::DeleteRight:
        return VirtualKey::Delete;
    case VirtualKey::InsertRight:
        return VirtualKey::Insert;
    default:
        return key;
    }
}

#if !ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
std::optional<CharKey> pressedCharKey(const String& pressedCharKeyString)
{
    switch (pressedCharKeyString.length()) {
    case 1:
        return pressedCharKeyString.codeUnitAt(0);
    case 2: {
        auto lead = pressedCharKeyString.codeUnitAt(0);
        auto trail = pressedCharKeyString.codeUnitAt(1);
        if (U16_IS_LEAD(lead) && U16_IS_TRAIL(trail))
            return U16_GET_SUPPLEMENTARY(lead, trail);
    }
    }

    return std::nullopt;
}
#endif // !ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)

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


SimulatedInputKeyFrame::SimulatedInputKeyFrame(Vector<StateEntry>&& entries, IsResetKeyFrame isResetKeyFrame)
    : states(WTF::move(entries))
    , isResetKeyFrame(isResetKeyFrame)
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

    return SimulatedInputKeyFrame(WTF::move(entries), IsResetKeyFrame::Yes);
}

SimulatedInputDispatcher::SimulatedInputDispatcher(WebPageProxy& page, SimulatedInputDispatcher::Client& client)
    : m_page(page)
    , m_client(client)
    , m_keyFrameTransitionDurationTimer(RunLoop::currentSingleton(), "SimulatedInputDispatcher::KeyFrameTransitionDurationTimer"_s, this, &SimulatedInputDispatcher::keyFrameTransitionDurationTimerFired)
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    , m_pointerMoveInterpolationTimer(RunLoop::currentSingleton(), "SimulatedInputDispatcher::PointerMoveInterpolationTimer"_s, this, &SimulatedInputDispatcher::pointerMoveInterpolationTimerFired)
#endif
{
}

SimulatedInputDispatcher::~SimulatedInputDispatcher()
{
    ASSERT(!m_runCompletionHandler);
    ASSERT(!m_keyFrameTransitionDurationTimer.isActive());
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    ASSERT(!m_pointerMoveInterpolationTimer.isActive());
#endif
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

    finishKeyFrameTransitionIfComplete();
}

bool SimulatedInputDispatcher::isKeyFrameTransitionComplete() const
{
    ASSERT(m_keyframeIndex < m_keyframes.size());

    if (m_inputSourceStateIndex < m_keyframes[m_keyframeIndex].states.size())
        return false;

    if (m_keyFrameTransitionDurationTimer.isActive())
        return false;

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    if (!m_pointerMoveInterpolations.isEmpty() || m_isDispatchingMouseInteraction || !m_pendingMouseInteractions.isEmpty())
        return false;
#endif

    return true;
}

void SimulatedInputDispatcher::finishKeyFrameTransitionIfComplete()
{
    if (!m_keyFrameTransitionCompletionHandler || !isKeyFrameTransitionComplete())
        return;

    auto finish = std::exchange(m_keyFrameTransitionCompletionHandler, nullptr);
    finish(std::nullopt);
}

void SimulatedInputDispatcher::failKeyFrameTransition(AutomationCommandError error)
{
    auto finish = std::exchange(m_keyFrameTransitionCompletionHandler, nullptr);
    if (!finish)
        return;

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    m_pointerMoveInterpolationTimer.stop();
    // https://w3c.github.io/webdriver/#dfn-perform-a-pointer-move
    auto interpolations = std::exchange(m_pointerMoveInterpolations, { });
    for (auto& interpolation : borrow(interpolations))
        interpolation.inputSource->state.location = interpolation.lastDeliveredLocation;
#endif

    finish(WTF::move(error));
}

void SimulatedInputDispatcher::transitionToNextKeyFrame()
{
    ++m_keyframeIndex;
    if (m_keyframeIndex == m_keyframes.size()) {
        finishDispatching(std::nullopt);
        return;
    }

    transitionBetweenKeyFrames([this, protectedThis = Ref { *this }](std::optional<AutomationCommandError> error) {
        if (error) {
            finishDispatching(error);
            return;
        }

        transitionToNextKeyFrame();
    });
}

void SimulatedInputDispatcher::transitionToNextInputSourceState()
{
    if (m_inputSourceStateIndex == m_keyframes[m_keyframeIndex].states.size()) {
        finishKeyFrameTransitionIfComplete();
        return;
    }

    // FIXME: These references do not enforce lifetime safety.
    // We keep them this way for now because the synchronous error case would crash otherwise.
    SUPPRESS_UNBORROWED_LOCAL auto& nextKeyFrame = m_keyframes[m_keyframeIndex];
    SUPPRESS_UNBORROWED_LOCAL auto& postStateEntry = nextKeyFrame.states[m_inputSourceStateIndex];
    Ref inputSource = postStateEntry.first;

    transitionInputSourceToState(inputSource, postStateEntry.second, [this, protectedThis = Ref { *this }, runCompletionCount = m_runCompletionCount](std::optional<AutomationCommandError> error) {
        if (runCompletionCount != m_runCompletionCount)
            return;

        if (error) {
            failKeyFrameTransition(WTF::move(*error));
            return;
        }

        // Perform state transitions in the order specified by the currentKeyFrame.
        ++m_inputSourceStateIndex;

        transitionToNextInputSourceState();
    });
}

void SimulatedInputDispatcher::transitionBetweenKeyFrames(AutomationCompletionHandler&& completionHandler)
{
    m_inputSourceStateIndex = 0;
    auto maximumDuration = m_keyframes[m_keyframeIndex].maximumDuration();

    // The "dispatch tick actions" algorithm (§17.4 Dispatching Actions).
    m_keyFrameTransitionCompletionHandler = WTF::move(completionHandler);
    m_keyFrameStartTime = MonotonicTime::now();
    m_keyFrameTransitionDurationTimer.startOneShot(maximumDuration);

    LOG(Automation, "SimulatedInputDispatcher[%p]: started transition between keyframes: %d --> %d", this, m_keyframeIndex - 1, m_keyframeIndex);
    LOG(Automation, "SimulatedInputDispatcher[%p]: timer started to ensure minimum duration of %.2f seconds for transition %d --> %d", this, maximumDuration.value(), m_keyframeIndex - 1, m_keyframeIndex);

    transitionToNextInputSourceState();
}

WebCore::IntPoint SimulatedInputDispatcher::currentLocationOfSource(const SimulatedInputSourceState& state) const
{
    return state.location.value_or(m_options.viewport ? flooredIntPoint(m_options.viewport->originInMainFrameViewport) : WebCore::IntPoint { });
}

void SimulatedInputDispatcher::resolveLocation(const WebCore::IntPoint& currentLocation, std::optional<WebCore::IntPoint> location, MouseMoveOrigin origin, std::optional<String> nodeHandle, Function<void (std::optional<WebCore::IntPoint>, std::optional<AutomationCommandError>)>&& completionHandler)
{
    if (!location) {
        completionHandler(currentLocation, std::nullopt);
        return;
    }

    if (m_options.viewport) {
        // https://w3c.github.io/webdriver/#dfn-perform-a-pointer-move
        completionHandler = [viewport = *m_options.viewport, completionHandler = WTF::move(completionHandler)](std::optional<WebCore::IntPoint> destination, std::optional<AutomationCommandError> error) mutable {
            if (error || !destination) {
                completionHandler(destination, error);
                return;
            }

            auto viewportOrigin = flooredIntPoint(viewport.originInMainFrameViewport);
            int x = destination->x() - viewportOrigin.x();
            int y = destination->y() - viewportOrigin.y();
            if (x < 0 || x > viewport.size.width() || y < 0 || y > viewport.size.height()) {
                completionHandler(std::nullopt, AUTOMATION_COMMAND_ERROR_WITH_NAME(TargetOutOfBounds));
                return;
            }

            completionHandler(destination, std::nullopt);
        };
    }

    switch (origin) {
    case MouseMoveOrigin::Viewport: {
        // https://w3c.github.io/webdriver/#dfn-get-coordinates-relative-to-an-origin
        WebCore::IntPoint destination(location.value());
        if (m_options.viewport)
            destination.moveBy(flooredIntPoint(m_options.viewport->originInMainFrameViewport));
        completionHandler(destination, std::nullopt);
        break;
    }
    case MouseMoveOrigin::Pointer: {
        WebCore::IntPoint destination(currentLocation);
        destination.moveBy(location.value());
        completionHandler(destination, std::nullopt);
        break;
    }
    case MouseMoveOrigin::Element: {
        m_client.viewportInViewCenterPointOfElement(protect(m_page), m_frameID, nodeHandle.value(), m_options.elementOriginHandleKind, [destination = location.value(), completionHandler = WTF::move(completionHandler)](std::optional<WebCore::IntPoint> inViewCenterPoint, std::optional<AutomationCommandError> error) mutable {
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

#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
static std::optional<TouchInteraction> touchInteractionForMouseInteraction(MouseInteraction interaction)
{
    switch (interaction) {
    case MouseInteraction::Down:
        return TouchInteraction::TouchDown;
    case MouseInteraction::Up:
        return TouchInteraction::LiftUp;
    case MouseInteraction::Move:
        return TouchInteraction::MoveTo;
    case MouseInteraction::SingleClick:
    case MouseInteraction::DoubleClick:
        break;
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}
#endif

static const String& pointerTypeForInputSource(SimulatedInputSourceType type)
{
    switch (type) {
    case SimulatedInputSourceType::Mouse:
        return WebCore::mousePointerEventType();
    case SimulatedInputSourceType::Touch:
        return WebCore::touchPointerEventType();
    case SimulatedInputSourceType::Pen:
        return WebCore::penPointerEventType();
    case SimulatedInputSourceType::Null:
    case SimulatedInputSourceType::Keyboard:
    case SimulatedInputSourceType::Wheel:
        break;
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

void SimulatedInputDispatcher::transitionInputSourceToState(SimulatedInputSource& inputSource, SimulatedInputSourceState& newState, AutomationCompletionHandler&& completionHandler)
{
    // Make cases and conditionals more readable by aliasing pre/post states as 'a' and 'b'.
    SimulatedInputSourceState& a = inputSource.state;
    SimulatedInputSourceState& b = newState;

    LOG(Automation, "SimulatedInputDispatcher[%p]: transition started between input source states: [%d.%d] --> %d.%d", this, m_keyframeIndex - 1, m_inputSourceStateIndex, m_keyframeIndex, m_inputSourceStateIndex);

    AutomationCompletionHandler eventDispatchFinished = [this, protectedThis = Ref { *this }, inputSource = Ref { inputSource }, &newState, runCompletionCount = m_runCompletionCount, completionHandler = WTF::move(completionHandler)](std::optional<AutomationCommandError> error) mutable {
        if (error || runCompletionCount != m_runCompletionCount) {
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
    case SimulatedInputSourceType::Touch:
    case SimulatedInputSourceType::Pen: {
        bool isTouch = inputSource.type == SimulatedInputSourceType::Touch;
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) && !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        RELEASE_ASSERT_NOT_REACHED();
#else
#if !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
        RELEASE_ASSERT(isTouch);
#elif !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        RELEASE_ASSERT(!isTouch);
#endif
        resolveLocation(currentLocationOfSource(a), b.location, b.origin.value_or(isTouch ? MouseMoveOrigin::Viewport : MouseMoveOrigin::Pointer), b.nodeHandle, [this, protectedThis = Ref { *this }, inputSource = Ref { inputSource }, &a, &b, pointerType = pointerTypeForInputSource(inputSource.type), isTouch, runCompletionCount = m_runCompletionCount, eventDispatchFinished = WTF::move(eventDispatchFinished)](std::optional<WebCore::IntPoint> location, std::optional<AutomationCommandError> error) mutable {
            if (error || runCompletionCount != m_runCompletionCount) {
                eventDispatchFinished(error);
                return;
            }

            if (!location) {
                eventDispatchFinished(AUTOMATION_COMMAND_ERROR_WITH_NAME(ElementNotInteractable));
                return;
            }

            b.location = location;
            // The "dispatch a pointer{Down,Up,Move} action" algorithms (§17.4 Dispatching Actions).
            if (b.mouseInteraction) {
                const auto stateTransitionIsNoop = [&a, &b] {
                    return std::tie(a.location, a.mouseInteraction, a.pressedMouseButton) == std::tie(b.location, b.mouseInteraction, b.pressedMouseButton);
                }();

                if (!stateTransitionIsNoop) {
                    if ((isTouch || m_options.interpolatePointerMoves) && b.mouseInteraction == MouseInteraction::Move && a.location == b.location) {
                        eventDispatchFinished(std::nullopt);
                        return;
                    }

                    auto button = b.mouseInteraction == MouseInteraction::Up && !b.pressedMouseButton ? a.pressedMouseButton : b.pressedMouseButton;

#if !LOG_DISABLED
                    String interactionName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(b.mouseInteraction.value());
                    String mouseButtonName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(button.value_or(MouseButton::None));
                    LOG_WITH_STREAM(Automation, stream << "SimulatedInputDispatcher["_s << this << "]: simulating "_s << pointerType << " "_s << interactionName << "[button="_s << mouseButtonName << "] @ ("_s << b.location.value().x() << ", "_s << b.location.value().y() << ") for transition to "_s << m_keyframeIndex << "."_s << m_inputSourceStateIndex);
#endif

                    if (isTouch) {
#if ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
                        auto touchInteraction = touchInteractionForMouseInteraction(b.mouseInteraction.value());
                        if (!touchInteraction) {
                            eventDispatchFinished(std::nullopt);
                            return;
                        }

                        std::optional<Seconds> duration = touchInteraction == TouchInteraction::MoveTo ? std::optional<Seconds>(a.duration.value_or(0_s)) : std::nullopt;
                        m_client.simulateTouchInteraction(protect(m_page), touchInteraction.value(), b.location.value(), duration, WTF::move(eventDispatchFinished));
#endif
                    } else {
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
                        if (m_options.interpolatePointerMoves && b.mouseInteraction == MouseInteraction::Move && b.duration && *b.duration > 0_s && a.location != b.location) {
                            startPointerMoveInterpolation(inputSource, currentLocationOfSource(a), b.location.value(), b.pressedMouseButton.value_or(MouseButton::None), pointerType, *b.duration);
                            eventDispatchFinished(std::nullopt);
                            return;
                        }

                        enqueueMouseInteraction(b.mouseInteraction.value(), button.value_or(MouseButton::None), b.location.value(), pointerType, WTF::move(eventDispatchFinished));
#else
                        UNUSED_VARIABLE(pointerType);
                        UNUSED_VARIABLE(button);
#endif
                    }
                } else
                    eventDispatchFinished({ });
            } else
                eventDispatchFinished(std::nullopt);
        });
#endif // !ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) && !ENABLE(WEBDRIVER_TOUCH_INTERACTIONS)
        break;
    }
    case SimulatedInputSourceType::Keyboard: {
#if !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        RELEASE_ASSERT_NOT_REACHED();
#else
        // The "dispatch a key{Down,Up} action" algorithms (§17.4 Dispatching Actions).
        //
        // A tick changes at most one key per input source, but the keyframe that resets every input
        // source (§17.6 Release Actions) releases everything still held at once, so several keys can
        // differ here. Character keys are released in reverse order of being pressed, as step 6 of
        // §17.6 requires.
        //
        // FIXME: §17.6 requires reverse press order across character and virtual keys together, but
        // pressedVirtualKeys does not record the order of presses, so virtual keys are released last.
        // That is observable: a character key pressed before Shift is reported shifted on release.
        Vector<KeyboardInteractionSpec> interactions;

        for (const auto& iter : b.pressedVirtualKeys) {
            if (a.pressedVirtualKeys.contains(iter.key))
                continue;
#if !LOG_DISABLED
            String virtualKeyName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(iter.value);
            LOG_WITH_STREAM(Automation, stream << "SimulatedInputDispatcher["_s << this << "]: simulating KeyPress[key="_s << virtualKeyName << "] for transition to "_s << m_keyframeIndex << "."_s << m_inputSourceStateIndex);
#endif
            interactions.append({ KeyboardInteraction::KeyPress, iter.value });
        }

        for (auto charKey : b.pressedCharKeys) {
            if (a.pressedCharKeys.contains(charKey))
                continue;
#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
            ASSERT_WITH_MESSAGE(WTF::numGraphemeClusters(charKey) <= 1, "A CharKey must either be a single unicode code point, a single grapheme cluster, or null.");
            LOG_WITH_STREAM(Automation, stream << "SimulatedInputDispatcher["_s << this << "]: simulating KeyPress[key="_s << charKey << "] for transition to "_s << m_keyframeIndex << "."_s << m_inputSourceStateIndex);
#else
            LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyPress[key=%c] for transition to %d.%d", this, charKey, m_keyframeIndex, m_inputSourceStateIndex);
#endif
            interactions.append({ KeyboardInteraction::KeyPress, charKey });
        }

        for (auto iter = a.pressedCharKeys.rbegin(); iter != a.pressedCharKeys.rend(); ++iter) {
            auto charKey = *iter;
            if (b.pressedCharKeys.contains(charKey))
                continue;
#if ENABLE(WEBDRIVER_KEYBOARD_GRAPHEME_CLUSTERS)
            ASSERT_WITH_MESSAGE(WTF::numGraphemeClusters(charKey) <= 1, "A CharKey must either be a single unicode code point, a single grapheme cluster, or null.");
            LOG_WITH_STREAM(Automation, stream << "SimulatedInputDispatcher["_s << this << "]: simulating KeyRelease[key="_s << charKey << "] for transition to "_s << m_keyframeIndex << "."_s << m_inputSourceStateIndex);
#else
            LOG(Automation, "SimulatedInputDispatcher[%p]: simulating KeyRelease[key=%c] for transition to %d.%d", this, charKey, m_keyframeIndex, m_inputSourceStateIndex);
#endif
            interactions.append({ KeyboardInteraction::KeyRelease, charKey });
        }

        for (const auto& iter : a.pressedVirtualKeys) {
            if (b.pressedVirtualKeys.contains(iter.key))
                continue;
#if !LOG_DISABLED
            String virtualKeyName = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(iter.value);
            LOG_WITH_STREAM(Automation, stream << "SimulatedInputDispatcher["_s << this << "]: simulating KeyRelease[key="_s << virtualKeyName << "] for transition to "_s << m_keyframeIndex << "."_s << m_inputSourceStateIndex);
#endif
            interactions.append({ KeyboardInteraction::KeyRelease, iter.value });
        }

        // Several differences outside the reset keyframe mean the client built its input source states
        // incorrectly; simulate them anyway rather than discarding input.
        ASSERT_WITH_MESSAGE(m_keyframes[m_keyframeIndex].isResetKeyFrame == SimulatedInputKeyFrame::IsResetKeyFrame::Yes || interactions.size() <= 1,
            "Only one key may differ at a time between two input source states, unless all input sources are being reset.");

        dispatchKeyboardInteractions(WTF::move(interactions), 0, WTF::move(eventDispatchFinished));
#endif // !ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
        break;
    }
    case SimulatedInputSourceType::Wheel:
#if !ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
        RELEASE_ASSERT_NOT_REACHED();
#else
        resolveLocation(currentLocationOfSource(a), b.location, b.origin.value_or(MouseMoveOrigin::Viewport), b.nodeHandle, [this, protectedThis = Ref { *this }, &a, &b, runCompletionCount = m_runCompletionCount, eventDispatchFinished = WTF::move(eventDispatchFinished)](std::optional<WebCore::IntPoint> location, std::optional<AutomationCommandError> error) mutable {
            if (error || runCompletionCount != m_runCompletionCount) {
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
            if (!m_options.scrollDeltasArePerAction && !aScrollDelta.isZero())
                usedScrollDelta.contract(aScrollDelta.width(), aScrollDelta.height());

            if (!usedScrollDelta.isZero()) {
                LOG(Automation, "SimulatedInputDispatcher[%p]: simulating Wheel by (%d, %d) for transition to %d.%d", this, usedScrollDelta.width(), usedScrollDelta.height(), m_keyframeIndex, m_inputSourceStateIndex);
                // FIXME: This does not interpolate mouse scrolls per the "perform a scroll" algorithm (§15.4.4 Wheel actions). <https://webkit.org/b/332566>
                m_client.simulateWheelInteraction(protect(m_page), b.location.value(), usedScrollDelta, WTF::move(eventDispatchFinished));
            } else
                eventDispatchFinished(std::nullopt);
        });
#endif // !ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
        break;
    }
}

#if ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)
void SimulatedInputDispatcher::dispatchKeyboardInteractions(Vector<KeyboardInteractionSpec>&& interactions, size_t nextIndex, AutomationCompletionHandler&& completionHandler)
{
    if (nextIndex == interactions.size()) {
        completionHandler(std::nullopt);
        return;
    }

    auto interaction = interactions[nextIndex].first;
    auto key = interactions[nextIndex].second;

    m_client.simulateKeyboardInteraction(protect(m_page), interaction, WTF::move(key), [this, protectedThis = Ref { *this }, interactions = WTF::move(interactions), nextIndex, runCompletionCount = m_runCompletionCount, completionHandler = WTF::move(completionHandler)](std::optional<AutomationCommandError> error) mutable {
        if (error || runCompletionCount != m_runCompletionCount) {
            completionHandler(error);
            return;
        }

        dispatchKeyboardInteractions(WTF::move(interactions), nextIndex + 1, WTF::move(completionHandler));
    });
}
#endif // ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS)

#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
// https://w3c.github.io/webdriver/#dfn-perform-a-pointer-move
static constexpr Seconds pointerMoveInterpolationInterval = 16_ms;
static constexpr unsigned maximumPointerMoveInterpolationSteps = 50;

void SimulatedInputDispatcher::startPointerMoveInterpolation(SimulatedInputSource& inputSource, const WebCore::IntPoint& start, const WebCore::IntPoint& end, MouseButton button, const String& pointerType, Seconds duration)
{
    auto stepInterval = std::max(pointerMoveInterpolationInterval, duration / maximumPointerMoveInterpolationSteps);

    LOG(Automation, "SimulatedInputDispatcher[%p]: interpolating pointer move from (%d, %d) to (%d, %d) over %.3f seconds", this, start.x(), start.y(), end.x(), end.y(), duration.value());

    if (m_pointerMoveInterpolations.isEmpty())
        m_pointerMoveInterpolationTimer.startOneShot(pointerMoveInterpolationInterval);

    m_pointerMoveInterpolations.append(PointerMoveInterpolation {
        inputSource,
        start,
        end,
        button,
        pointerType,
        m_keyFrameStartTime,
        duration,
        stepInterval,
        start,
        m_keyFrameStartTime,
        start,
    });
}

void SimulatedInputDispatcher::pointerMoveInterpolationTimerFired()
{
    auto now = MonotonicTime::now();
    Vector<std::tuple<Ref<SimulatedInputSource>, MouseButton, String, WebCore::IntPoint>> moves;
    for (auto& interpolation : borrow(m_pointerMoveInterpolations)) {
        auto elapsed = now - interpolation.startTime;
        bool isFinalStep = elapsed >= interpolation.duration;
        if (!isFinalStep && now - interpolation.lastDispatchTime < interpolation.stepInterval)
            continue;

        WebCore::IntPoint location = interpolation.end;
        if (!isFinalStep) {
            float progress = static_cast<float>(elapsed / interpolation.duration);
            WebCore::FloatPoint start(interpolation.start);
            location = flooredIntPoint(start + (WebCore::FloatPoint(interpolation.end) - start) * progress);
        }

        if (!isFinalStep && (location == interpolation.lastDispatchedLocation || location == interpolation.end))
            continue;

        interpolation.lastDispatchedLocation = location;
        interpolation.lastDispatchTime = now;

        LOG(Automation, "SimulatedInputDispatcher[%p]: simulating interpolated pointer move @ (%d, %d) for transition to %d.%d", this, location.x(), location.y(), m_keyframeIndex, m_inputSourceStateIndex);

        moves.append({ interpolation.inputSource, interpolation.button, interpolation.pointerType, location });
    }

    if (moves.isEmpty()) {
        m_pointerMoveInterpolationTimer.startOneShot(pointerMoveInterpolationInterval);
        return;
    }

    auto runCompletionCount = m_runCompletionCount;
    for (size_t i = 0; i < moves.size() && runCompletionCount == m_runCompletionCount; ++i) {
        auto [inputSource, button, pointerType, location] = moves[i];
        enqueueMouseInteraction(MouseInteraction::Move, button, location, pointerType, [this, protectedThis = Ref { *this }, runCompletionCount, inputSource = WTF::move(inputSource), location, isLastMove = i == moves.size() - 1](std::optional<AutomationCommandError> error) {
            if (runCompletionCount != m_runCompletionCount)
                return;

            if (error) {
                failKeyFrameTransition(WTF::move(*error));
                return;
            }

            auto index = m_pointerMoveInterpolations.findIf([&](auto& interpolation) {
                return interpolation.inputSource.ptr() == inputSource.ptr();
            });
            if (index != notFound)
                m_pointerMoveInterpolations[index].lastDeliveredLocation = location;

            if (!isLastMove)
                return;

            m_pointerMoveInterpolations.removeAllMatching([](auto& interpolation) {
                return interpolation.lastDeliveredLocation == interpolation.end;
            });

            if (!m_pointerMoveInterpolations.isEmpty()) {
                m_pointerMoveInterpolationTimer.startOneShot(pointerMoveInterpolationInterval);
                return;
            }

            finishKeyFrameTransitionIfComplete();
        });
    }
}

void SimulatedInputDispatcher::enqueueMouseInteraction(MouseInteraction interaction, MouseButton button, const WebCore::IntPoint& location, const String& pointerType, AutomationCompletionHandler&& completionHandler)
{
    m_pendingMouseInteractions.append(PendingMouseInteraction { interaction, button, location, pointerType, WTF::move(completionHandler) });
    dispatchNextMouseInteraction();
}

void SimulatedInputDispatcher::dispatchNextMouseInteraction()
{
    if (m_isDispatchingMouseInteraction || m_pendingMouseInteractions.isEmpty())
        return;

    m_isDispatchingMouseInteraction = true;

    auto interaction = m_pendingMouseInteractions.first().interaction;
    auto button = m_pendingMouseInteractions.first().button;
    auto location = m_pendingMouseInteractions.first().location;
    auto pointerType = m_pendingMouseInteractions.first().pointerType;
    m_client.simulateMouseInteraction(protect(m_page), interaction, button, location, pointerType, [this, protectedThis = Ref { *this }, runCompletionCount = m_runCompletionCount](std::optional<AutomationCommandError> error) {
        if (runCompletionCount != m_runCompletionCount)
            return;

        m_isDispatchingMouseInteraction = false;
        auto completed = m_pendingMouseInteractions.takeFirst();
        completed.completionHandler(error);
        dispatchNextMouseInteraction();
    });
}
#endif // ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)

void SimulatedInputDispatcher::run(std::optional<WebCore::FrameIdentifier> frameID, Vector<SimulatedInputKeyFrame>&& keyFrames, const HashMap<String, Ref<SimulatedInputSource>>& inputSources, SimulatedInputRunOptions options, AutomationCompletionHandler&& completionHandler)
{
    ASSERT(!isActive());
    if (isActive()) {
        completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
        return;
    }

    m_frameID = frameID;
    m_options = WTF::move(options);
    m_runCompletionHandler = WTF::move(completionHandler);

    // The "dispatch actions" algorithm (§17.4 Dispatching Actions).
    m_keyframes.reserveCapacity(keyFrames.size() + 1);
    m_keyframes.append(SimulatedInputKeyFrame::keyFrameFromStateOfInputSources(inputSources));
    m_keyframes.appendVector(WTF::move(keyFrames));

    LOG(Automation, "SimulatedInputDispatcher[%p]: starting input simulation using %zu keyframes", this, m_keyframes.size());

    transitionToNextKeyFrame();
}

void SimulatedInputDispatcher::cancel(AutomationCommandError error)
{
    if (!isActive())
        return;

    if (m_keyFrameTransitionCompletionHandler) {
        failKeyFrameTransition(WTF::move(error));
        ASSERT(!isActive());
        return;
    }

    finishDispatching(WTF::move(error));
}

void SimulatedInputDispatcher::finishDispatching(std::optional<AutomationCommandError> error)
{
    ASSERT(!m_keyFrameTransitionCompletionHandler);
    auto finish = std::exchange(m_runCompletionHandler, nullptr);
    if (!finish)
        return;

    m_keyFrameTransitionDurationTimer.stop();
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    m_pointerMoveInterpolationTimer.stop();
    ASSERT(m_pointerMoveInterpolations.isEmpty());
#endif

    LOG(Automation, "SimulatedInputDispatcher[%p]: finished all input simulation at [%u.%u]", this, m_keyframeIndex, m_inputSourceStateIndex);

    m_frameID = std::nullopt;
    m_options = { };
    m_keyframes.clear();
    ++m_runCompletionCount;
    m_keyframeIndex = 0;
    m_inputSourceStateIndex = 0;
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS)
    m_isDispatchingMouseInteraction = false;
    while (!m_pendingMouseInteractions.isEmpty()) {
        auto pending = m_pendingMouseInteractions.takeFirst();
        pending.completionHandler(AUTOMATION_COMMAND_ERROR_WITH_NAME(InternalError));
    }

    // It is unclear if this is desirable; see
    // https://github.com/w3c/webdriver/issues/1772 for ongoing discussion:
    m_client.clearDoubleClicks();
#endif
    finish(error);
}

} // namespace Webkit

#endif // ENABLE(WEBDRIVER_ACTIONS_API)
