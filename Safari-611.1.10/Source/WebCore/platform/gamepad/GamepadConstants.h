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

#pragma once

#if ENABLE(GAMEPAD)

namespace WTF {
class String;
};

namespace WebCore {

// Buttons in the "standard" gamepad layout in the Web Gamepad spec
// https://www.w3.org/TR/gamepad/#dfn-standard-gamepad-layout
enum class GamepadButtonRole : uint8_t {
    RightClusterBottom = 0,
    RightClusterRight = 1,
    RightClusterLeft = 2,
    RightClusterTop = 3,
    LeftShoulderFront = 4,
    RightShoulderFront = 5,
    LeftShoulderBack = 6,
    RightShoulderBack = 7,
    CenterClusterLeft = 8,
    CenterClusterRight = 9,
    LeftStick = 10,
    RightStick = 11,
    LeftClusterTop = 12,
    LeftClusterBottom = 13,
    LeftClusterLeft = 14,
    LeftClusterRight = 15,
    CenterClusterCenter = 16,
    Nonstandard1 = 17,
    Nonstandard2 = 18,
};

extern const size_t numberOfStandardGamepadButtonsWithoutHomeButton;
extern const size_t numberOfStandardGamepadButtonsWithHomeButton;
extern const GamepadButtonRole maximumGamepadButton;

const WTF::String& standardGamepadMappingString();

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
