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

#if ENABLE(GAMEPAD) && PLATFORM(GTK)

#include <WebCore/PlatformGamepad.h>
#include <WebCore/SharedGamepadValue.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class AutomationGamepad final : public WebCore::PlatformGamepad {
    WTF_MAKE_TZONE_ALLOCATED(AutomationGamepad);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AutomationGamepad);
public:
    AutomationGamepad(unsigned index, const String& gamepadID, const String& mapping, unsigned axisCount, unsigned buttonCount, bool supportsDualRumble, bool wasConnected);

    const Vector<WebCore::SharedGamepadValue>& axisValues() const LIFETIME_BOUND final { return m_axisValues; }
    const Vector<WebCore::SharedGamepadValue>& buttonValues() const LIFETIME_BOUND final { return m_buttonValues; }

    void updateDetails(const String& gamepadID, const String& mapping, unsigned axisCount, unsigned buttonCount, bool supportsDualRumble, bool wasConnected);
    bool setAxisValue(unsigned index, double value);
    bool setButtonValue(unsigned index, double value);
    bool wasConnected() { return m_wasConnected; }
    void setLastUpdateTime(MonotonicTime time) { m_lastUpdateTime = time; }

private:
    Vector<WebCore::SharedGamepadValue> m_axisValues;
    Vector<WebCore::SharedGamepadValue> m_buttonValues;
    bool m_wasConnected;
};

}

#endif // ENABLE(GAMEPAD) && PLATFORM(GTK)
