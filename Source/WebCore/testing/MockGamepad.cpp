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
#include "MockGamepad.h"

#if ENABLE(GAMEPAD)

namespace WebCore {

MockGamepad::MockGamepad(unsigned index, const String& gamepadID, unsigned axisCount, unsigned buttonCount)
    : PlatformGamepad(index)
{
    m_connectTime = m_lastUpdateTime = MonotonicTime::now();
    updateDetails(gamepadID, axisCount, buttonCount);
}

void MockGamepad::updateDetails(const String& gamepadID, unsigned axisCount, unsigned buttonCount)
{
    m_id = gamepadID;
    m_axisValues = Vector<double>(axisCount, 0.0);
    m_buttonValues = Vector<double>(buttonCount, 0.0);
    m_lastUpdateTime = MonotonicTime::now();
}

bool MockGamepad::setAxisValue(unsigned index, double value)
{
    if (index >= m_axisValues.size()) {
        LOG_ERROR("MockGamepad (%u): Attempt to set value on axis %u which doesn't exist", m_index, index);
        return false;
    }

    m_axisValues[index] = value;
    m_lastUpdateTime = MonotonicTime::now();
    return true;
}

bool MockGamepad::setButtonValue(unsigned index, double value)
{
    if (index >= m_buttonValues.size()) {
        LOG_ERROR("MockGamepad (%u): Attempt to set value on button %u which doesn't exist", m_index, index);
        return false;
    }

    m_buttonValues[index] = value;
    m_lastUpdateTime = MonotonicTime::now();
    return true;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
