/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "WebXRGamepad.h"

#if ENABLE(WEBXR) && ENABLE(GAMEPAD)

#include "Gamepad.h"
#include "GamepadConstants.h"
#include "XRTargetRayMode.h"

namespace WebCore {

// https://immersive-web.github.io/webxr-gamepads-module/#gamepad-differences
// Gamepad's index attribute must be -1.
constexpr int DefaultXRGamepadId = -1;

// https://immersive-web.github.io/webxr-gamepads-module/#gamepad-differences
WebXRGamepad::WebXRGamepad(double timestamp, double connectTime, const PlatformXR::FrameData::InputSource& source)
    : PlatformGamepad(DefaultXRGamepadId)
{
    m_lastUpdateTime = MonotonicTime::fromRawSeconds(Seconds::fromMilliseconds(timestamp).value());
    m_connectTime = MonotonicTime::fromRawSeconds(Seconds::fromMilliseconds(connectTime).value());
    // In order to report a mapping of "xr-standard" the device MUST report a targetRayMode of "tracked-pointer" and MUST have a non-null gripSpace.
    // It MUST have at least one primary trigger, separate from any touchpads or thumbsticks
    if (source.targetRayMode == XRTargetRayMode::TrackedPointer && !source.buttons.isEmpty() && source.gripOrigin)
        m_mapping = xrStandardGamepadMappingString();
    m_axes = source.axes.map([](auto value) {
        return SharedGamepadValue(value);
    });
    m_buttons = source.buttons.map([](auto& value) {
        return SharedGamepadValue(value.pressedValue);
    });
}

} // namespace WebCore

#endif // ENABLE(WEBXR) && ENABLE(GAMEPAD)
