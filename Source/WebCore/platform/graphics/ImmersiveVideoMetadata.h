/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/DoubleSize.h>
#include <WebCore/IntSize.h>
#include <optional>
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>
#if __has_include(<simd/matrix_types.h>)
#include <simd/matrix_types.h>
#endif

namespace WebCore {

enum class VideoProjectionMetadataKind : uint8_t {
    Unknown,
    Rectilinear,
    Equirectangular,
    HalfEquirectangular,
    EquiAngularCubemap,
    Parametric,
    Pyramid,
    AppleImmersiveVideo,
};

enum class HeroEye : uint8_t {
    Left,
    Right,
};

enum class ViewPackingKind : uint8_t {
    SideBySide,
    OverUnder,
};

enum class LensAlgorithmKind : uint8_t {
    ParametricLens,
};

enum class LensDomain : uint8_t {
    Color,
};

enum class LensRole : uint8_t {
    Mono,
    Left,
    Right,
};

enum class ExtrinsicOriginSource : uint8_t {
    StereoCameraSystemBaseline,
};

using RadialDistortionCoefficients = Vector<float, 4>;
using LensFrameAdjustmentsPolynomial = Vector<float, 3>;
using ExtrinsicOrientationQuaternion = Vector<float, 3>;
using IntrinsicMatrix = std::array<std::array<float, 4>, 3>;

struct CameraCalibration {
    LensAlgorithmKind lensAlgorithmKind;
    LensDomain lensDomain;
    int32_t lensIdentifier;
    LensRole lensRole;
    RadialDistortionCoefficients lensDistortions;
    LensFrameAdjustmentsPolynomial lensFrameAdjustmentsPolynomialX;
    LensFrameAdjustmentsPolynomial lensFrameAdjustmentsPolynomialY;
    float radialAngleLimit;
    IntrinsicMatrix intrinsicMatrix;
    float intrinsicMatrixProjectionOffset;
    DoubleSize intrinsicMatrixReferenceDimensions;
    ExtrinsicOriginSource extrinsicOriginSource;
    ExtrinsicOrientationQuaternion extrinsicOrientationQuaternion;
    bool operator==(const CameraCalibration& other) const
    {
#if __has_include(<simd/matrix_types.h>)
        static_assert(sizeof(matrix_float3x3) == sizeof(IntrinsicMatrix));
#endif
        return lensAlgorithmKind == other.lensAlgorithmKind
            && lensDomain == other.lensDomain
            && lensIdentifier == other.lensIdentifier
            && lensRole == other.lensRole
            && lensDistortions == other.lensDistortions
            && lensFrameAdjustmentsPolynomialX == other.lensFrameAdjustmentsPolynomialX
            && lensFrameAdjustmentsPolynomialY == other.lensFrameAdjustmentsPolynomialY
            && radialAngleLimit == other.radialAngleLimit
            && intrinsicMatrix == other.intrinsicMatrix
            && intrinsicMatrixProjectionOffset == other.intrinsicMatrixProjectionOffset
            && intrinsicMatrixReferenceDimensions == other.intrinsicMatrixReferenceDimensions
            && extrinsicOriginSource == other.extrinsicOriginSource
            && extrinsicOrientationQuaternion == other.extrinsicOrientationQuaternion;
    }
};

struct ImmersiveVideoMetadata {
    using Kind = VideoProjectionMetadataKind;
    Kind kind;
    IntSize size;
    std::optional<int32_t> horizontalFieldOfView;
    std::optional<uint32_t> stereoCameraBaseline;
    std::optional<int32_t> horizontalDisparityAdjustment;
    std::optional<bool> hasLeftStereoEyeView;
    std::optional<bool> hasRightStereoEyeView;
    std::optional<HeroEye> heroEye;
    std::optional<ViewPackingKind> viewPackingKind;
    Vector<CameraCalibration> cameraCalibrationDataLensCollection;

    RefPtr<JSON::Value> parameters;

    friend bool operator==(const ImmersiveVideoMetadata&, const ImmersiveVideoMetadata&) = default;
    bool isSpatial() const { return kind == Kind::Rectilinear && horizontalFieldOfView && stereoCameraBaseline && horizontalDisparityAdjustment; }
    bool isImmersive() const { return kind != Kind::Rectilinear && kind != Kind::Unknown; }
};

WEBCORE_EXPORT String convertImmersiveVideoMetadataToString(const ImmersiveVideoMetadata&);
WEBCORE_EXPORT String convertEnumerationToString(VideoProjectionMetadataKind);

} // namespace WebCore

namespace WTF {

template<typename> struct LogArgument;

template <>
struct LogArgument<WebCore::ImmersiveVideoMetadata> {
    static String toString(const WebCore::ImmersiveVideoMetadata& metadata)
    {
        return convertImmersiveVideoMetadataToString(metadata);
    }
};

}
