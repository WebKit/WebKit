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

#include "config.h"
#include "WebGamepad.h"

#if ENABLE(GAMEPAD)

#include "GamepadData.h"
#include "Logging.h"


namespace WebKit {

WebGamepad::WebGamepad(const GamepadData& gamepadData)
    : PlatformGamepad(gamepadData.index())
{
    LOG(Gamepad, "Connecting WebGamepad %u", gamepadData.index());

    m_id = gamepadData.id();
    m_axisValues.resize(gamepadData.axisValues().size());
    m_buttonValues.resize(gamepadData.buttonValues().size());

    updateValues(gamepadData);
}

const Vector<double>& WebGamepad::axisValues() const
{
    return m_axisValues;
}

const Vector<double>& WebGamepad::buttonValues() const
{
    return m_buttonValues;
}

void WebGamepad::updateValues(const GamepadData& gamepadData)
{
    ASSERT(!gamepadData.isNull());
    ASSERT(gamepadData.index() == index());
    ASSERT(m_axisValues.size() == gamepadData.axisValues().size());
    ASSERT(m_buttonValues.size() == gamepadData.buttonValues().size());

    m_axisValues = gamepadData.axisValues();
    m_buttonValues = gamepadData.buttonValues();

    m_lastUpdateTime = gamepadData.lastUpdateTime();
}

}

#endif // ENABLE(GAMEPAD)
