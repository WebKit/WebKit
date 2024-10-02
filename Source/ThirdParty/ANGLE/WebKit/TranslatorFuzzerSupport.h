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

// name, index, allowed output languages expression, forced option value expression for output language
// default option for fuzzer to prevent unexpected behavior during compilation
// limitExpressionComplexity and limitCallStackDepth forced for all backends for fuzzing robustness purposes.
#define FOR_EACH_SH_COMPILE_OPTIONS_BOOL_OPTION(MACRO) \
    MACRO(objectCode, 0, any, none) \
    MACRO(outputDebugInfo, 1, any, none) \
    MACRO(sourcePath, 2, any, none) \
    MACRO(intermediateTree, 3, any, none) \
    MACRO(validateAST, 4, any, none) \
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
    MACRO(initializeUninitializedLocals, 29, any, none) \
    MACRO(initializeBuiltinsForInstancedMultiview, 30, glsl || hlsl, none) \
    MACRO(selectViewInNvGLSLVertexShader, 31, glsl, none) \
    MACRO(clampPointSize, 32, glsl || msl || spirvVk, none) \
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
    MACRO(rejectWebglShadersWithUndefinedBehavior, 69, any, none)

void filterOptions(ShShaderOutput output, ShCompileOptions& options);

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
