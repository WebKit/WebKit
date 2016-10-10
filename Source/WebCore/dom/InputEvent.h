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

#pragma once

#include "UIEvent.h"

namespace WebCore {

class DOMWindow;
class DataTransfer;

struct InputEventInit : UIEventInit {
    String data;
    bool isComposing;
};

class InputEvent final : public UIEvent {
public:
    static Ref<InputEvent> create(const AtomicString& eventType, const String& inputType, bool canBubble, bool cancelable, DOMWindow* view, const String& data, int detail)
    {
        return adoptRef(*new InputEvent(eventType, inputType, canBubble, cancelable, view, data, detail));
    }

    static Ref<InputEvent> createForBindings(const AtomicString& type, const InputEventInit& initializer)
    {
        return adoptRef(*new InputEvent(type, initializer));
    }

    InputEvent(const AtomicString& eventType, const String& inputType, bool canBubble, bool cancelable, DOMWindow*, const String& data, int detail);
    InputEvent(const AtomicString& eventType, const InputEventInit&);

    virtual ~InputEvent() { }

    bool isInputEvent() const override { return true; }
    EventInterface eventInterface() const final { return InputEventInterfaceType; }
    const String& inputType() const { return m_inputType; }
    const String& data() const { return m_data; }

private:
    String m_inputType;
    String m_data;
};

} // namespace WebCore
