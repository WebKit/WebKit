/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FELightingCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "DistantLightSource.h"
#import "FELighting.h"
#import "Filter.h"
#import "FilterImage.h"
#import "PointLightSource.h"
#import "SpotLightSource.h"
#import <CoreImage/CoreImage.h>
#import <simd/simd.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

static CIKernel* specularLightingKernel()
{
    static NeverDestroyed<RetainPtr<CIKernel>> kernel;
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        NSError *error = nil;
        NSArray<CIKernel *> *kernels = [CIKernel kernelsWithMetalString:@R"( /* NOLINT */
extern "C" {
namespace coreimage {

enum class LightingType : uint32_t {
    Diffuse,
    Specular,
};

enum class LightSourceType : uint32_t {
    Distant,
    Point,
    Spot,
};

struct LightingConstants {
    LightingType lightingType;
    vector_float2 textureOrigin;
    float surfaceScale;
    float lightingConstant; // diffuse or specular.
    float specularExponent;
    float extentBottom;
    vector_float2 kernelUnitLength;
    vector_float4 lightingColor;
};

struct LightSourceConstants {
    LightSourceType lightSourceType;
    float azimuthRadians;
    float elevationRadians;
    float specularExponent;
    float limitingConeAngleRadians;
    float coneCutOffLimit;
    float coneFullLight;
    vector_float3 position; // x, y, z.
    vector_float3 pointsAt; // pointsAtX, Y, Z.
};

struct Kernel3x3 {
    vector_float2 data[3][3];
};

struct Normal {
    vector_float2 factor;
    vector_float2 data[3][3];
};


using LightingFactors = vector_float2[3][3];

// These are the kernels specified in https://drafts.csswg.org/filter-effects/#feDiffuseLightingElement
static constant Normal topLeftKernel = {
    { 2.f / 3, 2.f / 3 },
    {
        { {  0,  0 }, {  0,  0 },  {  0,  0 } },
        { {  0,  0 }, { -2, -2 },  {  2, -1 } },
        { {  0,  0 }, { -1,  2 },  {  1,  1 } },
    }
};

static constant Normal topRowKernel = {
    { 1 / 3, 1 / 2 },
    {
        { {  0,  0 }, {  0,  0 },  {  0,  0 } },
        { { -2, -1 }, {  0, -2 },  {  2, -1 } },
        { { -1,  1 }, {  0,  2 },  {  1,  1 } },
    }
};

static constant Normal topRightKernel = {
    { 2 / 3, 2 / 3 },
    {
        { {  0,  0 }, {  0,  0 },  {  0,  0 } },
        { { -2, -1 }, {  2, -2 },  {  0,  0 } },
        { { -1,  1 }, {  1,  2 },  {  0,  0 } },
    }
};

static constant Normal leftKernel = {
    { 1 / 2, 1 / 3 },
    {
        { {  0,  0 }, { -1, -2 },  {  1, -1 } },
        { {  0,  0 }, { -2,  0 },  {  2,  0 } },
        { {  0,  0 }, { -1,  2 },  {  1,  1 } },
    }
};

static constant Normal interiorKernel = {
    { 1.f / 4, 1.f / 4 },
    {
        { { -1, -1 }, {  0, -2 },  {  1, -1 } },
        { { -2,  0 }, {  0,  0 },  {  2,  0 } },
        { { -1,  1 }, {  0,  2 },  {  1,  1 } },
    }
};

static constant Normal rightKernel = {
    { 1 / 2, 1 / 3 },
    {
        { { -1, -1 }, {  1, -2 },  {  0,  0 } },
        { { -2,  0 }, {  2,  0 },  {  0,  0 } },
        { { -1,  1 }, {  1,  2 },  {  0,  0 } },
    }
};

static constant Normal bottomLeftKernel = {
    { 2 / 3, 2 / 3 },
    {
        { {  0,  0 }, { -1, -2 },  {  1, -1 } },
        { {  0,  0 }, { -2,  2 },  {  2,  1 } },
        { {  0,  0 }, {  0,  0 },  {  0,  0 } },
    }
};

static constant Normal bottomRowKernel = {
    { 1 / 3, 1 / 2 },
    {
        { { -1, -1 }, {  0, -2 },  {  1, -1 } },
        { { -2,  1 }, {  0,  2 },  {  2,  1 } },
        { {  0,  0 }, {  0,  0 },  {  0,  0 } },
    }
};

static constant Normal bottomRightKernel = {
    { 2 / 3, 2 / 3 },
    {
        { { -1, -1 }, {  1, -2 },  {  0,  0 } },
        { { -2,  1 }, {  2,  2 },  {  0,  0 } },
        { {  0,  0 }, {  0,  0 },  {  0,  0 } },
    }
};


using Normals = Normal[3][3];

static constant Normals normals = {
    { topLeftKernel,    topRowKernel,       topRightKernel      },
    { leftKernel,       interiorKernel,     rightKernel         },
    { bottomLeftKernel, bottomRowKernel,    bottomRightKernel   }
};

bool isZero(float2 value)
{
    return value.x == 0 && value.y == 0;
}

float computeLightFactor(
    constant LightingConstants* constants,
    float3 lightVector, float2 factor, float2 normal)
{
    float k = 0;
    switch (constants->lightingType) {
    case LightingType::Diffuse:
        if (isZero(normal)) // approx
            k = lightVector.z;
        else {
            float2 n = normal * -constants->surfaceScale;
            n *= factor;

            float3 surfaceNormal = { 0, 0, 1 };
            surfaceNormal.xy = n;
            k = dot(surfaceNormal, lightVector) / length(surfaceNormal);
        }
        break;
    case LightingType::Specular: {
        const float3 eyeVector = { 0, 0, 1 };

        float3 halfwayVector = lightVector + eyeVector;
        if (length(halfwayVector) == 0)
            return 0;

        // FIXME: Need an "approximately zero".
        if (isZero(normal)) {
            float nDotH = halfwayVector.z / length(halfwayVector);
            k = pow(nDotH, constants->specularExponent);
        } else {
            float2 n = normal * -constants->surfaceScale;
            n *= factor;

            float3 surfaceNormal = { 0, 0, 1 };
            surfaceNormal.xy = n;

            float nDotH = dot(surfaceNormal, halfwayVector) / length(surfaceNormal) / length(halfwayVector);
            k = pow(nDotH, constants->specularExponent);
        }
        break;
    }
    }
    return constants->lightingConstant * k;
}

float3 computeLightColor(
    constant LightSourceConstants* lightSource,
    float3 color,
    float3 lightVector)
{
    switch (lightSource->lightSourceType) {
    case LightSourceType::Distant:
    case LightSourceType::Point:
        return color;

    case LightSourceType::Spot: {
        // Could do outside the shader.
        float3 directionVector = normalize(lightSource->pointsAt - lightSource->position);

        float3 lightColor;
        float cosineOfAngle = dot(lightVector, directionVector);
        if (cosineOfAngle > lightSource->coneCutOffLimit)
            lightColor = { };
        else
            lightColor = color * pow(-cosineOfAngle, lightSource->specularExponent);

        if (cosineOfAngle > lightSource->coneFullLight)
            lightColor *= (lightSource->coneCutOffLimit - cosineOfAngle) / (lightSource->coneCutOffLimit - lightSource->coneFullLight);
        return lightColor;
    }
    }

    return 0;
}

float3 computeLightVector(sampler src,
    constant LightingConstants* constants,
    constant LightSourceConstants* lightSource,
    float2 srcPosition, float2 destPosition)
{
    switch (lightSource->lightSourceType) {
    case LightSourceType::Distant: {
        // FIXME: we could compute these outside of the shader.
        float3 lightVector = {
            cos(lightSource->azimuthRadians) * cos(lightSource->elevationRadians),
            sin(lightSource->azimuthRadians) * cos(lightSource->elevationRadians),
            sin(lightSource->elevationRadians),
        };
        return normalize(lightVector); // Should already be unit length.
    }

    case LightSourceType::Point:
    case LightSourceType::Spot: {
        float4 pixel = src.sample(srcPosition);
        float z = constants->surfaceScale * pixel.a;
        // Flipped texture coords.
        float2 position = destPosition;
        position.y = constants->extentBottom - position.y;

        float3 lightVector;
        lightVector.xy = lightSource->position.xy - position;
        lightVector.z = lightSource->position.z - z;
        return normalize(lightVector);
    }
    }
    return 0;
}

[[stitchable]] float4 specular_lighting(sampler src,
    constant LightingConstants* constants,
    constant LightSourceConstants* lightSource,
    destination dest)
{
    const float2 halfPixel = 0.5f;
    float2 destPosition = dest.coord();
    float2 srcPosition = src.transform(destPosition);

    // z = width, w = height.
    const float2 maxSampleOffset = { src.extent().x + src.extent().z - 1, src.extent().y + src.extent().w - 1 };

    // FIXME: The edge detection is not working.
    const int isLeftEdge = static_cast<int>(srcPosition.x == src.extent().x);
    const int isRightEdge = static_cast<int>(srcPosition.x == maxSampleOffset.x);

    // Flipped coordinates.
    const int isBottomEdge = static_cast<int>(srcPosition.y == src.extent().y);
    const int isTopEdge = static_cast<int>(srcPosition.y == maxSampleOffset.y);

    const int rowIndex = 1 - isTopEdge + isBottomEdge;
    const int colIndex = 1 - isLeftEdge + isRightEdge;

    // FIXME: This edge detection doesn't work. rowIndex and colIndex are always 1.

    constant Normal& normalForPixel = normals[1][1];
    const float2 factor = normalForPixel.factor;

    float2 alphaXYSum = 0;
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            // Need to flip when picking out the constants.
            int flippedRow = 2 - row;
            float2 k = normalForPixel.data[flippedRow][col];
            if (isZero(k))
                continue;

            const float2 sampleOffset = { constants->kernelUnitLength.x * (col - 1), constants->kernelUnitLength.y * (row - 1) };
            const float2 sampleLocation = srcPosition + sampleOffset;

            float4 nearbyPixel = src.sample(sampleLocation + halfPixel);
            alphaXYSum += nearbyPixel.a * k;
        }
    }

    float3 lightVector = computeLightVector(src, constants, lightSource, srcPosition, destPosition);
    float lightFactor = computeLightFactor(constants, lightVector, normalForPixel.factor, alphaXYSum);
    float3 lightColor = computeLightColor(lightSource, constants->lightingColor.rgb, lightVector);

    lightColor *= lightFactor;

    float4 resultPixel = { };
    resultPixel.rgb = clamp(lightColor, 0, 1);

    if (constants->lightingType == LightingType::Diffuse)
        resultPixel.a = 1;
    else
        resultPixel.a = max(max(resultPixel.r, resultPixel.g), resultPixel.b);

    return resultPixel;
}

} // namespace coreimage
} // extern "C"
        )" error:&error]; /* NOLINT */

        if (error || !kernels || !kernels.count) {
            LOG(Filters, "DisplacementMap kernel compilation failed: %@", error);
            return;
        }

        kernel.get() = kernels[0];
    });

    return kernel.get().get();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(FELightingCoreImageApplier);

