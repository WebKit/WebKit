/*
 * Copyright (C) 2018 Igalia, S.L. All right reserved.
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "VRPlatformDisplayOpenVR.h"

#if USE(OPENVR)

#include <wtf/text/StringBuilder.h>

namespace WebCore {

uint32_t VRPlatformDisplayOpenVR::s_displayIdentifier = 0;

VRPlatformDisplayOpenVR::VRPlatformDisplayOpenVR(vr::IVRSystem* system, vr::IVRChaperone* chaperone, vr::IVRCompositor* compositor)
    : m_system(system)
    , m_chaperone(chaperone)
    , m_compositor(compositor)
{
    m_displayInfo.setDisplayIdentifier(++s_displayIdentifier);
    m_displayInfo.setIsConnected(m_system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd));

    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("OpenVR HMD");
    char HMDName[128];
    if (auto length = m_system->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ManufacturerName_String, HMDName, 128)) {
        stringBuilder.append(" (");
        stringBuilder.append(HMDName, length);
        stringBuilder.append(')');
    }
    m_displayInfo.setDisplayName(stringBuilder.toString());
    m_displayInfo.setIsMounted(false);
    // FIXME: We're assuming an HTC Vive HMD here. Get this info from OpenVR?.
    m_displayInfo.setCapabilityFlags(VRDisplayCapabilityFlagNone | VRDisplayCapabilityFlagPosition | VRDisplayCapabilityFlagOrientation | VRDisplayCapabilityFlagExternalDisplay | VRDisplayCapabilityFlagPresent);

    m_compositor->SetTrackingSpace(vr::TrackingUniverseSeated);

    updateEyeParameters();
    updateStageParameters();
}

VRPlatformDisplayInfo::FieldOfView VRPlatformDisplayOpenVR::computeFieldOfView(vr::Hmd_Eye eye)
{
    float left, right, top, bottom;
    // OpenVR returns the tangents of the half-angles from the center view axis.
    m_system->GetProjectionRaw(static_cast<vr::Hmd_Eye>(eye), &left, &right, &top, &bottom);
    return { -rad2deg(atanf(top)), rad2deg(atanf(bottom)), -rad2deg(atanf(left)), rad2deg(atanf(right)) };
}

void VRPlatformDisplayOpenVR::updateEyeParameters()
{
    for (unsigned eye = 0; eye < VRPlatformDisplayInfo::NumEyes; ++eye) {
        auto platformEye = static_cast<VRPlatformDisplayInfo::Eye>(eye);
        m_displayInfo.setEyeFieldOfView(platformEye, computeFieldOfView(static_cast<vr::Hmd_Eye>(eye)));

        vr::HmdMatrix34_t eyeToHead = m_system->GetEyeToHeadTransform(static_cast<vr::Hmd_Eye>(eye));
        m_displayInfo.setEyeTranslation(platformEye, { eyeToHead.m[0][3], eyeToHead.m[1][3], eyeToHead.m[2][3] });
    }

    uint32_t width;
    uint32_t height;
    m_system->GetRecommendedRenderTargetSize(&width, &height);
    m_displayInfo.setRenderSize({ width, height });
}

void VRPlatformDisplayOpenVR::updateStageParameters()
{
    float playAreaWidth = 1;
    float playAreaDepth = 1;
    if (!m_chaperone->GetPlayAreaSize(&playAreaWidth, &playAreaDepth)) {
        // Fallback to sensible values, 1mx1m play area and 0.75m high seated position. We do as
        // Firefox does.
        TransformationMatrix matrix;
        matrix.setM42(0.75);
        m_displayInfo.setSittingToStandingTransform(WTFMove(matrix));
    } else {
        vr::HmdMatrix34_t transformMatrix = m_system->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
        auto matrix =  TransformationMatrix(transformMatrix.m[0][0], transformMatrix.m[1][0], transformMatrix.m[2][0], 0,
            transformMatrix.m[0][1], transformMatrix.m[1][1], transformMatrix.m[2][1], 0,
            transformMatrix.m[0][2], transformMatrix.m[1][2], transformMatrix.m[2][2], 0,
            transformMatrix.m[0][3], transformMatrix.m[1][3], transformMatrix.m[2][3], 1);
        m_displayInfo.setSittingToStandingTransform(WTFMove(matrix));
    }
    m_displayInfo.setPlayAreaBounds(FloatSize(playAreaWidth, playAreaDepth));
}

// FIXME: we might want to generalize this function for other backends.
static VRPlatformTrackingInfo::Quaternion rotationMatrixToQuaternion(const float (&matrix)[3][4])
{
    // See https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf.
    VRPlatformTrackingInfo::Quaternion quaternion;
    float trace;
    if (matrix[2][2] < 0) {
        if (matrix[0][0] > matrix[1][1]) {
            trace = 1 + matrix[0][0] - matrix[1][1] - matrix[2][2];
            quaternion = { trace, matrix[0][1]+matrix[1][0], matrix[2][0]+matrix[0][2], matrix[1][2] - matrix[2][1] };
        } else {
            trace = 1 - matrix[0][0] + matrix[1][1] - matrix[2][2];
            quaternion = { matrix[0][1]+matrix[1][0], trace, matrix[1][2]+matrix[2][1], matrix[2][0] - matrix[0][2] };
        }
    } else {
        if (matrix[0][0] < -matrix[1][1]) {
            trace = 1 - matrix[0][0] - matrix[1][1] + matrix[2][2];
            quaternion = { matrix[2][0]+matrix[0][2], matrix[1][2]+matrix[2][1], trace , matrix[0][1] - matrix[1][0] };
        } else {
            trace = 1 + matrix[0][0] + matrix[1][1] + matrix[2][2];
            quaternion = { matrix[1][2] - matrix[2][1], matrix[2][0] - matrix[0][2], matrix[0][1] - matrix[1][0], trace };
        }
    }
    return quaternion * (0.5 / sqrt(trace));
}

VRPlatformTrackingInfo VRPlatformDisplayOpenVR::getTrackingInfo()
{
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];

    m_compositor->WaitGetPoses(nullptr, 0, poses, vr::k_unMaxTrackedDeviceCount);

    m_trackingInfo.clear();

    vr::Compositor_FrameTiming timing;
    timing.m_nSize = sizeof(vr::Compositor_FrameTiming);
    m_compositor->GetFrameTiming(&timing);
    m_trackingInfo.timestamp = timing.m_flSystemTimeInSeconds;

    if (!poses[vr::k_unTrackedDeviceIndex_Hmd].bDeviceIsConnected
        || !poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid
        || poses[vr::k_unTrackedDeviceIndex_Hmd].eTrackingResult != vr::TrackingResult_Running_OK) {
        // FIXME: Init some data maybe???.
        return m_trackingInfo;
    }

    const auto& HMDPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
    const auto& transform = HMDPose.mDeviceToAbsoluteTracking;
    m_trackingInfo.orientation = rotationMatrixToQuaternion(transform.m);
    m_trackingInfo.orientation->conjugate();
    m_trackingInfo.position = FloatPoint3D(transform.m[0][3], transform.m[1][3], transform.m[2][3]);
    m_trackingInfo.angularVelocity = VRPlatformTrackingInfo::Float3(HMDPose.vAngularVelocity.v[0], HMDPose.vAngularVelocity.v[1], HMDPose.vAngularVelocity.v[2]);
    m_trackingInfo.linearVelocity = VRPlatformTrackingInfo::Float3(HMDPose.vVelocity.v[0], HMDPose.vVelocity.v[1], HMDPose.vVelocity.v[2]);

    return m_trackingInfo;
}

void VRPlatformDisplayOpenVR::updateDisplayInfo()
{
    if (!vr::VR_IsHmdPresent())
        return;

    vr::VREvent_t event;
    while (m_system && m_system->PollNextEvent(&event, sizeof(event))) {
        switch (event.eventType) {
        case vr::VREvent_TrackedDeviceUserInteractionStarted:
        case vr::VREvent_TrackedDeviceUserInteractionEnded:
            if (event.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd)
                m_displayInfo.setIsMounted(event.eventType == vr::VREvent_TrackedDeviceUserInteractionStarted ? true : false);
            break;
        case vr::EVREventType::VREvent_TrackedDeviceActivated:
        case vr::EVREventType::VREvent_TrackedDeviceDeactivated:
            if (event.trackedDeviceIndex == vr::k_unTrackedDeviceIndex_Hmd)
                m_displayInfo.setIsConnected(event.eventType == vr::VREvent_TrackedDeviceActivated ? true : false);
            break;
        case vr::EVREventType::VREvent_DriverRequestedQuit:
        case vr::EVREventType::VREvent_Quit:
        case vr::EVREventType::VREvent_ProcessQuit:
        case vr::EVREventType::VREvent_QuitAcknowledged:
        case vr::EVREventType::VREvent_QuitAborted_UserPrompt:
            // FIXME: should we notify the platform manager about this and call VR_Shutdown().
        default:
            break;
        }
    }
}

}; // namespace WebCore

#endif // USE(OPENVR)
