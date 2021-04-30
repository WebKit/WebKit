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

#include "config.h"
#include "GamepadConstants.h"

#if ENABLE(GAMEPAD)

#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

const GamepadButtonRole maximumGamepadButton = GamepadButtonRole::CenterClusterCenter;
const size_t numberOfStandardGamepadButtonsWithoutHomeButton = static_cast<size_t>(maximumGamepadButton);
const size_t numberOfStandardGamepadButtonsWithHomeButton = numberOfStandardGamepadButtonsWithoutHomeButton + 1;

const WTF::String& standardGamepadMappingString()
{
    static NeverDestroyed<String> standardGamepadMapping = "standard";
    return standardGamepadMapping;
}

#if ENABLE(WEBXR)
// https://immersive-web.github.io/webxr-gamepads-module/#dom-gamepadmappingtype-xr-standard
const WTF::String& xrStandardGamepadMappingString()
{
    static NeverDestroyed<String> xrStandardGamepadMapping = "xr-standard";
    return xrStandardGamepadMapping;
}
#endif


} // namespace WebCore

#endif // ENABLE(GAMEPAD)
