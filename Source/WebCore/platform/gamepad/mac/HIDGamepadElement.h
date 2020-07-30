/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

#include "HIDElement.h"
#include "SharedGamepadValue.h"
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

enum class HIDInputType {
    ButtonPress,
    NotAButtonPress,
};

class HIDGamepadElement : public HIDElement {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~HIDGamepadElement() { }

    virtual double normalizedValue();
    virtual HIDInputType gamepadValueChanged(IOHIDValueRef) = 0;

    void refreshCurrentValue();

protected:
    HIDGamepadElement(const HIDElement&, SharedGamepadValue&);

    virtual bool isButton() const { return false; }
    virtual bool isAxis() const { return false; }

    SharedGamepadValue m_value;
};

class HIDGamepadButton final : public HIDGamepadElement {
public:
    HIDGamepadButton(const HIDElement& element, SharedGamepadValue& value)
        : HIDGamepadElement(element, value)
    {
    }

    bool isButton() const final { return true; }

private:
    HIDInputType gamepadValueChanged(IOHIDValueRef) override;
};

class HIDGamepadAxis final : public HIDGamepadElement {
public:
    HIDGamepadAxis(const HIDElement& element, SharedGamepadValue& value)
        : HIDGamepadElement(element, value)
    {
    }

    bool isAxis() const final { return true; }
    double normalizedValue() final;

private:
    HIDInputType gamepadValueChanged(IOHIDValueRef) override;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
