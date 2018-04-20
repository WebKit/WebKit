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

#pragma once

#include "WebEvent.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector { namespace Protocol { namespace Automation {
enum class ErrorMessage;
enum class KeyboardInteractionType;
enum class MouseInteraction;
enum class VirtualKey;
} } }

namespace WebKit {

class AutomationCommandError;
using AutomationCompletionHandler = WTF::CompletionHandler<void(std::optional<AutomationCommandError>)>;

class WebPageProxy;

using KeyboardInteraction = Inspector::Protocol::Automation::KeyboardInteractionType;
using VirtualKey = Inspector::Protocol::Automation::VirtualKey;
using CharKey = char; // For WebDriver, this only needs to support ASCII characters on 102-key keyboard.
using MouseButton = WebMouseEvent::Button;
using MouseInteraction = Inspector::Protocol::Automation::MouseInteraction;

struct SimulatedInputSourceState {
    std::optional<CharKey> pressedCharKey;
    std::optional<VirtualKey> pressedVirtualKey;
    std::optional<MouseButton> pressedMouseButton;
    std::optional<WebCore::IntPoint> location;
    std::optional<Seconds> duration;

    static SimulatedInputSourceState emptyState() { return SimulatedInputSourceState(); }
};

struct SimulatedInputSource : public RefCounted<SimulatedInputSource> {
public:
    enum class Type {
        Null, // Used to induce a minimum duration.
        Keyboard,
        Mouse,
        Touch,
    };

    Type type;

    // The last state associated with this input source.
    SimulatedInputSourceState state;

    static Ref<SimulatedInputSource> create(Type type)
    {
        return adoptRef(*new SimulatedInputSource(type));
    }

private:
    SimulatedInputSource(Type type)
        : type(type)
        , state(SimulatedInputSourceState::emptyState())
    { }
};

struct SimulatedInputKeyFrame {
public:
    using StateEntry = std::pair<SimulatedInputSource&, SimulatedInputSourceState>;

    explicit SimulatedInputKeyFrame(Vector<StateEntry>&&);
    Seconds maximumDuration() const;

    static SimulatedInputKeyFrame keyFrameFromStateOfInputSources(HashSet<Ref<SimulatedInputSource>>&);
    static SimulatedInputKeyFrame keyFrameToResetInputSources(HashSet<Ref<SimulatedInputSource>>&);

    Vector<StateEntry> states;
};

class SimulatedInputDispatcher : public RefCounted<SimulatedInputDispatcher> {
    WTF_MAKE_NONCOPYABLE(SimulatedInputDispatcher);
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void simulateMouseInteraction(WebPageProxy&, MouseInteraction, WebMouseEvent::Button, const WebCore::IntPoint& locationInView, AutomationCompletionHandler&&) = 0;
        virtual void simulateKeyboardInteraction(WebPageProxy&, KeyboardInteraction, std::optional<VirtualKey>, std::optional<CharKey>, AutomationCompletionHandler&&) = 0;
    };

    static Ref<SimulatedInputDispatcher> create(WebPageProxy& page, SimulatedInputDispatcher::Client& client)
    {
        return adoptRef(*new SimulatedInputDispatcher(page, client));
    }

    ~SimulatedInputDispatcher();

    void run(Vector<SimulatedInputKeyFrame>&& keyFrames, HashSet<Ref<SimulatedInputSource>>& inputSources, AutomationCompletionHandler&&);
    void cancel();

    bool isActive() const;

private:
    SimulatedInputDispatcher(WebPageProxy&, SimulatedInputDispatcher::Client&);

    void transitionToNextKeyFrame();
    void transitionBetweenKeyFrames(const SimulatedInputKeyFrame&, const SimulatedInputKeyFrame&, AutomationCompletionHandler&&);

    void transitionToNextInputSourceState();
    void transitionInputSourceToState(SimulatedInputSource&, const SimulatedInputSourceState& newState, AutomationCompletionHandler&&);
    void finishDispatching(std::optional<AutomationCommandError>);

    void keyFrameTransitionDurationTimerFired();
    bool isKeyFrameTransitionComplete() const;

    WebPageProxy& m_page;
    SimulatedInputDispatcher::Client& m_client;

    AutomationCompletionHandler m_runCompletionHandler;
    AutomationCompletionHandler m_keyFrameTransitionCompletionHandler;
    RunLoop::Timer<SimulatedInputDispatcher> m_keyFrameTransitionDurationTimer;

    Vector<SimulatedInputKeyFrame> m_keyframes;
    HashSet<Ref<SimulatedInputSource>> m_inputSources;

    // The position within m_keyframes.
    unsigned m_keyframeIndex { 0 };

    // The position within the input source state vector at m_keyframes[m_keyframeIndex].
    // Events that reflect input source state transitions are dispatched serially based on this order.
    unsigned m_inputSourceStateIndex { 0 };
};

} // namespace WebKit
