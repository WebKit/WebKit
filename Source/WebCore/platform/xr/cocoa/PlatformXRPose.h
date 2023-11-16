/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 */

#pragma once

#if ENABLE(WEBXR) && PLATFORM(COCOA)

#include <WebCore/PlatformXR.h>
#include <simd/simd.h>

class PlatformXRPose {
    WTF_MAKE_FAST_ALLOCATED;

public:
    simd_float4x4 simdTransform() const { return m_simdTransform; }
    simd_float3 simdPosition() const { return m_simdTransform.columns[3].xyz; }
    simd_quatf simdOrientation() const { return simd_quaternion(m_simdTransform); }
    WEBCORE_EXPORT WebCore::FloatPoint3D position() const;
    WEBCORE_EXPORT PlatformXR::Device::FrameData::FloatQuaternion orientation() const;
    WEBCORE_EXPORT PlatformXR::Device::FrameData::Pose pose() const;

    WEBCORE_EXPORT float distanceToPose(const PlatformXRPose&) const;
    WEBCORE_EXPORT PlatformXRPose verticalTransformPose() const;

    WEBCORE_EXPORT PlatformXRPose(const simd_float4x4&);
    WEBCORE_EXPORT PlatformXRPose(const simd_float4x4&, const simd_float4x4& parentTransform);

private:
    simd_float4x4 m_simdTransform;
};

#endif // ENABLE(WEBXR) && PLATFORM(COCOA)
