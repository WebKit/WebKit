/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#import "MetalColorConversion.h"

#import "ColorConversion.h"
#import "ColorSpace.h"
#import "ColorSpaceName.h"
#import "ColorTransferFunctions.h"
#import "ColorTypes.h"

namespace WebCore {

constexpr ASCIILiteral metalColorConversionShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

constant uint colorModelRGB = 0;
constant uint colorModelLab = 1;
constant uint colorModelLCH = 2;
constant uint colorModelOKLab = 3;
constant uint colorModelOKLCH = 4;
constant uint colorModelHSL = 5;
constant uint colorModelHWB = 6;

struct TransferFunction {
    float threshold;
    float linearThreshold;
    float slope;
    float scale;
    float offset;
    float exponent;
};

struct ColorConversionUniforms {
    float3x3 toXYZD65;
    float3x3 fromXYZD65;
    float3x3 okLabToNonLinearLMS;
    float3 minimum;
    float3 maximum;
    float3 whitePoint;
    TransferFunction sourceTransferFunction;
    TransferFunction destinationTransferFunction;
    float labE;
    float labK;
    int hueIndex;
    uint model;
};

static float3 toLinear(float3 components, TransferFunction function)
{
    // Equivalent to PiecewiseTransferFunction::toLinear().
    float3 magnitude = abs(components);
    float3 curve = sign(components) * pow((magnitude + function.offset) / function.scale, float3(function.exponent));
    return select(curve, components / function.slope, magnitude <= function.threshold);
}

static float3 toEncoded(float3 components, TransferFunction function)
{
    // Equivalent to PiecewiseTransferFunction::toGammaEncoded().
    float3 magnitude = abs(components);
    float3 curve = sign(components) * (function.scale * pow(magnitude, float3(1 / function.exponent)) - function.offset);
    return select(curve, components * function.slope, magnitude <= function.linearThreshold);
}

static float3 rectangularFromPolar(float3 lightnessChromaHue)
{
    // Equivalent to convertToRectangularForm().
    float radians = lightnessChromaHue.z * M_PI_F / 180;
    return float3(lightnessChromaHue.x, lightnessChromaHue.y * cos(radians), lightnessChromaHue.y * sin(radians));
}

static float3 xyzD50FromLab(float3 lab, constant ColorConversionUniforms& uniforms)
{
    // Equivalent to ColorConversion<XYZA<float, WhitePoint::D50>, Lab<float>>::convert().
    float lightness116 = lab.x / 116;
    float a500 = lab.y / 500;
    float b200 = lab.z / 200;

    float f1 = lightness116 + 16.0 / 116;
    float3 f(f1 + a500, f1, f1 - b200);
    float3 linear(lightness116 + a500, lightness116, lightness116 - b200);
    float3 cubed = f * f * f;
    float3 result = select(linear * (116 / uniforms.labK), cubed, cubed > float3(uniforms.labE));
    result.y = lab.x > uniforms.labK * uniforms.labE ? cubed.y : lab.x / uniforms.labK;
    return result * uniforms.whitePoint;
}

static float3 linearLMSFromOKLab(float3 oklab, constant ColorConversionUniforms& uniforms)
{
    // Equivalent to ColorConversion<XYZA<float, WhitePoint::D65>, OKLab<float>>::convert(), up to linear LMS.
    float3 nonLinearLMS = uniforms.okLabToNonLinearLMS * oklab;
    return nonLinearLMS * nonLinearLMS * nonLinearLMS;
}

static float3 pureHueRGB(float hue)
{
    // Equivalent to `component` in `hueToRGB` in ColorConversion<ExtendedSRGBA<float>, HWBA<float>>::convert().
    float3 k = fmod(float3(0, 8, 4) + hue / 30, float3(12));
    return 0.5 - max(float3(-1), min(min(k - 3, 9 - k), float3(1))) / 2;
}

static float3 sRGBFromHSL(float3 hsl)
{
    // Equivalent to ColorConversion<ExtendedSRGBA<float>, HSLA<float>>::convert().
    float saturation = hsl.y / 100;
    float lightness = hsl.z / 100;
    return lightness + (pureHueRGB(hsl.x) * 2 - 1) * saturation * min(lightness, 1 - lightness);
}

static float3 sRGBFromHWB(float3 hwb)
{
    // Equivalent to ColorConversion<ExtendedSRGBA<float>, HWBA<float>>::convert().
    float whiteness = hwb.y / 100;
    float blackness = hwb.z / 100;
    if (whiteness + blackness >= 1)
        return float3(whiteness / (whiteness + blackness));
    return pureHueRGB(hwb.x) * (1 - whiteness - blackness) + whiteness;
}

static float3 linearComponents(float3 components, constant ColorConversionUniforms& uniforms)
{
    switch (uniforms.model) {
    case colorModelLab:
        return xyzD50FromLab(components, uniforms);
    case colorModelLCH:
        return xyzD50FromLab(rectangularFromPolar(components), uniforms);
    case colorModelOKLab:
        return linearLMSFromOKLab(components, uniforms);
    case colorModelOKLCH:
        return linearLMSFromOKLab(rectangularFromPolar(components), uniforms);
    case colorModelHSL:
        return toLinear(sRGBFromHSL(components), uniforms.sourceTransferFunction);
    case colorModelHWB:
        return toLinear(sRGBFromHWB(components), uniforms.sourceTransferFunction);
    case colorModelRGB:
    default:
        return toLinear(components, uniforms.sourceTransferFunction);
    }
}

float3 normalizedColorComponents(float3 components, constant ColorConversionUniforms& uniforms)
{
    // Equivalent to makeColorTypeByNormalizingComponents().
    if (uniforms.hueIndex >= 0)
        components[uniforms.hueIndex] -= 360 * floor(components[uniforms.hueIndex] / 360);
    return clamp(components, uniforms.minimum, uniforms.maximum);
}

float3 convertColorComponents(float3 components, constant ColorConversionUniforms& uniforms)
{
    float3 linear = uniforms.fromXYZD65 * (uniforms.toXYZD65 * linearComponents(components, uniforms));
    return toEncoded(linear, uniforms.destinationTransferFunction);
}
)"_s;

