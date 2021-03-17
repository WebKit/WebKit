//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_constnats.h:
//      Declares numerical and string constants used in Metal
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_CONSTANTS_H
#define LIBANGLE_RENDERER_METAL_MTL_CONSTANTS_H
#include "libANGLE/Constants.h"
#include "libANGLE/Version.h"
#include "libANGLE/angletypes.h"

namespace rx
{
namespace mtl
{

// NOTE(hqle): support variable max number of vertex attributes
constexpr uint32_t kMaxVertexAttribs = gl::MAX_VERTEX_ATTRIBS;
// NOTE(hqle): support variable max number of render targets
constexpr uint32_t kMaxRenderTargets = 4;

constexpr uint32_t kMaxShaderUBOs = 12;
constexpr uint32_t kMaxUBOSize    = 16384;

constexpr uint32_t kMaxShaderXFBs = gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS;

// The max size of a buffer that will be allocated in shared memory.
// NOTE(hqle): This is just a hint. There is no official document on what is the max allowed size
// for shared memory.
constexpr size_t kSharedMemBufferMaxBufSizeHint = 128 * 1024;

constexpr size_t kDefaultAttributeSize = 4 * sizeof(float);

// Metal limits
constexpr uint32_t kMaxShaderBuffers     = 31;
constexpr uint32_t kMaxShaderSamplers    = 16;
constexpr size_t kInlineConstDataMaxSize = 4 * 1024;
constexpr size_t kDefaultUniformsMaxSize = kInlineConstDataMaxSize;
constexpr uint32_t kMaxViewports         = 1;

constexpr uint32_t kVertexAttribBufferStrideAlignment = 4;
// Alignment requirement for offset passed to setVertex|FragmentBuffer
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
constexpr uint32_t kUniformBufferSettingOffsetMinAlignment = 256;
#else
constexpr uint32_t kUniformBufferSettingOffsetMinAlignment = 4;
#endif
constexpr uint32_t kIndexBufferOffsetAlignment       = 4;
constexpr uint32_t kArgumentBufferOffsetAlignment    = kUniformBufferSettingOffsetMinAlignment;
constexpr uint32_t kTextureToBufferBlittingAlignment = 256;

// Font end binding limits
constexpr uint32_t kMaxGLSamplerBindings = 2 * kMaxShaderSamplers;
constexpr uint32_t kMaxGLUBOBindings     = 2 * kMaxShaderUBOs;

// Binding index start for vertex data buffers:
constexpr uint32_t kVboBindingIndexStart = 0;

// Binding index for default attribute buffer:
constexpr uint32_t kDefaultAttribsBindingIndex = kVboBindingIndexStart + kMaxVertexAttribs;
// Binding index for driver uniforms:
constexpr uint32_t kDriverUniformsBindingIndex = kDefaultAttribsBindingIndex + 1;
// Binding index for default uniforms:
constexpr uint32_t kDefaultUniformsBindingIndex = kDefaultAttribsBindingIndex + 3;
// Binding index for Transform Feedback Buffers (4)
constexpr uint32_t kTransformFeedbackBindingIndex = kDefaultUniformsBindingIndex + 1;
// Binding index for shadow samplers' compare modes
constexpr uint32_t kShadowSamplerCompareModesBindingIndex = kTransformFeedbackBindingIndex + 4;
// Binding index for UBO's argument buffer
constexpr uint32_t kUBOArgumentBufferBindingIndex = kShadowSamplerCompareModesBindingIndex + 1;

constexpr uint32_t kStencilMaskAll = 0xff;  // Only 8 bit stencil is supported

static const char * kUnassignedAttributeString = " __unassigned_attribute__";


constexpr float kEmulatedAlphaValue = 1.0f;

constexpr uint32_t kOcclusionQueryResultSize = sizeof(uint64_t);



}  // namespace mtl
}  // namespace rx
#endif
