/*
 * Copyright (C) 2021 Igalia, S.L.
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

#include "config.h"
#include "OpenXRInputSource.h"

#if ENABLE(WEBXR) && USE(OPENXR)

#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

constexpr auto OPENXR_INPUT_HAND_PATH { "/user/hand/"_s };
constexpr auto OPENXR_INPUT_GRIP_PATH { "/input/grip/pose"_s };
constexpr auto OPENXR_INPUT_AIM_PATH { "/input/aim/pose"_s };

using namespace WebCore;

namespace PlatformXR {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRInputSource);

std::unique_ptr<OpenXRInputSource> OpenXRInputSource::create(XrInstance instance, XrSession session, XRHandedness handeness, InputSourceHandle handle)
{
    auto input = std::unique_ptr<OpenXRInputSource>(new OpenXRInputSource(instance, session, handeness, handle));
    if (XR_FAILED(input->initialize()))
        return nullptr;
    return input;
}

OpenXRInputSource::OpenXRInputSource(XrInstance instance, XrSession session, XRHandedness handeness, InputSourceHandle handle)
    : m_instance(instance)
    , m_session(session)
    , m_handeness(handeness)
    , m_handle(handle)
{
}

OpenXRInputSource::~OpenXRInputSource()
{
    if (m_actionSet != XR_NULL_HANDLE)
        xrDestroyActionSet(m_actionSet);
    if (m_gripSpace != XR_NULL_HANDLE)
        xrDestroySpace(m_gripSpace);
    if (m_pointerSpace != XR_NULL_HANDLE)
        xrDestroySpace(m_pointerSpace);
}

XrResult OpenXRInputSource::initialize()
{
    String handenessName = handenessToString(m_handeness);
    m_subactionPathName = makeString(OPENXR_INPUT_HAND_PATH, handenessName);
    RETURN_RESULT_IF_FAILED(xrStringToPath(m_instance, m_subactionPathName.utf8().data(), &m_subactionPath), m_instance);

    // Initialize Action Set.
    auto prefix = makeString("input_"_s, handenessName);
    auto actionSetName = makeString(prefix, "_action_set"_s);
    auto createInfo =  createStructure<XrActionSetCreateInfo, XR_TYPE_ACTION_SET_CREATE_INFO>();
    std::strncpy(createInfo.actionSetName, actionSetName.utf8().data(), XR_MAX_ACTION_SET_NAME_SIZE - 1);
    std::strncpy(createInfo.localizedActionSetName, actionSetName.utf8().data(), XR_MAX_ACTION_SET_NAME_SIZE - 1);

    RETURN_RESULT_IF_FAILED(xrCreateActionSet(m_instance, &createInfo, &m_actionSet), m_instance);

    // Initialize pose actions and spaces.
    RETURN_RESULT_IF_FAILED(createAction(XR_ACTION_TYPE_POSE_INPUT, makeString(prefix, "_grip"_s), m_gripAction), m_instance);
    RETURN_RESULT_IF_FAILED(createSpaceAction(m_gripAction, m_gripSpace), m_instance);
    RETURN_RESULT_IF_FAILED(createAction(XR_ACTION_TYPE_POSE_INPUT, makeString(prefix, "_pointer"_s), m_pointerAction), m_instance);
    RETURN_RESULT_IF_FAILED(createSpaceAction(m_pointerAction, m_pointerSpace), m_instance);

    // Initialize button actions.
    for (auto buttonType : openXRButtonTypes) {
        OpenXRButtonActions actions;
        createButtonActions(buttonType, prefix, actions);
        m_buttonActions.add(buttonType, actions);
    }

    // Initialize axes.
    for (auto axisType : openXRAxisTypes) {
        XrAction axisAction = XR_NULL_HANDLE;
        auto name = makeString(prefix, "_axis_"_s, axisTypetoString(axisType));
        RETURN_RESULT_IF_FAILED(createAction(XR_ACTION_TYPE_VECTOR2F_INPUT, name, axisAction), m_instance, false);
        m_axisActions.add(axisType, axisAction);
    }

    return XR_SUCCESS;
}

XrResult OpenXRInputSource::suggestBindings(SuggestedBindings& bindings) const
{
    for (auto& profile : openXRInputProfiles) {
        // Suggest binding for pose actions.
        RETURN_RESULT_IF_FAILED(createBinding(profile.path, m_gripAction, makeString(m_subactionPathName, OPENXR_INPUT_GRIP_PATH), bindings), m_instance);
        RETURN_RESULT_IF_FAILED(createBinding(profile.path, m_pointerAction, makeString(m_subactionPathName, OPENXR_INPUT_AIM_PATH), bindings), m_instance);

        // Suggest binding for button actions.
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // WPE port
        const OpenXRButton* buttons;
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        size_t buttonsSize;
        if (m_handeness == XRHandedness::Left) {
            buttons = profile.leftButtons;
            buttonsSize = profile.leftButtonsSize;
        } else {
            buttons = profile.rightButtons;
            buttonsSize = profile.rightButtonsSize;
        }

        for (size_t i = 0; i < buttonsSize; ++i) {
            const auto& button = buttons[i];
            const auto& actions = m_buttonActions.get(button.type);
            if (button.press) {
                ASSERT(actions.press != XR_NULL_HANDLE);
                RETURN_RESULT_IF_FAILED(createBinding(profile.path, actions.press, makeString(m_subactionPathName, span(button.press)), bindings), m_instance);
            }
            if (button.touch) {
                ASSERT(actions.touch != XR_NULL_HANDLE);
                RETURN_RESULT_IF_FAILED(createBinding(profile.path, actions.touch, makeString(m_subactionPathName, span(button.touch)), bindings), m_instance);
            }
            if (button.value) {
                ASSERT(actions.value != XR_NULL_HANDLE);
                RETURN_RESULT_IF_FAILED(createBinding(profile.path, actions.value, makeString(m_subactionPathName, span(button.value)), bindings), m_instance);
            }
        }

        // Suggest binding for axis actions.
        for (size_t i = 0; i < profile.axesSize; ++i) {
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // WPE port
            const auto& axis = profile.axes[i];
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
            auto action = m_axisActions.get(axis.type);
            ASSERT(action != XR_NULL_HANDLE);
            RETURN_RESULT_IF_FAILED(createBinding(profile.path, action, makeString(m_subactionPathName, span(axis.path)), bindings), m_instance);
        }
    }

    return XR_SUCCESS;
}

std::optional<FrameData::InputSource> OpenXRInputSource::getInputSource(XrSpace localSpace, const XrFrameState& frameState) const
{
    FrameData::InputSource data;
    data.handeness = m_handeness;
    data.handle = m_handle;
    data.targetRayMode = XRTargetRayMode::TrackedPointer;
    data.profiles = m_profiles;

    // Pose transforms.
    getPose(m_pointerSpace, localSpace, frameState, data.pointerOrigin);
    FrameData::InputSourcePose gripPose;
    if (XR_SUCCEEDED(getPose(m_gripSpace, localSpace, frameState, gripPose)))
        data.gripOrigin = gripPose;

    // Buttons.
    Vector<std::optional<FrameData::InputSourceButton>> buttons;
    for (auto& type : openXRButtonTypes) 
        buttons.append(getButton(type));

    // Trigger is mandatory in xr-standard mapping.
    if (buttons.isEmpty() || !buttons.first().has_value())
        return std::nullopt;

    for (size_t i = 0; i < buttons.size(); ++i) {
        if (buttons[i]) {
            data.buttons.append(*buttons[i]);
            continue;
        }
        // Add placeholder if there are more valid buttons in the list.
        for (size_t j = i + 1; j < buttons.size(); ++j) {
            if (buttons[j]) {
                data.buttons.append({ });
                break;
            }
        }
    }

    // Axes.
    Vector<std::optional<XrVector2f>> axes;
    for (auto& type : openXRAxisTypes) 
        axes.append(getAxis(type));

    for (size_t i = 0; i < axes.size(); ++i) {
        if (axes[i]) {
            data.axes.append(axes[i]->x);
            data.axes.append(axes[i]->y);
            continue;
        }
        // Add placeholder if there are more valid axes in the list.
        for (size_t j = i + 1; j < buttons.size(); ++j) {
            if (axes[j]) {
                data.axes.append(0.0f);
                data.axes.append(0.0f);
                break;
            }
        }
    }

    return data;
}

XrResult OpenXRInputSource::updateInteractionProfile()
{
    auto state = createStructure<XrInteractionProfileState, XR_TYPE_INTERACTION_PROFILE_STATE>();
    RETURN_RESULT_IF_FAILED(xrGetCurrentInteractionProfile(m_session, m_subactionPath, &state), m_instance);

    constexpr uint32_t bufferSize = 100; 
    char buffer[bufferSize];
    uint32_t writtenCount = 0;
    RETURN_RESULT_IF_FAILED(xrPathToString(m_instance, state.interactionProfile, bufferSize, &writtenCount, buffer), m_instance);

    m_profiles.clear();
    for (auto& profile : openXRInputProfiles) {
        if (!strncmp(profile.path, buffer, writtenCount)) {
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // WPE port
            for (size_t i = 0; i < profile.profileIdsSize; ++i)
                m_profiles.append(String::fromUTF8(profile.profileIds[i]));
            break;
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        }
    }

    return XR_SUCCESS;
}


XrResult OpenXRInputSource::createSpaceAction(XrAction action, XrSpace& space) const
{
    auto createInfo =  createStructure<XrActionSpaceCreateInfo, XR_TYPE_ACTION_SPACE_CREATE_INFO>();
    createInfo.action = action;
    createInfo.subactionPath = m_subactionPath;
    createInfo.poseInActionSpace = XrPoseIdentity();

    return xrCreateActionSpace(m_session, &createInfo, &space);
}

XrResult OpenXRInputSource::createAction(XrActionType actionType, const String& name, XrAction& action) const
{
    auto createInfo =  createStructure<XrActionCreateInfo, XR_TYPE_ACTION_CREATE_INFO>();
    createInfo.actionType = actionType;
    createInfo.countSubactionPaths = 1;
    createInfo.subactionPaths = &m_subactionPath;
    std::strncpy(createInfo.actionName, name.utf8().data(), XR_MAX_ACTION_SET_NAME_SIZE - 1);
    std::strncpy(createInfo.localizedActionName, name.utf8().data(), XR_MAX_ACTION_SET_NAME_SIZE - 1);

    return xrCreateAction(m_actionSet, &createInfo, &action);
}

XrResult OpenXRInputSource::createButtonActions(OpenXRButtonType type, const String& prefix, OpenXRButtonActions& actions) const
{
    auto name = makeString(prefix, "_button_"_s, buttonTypeToString(type));

    RETURN_RESULT_IF_FAILED(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, makeString(name, "_press"_s), actions.press), m_instance);
    RETURN_RESULT_IF_FAILED(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, makeString(name, "_touch"_s), actions.touch), m_instance);
    RETURN_RESULT_IF_FAILED(createAction(XR_ACTION_TYPE_FLOAT_INPUT, makeString(name, "_value"_s), actions.value), m_instance);
    
    return XR_SUCCESS;
}

XrResult OpenXRInputSource::createBinding(const char* profilePath, XrAction action, const String& bindingPath, SuggestedBindings& bindings) const
{
    ASSERT(profilePath != XR_NULL_PATH);
    ASSERT(action != XR_NULL_HANDLE);
    ASSERT(!bindingPath.isEmpty());

    XrPath path = XR_NULL_PATH;
    RETURN_RESULT_IF_FAILED(xrStringToPath(m_instance, bindingPath.utf8().data(), &path), m_instance);

    XrActionSuggestedBinding binding { action, path };
    if (auto it = bindings.find(profilePath); it != bindings.end())
        it->value.append(binding);
    else
        bindings.add(profilePath, Vector<XrActionSuggestedBinding> { binding });

    return XR_SUCCESS;
}

XrResult OpenXRInputSource::getPose(XrSpace space, XrSpace baseSpace, const XrFrameState& frameState, FrameData::InputSourcePose& pose) const
{
    auto location = createStructure<XrSpaceLocation, XR_TYPE_SPACE_LOCATION>();
    RETURN_RESULT_IF_FAILED(xrLocateSpace(space, baseSpace, frameState.predictedDisplayTime, &location), m_instance);

    if (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)
        pose.pose = XrPosefToPose(location.pose);
    pose.isPositionEmulated = !(location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT);

    return XR_SUCCESS;
}

std::optional<FrameData::InputSourceButton> OpenXRInputSource::getButton(OpenXRButtonType buttonType) const
{
    auto it = m_buttonActions.find(buttonType);
    if (it == m_buttonActions.end())
        return std::nullopt;

    FrameData::InputSourceButton result;
    bool hasValue = false;
    auto& actions = it->value;

    auto queryActionState = [this, &hasValue](XrAction action, auto& value, auto defaultValue) {
        if (action != XR_NULL_HANDLE && XR_SUCCEEDED(this->getActionState(action, &value)))
            hasValue = true;
        else
            value = defaultValue;
    };

    queryActionState(actions.press, result.pressed, false);
    queryActionState(actions.touch, result.touched, result.pressed);
    queryActionState(actions.value, result.pressedValue, result.pressed ? 1.0 : 0.0);

    return hasValue ?  std::make_optional(result) : std::nullopt;
}

std::optional<XrVector2f> OpenXRInputSource::getAxis(OpenXRAxisType axisType) const
{
    auto it = m_axisActions.find(axisType);
    if (it == m_axisActions.end())
        return std::nullopt;

    XrVector2f axis;
    if (XR_FAILED(getActionState(it->value, &axis)))
        return std::nullopt;

    return axis;
}

XrResult OpenXRInputSource::getActionState(XrAction action, bool* value) const
{
    ASSERT(value);
    ASSERT(action != XR_NULL_HANDLE);

    auto state = createStructure<XrActionStateBoolean, XR_TYPE_ACTION_STATE_BOOLEAN>();
    auto info = createStructure<XrActionStateGetInfo, XR_TYPE_ACTION_STATE_GET_INFO>();
    info.action = action;

    RETURN_RESULT_IF_FAILED(xrGetActionStateBoolean(m_session, &info, &state), m_instance);
    *value = state.currentState;

    return XR_SUCCESS;
}

XrResult OpenXRInputSource::getActionState(XrAction action, float* value) const
{
    ASSERT(value);
    ASSERT(action != XR_NULL_HANDLE);

    auto state = createStructure<XrActionStateFloat, XR_TYPE_ACTION_STATE_FLOAT>();
    auto info = createStructure<XrActionStateGetInfo, XR_TYPE_ACTION_STATE_GET_INFO>();
    info.action = action;

    RETURN_RESULT_IF_FAILED(xrGetActionStateFloat(m_session, &info, &state), m_instance);
    *value = state.currentState;

    return XR_SUCCESS;
}

XrResult OpenXRInputSource::getActionState(XrAction action, XrVector2f* value) const
{
    ASSERT(value);
    ASSERT(action != XR_NULL_HANDLE);

    auto state = createStructure<XrActionStateVector2f, XR_TYPE_ACTION_STATE_VECTOR2F>();
    auto info = createStructure<XrActionStateGetInfo, XR_TYPE_ACTION_STATE_GET_INFO>();
    info.action = action;

    RETURN_RESULT_IF_FAILED(xrGetActionStateVector2f(m_session, &info, &state), m_instance);
    *value = state.currentState;

    return XR_SUCCESS;
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