// Matches the constants in the shader.
static constexpr uint32_t colorModelRGB = 0;
static constexpr uint32_t colorModelLab = 1;
static constexpr uint32_t colorModelLCH = 2;
static constexpr uint32_t colorModelOKLab = 3;
static constexpr uint32_t colorModelOKLCH = 4;
static constexpr uint32_t colorModelHSL = 5;
static constexpr uint32_t colorModelHWB = 6;

static constexpr auto infinity = std::numeric_limits<float>::infinity();
static constexpr MetalTransferFunction linearTransferFunction { infinity, infinity, 1, 1, 0, 1 };

static constexpr MetalTransferFunction toMetal(const ParametricTransferFunction<float>& function)
{
    return { function.threshold, function.linearThreshold, function.slope, function.scale, function.offset, function.exponent };
}

static simd_float3x3 toMetal(const ColorMatrix<3, 3>& matrix)
{
    return simd_matrix_from_rows(
        simd_float3 { matrix.at(0, 0), matrix.at(0, 1), matrix.at(0, 2) },
        simd_float3 { matrix.at(1, 0), matrix.at(1, 1), matrix.at(1, 2) },
        simd_float3 { matrix.at(2, 0), matrix.at(2, 1), matrix.at(2, 2) });
}

static simd_float3x3 toXYZD65(WhitePoint whitePoint)
{
    if (whitePoint == WhitePoint::D50)
        return toMetal(ChromaticAdaptation<WhitePoint::D50, WhitePoint::D65>::matrix);
    return matrix_identity_float3x3;
}

struct MetalColorSource {
    uint32_t model;
    simd_float3x3 toXYZD65;
    MetalTransferFunction transferFunction;
};

