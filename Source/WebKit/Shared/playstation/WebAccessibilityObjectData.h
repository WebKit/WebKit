/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AccessibilityObject.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/IntRect.h>
#include <wtf/OptionSet.h>

namespace WebKit {

class WebAccessibilityObjectData {
public:
    explicit WebAccessibilityObjectData() { };
    WebAccessibilityObjectData(WebCore::AXCoreObject*);
    ~WebAccessibilityObjectData() { };

    void set(WebCore::AXCoreObject*);

    std::optional<WebCore::FrameIdentifier> webFrameID() const { return m_webFrameID; }
    WebCore::AccessibilityRole role() const { return m_role; };
    String title() const { return m_title; };
    String description() const { return m_description; };
    String helpText() const { return m_helpText; };
    URL url() const { return m_url; };
    WebCore::IntRect rect() const { return m_rect; };

    WebCore::AccessibilityButtonState buttonState() const { return m_buttonState; };
    String value() const { return m_value; };
    bool isFocused() const { return m_state.contains(State::Focused); };
    bool isDisabled() const { return m_state.contains(State::Unavailable); };
    bool isSelected() const { return m_state.contains(State::Selected); };
    bool isVisited() const { return m_state.contains(State::Visited); };
    bool isLinked() const { return m_state.contains(State::Linked); };

    enum class State : uint8_t {
        Focused = 1 << 0,
        Unavailable = 1 << 1,
        Selected = 1 << 2,
        Visited = 1 << 3,
        Linked = 1 << 4,
    };

private:
    friend struct IPC::ArgumentCoder<WebKit::WebAccessibilityObjectData, void>;

    String getAccessibilityTitle(WebCore::AXCoreObject*, Vector<WebCore::AccessibilityText>&);
    String getAccessibilityDescription(WebCore::AXCoreObject*, Vector<WebCore::AccessibilityText>&);
    String getAccessibilityHelpText(WebCore::AXCoreObject*, Vector<WebCore::AccessibilityText>&);

    bool isImageLinked(WebCore::AXCoreObject*);
    bool isImageVisited(WebCore::AXCoreObject*);

    void setValue(WebCore::AXCoreObject*);
    void setState(WebCore::AXCoreObject*);

    // common
    std::optional<WebCore::FrameIdentifier> m_webFrameID;
    WebCore::AccessibilityRole m_role { WebCore::AccessibilityRole::Unknown };
    String m_title;
    String m_description;
    String m_helpText;
    URL m_url;
    WebCore::IntRect m_rect;

    // checkbox or radio button state
    WebCore::AccessibilityButtonState m_buttonState { WebCore::AccessibilityButtonState::Off };
    String m_value;

    OptionSet<State> m_state;
};

} // namespace WebKit
