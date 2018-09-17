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
#include "UIGamepad.h"

#if ENABLE(GAMEPAD)

#include "GamepadData.h"
#include <WebCore/PlatformGamepad.h>

namespace WebKit {
using namespace WebCore;

UIGamepad::UIGamepad(WebCore::PlatformGamepad& platformGamepad)
    : m_index(platformGamepad.index())
    , m_id(platformGamepad.id())
    , m_lastUpdateTime(platformGamepad.lastUpdateTime())
{
    m_axisValues.resize(platformGamepad.axisValues().size());
    m_buttonValues.resize(platformGamepad.buttonValues().size());

    updateFromPlatformGamepad(platformGamepad);
}

void UIGamepad::updateFromPlatformGamepad(WebCore::PlatformGamepad& platformGamepad)
{
    ASSERT(m_index == platformGamepad.index());
    ASSERT(m_axisValues.size() == platformGamepad.axisValues().size());
    ASSERT(m_buttonValues.size() == platformGamepad.buttonValues().size());

    m_axisValues = platformGamepad.axisValues();
    m_buttonValues = platformGamepad.buttonValues();
    m_lastUpdateTime = platformGamepad.lastUpdateTime();
}

GamepadData UIGamepad::condensedGamepadData() const
{
    return { m_index, m_axisValues, m_buttonValues, m_lastUpdateTime };
}

GamepadData UIGamepad::fullGamepadData() const
{
    return { m_index, m_id, m_axisValues, m_buttonValues, m_lastUpdateTime };
}


}

#endif // ENABLE(GAMEPAD)
