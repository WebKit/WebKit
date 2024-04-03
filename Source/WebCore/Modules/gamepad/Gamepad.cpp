/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "Gamepad.h"

#if ENABLE(GAMEPAD)

#include "GamepadButton.h"
#include "GamepadHapticActuator.h"
#include "PlatformGamepad.h"
#include "ScriptExecutionContext.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

Gamepad::Gamepad(Document* document, const PlatformGamepad& platformGamepad)
    : m_id(platformGamepad.id())
    , m_index(platformGamepad.index())
    , m_connected(true)
    , m_timestamp(platformGamepad.lastUpdateTime())
    , m_mapping(platformGamepad.mapping())
    , m_supportedEffectTypes(platformGamepad.supportedEffectTypes())
    , m_axes(platformGamepad.axisValues().size(), 0.0)
    , m_vibrationActuator(platformGamepad.supportedEffectTypes().contains(GamepadHapticEffectType::DualRumble) ? RefPtr { GamepadHapticActuator::create(document, GamepadHapticActuator::Type::DualRumble, *this) } : nullptr)
{
    unsigned buttonCount = platformGamepad.buttonValues().size();
    m_buttons = Vector<Ref<GamepadButton>>(buttonCount, [](size_t) {
        return GamepadButton::create();
    });
}

Gamepad::~Gamepad() = default;

const Vector<double>& Gamepad::axes() const
{
    return m_axes;
}

const Vector<Ref<GamepadButton>>& Gamepad::buttons() const
{
    return m_buttons;
}

void Gamepad::updateFromPlatformGamepad(const PlatformGamepad& platformGamepad)
{
    for (unsigned i = 0; i < m_axes.size(); ++i)
        m_axes[i] = platformGamepad.axisValues()[i].value();
    for (unsigned i = 0; i < m_buttons.size(); ++i)
        m_buttons[i]->setValue(platformGamepad.buttonValues()[i].value());

    m_timestamp = platformGamepad.lastUpdateTime();
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
