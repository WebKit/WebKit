/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"

#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
#include "AccessibilitySupportSPI.h"
#endif

namespace WebKit {

#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
enum class WebKitAXValueState : int {
    AXValueStateInvalid = -2,
    AXValueStateEmpty = -1,
    AXValueStateOff,
    AXValueStateOn
};

inline WebKitAXValueState toWebKitAXValueState(AXValueState value)
{
    switch (value) {
    case AXValueState::AXValueStateInvalid:
        return WebKitAXValueState::AXValueStateInvalid;
    case AXValueState::AXValueStateEmpty:
        return WebKitAXValueState::AXValueStateEmpty;
    case AXValueState::AXValueStateOff:
        return WebKitAXValueState::AXValueStateOff;
    case AXValueState::AXValueStateOn:
        return WebKitAXValueState::AXValueStateOn;
    }

    ASSERT_NOT_REACHED();
    return WebKitAXValueState::AXValueStateInvalid;
}

inline AXValueState fromWebKitAXValueState(WebKitAXValueState value)
{
    switch (value) {
    case WebKitAXValueState::AXValueStateInvalid:
        return AXValueState::AXValueStateInvalid;
    case WebKitAXValueState::AXValueStateEmpty:
        return AXValueState::AXValueStateEmpty;
    case WebKitAXValueState::AXValueStateOff:
        return AXValueState::AXValueStateOff;
    case WebKitAXValueState::AXValueStateOn:
        return AXValueState::AXValueStateOn;
    }

    ASSERT_NOT_REACHED();
    return AXValueState::AXValueStateInvalid;
}
#endif

inline constexpr bool initialImageAnimationEnabled = true;
inline constexpr bool initialShouldEnhanceTextLegibilityOverall = false;
inline constexpr bool initialPrefersNonBlinkingCursor = false;
#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
inline constexpr WebKitAXValueState initialPerAppSettingsState = WebKitAXValueState::AXValueStateEmpty;
#endif

struct AccessibilityPreferences {
#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
    WebKitAXValueState reduceMotionEnabled { initialPerAppSettingsState };
    WebKitAXValueState increaseButtonLegibility { initialPerAppSettingsState };
    WebKitAXValueState enhanceTextLegibility { initialPerAppSettingsState };
    WebKitAXValueState darkenSystemColors { initialPerAppSettingsState };
    WebKitAXValueState invertColorsEnabled { initialPerAppSettingsState };

#endif
    bool imageAnimationEnabled { initialImageAnimationEnabled };
    bool enhanceTextLegibilityOverall { initialShouldEnhanceTextLegibilityOverall };
#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    bool prefersNonBlinkingCursor { initialPrefersNonBlinkingCursor };
#endif
};

} // namespace WebKit

namespace AXPreferenceHelpers {
#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
WebKit::WebKitAXValueState reduceMotionEnabled();
WebKit::WebKitAXValueState increaseButtonLegibility();
WebKit::WebKitAXValueState enhanceTextLegibility();
WebKit::WebKitAXValueState darkenSystemColors();
WebKit::WebKitAXValueState invertColorsEnabled();
#endif // HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)

bool imageAnimationEnabled();
bool enhanceTextLegibilityOverall();

#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
bool prefersNonBlinkingCursor();
#endif
} // namespace AXPreferenceHelpers
