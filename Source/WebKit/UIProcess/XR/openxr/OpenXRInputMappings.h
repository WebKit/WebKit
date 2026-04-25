/*
 * Copyright (C) 2025 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
