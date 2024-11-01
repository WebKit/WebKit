/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 */

#include "config.h"
#include "PlatformXRPose.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBXR) && PLATFORM(COCOA)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformXRPose);

WebCore::FloatPoint3D PlatformXRPose::position() const
{
    simd_float3 simdPosition = this->simdPosition();
    return { static_cast<float>(simdPosition.x), static_cast<float>(simdPosition.y), static_cast<float>(simdPosition.z) };
}

PlatformXR::FrameData::FloatQuaternion PlatformXRPose::orientation() const
{
    simd_quatf simdOrientation = this->simdOrientation();
    return { simdOrientation.vector.x, simdOrientation.vector.y, simdOrientation.vector.z, simdOrientation.vector.w };
}

PlatformXR::FrameData::Pose PlatformXRPose::pose() const
{
    PlatformXR::FrameData::Pose pose;
    pose.position = position();
    pose.orientation = orientation();
    return pose;
}

PlatformXRPose::FloatMatrix4 PlatformXRPose::toColumnMajorFloatArray() const
{
    const simd_float4 (&columns)[4] = m_simdTransform.columns;
    return { {
        columns[0][0], columns[0][1], columns[0][2], columns[0][3],
        columns[1][0], columns[1][1], columns[1][2], columns[1][3],
        columns[2][0], columns[2][1], columns[2][2], columns[2][3],
        columns[3][0], columns[3][1], columns[3][2], columns[3][3],
    } };
}

float PlatformXRPose::distanceToPose(const PlatformXRPose& otherPose) const
{
    simd_float3 position = simdPosition();
    simd_float3 otherPosition = otherPose.simdPosition();
    return simd_distance(position, otherPosition);
}

PlatformXRPose PlatformXRPose::verticalTransformPose() const
{
    simd_float3 position = simdPosition();
    simd_float4x4 transform = matrix_identity_float4x4;
    transform.columns[3].y  = position.y;
    return PlatformXRPose(transform);
}

PlatformXRPose::PlatformXRPose(const simd_float4x4& transform)
    : m_simdTransform(transform)
{
}

PlatformXRPose::PlatformXRPose(const simd_float4x4& transform, const simd_float4x4& parentTransform)
{
    m_simdTransform = simd_mul(parentTransform, transform);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBXR) && PLATFORM(COCOA)