template<typename ColorType> static MetalColorSource metalColorSource()
{
    if constexpr (HasDescriptorMember<ColorType>) {
        auto matrix = simd_mul(toXYZD65(ColorType::whitePoint), toMetal(ColorType::Descriptor::linearToXYZ));
        if constexpr (HasLinearCounterpartMember<ColorType>)
            return { colorModelRGB, matrix, toMetal(ColorType::TransferFunction::parametric) };
        else
            return { colorModelRGB, matrix, linearTransferFunction };
    } else if constexpr (std::same_as<ColorType, HSLA<float>> || std::same_as<ColorType, HWBA<float>>) {
        auto model = std::same_as<ColorType, HSLA<float>> ? colorModelHSL : colorModelHWB;
        return { model, toMetal(SRGBADescriptor::linearToXYZ), toMetal(SRGBA<float>::TransferFunction::parametric) };
    } else if constexpr (std::same_as<ColorType, Lab<float>> || std::same_as<ColorType, LCHA<float>>) {
        auto model = std::same_as<ColorType, Lab<float>> ? colorModelLab : colorModelLCH;
        return { model, toXYZD65(WhitePoint::D50), linearTransferFunction };
    } else if constexpr (std::same_as<ColorType, OKLab<float>> || std::same_as<ColorType, OKLCHA<float>>) {
        auto model = std::same_as<ColorType, OKLab<float>> ? colorModelOKLab : colorModelOKLCH;
        return { model, toMetal(LinearLMSToXYZD65), linearTransferFunction };
    } else {
        static_assert(std::same_as<ColorType, XYZA<float, WhitePoint::D50>> || std::same_as<ColorType, XYZA<float, WhitePoint::D65>>);
        return { colorModelRGB, toXYZD65(ColorType::whitePoint), linearTransferFunction };
    }
}

struct MetalColorDestination {
    simd_float3x3 fromXYZD65;
    MetalTransferFunction transferFunction;
};

template<typename ColorType> static MetalColorDestination metalColorDestination()
{
    auto matrix = toMetal(ColorType::Descriptor::xyzToLinear);
    if constexpr (HasLinearCounterpartMember<ColorType>)
        return { matrix, toMetal(ColorType::TransferFunction::parametric) };
    else
        return { matrix, linearTransferFunction };
}

static std::optional<MetalColorDestination> metalColorDestination(const ColorSpace& colorSpace)
{
    if (colorSpace == ColorSpace::SRGB() || colorSpace == ColorSpace::ExtendedSRGB())
        return metalColorDestination<SRGBA<float>>();
    if (colorSpace == ColorSpace::LinearSRGB() || colorSpace == ColorSpace::ExtendedLinearSRGB())
        return metalColorDestination<LinearSRGBA<float>>();
    if (colorSpace == ColorSpace::DisplayP3() || colorSpace == ColorSpace::ExtendedDisplayP3())
        return metalColorDestination<DisplayP3<float>>();
    if (colorSpace == ColorSpace::LinearDisplayP3() || colorSpace == ColorSpace::ExtendedLinearDisplayP3())
        return metalColorDestination<LinearDisplayP3<float>>();
    return std::nullopt;
}

std::optional<MetalColorConversionUniforms> makeMetalColorConversionUniforms(ColorSpaceName sourceColorSpace, const ColorSpace& destinationColorSpace)
{
    auto destination = metalColorDestination(destinationColorSpace);
    if (!destination)
        return std::nullopt;

    return callWithColorType<float>(sourceColorSpace, [&]<typename ColorType> -> MetalColorConversionUniforms {
        auto source = metalColorSource<ColorType>();
        constexpr auto componentInfo = ColorType::Model::componentInfo;

        int32_t hueIndex = -1;
        for (int32_t i = 0; i < 3; ++i) {
            if (componentInfo[i].type == ColorComponentType::Angle)
                hueIndex = i;
        }

        return {
            source.toXYZD65,
            destination->fromXYZD65,
            toMetal(OKLabToNonLinearLMS),
            { componentInfo[0].min, componentInfo[1].min, componentInfo[2].min },
            { componentInfo[0].max, componentInfo[1].max, componentInfo[2].max },
            { D50WhitePoint[0], D50WhitePoint[1], D50WhitePoint[2] },
            source.transferFunction,
            destination->transferFunction,
            LABe,
            LABk,
            hueIndex,
            source.model,
        };
    });
}

} // namespace WebCore