FELightingCoreImageApplier::FELightingCoreImageApplier(const FELighting& effect)
    : Base(effect)
{
}

bool FELightingCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT(inputs.size() == 1);
    Ref input = inputs[0].get();

    RetainPtr inputImage = input->ciImage();
    if (!inputImage)
        return false;

    RetainPtr kernel = specularLightingKernel();
    if (!kernel)
        return false;

    auto filterRegion = filter.absoluteEnclosingFilterRegion();
    auto extent = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());
    // Work around a bug where the extent of the kernel is too large after compositing with other CIImages (TODO: file the radar).
    extent.shiftMaxXEdgeBy(1);

    auto lightingColor = m_effect->lightingColor();
    auto [r, g, b, a] = lightingColor.toResolvedColorComponentsInColorSpace(m_effect->operatingColorSpace());

    enum class LightingType : uint32_t {
        Diffuse,
        Specular,
    };

    enum class LightSourceType : uint32_t {
        Distant,
        Point,
        Spot,
    };

    auto lightSourceType = [](LightType lightType) {
        switch (lightType) {
        case LightType::LS_DISTANT: return LightSourceType::Distant;
        case LightType::LS_POINT: return LightSourceType::Point;
        case LightType::LS_SPOT: return LightSourceType::Spot;
        }
        return LightSourceType::Distant;
    };

    struct LightingConstants {
        LightingType lightingType;
        vector_float2 textureOrigin;
        float surfaceScale;
        float lightingConstant; // diffuse or specular.
        float specularExponent;
        float extentBottom;
        vector_float2 kernelUnitLength;
        vector_float4 lightingColor;
    };

    struct LightSourceConstants {
        LightSourceType lightSourceType;
        float azimuthRadians;
        float elevationRadians;
        float specularExponent;
        float limitingConeAngleRadians;
        float coneCutOffLimit;
        float coneFullLight;
        vector_float3 position;
        vector_float3 pointsAt;
    };

    auto kernelUnitLength = FloatSize { m_effect->kernelUnitLengthX(), m_effect->kernelUnitLengthY() };
    if (kernelUnitLength.isZero())
        kernelUnitLength = FloatSize { 1.f, 1.f };

    auto lightingConstants = LightingConstants {
        .lightingType = m_effect->filterType() == FilterEffect::Type::FEDiffuseLighting ? LightingType::Diffuse : LightingType::Specular,
        .textureOrigin = { 0, 0 },
        .surfaceScale = m_effect->surfaceScale(),
        .lightingConstant = m_effect->filterType() == FilterEffect::Type::FEDiffuseLighting ? m_effect->diffuseConstant() : m_effect->specularConstant(),
        .specularExponent = m_effect->specularExponent(),
        .extentBottom = filterRegion.maxY(),
        .kernelUnitLength = { kernelUnitLength.width(), kernelUnitLength.height() },
        .lightingColor = { r, g, b, a },
    };

    auto lightSourceConstants = LightSourceConstants {
        .lightSourceType = lightSourceType(m_effect->lightSource().type()),
        .azimuthRadians = 0,
        .elevationRadians = 0,
        .specularExponent = 0,
        .limitingConeAngleRadians = 0,
        .coneCutOffLimit = 0,
        .coneFullLight = 0,
        .position = { },
        .pointsAt = { },
    };

    if (RefPtr distantLight = dynamicDowncast<DistantLightSource>(m_effect->lightSource())) {
        lightSourceConstants.azimuthRadians = deg2rad(distantLight->azimuth());
        lightSourceConstants.elevationRadians = deg2rad(distantLight->elevation());
    }

    if (RefPtr pointLight = dynamicDowncast<PointLightSource>(m_effect->lightSource())) {
        auto position = filter.resolvedPoint3D(pointLight->position());
        position.setXY(filter.scaledByFilterScale(position.xy()));
        position.setZ(position.z() * filter.filterScale().width()); // FIXME: this Z scale is different to what the software applier does.

        lightSourceConstants.position = { position.x(), position.y(), position.z() };
    }

    if (RefPtr spotLight = dynamicDowncast<SpotLightSource>(m_effect->lightSource())) {
        auto position = filter.resolvedPoint3D(spotLight->position());
        auto pointsAt = filter.resolvedPoint3D(spotLight->direction());

        position.setXY(filter.scaledByFilterScale(position.xy()));
        position.setZ(position.z() * filter.filterScale().width()); // FIXME: this Z scale is different to what the software applier does.

        pointsAt.setXY(filter.scaledByFilterScale(pointsAt.xy()));
        pointsAt.setZ(pointsAt.z() * filter.filterScale().width()); // FIXME: this Z scale is different to what the software applier does.

        // FIXME: need relative computation and normalizing.
        lightSourceConstants.position = { position.x(), position.y(), position.z() };
        lightSourceConstants.pointsAt = { pointsAt.x(), pointsAt.y(), pointsAt.z() };

        lightSourceConstants.specularExponent = spotLight->specularExponent();
        lightSourceConstants.limitingConeAngleRadians = deg2rad(spotLight->limitingConeAngle());

        // FIXME: Share code with the software applier.
        // Spot light edge darkening depends on an absolute treshold
        // according to the SVG 1.1 SE light regression tests (but this is not described in the spec?).
        static const float kAntiAliasTreshold = 0.016;
        if (spotLight->limitingConeAngle() == 0) {
            lightSourceConstants.coneCutOffLimit = 0;
            lightSourceConstants.coneFullLight = -kAntiAliasTreshold;
        } else {
            float limitingConeAngle = fabs(spotLight->limitingConeAngle());
            limitingConeAngle = std::min(limitingConeAngle, 90.0f);

            lightSourceConstants.coneCutOffLimit = std::cos(deg2rad(180.0f - limitingConeAngle));
            lightSourceConstants.coneFullLight = lightSourceConstants.coneCutOffLimit - kAntiAliasTreshold;
        }
    }

    RetainPtr<NSArray> arguments = @[
        inputImage.get(),
        [NSData dataWithBytes:&lightingConstants length:sizeof(LightingConstants)],
        [NSData dataWithBytes:&lightSourceConstants length:sizeof(LightSourceConstants)],
    ];

    RetainPtr<CIImage> outputImage = [kernel applyWithExtent:extent
        roiCallback:^CGRect(int, CGRect destRect) {
            return destRect;
        }
        arguments:arguments.get()];

    if (!outputImage)
        return false;

    // Part 2 of the workaround; this extent modification is necessary to avoid this -imageByCroppingToRect: being a no-op.
    extent.shiftMaxXEdgeBy(-1);
    outputImage = [outputImage imageByCroppingToRect:extent];
    result.setCIImage(WTF::move(outputImage));
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
