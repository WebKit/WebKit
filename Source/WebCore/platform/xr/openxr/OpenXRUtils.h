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

#pragma once

#if ENABLE(WEBXR) && USE(OPENXR)

#include "Logging.h"
#include "PlatformXR.h"
#include <openxr/openxr.h>

#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace PlatformXR {

template<typename T, XrStructureType StructureType>
T createStructure()
{
    T object;
    std::memset(&object, 0, sizeof(T));
    object.type = StructureType;
    object.next = nullptr;
    return object;
}

inline String resultToString(XrResult value, XrInstance instance)
{
    char buffer[XR_MAX_RESULT_STRING_SIZE];
    XrResult result = xrResultToString(instance, value, buffer);
    if (result == XR_SUCCESS)
        return String(buffer);
    return makeString("<unknown ", int(value), ">");
}

#define RETURN_IF_FAILED(result, call, instance, ...)                                           \
    if (XR_FAILED(result)) {                                                                    \
        LOG(XR, "%s %s: %s\n", __func__, call, resultToString(result, instance).utf8().data()); \
        return __VA_ARGS__;                                                                     \
    }

inline Device::FrameData::Pose XrPosefToPose(XrPosef pose)
{
    Device::FrameData::Pose result;
    result.orientation = { pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w };
    result.position = { pose.position.x, pose.position.y, pose.position.z };
    return result;
}

inline Device::FrameData::View xrViewToPose(XrView view)
{
    Device::FrameData::View pose;
    pose.projection = Device::FrameData::Fov { view.fov.angleUp, -view.fov.angleDown, -view.fov.angleLeft, view.fov.angleRight };
    pose.offset = XrPosefToPose(view.pose);
    return pose;
}

inline XrViewConfigurationType toXrViewConfigurationType(SessionMode mode)
{
    switch (mode) {
    case SessionMode::ImmersiveVr:
        return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    case SessionMode::Inline:
    case SessionMode::ImmersiveAr:
        return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
    };
    ASSERT_NOT_REACHED();
    return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
