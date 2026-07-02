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

    const Vector<String>& profiles() const LIFETIME_BOUND { return m_profiles; }
    XrSpace aimSpace() const { return m_aimSpace; }
    XrSpace gripSpace() const { return m_gripSpace; }
    PlatformXR::InputSourceHandle handle() const { return m_handle; }

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
    XrAction m_aimAction { XR_NULL_HANDLE };
    XrSpace m_aimSpace { XR_NULL_HANDLE };
#if defined(XR_EXT_hand_interaction)
    XrAction m_pinchPoseAction { XR_NULL_HANDLE };
    XrSpace m_pinchSpace { XR_NULL_HANDLE };
    XrAction m_pokePoseAction { XR_NULL_HANDLE };
    XrSpace m_pokeSpace { XR_NULL_HANDLE };
#endif
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
