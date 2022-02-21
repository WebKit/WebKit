//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FrontendFeatures.h: Features/workarounds for driver bugs and other behaviors seen
// on all platforms.

#ifndef ANGLE_PLATFORM_FRONTENDFEATURES_H_
#define ANGLE_PLATFORM_FRONTENDFEATURES_H_

#include "platform/Feature.h"

namespace angle
{

struct FrontendFeatures : angle::FeatureSetBase
{
    FrontendFeatures();
    ~FrontendFeatures();

    // Force the context to be lost (via KHR_robustness) if a GL_OUT_OF_MEMORY error occurs. The
    // driver may be in an inconsistent state if this happens, and some users of ANGLE rely on this
    // notification to prevent further execution.
    angle::Feature loseContextOnOutOfMemory = {
        "lose_context_on_out_of_memory", angle::FeatureCategory::FrontendWorkarounds,
        "Some users rely on a lost context notification if a GL_OUT_OF_MEMORY "
        "error occurs",
        &members};

    // Program binaries don't contain transform feedback varyings on Qualcomm GPUs.
    // Work around this by disabling the program cache for programs with transform feedback.
    angle::Feature disableProgramCachingForTransformFeedback = {
        "disable_program_caching_for_transform_feedback",
        angle::FeatureCategory::FrontendWorkarounds,
        "On some GPUs, program binaries don't contain transform feedback varyings", &members};

    angle::Feature scalarizeVecAndMatConstructorArgs = {
        "scalarize_vec_and_mat_constructor_args", angle::FeatureCategory::FrontendWorkarounds,
        "Always rewrite vec/mat constructors to be consistent", &members,
        "http://crbug.com/1165751"};

    // Disable support for GL_OES_get_program_binary
    angle::Feature disableProgramBinary = {
        "disable_program_binary", angle::FeatureCategory::FrontendFeatures,
        "Disable support for GL_OES_get_program_binary", &members, "http://anglebug.com/5007"};

    // Allow disabling of GL_EXT_texture_filter_anisotropic through a runtime feature for
    // performance comparisons.
    angle::Feature disableAnisotropicFiltering = {
        "disable_anisotropic_filtering", angle::FeatureCategory::FrontendWorkarounds,
        "Disable support for anisotropic filtering", &members};

    // We can use this feature to override compressed format support for portability.
    angle::Feature allowCompressedFormats = {"allow_compressed_formats",
                                             angle::FeatureCategory::FrontendWorkarounds,
                                             "Allow compressed formats", &members};

    angle::Feature captureLimits = {"enable_capture_limits",
                                    angle::FeatureCategory::FrontendFeatures,
                                    "Set the context limits like frame capturing was enabled",
                                    &members, "http://anglebug.com/5750"};

    // Whether we should compress pipeline cache in thread pool before it's stored in blob cache.
    // http://anglebug.com/4722
    angle::Feature enableCompressingPipelineCacheInThreadPool = {
        "enableCompressingPipelineCacheInThreadPool", angle::FeatureCategory::FrontendWorkarounds,
        "Enable compressing pipeline cache in thread pool.", &members, "http://anglebug.com/4722"};

    // Forces on robust resource init. Useful for some tests to avoid undefined values.
    angle::Feature forceRobustResourceInit = {
        "forceRobustResourceInit", angle::FeatureCategory::FrontendFeatures,
        "Force-enable robust resource init", &members, "http://anglebug.com/6041"};

    // Forces on shader variable init to avoid undefined values in tests. This feature is enabled
    // for WebGL and frame capture, which both require deterministic results.
    angle::Feature forceInitShaderVariables = {
        "forceInitShaderVariables", angle::FeatureCategory::FrontendFeatures,
        "Force-enable shader variable initialization", &members};

    angle::Feature enableProgramBinaryForCapture = {
        "enableProgramBinaryForCapture", angle::FeatureCategory::FrontendFeatures,
        "Even if FrameCapture is enabled, enable GL_OES_get_program_binary", &members,
        "http://anglebug.com/5658"};
};

inline FrontendFeatures::FrontendFeatures()  = default;
inline FrontendFeatures::~FrontendFeatures() = default;

}  // namespace angle

#endif  // ANGLE_PLATFORM_FRONTENDFEATURES_H_
