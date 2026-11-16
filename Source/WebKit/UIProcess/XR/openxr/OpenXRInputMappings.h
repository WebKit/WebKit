/*
 * Copyright (C) 2025-2026 Igalia, S.L.
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

#if ENABLE(WEBXR) && USE(OPENXR)

#include <array>
#include <wtf/text/WTFString.h>

namespace WebKit {

using OpenXRProfileId = ASCIILiteral;
using OpenXRButtonPath = ASCIILiteral;

enum class OpenXRButtonType {
    Trigger,
    Squeeze,
    Touchpad,
    Thumbstick,
    Thumbrest,
    ButtonA,
    ButtonB
};

constexpr std::array<OpenXRButtonType, 7> openXRButtonTypes {
    OpenXRButtonType::Trigger, OpenXRButtonType::Squeeze, OpenXRButtonType::Touchpad, OpenXRButtonType::Thumbstick, OpenXRButtonType::Thumbrest,
    OpenXRButtonType::ButtonA, OpenXRButtonType::ButtonB
};

constexpr ASCIILiteral s_pathSelect { "/input/select"_s };
constexpr ASCIILiteral s_pathPinchExt { "/input/pinch_ext"_s };
constexpr ASCIILiteral s_pathGraspExt { "/input/grasp_ext"_s };

constexpr ASCIILiteral s_pathActionClick { "/click"_s };
constexpr ASCIILiteral s_pathActionTouch { "/touch"_s };
constexpr ASCIILiteral s_pathActionValue { "/value"_s };

inline String buttonTypeToString(OpenXRButtonType type)
{
    switch (type) {
    case OpenXRButtonType::Trigger: return "trigger"_s;
    case OpenXRButtonType::Squeeze: return "squeeze"_s;
    case OpenXRButtonType::Touchpad: return "touchpad"_s;
    case OpenXRButtonType::Thumbstick: return "thumbstick"_s;
    case OpenXRButtonType::Thumbrest: return "thumbrest"_s;
    case OpenXRButtonType::ButtonA: return "buttona"_s;
    case OpenXRButtonType::ButtonB: return "buttonb"_s;

    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

enum OpenXRButtonFlags {
    Click = 1u << 0,
    Touch = 1u << 1,
    Value  = 1u << 2,
};

enum class OpenXRHandFlags {
    Left = 1u << 0,
    Right = 1u << 1,
    Both = Left | Right
};

struct OpenXRButton {
    OpenXRButtonType type;
    OpenXRButtonPath path;
    OpenXRButtonFlags flags;
    OpenXRHandFlags hand;
};

enum class OpenXRAxisType {
    Touchpad,
    Thumbstick
};

constexpr std::array<OpenXRAxisType, 2> openXRAxisTypes {
    OpenXRAxisType::Touchpad, OpenXRAxisType::Thumbstick
};

inline String axisTypetoString(OpenXRAxisType type)
{
    switch (type) {
    case OpenXRAxisType::Touchpad: return "touchpad"_s;
    case OpenXRAxisType::Thumbstick: return "thumbstick"_s;
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

struct OpenXRAxis {
    OpenXRAxisType type;
    OpenXRButtonPath path;
};

struct OpenXRInteractionProfile {
    const ASCIILiteral path;
    std::span<const OpenXRProfileId> profileIds;
    std::span<const OpenXRButton> buttons;
    std::span<const OpenXRAxis> axes;
};

constexpr ASCIILiteral handInteractionProfilePath { "/interaction_profiles/ext/hand_interaction_ext"_s };
constexpr std::array<OpenXRProfileId, 3> handInteractionProfileIds { "generic-hand-select-grasp", "generic-hand-select", "generic-hand" };
constexpr std::array<OpenXRButton, 2> handInteractionProfileButtons {
    OpenXRButton { .type = OpenXRButtonType::Trigger, .path = s_pathPinchExt, .flags = OpenXRButtonFlags::Value, .hand = OpenXRHandFlags::Both },
    OpenXRButton { .type = OpenXRButtonType::Squeeze, .path = s_pathGraspExt, .flags = OpenXRButtonFlags::Value, .hand = OpenXRHandFlags::Both },
};

constexpr OpenXRInteractionProfile handInteractionProfile {
    handInteractionProfilePath,
    handInteractionProfileIds,
    handInteractionProfileButtons,
    { }
};

// Default fallback when there isn't a specific controller binding.
constexpr ASCIILiteral khrSimpleControllerPath { "/interaction_profiles/khr/simple_controller"_s };
constexpr std::array<ASCIILiteral, 1> khrSimpleProfileIds { "generic-button"_s };

constexpr std::array<OpenXRButton, 1> khrSimpleButtons {
    OpenXRButton { .type = OpenXRButtonType::Trigger, .path = s_pathSelect, .flags = OpenXRButtonFlags::Click, .hand = OpenXRHandFlags::Both }
};

constexpr OpenXRInteractionProfile khrSimpleControllerProfile {
    khrSimpleControllerPath,
    khrSimpleProfileIds,
    khrSimpleButtons,
    { }
};

constexpr std::array<OpenXRInteractionProfile, 2> openXRInteractionProfiles { handInteractionProfile, khrSimpleControllerProfile };

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
