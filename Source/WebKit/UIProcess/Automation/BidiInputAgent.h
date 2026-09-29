/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEBDRIVER_BIDI)

#include "WebDriverBidiBackendDispatchers.h"
#include <wtf/HashMap.h>
#include <wtf/JSONValues.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit::BidiInput {

class State;

enum class SourceType : uint8_t { None, Key, Pointer, Wheel };
enum class PointerType : uint8_t { Mouse, Pen, Touch };
enum class ActionType : uint8_t { Pause, KeyDown, KeyUp, PointerDown, PointerUp, PointerMove, PointerCancel, Scroll };
enum class OriginType : uint8_t { Viewport, Pointer, Element };

struct PointerProperties {
    std::optional<uint64_t> width;
    std::optional<uint64_t> height;
    std::optional<double> pressure;
    std::optional<double> tangentialPressure;
    std::optional<int> tiltX;
    std::optional<int> tiltY;
    std::optional<int> twist;
    std::optional<double> altitudeAngle;
    std::optional<double> azimuthAngle;
};

// One action object from the spec (https://w3c.github.io/webdriver/#dfn-action-object).
struct Action {
    String id;
    SourceType sourceType { SourceType::None };
    PointerType pointerType { PointerType::Mouse };
    ActionType type { ActionType::Pause };
    std::optional<uint64_t> duration;
    String value; // keyDown / keyUp: a single grapheme cluster.
    uint64_t button { 0 };
    double x { 0 };
    double y { 0 };
    int64_t deltaX { 0 };
    int64_t deltaY { 0 };
    OriginType origin { OriginType::Viewport };
    String originSharedId; // Only for OriginType::Element.
    PointerProperties properties;
};

using ActionsByTick = Vector<Vector<Action>>;

struct Source {
    SourceType type;
    PointerType pointerType { PointerType::Mouse };
};

// https://w3c.github.io/webdriver/#dfn-input-state — one per (session, top-level traversable).
class State {
    WTF_MAKE_TZONE_ALLOCATED(State);
public:
    const Source* find(const String& id) const;
    void add(const String& id, Source);

private:
    HashMap<String, Source> m_inputStateMap;
};

} // namespace WebKit::BidiInput

namespace WebKit {

class WebAutomationSession;

class BidiInputAgent final : public Inspector::BidiInputBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiInputAgent);
public:
    BidiInputAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiInputAgent() override;

    void performActions(const String& context, Ref<JSON::Array>&& actions, Inspector::CommandCallback<void>&&) override;
    void releaseActions(const String& context, Inspector::CommandCallback<void>&&) override;
    void setFiles(const String& context, Ref<JSON::Object>&& element, Ref<JSON::Array>&& files, Inspector::CommandCallback<void>&&) override;

private:
    // https://w3c.github.io/webdriver/#dfn-get-the-input-state — keyed by top-level context handle.
    BidiInput::State& inputStateForTopLevelContext(const String& pageHandle);
    // https://w3c.github.io/webdriver/#dfn-reset-the-input-state
    void resetInputState(const String& pageHandle);

    WeakPtr<WebAutomationSession> m_session;
    Ref<Inspector::BidiInputBackendDispatcher> m_inputDomainDispatcher;
    HashMap<String, std::unique_ptr<BidiInput::State>> m_inputStates;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
