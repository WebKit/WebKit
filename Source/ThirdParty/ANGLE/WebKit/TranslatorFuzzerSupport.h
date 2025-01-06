/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "compiler/translator/Compiler.h"

#if ANGLE_SH_VERSION != 371
#error Check if there are added options and update this check.
#endif

// name, index, allowed output languages expression, forced option value expression for output language
// default option for fuzzer to prevent unexpected behavior during compilation
// limitExpressionComplexity and limitCallStackDepth forced for all backends for fuzzing robustness purposes.
// validateAST is forced for all backeds for fixing the validation issues.
#define FOR_EACH_SH_COMPILE_OPTIONS_BOOL_OPTION(MACRO) \
    MACRO(objectCode, 0, any, none) \
    MACRO(outputDebugInfo, 1, any, none) \
    MACRO(sourcePath, 2, any, none) \
    MACRO(intermediateTree, 3, any, none) \
    MACRO(validateAST, 4, none, any) \
    MACRO(validateLoopIndexing, 5, any, none) \
    MACRO(lineDirectives, 6, any, none) \
    MACRO(removeInvariantAndCentroidForESSL3, 7, glsl, none) \
    MACRO(emulateAbsIntFunction, 8, glsl, none) \
    MACRO(enforcePackingRestrictions, 9, any, none) \
    MACRO(clampIndirectArrayBounds, 10, glsl, none) \
    MACRO(limitExpressionComplexity, 11, none, any) \
    MACRO(limitCallStackDepth, 12, none, any) \
    MACRO(initGLPosition, 13, any, none) \
    MACRO(unfoldShortCircuit, 14, appleGLSL, none) \
    MACRO(initOutputVariables, 15, any, none) \
    MACRO(scalarizeVecAndMatConstructorArgs, 16, appleGLSL, none) \
    MACRO(regenerateStructNames, 17, glsl, none) \
    MACRO(rewriteDoWhileLoops, 18, appleGLSL, none) \
    MACRO(expandSelectHLSLIntegerPowExpressions, 19, hlsl, none) \
    MACRO(flattenPragmaSTDGLInvariantAll, 20, any, none) \
    MACRO(HLSLGetDimensionsIgnoresBaseLevel, 21, hlsl, none) \
    MACRO(rewriteTexelFetchOffsetToTexelFetch, 22, glsl || hlsl, none) \
    MACRO(addAndTrueToLoopCondition, 23, appleGLSL, none) \
    MACRO(rewriteIntegerUnaryMinusOperator, 24, hlsl, none) \
    MACRO(emulateIsnanFloatFunction, 25, glsl || hlsl, none) \
    MACRO(useUnusedStandardSharedBlocks, 26, glsl, none) \
    MACRO(rewriteFloatUnaryMinusOperator, 27, appleGLSL, none) \
    MACRO(emulateAtan2FloatFunction, 28, glsl, none) \
    MACRO(initializeUninitializedLocals, 29, any, msl) \
    MACRO(initializeBuiltinsForInstancedMultiview, 30, glsl || hlsl, none) \
    MACRO(selectViewInNvGLSLVertexShader, 31, glsl, none) \
    MACRO(clampPointSize, 32, glsl || spirvVk, msl) \
    MACRO(addAdvancedBlendEquationsEmulation, 33, spirvVk, none) \
    MACRO(dontUseLoopsToInitializeVariables, 34, glsl, none) \
    MACRO(skipD3DConstantRegisterZero, 35, hlsl, none) \
    MACRO(clampFragDepth, 36, glsl || spirvVk || msl, none) \
    MACRO(rewriteRepeatedAssignToSwizzled, 37, glsl, none) \
    MACRO(emulateGLDrawID, 38, any, none) \
    MACRO(initSharedVariables, 39, any, none) \
    MACRO(forceAtomicValueResolution, 40, hlsl, none) \
    MACRO(emulateGLBaseVertexBaseInstance, 41, any, none) \
    MACRO(wrapSwitchInIfTrue, 42, spirvVk, none) \
    MACRO(takeVideoTextureAsExternalOES, 43, glsl, none) \
    MACRO(addBaseVertexToVertexID, 44, any, none) \
    MACRO(removeDynamicIndexingOfSwizzledVector, 45, glsl, none) \
    MACRO(allowTranslateUniformBlockToStructuredBuffer, 46, hlsl, none) \
    MACRO(addVulkanYUVLayoutQualifier, 47, spirvVk, none) \
    MACRO(disableARBTextureRectangle, 48, glsl, none) \
    MACRO(rewriteRowMajorMatrices, 49, appleGLSL, none) \
    MACRO(ignorePrecisionQualifiers, 50, spirvVk, none) \
    MACRO(addVulkanDepthCorrection, 51, spirvVk, none) \
    MACRO(forceShaderPrecisionHighpToMediump, 52, spirvVk, none) \
    MACRO(useSpecializationConstant, 53, spirvVk, none) \
    MACRO(addVulkanXfbEmulationSupportCode, 54, spirvVk, none) \
    MACRO(addVulkanXfbExtensionSupportCode, 55, spirvVk, none) \
    MACRO(initFragmentOutputVariables, 56, glsl, none) \
    MACRO(addExplicitBoolCasts, 57, msl, none) \
    MACRO(roundOutputAfterDithering, 58, spirvVk, none) \
    MACRO(castMediumpFloatTo16Bit, 59, spirvVk, none) \
    MACRO(passHighpToPackUnormSnormBuiltins, 60, glsl, none) \
    MACRO(emulateClipDistanceState, 61, glsl, none) \
    MACRO(emulateClipOrigin, 62, glsl, none) \
    MACRO(aliasedUnlessRestrict, 63, spirvVk, none) \
    MACRO(emulateAlphaToCoverage, 64, msl, none) \
    MACRO(rescopeGlobalVariables, 65, msl, none) \
    MACRO(preTransformTextureCubeGradDerivatives, 66, appleGLSL || msl, none) \
    MACRO(avoidOpSelectWithMismatchingRelaxedPrecision, 67, spirvVk, none) \
    MACRO(emitSPIRV14, 68, spirvVk, none) \
    MACRO(rejectWebglShadersWithUndefinedBehavior, 69, any, none) \
    MACRO(emulateR32fImageAtomicExchange, 70, spirvVk, none) \
    MACRO(simplifyLoopConditions, 71, none, msl) \
    MACRO(separateCompoundStructDeclarations, 72, none, msl || wgsl)

