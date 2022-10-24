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

#include "config.h"
#include "AccessibilityPreferences.h"

namespace IPC {

void ArgumentCoder<WebKit::AccessibilityPreferences>::encode(Encoder& encoder, const WebKit::AccessibilityPreferences& preferences)
{
#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
    encoder << preferences.reduceMotionEnabled;
    encoder << preferences.increaseButtonLegibility;
    encoder << preferences.enhanceTextLegibility;
    encoder << preferences.darkenSystemColors;
    encoder << preferences.invertColorsEnabled;
#endif
    encoder << preferences.imageAnimationEnabled;
    encoder << preferences.enhanceTextLegibilityOverall;
}

std::optional<WebKit::AccessibilityPreferences> ArgumentCoder<WebKit::AccessibilityPreferences>::decode(Decoder& decoder)
{
    WebKit::AccessibilityPreferences preferences;
#if HAVE(PER_APP_ACCESSIBILITY_PREFERENCES)
    if (!decoder.decode(preferences.reduceMotionEnabled))
        return std::nullopt;
    if (!decoder.decode(preferences.increaseButtonLegibility))
        return std::nullopt;
    if (!decoder.decode(preferences.enhanceTextLegibility))
        return std::nullopt;
    if (!decoder.decode(preferences.darkenSystemColors))
        return std::nullopt;
    if (!decoder.decode(preferences.invertColorsEnabled))
        return std::nullopt;
#endif
    if (!decoder.decode(preferences.imageAnimationEnabled))
        return std::nullopt;
    if (!decoder.decode(preferences.enhanceTextLegibilityOverall))
        return std::nullopt;
    return preferences;
}

} // namespace IPC
