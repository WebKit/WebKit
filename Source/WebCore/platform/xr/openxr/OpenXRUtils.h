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

#if USE(EGL)
// EGL symbols required by openxr_platform.h
#if USE(LIBEPOXY)
#define __GBM__ 1
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#endif

#endif // USE(EGL)

#include "Logging.h"
#include "PlatformXR.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

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
        return String::fromLatin1(buffer);
    return makeString("<unknown ", int(value), ">");
}

#define RETURN_IF_FAILED(call, label, instance, ...)                                                      \
    {                                                                                                     \
        auto xrResult = call;                                                                             \
        if (XR_FAILED(xrResult)) {                                                                        \
            LOG(XR, "%s %s: %s\n", __func__, label, resultToString(xrResult, instance).utf8().data());    \
            return __VA_ARGS__;                                                                           \
        }                                                                                                 \
    }

#define RETURN_RESULT_IF_FAILED(call, instance, ...)                                                                  \
    {                                                                                                                 \
        auto xrResult = call;                                                                                         \
        if (XR_FAILED(xrResult)) {                                                                                    \
            LOG(XR, "%s %s: %s\n", __func__, #call, resultToString(xrResult, instance).utf8().data());                \
            return xrResult;                                                                                          \
        }                                                                                                             \
    }

#define LOG_IF_FAILED(result, call, instance, ...)                                           \
    if (XR_FAILED(result))                                                                   \
        LOG(XR, "%s %s: %s\n", __func__, call, resultToString(result, instance).utf8().data());


inline FrameData::Pose XrPosefToPose(XrPosef pose)
{
    FrameData::Pose result;
    result.orientation = { pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w };
    result.position = { pose.position.x, pose.position.y, pose.position.z };
    return result;
}

inline FrameData::View xrViewToPose(XrView view)
{
    FrameData::View pose;
    pose.projection = FrameData::Fov { std::abs(view.fov.angleUp), std::abs(view.fov.angleDown), std::abs(view.fov.angleLeft), std::abs(view.fov.angleRight) };
    pose.offset = XrPosefToPose(view.pose);
    return pose;
}

inline XrPosef XrPoseIdentity()
{
    XrPosef pose;
    pose.orientation.w = 1.0;
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

inline String handenessToString(XRHandedness handeness)
{
    switch (handeness) {
    case XRHandedness::Left:
        return "left"_s;
    case XRHandedness::Right:
        return "right"_s;
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