void filterOptions(ShShaderOutput output, ShCompileOptions& options);
ShShaderOutput resolveShaderOutput(ShShaderOutput output);

#define FOR_EACH_SH_BUILT_IN_RESOURCES_EXTENSION_OPTION(MACRO) \
    MACRO(OES_standard_derivatives, any) \
    MACRO(OES_EGL_image_external, any) \
    MACRO(OES_EGL_image_external_essl3, !msl) \
    MACRO(NV_EGL_stream_consumer_external, !msl) \
    MACRO(ARB_texture_rectangle, !msl) \
    MACRO(EXT_blend_func_extended, any) \
    MACRO(EXT_conservative_depth, any) \
    MACRO(EXT_draw_buffers, any) \
    MACRO(EXT_frag_depth, any) \
    MACRO(EXT_shader_texture_lod, any) \
    MACRO(EXT_shader_framebuffer_fetch, !msl) \
    MACRO(EXT_shader_framebuffer_fetch_non_coherent, !msl) \
    MACRO(NV_shader_framebuffer_fetch, !msl) \
    MACRO(NV_shader_noperspective_interpolation, any) \
    MACRO(ARM_shader_framebuffer_fetch, !msl) \
    MACRO(ARM_shader_framebuffer_fetch_depth_stencil, !msl) \
    MACRO(OVR_multiview, !msl) \
    MACRO(OVR_multiview2, !msl) \
    MACRO(EXT_multisampled_render_to_texture, any) \
    MACRO(EXT_multisampled_render_to_texture2, !msl) \
    MACRO(EXT_YUV_target, !msl) \
    MACRO(EXT_geometry_shader, !msl) \
    MACRO(OES_geometry_shader, !msl) \
    MACRO(OES_shader_io_blocks, !msl) \
    MACRO(EXT_shader_io_blocks, !msl) \
    MACRO(EXT_gpu_shader5, !msl) \
    MACRO(OES_gpu_shader5, !msl) \
    MACRO(EXT_shader_non_constant_global_initializers, !msl) \
    MACRO(OES_texture_storage_multisample_2d_array, !msl) \
    MACRO(OES_texture_3D, any) \
    MACRO(ANGLE_shader_pixel_local_storage, any) \
    MACRO(ANGLE_texture_multisample, !msl) \
    MACRO(ANGLE_multi_draw, !msl) \
    MACRO(ANGLE_base_vertex_base_instance, any) \
    MACRO(WEBGL_video_texture, !msl) \
    MACRO(APPLE_clip_distance, any) \
    MACRO(OES_texture_cube_map_array, !msl) \
    MACRO(EXT_texture_cube_map_array, !msl) \
    MACRO(EXT_texture_query_lod, !msl) \
    MACRO(EXT_texture_shadow_lod, any) \
    MACRO(EXT_shadow_samplers, !msl) \
    MACRO(OES_shader_multisample_interpolation, any) \
    MACRO(OES_shader_image_atomic, !msl) \
    MACRO(EXT_tessellation_shader, !msl) \
    MACRO(OES_tessellation_shader, !msl) \
    MACRO(OES_texture_buffer, !msl) \
    MACRO(EXT_texture_buffer, !msl) \
    MACRO(OES_sample_variables, any) \
    MACRO(EXT_clip_cull_distance, !msl) \
    MACRO(ANGLE_clip_cull_distance, any) \
    MACRO(EXT_primitive_bounding_box, !msl) \
    MACRO(OES_primitive_bounding_box, !msl) \
    MACRO(EXT_separate_shader_objects, !msl) \
    MACRO(ANGLE_base_vertex_base_instance_shader_builtin, any) \
    MACRO(ANDROID_extension_pack_es31a, !msl) \
    MACRO(KHR_blend_equation_advanced, !msl)

