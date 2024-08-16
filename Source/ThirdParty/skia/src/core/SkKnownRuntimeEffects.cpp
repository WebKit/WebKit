/*
 * Copyright 2024 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkKnownRuntimeEffects.h"

#include "include/core/SkString.h"
#include "include/effects/SkRuntimeEffect.h"
#include "src/core/SkRuntimeEffectPriv.h"
#include "src/effects/imagefilters/SkMatrixConvolutionImageFilter.h"

namespace SkKnownRuntimeEffects {

namespace {

// This must be kept in sync w/ the version in BlurUtils.h
static constexpr int kMaxBlurSamples = 28;

SkRuntimeEffect* make_blur_1D_effect(int kernelWidth, const SkRuntimeEffect::Options& options) {
    SkASSERT(kernelWidth <= kMaxBlurSamples);
    // The SkSL structure performs two kernel taps; if the kernel has an odd width the last
    // sample will be skipped with the current loop limit calculation.
    SkASSERT(kernelWidth % 2 == 0);
    return SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
            SkStringPrintf(
                    // The coefficients are always stored for the max radius to keep the
                    // uniform block consistent across all effects.
                    "const int kMaxUniformKernelSize = %d / 2;"
                    // But we generate an exact loop over the kernel size. Note that this
                    // program can be used for kernels smaller than the constructed max as long
                    // as the kernel weights for excess entries are set to 0.
                    "const int kMaxLoopLimit = %d / 2;"

                    "uniform half4 offsetsAndKernel[kMaxUniformKernelSize];"
                    "uniform half2 dir;"

                    "uniform shader child;"

                    "half4 main(float2 coord) {"
                        "half4 sum = half4(0);"
                        "for (int i = 0; i < kMaxLoopLimit; ++i) {"
                            "half4 s = offsetsAndKernel[i];"
                            "sum += s.y * child.eval(coord + s.x*dir);"
                            "sum += s.w * child.eval(coord + s.z*dir);"
                        "}"
                        "return sum;"
                    "}", kMaxBlurSamples, kernelWidth).c_str(),
                    options);
}

SkRuntimeEffect* make_blur_2D_effect(int maxKernelSize, const SkRuntimeEffect::Options& options) {
    SkASSERT(maxKernelSize % 4 == 0);
    return SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
            SkStringPrintf(
                    // The coefficients are always stored for the max radius to keep the
                    // uniform block consistent across all effects.
                    "const int kMaxUniformKernelSize = %d / 4;"
                    "const int kMaxUniformOffsetsSize = 2*kMaxUniformKernelSize;"
                    // But we generate an exact loop over the kernel size. Note that this
                    // program can be used for kernels smaller than the constructed max as long
                    // as the kernel weights for excess entries are set to 0.
                    "const int kMaxLoopLimit = %d / 4;"

                    // Pack scalar coefficients into half4 for better packing on std140, and
                    // upload offsets to avoid having to transform the 1D index into a 2D coord
                    "uniform half4 kernel[kMaxUniformKernelSize];"
                    "uniform half4 offsets[kMaxUniformOffsetsSize];"

                    "uniform shader child;"

                    "half4 main(float2 coord) {"
                        "half4 sum = half4(0);"

                        "for (int i = 0; i < kMaxLoopLimit; ++i) {"
                            "half4 k = kernel[i];"
                            "half4 o = offsets[2*i];"
                            "sum += k.x * child.eval(coord + o.xy);"
                            "sum += k.y * child.eval(coord + o.zw);"
                            "o = offsets[2*i + 1];"
                            "sum += k.z * child.eval(coord + o.xy);"
                            "sum += k.w * child.eval(coord + o.zw);"
                        "}"
                        "return sum;"
                    "}", kMaxBlurSamples, maxKernelSize).c_str(),
                    options);
}

enum class MatrixConvolutionImpl {
    kUniformBased,
    kTextureBasedSm,
    kTextureBasedLg,
};

// There are three shader variants:
//    a smaller kernel version that stores the matrix in uniforms and iterates in 1D
//    a larger kernel version that stores the matrix in a 1D texture. The texture version has small
//    and large variants w/ the actual kernel size uploaded as a uniform.
SkRuntimeEffect* make_matrix_conv_effect(MatrixConvolutionImpl impl,
                                         const SkRuntimeEffect::Options& options) {
    // While the uniforms and kernel access are different, pieces of the algorithm are common and
    // defined statically for re-use in the two shaders:
    static const char* kHeaderAndBeginLoopSkSL =
        "uniform int2 size;"
        "uniform int2 offset;"
        "uniform half2 gainAndBias;"
        "uniform int convolveAlpha;" // FIXME not a full  int? Put in a half3 w/ gainAndBias?

        "uniform shader child;"

        "half4 main(float2 coord) {"
            "half4 sum = half4(0);"
            "half origAlpha = 0;"
            "int2 kernelPos = int2(0);"
            "for (int i = 0; i < kMaxKernelSize; ++i) {"
                "if (kernelPos.y >= size.y) { break; }";

    // Used in the inner loop to accumulate convolution sum and increment the kernel position
    static const char* kAccumulateAndIncrementSkSL =
                "half4 c = child.eval(coord + half2(kernelPos) - half2(offset));"
                "if (convolveAlpha == 0) {"
                    // When not convolving alpha, remember the original alpha for actual sample
                    // coord, and perform accumulation on unpremul colors.
                    "if (kernelPos == offset) {"
                        "origAlpha = c.a;"
                    "}"
                    "c = unpremul(c);"
                "}"
                "sum += c*k;"
                "kernelPos.x += 1;"
                "if (kernelPos.x >= size.x) {"
                    "kernelPos.x = 0;"
                    "kernelPos.y += 1;"
                "}";

    // Closes the loop and calculates final color
    static const char* kCloseLoopAndFooterSkSL =
            "}"
            "half4 color = sum*gainAndBias.x + gainAndBias.y;"
            "if (convolveAlpha == 0) {"
                // Reset the alpha to the original and convert to premul RGB
                "color = half4(color.rgb*origAlpha, origAlpha);"
            "} else {"
                // Ensure convolved alpha is within [0, 1]
                "color.a = saturate(color.a);"
            "}"
            // Make RGB valid premul w/ respect to the alpha (either original or convolved)
            "color.rgb = clamp(color.rgb, 0, color.a);"
            "return color;"
        "}";

    static const auto makeTextureEffect = [](int maxTextureKernelSize,
                                             const SkRuntimeEffect::Options& options) {
        return SkMakeRuntimeEffect(
                        SkRuntimeEffect::MakeForShader,
                        SkStringPrintf("const int kMaxKernelSize = %d;"
                                       "uniform shader kernel;"
                                       "uniform half2 innerGainAndBias;"
                                       "%s" // kHeaderAndBeginLoopSkSL
                                               "half k = kernel.eval(half2(half(i) + 0.5, 0.5)).a;"
                                               "k = k * innerGainAndBias.x + innerGainAndBias.y;"
                                               "%s" // kAccumulateAndIncrementSkSL
                                       "%s", // kCloseLoopAndFooterSkSL
                                       maxTextureKernelSize,
                                       kHeaderAndBeginLoopSkSL,
                                       kAccumulateAndIncrementSkSL,
                                       kCloseLoopAndFooterSkSL).c_str(),
                        options);
    };

    switch (impl) {
        case MatrixConvolutionImpl::kUniformBased: {
            return SkMakeRuntimeEffect(
                        SkRuntimeEffect::MakeForShader,
                        SkStringPrintf("const int kMaxKernelSize = %d / 4;"
                                       "uniform half4 kernel[kMaxKernelSize];"
                                       "%s" // kHeaderAndBeginLoopSkSL
                                                "half4 k4 = kernel[i];"
                                                "for (int j = 0; j < 4; ++j) {"
                                                    "if (kernelPos.y >= size.y) { break; }"
                                                    "half k = k4[j];"
                                                    "%s" // kAccumulateAndIncrementSkSL
                                                "}"
                                       "%s", // kCloseLoopAndFooterSkSL
                                       MatrixConvolutionImageFilter::kMaxUniformKernelSize,
                                       kHeaderAndBeginLoopSkSL,
                                       kAccumulateAndIncrementSkSL,
                                       kCloseLoopAndFooterSkSL).c_str(),
                        options);
        }
        case MatrixConvolutionImpl::kTextureBasedSm:
            return makeTextureEffect(MatrixConvolutionImageFilter::kSmallKernelSize, options);
        case MatrixConvolutionImpl::kTextureBasedLg:
            return makeTextureEffect(MatrixConvolutionImageFilter::kLargeKernelSize, options);
    }

    SkUNREACHABLE;
}

} // anonymous namespace

const SkRuntimeEffect* GetKnownRuntimeEffect(StableKey stableKey) {
    SkRuntimeEffect::Options options;
    SkRuntimeEffectPriv::SetStableKey(&options, static_cast<uint32_t>(stableKey));
    SkRuntimeEffectPriv::AllowPrivateAccess(&options);

    switch (stableKey) {
        case StableKey::kInvalid:
            return nullptr;

        // Shaders
        case StableKey::k1DBlur4: {
            static SkRuntimeEffect* s1DBlurEffect = make_blur_1D_effect(4, options);
            return s1DBlurEffect;
        }
        case StableKey::k1DBlur8: {
            static SkRuntimeEffect* s1DBlurEffect = make_blur_1D_effect(8, options);
            return s1DBlurEffect;
        }
        case StableKey::k1DBlur12: {
            static SkRuntimeEffect* s1DBlurEffect = make_blur_1D_effect(12, options);
            return s1DBlurEffect;
        }
        case StableKey::k1DBlur16: {
            static SkRuntimeEffect* s1DBlurEffect = make_blur_1D_effect(16, options);
            return s1DBlurEffect;
        }
        case StableKey::k1DBlur20: {
            static SkRuntimeEffect* s1DBlurEffect = make_blur_1D_effect(20, options);
            return s1DBlurEffect;
        }
        case StableKey::k1DBlur28: {
            static SkRuntimeEffect* s1DBlurEffect = make_blur_1D_effect(28, options);
            return s1DBlurEffect;
        }
        case StableKey::k2DBlur4: {
            static SkRuntimeEffect* s2DBlurEffect = make_blur_2D_effect(4, options);
            return s2DBlurEffect;
        }
        case StableKey::k2DBlur8: {
            static SkRuntimeEffect* s2DBlurEffect = make_blur_2D_effect(8, options);
            return s2DBlurEffect;
        }
        case StableKey::k2DBlur12: {
            static SkRuntimeEffect* s2DBlurEffect = make_blur_2D_effect(12, options);
            return s2DBlurEffect;
        }
        case StableKey::k2DBlur16: {
            static SkRuntimeEffect* s2DBlurEffect = make_blur_2D_effect(16, options);
            return s2DBlurEffect;
        }
        case StableKey::k2DBlur20: {
            static SkRuntimeEffect* s2DBlurEffect = make_blur_2D_effect(20, options);
            return s2DBlurEffect;
        }
        case StableKey::k2DBlur28: {
            static SkRuntimeEffect* s2DBlurEffect = make_blur_2D_effect(28, options);
            return s2DBlurEffect;
        }
        case StableKey::kBlend: {
            static constexpr char kBlendShaderCode[] =
                "uniform shader s, d;"
                "uniform blender b;"
                "half4 main(float2 xy) {"
                    "return b.eval(s.eval(xy), d.eval(xy));"
                "}";

            static const SkRuntimeEffect* sBlendEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
                                        kBlendShaderCode,
                                        options);
            return sBlendEffect;
        }
        case StableKey::kLerp: {
            static constexpr char kLerpFilterCode[] =
                "uniform colorFilter cf0;"
                "uniform colorFilter cf1;"
                "uniform half weight;"

                "half4 main(half4 color) {"
                    "return mix(cf0.eval(color), cf1.eval(color), weight);"
                "}";

            static const SkRuntimeEffect* sLerpEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter,
                                        kLerpFilterCode,
                                        options);
            return sLerpEffect;
        }
        case StableKey::kMatrixConvUniforms: {
            static const SkRuntimeEffect* sMatrixConvUniformsEffect =
                    make_matrix_conv_effect(MatrixConvolutionImpl::kUniformBased, options);
            return sMatrixConvUniformsEffect;
        }

        case StableKey::kMatrixConvTexSm: {
            static const SkRuntimeEffect* sMatrixConvTexSmEffect =
                    make_matrix_conv_effect(MatrixConvolutionImpl::kTextureBasedSm, options);
            return sMatrixConvTexSmEffect;
        }

        case StableKey::kMatrixConvTexLg: {
            static const SkRuntimeEffect* sMatrixConvTexMaxEffect =
                    make_matrix_conv_effect(MatrixConvolutionImpl::kTextureBasedLg, options);
            return sMatrixConvTexMaxEffect;
        }
        case StableKey::kDecal: {
            static constexpr char kDecalShaderCode[] =
                "uniform shader image;"
                "uniform float4 decalBounds;"

                "half4 main(float2 coord) {"
                    "return sk_decal(image, coord, decalBounds);"
                "}";

            static const SkRuntimeEffect* sDecalEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
                                        kDecalShaderCode,
                                        options);
            return sDecalEffect;
        }
        case StableKey::kDisplacement: {
            // NOTE: This uses dot product selection to work on all GLES2 hardware (enforced by
            // public runtime effect restrictions). Otherwise, this would use a "uniform ivec2"
            // and component indexing to convert the displacement color into a vector.
            static constexpr char kDisplacementShaderCode[] =
                "uniform shader displMap;"
                "uniform shader colorMap;"
                "uniform half2 scale;"
                "uniform half4 xSelect;" // Only one of RGBA will be 1, the rest are 0
                "uniform half4 ySelect;"

                "half4 main(float2 coord) {"
                    "return sk_displacement(displMap, colorMap, coord, scale, xSelect, ySelect);"
                "}";

            static const SkRuntimeEffect* sDisplacementEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
                                        kDisplacementShaderCode,
                                        options);
            return sDisplacementEffect;
        }
        case StableKey::kLighting: {
            static constexpr char kLightingShaderCode[] =
                "uniform shader normalMap;"

                // Packs surface depth, shininess, material type (0 == diffuse) and light type
                // (< 0 = distant, 0 = point, > 0 = spot)
                "uniform half4 materialAndLightType;"

                "uniform half4 lightPosAndSpotFalloff;" // (x,y,z) are lightPos, w is spot falloff
                                                        // exponent
                "uniform half4 lightDirAndSpotCutoff;" // (x,y,z) are lightDir,
                                                       // w is spot cos(cutoffAngle)
                "uniform half3 lightColor;" // Material's k has already been multiplied in

                "half4 main(float2 coord) {"
                    "return sk_lighting(normalMap, coord,"
                                        /*depth=*/"materialAndLightType.x,"
                                        /*shininess=*/"materialAndLightType.y,"
                                        /*materialType=*/"materialAndLightType.z,"
                                        /*lightType=*/"materialAndLightType.w,"
                                        /*lightPos=*/"lightPosAndSpotFalloff.xyz,"
                                        /*spotFalloff=*/"lightPosAndSpotFalloff.w,"
                                        /*lightDir=*/"lightDirAndSpotCutoff.xyz,"
                                        /*cosCutoffAngle=*/"lightDirAndSpotCutoff.w,"
                                        "lightColor);"
                "}";

            static const SkRuntimeEffect* sLightingEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
                                        kLightingShaderCode,
                                        options);
            return sLightingEffect;
        }
        case StableKey::kLinearMorphology: {
            static constexpr char kLinearMorphologyShaderCode[] =
                "uniform shader child;"
                "uniform half2 offset;"
                "uniform half flip;" // -1 converts the max() calls to min()
                "uniform int radius;"

                "half4 main(float2 coord) {"
                    "return sk_linear_morphology(child, coord, offset, flip, radius);"
                "}";

            static const SkRuntimeEffect* sLinearMorphologyEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
                                        kLinearMorphologyShaderCode,
                                        options);
            return sLinearMorphologyEffect;
        }

        case StableKey::kMagnifier: {
            static constexpr char kMagnifierShaderCode[] =
                "uniform shader src;"
                "uniform float4 lensBounds;"
                "uniform float4 zoomXform;"
                "uniform float2 invInset;"

                "half4 main(float2 coord) {"
                    "return sk_magnifier(src, coord, lensBounds, zoomXform, invInset);"
                "}";

            static const SkRuntimeEffect* sMagnifierEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
                                        kMagnifierShaderCode,
                                        options);
            return sMagnifierEffect;
        }
        case StableKey::kNormal: {
            static constexpr char kNormalShaderCode[] =
                "uniform shader alphaMap;"
                "uniform float4 edgeBounds;"
                "uniform half negSurfaceDepth;"

                "half4 main(float2 coord) {"
                   "return sk_normal(alphaMap, coord, edgeBounds, negSurfaceDepth);"
                "}";

            static const SkRuntimeEffect* sNormalEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
                                        kNormalShaderCode,
                                        options);
            return sNormalEffect;
        }
        case StableKey::kSparseMorphology: {
            static constexpr char kSparseMorphologyShaderCode[] =
                "uniform shader child;"
                "uniform half2 offset;"
                "uniform half flip;"

                "half4 main(float2 coord) {"
                    "return sk_sparse_morphology(child, coord, offset, flip);"
                "}";

            static const SkRuntimeEffect* sSparseMorphologyEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader,
                                        kSparseMorphologyShaderCode,
                                        options);
            return sSparseMorphologyEffect;
        }

        // Blenders
        case StableKey::kArithmetic: {
            static constexpr char kArithmeticBlenderCode[] =
                "uniform half4 k;"
                "uniform half pmClamp;"

                "half4 main(half4 src, half4 dst) {"
                    "return sk_arithmetic_blend(src, dst, k, pmClamp);"
                "}";

            static const SkRuntimeEffect* sArithmeticEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForBlender,
                                        kArithmeticBlenderCode,
                                        options);
            return sArithmeticEffect;
        }

        // Color Filters
        case StableKey::kHighContrast: {
            static constexpr char kHighContrastFilterCode[] =
                "uniform half grayscale, invertStyle, contrast;"
                "half4 main(half4 color) {"
                    "return half4(sk_high_contrast(color.rgb, grayscale, invertStyle, contrast),"
                                 "color.a);"
                "}";

            static const SkRuntimeEffect* sHighContrastEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter,
                                        kHighContrastFilterCode,
                                        options);
            return sHighContrastEffect;
        }

        case StableKey::kLuma: {
            static constexpr char kLumaFilterCode[] =
                "half4 main(half4 color) {"
                    "return sk_luma(color.rgb);"
                "}";

            static const SkRuntimeEffect* sLumaEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter,
                                        kLumaFilterCode,
                                        options);
            return sLumaEffect;
        }

        case StableKey::kOverdraw: {
            static constexpr char kOverdrawFilterCode[] =
                "uniform half4 color0, color1, color2, color3, color4, color5;"

                "half4 main(half4 color) {"
                    "return sk_overdraw(color.a, color0, color1, color2, color3, color4, color5);"
                "}";

            static const SkRuntimeEffect* sOverdrawEffect =
                    SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter,
                                        kOverdrawFilterCode,
                                        options);
            return sOverdrawEffect;
        }
    }

    SkUNREACHABLE;
}

} // namespace SkKnownRuntimeEffects
