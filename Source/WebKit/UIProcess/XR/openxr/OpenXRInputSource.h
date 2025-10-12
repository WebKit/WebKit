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

#include "OpenXRInputMappings.h"
#include "OpenXRUtils.h"
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebKit {

class OpenXRInputSource {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRInputSource);
    WTF_MAKE_NONCOPYABLE(OpenXRInputSource);
public:
    using SuggestedBindings = HashMap<const char*, Vector<XrActionSuggestedBinding>>;
    static std::unique_ptr<OpenXRInputSource> create(XrInstance, XrSession, PlatformXR::XRHandedness, PlatformXR::InputSourceHandle, OpenXRSystemProperties&&);
    ~OpenXRInputSource();

    XrResult suggestBindings(SuggestedBindings&) const;
    std::optional<PlatformXR::FrameData::InputSource> collectInputSource(XrSpace, const XrFrameState&) const;
    XrActionSet actionSet() const { return m_actionSet; }
    XrResult updateInteractionProfile();

private:
    OpenXRInputSource(XrInstance, XrSession, PlatformXR::XRHandedness, PlatformXR::InputSourceHandle);

    struct OpenXRButtonActions {
        XrAction press { XR_NULL_HANDLE };
        XrAction touch { XR_NULL_HANDLE };
        XrAction value { XR_NULL_HANDLE };
    };

    XrResult initialize(OpenXRSystemProperties&&);
    XrResult createAction(XrActionType, const String& name, XrAction&) const;
    XrResult createActionSpace(XrAction, XrSpace&) const;
    XrResult createBinding(const char* profilePath, XrAction, const String& bindingPath, SuggestedBindings&) const;
    XrResult createButtonActions(OpenXRButtonType, const String& prefix, OpenXRButtonActions&) const;

    XrResult getPose(XrSpace, XrSpace, const XrFrameState&, PlatformXR::FrameData::InputSourcePose&) const;
    std::optional<PlatformXR::FrameData::InputSourceButton> collectButton(OpenXRButtonType) const;
    std::optional<XrVector2f> collectAxis(OpenXRAxisType) const;
#if ENABLE(WEBXR_HANDS) && defined(XR_EXT_hand_tracking)
    std::optional<PlatformXR::FrameData::HandJointsVector> collectHandTrackingData(XrSpace, const XrFrameState&) const;
#endif
    XrResult getActionState(XrAction, bool*) const;
    XrResult getActionState(XrAction, float*) const;
    XrResult getActionState(XrAction, XrVector2f*) const;

    XrInstance m_instance { XR_NULL_HANDLE };
    XrSession m_session { XR_NULL_HANDLE };
    PlatformXR::XRHandedness m_handedness { PlatformXR::XRHandedness::Left };
    PlatformXR::InputSourceHandle m_handle { 0 };
    String m_subactionPathName;
    XrPath m_subactionPath { XR_NULL_PATH };
    XrActionSet m_actionSet { XR_NULL_HANDLE };
    XrAction m_gripAction { XR_NULL_HANDLE };
    XrSpace m_gripSpace { XR_NULL_HANDLE };
    XrAction m_pointerAction { XR_NULL_HANDLE };
    XrSpace m_pointerSpace { XR_NULL_HANDLE };
    XrAction m_pinchPoseAction { XR_NULL_HANDLE };
    XrSpace m_pinchSpace { XR_NULL_HANDLE };
    XrAction m_pokePoseAction { XR_NULL_HANDLE };
    XrSpace m_pokeSpace { XR_NULL_HANDLE };
    using OpenXRButtonActionsMap = HashMap<OpenXRButtonType, OpenXRButtonActions, IntHash<OpenXRButtonType>, WTF::StrongEnumHashTraits<OpenXRButtonType>>;
    OpenXRButtonActionsMap m_buttonActions;
    using OpenXRAxesMap = HashMap<OpenXRAxisType, XrAction, IntHash<OpenXRAxisType>, WTF::StrongEnumHashTraits<OpenXRAxisType>>;
    OpenXRAxesMap m_axisActions;
    Vector<String> m_profiles;
#if defined(XR_EXT_hand_tracking)
    XrHandTrackerEXT m_handTracker { XR_NULL_HANDLE };
#endif
#if defined(XR_EXT_hand_joints_motion_range)
    bool m_supportsHandJointsMotionRange { false };
#endif
    bool m_usingHandInteractionProfile { false };
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