struct GLSLDumpHeader {
    static constexpr int kHeaderSize = 128;
    static constexpr int kBaseOptionsSize = offsetof(ShCompileOptions, metal);

    GLSLDumpHeader(const uint8_t* data)
    {
        memcpy(&type, data, 4);
        data += 4;
        uint32_t local = 0;
        memcpy(&local, data, 4);
        spec = static_cast<ShShaderSpec>(local);
        data += 4;
        memcpy(&local, data, 4);
        output = static_cast<ShShaderOutput>(local);
        data += 4;
        memcpy(&options, data, kBaseOptionsSize);
        data += 32;
        memcpy(&options.metal, data, sizeof(options.metal));
        data += 32;
        memcpy(&options.pls, data, sizeof(options.pls));
    }

    void write(uint8_t* data)
    {
        memset(data, 0, kHeaderSize);
        memcpy(data, &type, 4);
        data += 4;
        memcpy(data, &spec, 4);
        data += 4;
        memcpy(data, &output, 4);
        data += 4;
        memcpy(data, &options, kBaseOptionsSize);
        data += 32;
        memcpy(data, &options.metal, sizeof(options.metal));
        data += 32;
        memcpy(data, &options.pls, sizeof(options.pls));
    }

    uint32_t type = 0;
    ShShaderSpec spec = static_cast<ShShaderSpec>(0);
    ShShaderOutput output = static_cast<ShShaderOutput>(0);
    ShCompileOptions options { };
};
