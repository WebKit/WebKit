/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 */

#include "config.h"
#include "PlatformXRPose.h"

#if ENABLE(WEBXR) && PLATFORM(COCOA)

WebCore::FloatPoint3D PlatformXRPose::position() const
{
    simd_float3 simdPosition = this->simdPosition();
    return { static_cast<float>(simdPosition.x), static_cast<float>(simdPosition.y), static_cast<float>(simdPosition.z) };
}

PlatformXR::Device::FrameData::FloatQuaternion PlatformXRPose::orientation() const
{
    simd_quatf simdOrientation = this->simdOrientation();
    return { simdOrientation.vector.x, simdOrientation.vector.y, simdOrientation.vector.z, simdOrientation.vector.w };
}

PlatformXR::Device::FrameData::Pose PlatformXRPose::pose() const
{
    PlatformXR::Device::FrameData::Pose pose;
    pose.position = position();
    pose.orientation = orientation();
    return pose;
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

#endif // ENABLE(WEBXR) && PLATFORM(COCOA)
