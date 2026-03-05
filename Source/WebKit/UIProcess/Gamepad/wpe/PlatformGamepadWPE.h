/*
 * Copyright (C) 2025 Igalia S.L.
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

#if ENABLE(GAMEPAD) && ENABLE(WPE_PLATFORM)
#include <WebCore/PlatformGamepad.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _WPEGamepad WPEGamepad;

namespace WebKit {

class PlatformGamepadWPE final : public WebCore::PlatformGamepad {
    WTF_MAKE_TZONE_ALLOCATED(PlatformGamepadWPE);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PlatformGamepadWPE);
public:
    PlatformGamepadWPE(WPEGamepad*, unsigned index);
    virtual ~PlatformGamepadWPE();

private:
    const Vector<WebCore::SharedGamepadValue>& buttonValues() const final { return m_buttonValues; }
    const Vector<WebCore::SharedGamepadValue>& axisValues() const final { return m_axisValues; }

    void buttonEvent(size_t button, bool isPressed);
    void axisEvent(size_t axis, double value);

    GRefPtr<WPEGamepad> m_gamepad;
    Vector<WebCore::SharedGamepadValue> m_buttonValues;
    Vector<WebCore::SharedGamepadValue> m_axisValues;
};

} // namespace WebKit

#endif // ENABLE(GAMEPAD) && ENABLE(WPE_PLATFORM)
