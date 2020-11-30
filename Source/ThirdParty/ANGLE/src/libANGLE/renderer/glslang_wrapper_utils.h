//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Wrapper for Khronos glslang compiler. This file is used by Vulkan and Metal backends.
//

#ifndef LIBANGLE_RENDERER_GLSLANG_WRAPPER_UTILS_H_
#define LIBANGLE_RENDERER_GLSLANG_WRAPPER_UTILS_H_

#include <functional>

#include "libANGLE/renderer/ProgramImpl.h"

namespace rx
{
enum class GlslangError
{
    InvalidShader,
    InvalidSpirv,
};

constexpr gl::ShaderMap<const char *> kDefaultUniformNames = {
    {gl::ShaderType::Vertex, sh::vk::kDefaultUniformsNameVS},
    {gl::ShaderType::Geometry, sh::vk::kDefaultUniformsNameGS},
    {gl::ShaderType::Fragment, sh::vk::kDefaultUniformsNameFS},
    {gl::ShaderType::Compute, sh::vk::kDefaultUniformsNameCS},
};

struct GlslangProgramInterfaceInfo
{
    // Uniforms set index:
    uint32_t uniformsAndXfbDescriptorSetIndex;
    uint32_t currentUniformBindingIndex;
    // Textures set index:
    uint32_t textureDescriptorSetIndex;
    uint32_t currentTextureBindingIndex;
    // Other shader resources set index:
    uint32_t shaderResourceDescriptorSetIndex;
    uint32_t currentShaderResourceBindingIndex;
    // ANGLE driver uniforms set index:
    uint32_t driverUniformsDescriptorSetIndex;

    uint32_t locationsUsedForXfbExtension;
};

struct GlslangSourceOptions
{
    bool useOldRewriteStructSamplers        = false;
    bool supportsTransformFeedbackExtension = false;
    bool emulateTransformFeedback           = false;
    bool emulateBresenhamLines              = false;
};

using SpirvBlob = std::vector<uint32_t>;

using GlslangErrorCallback = std::function<angle::Result(GlslangError)>;

// Information for each shader interface variable.  Not all fields are relevant to each shader
// interface variable.  For example opaque uniforms require a set and binding index, while vertex
// attributes require a location.
struct ShaderInterfaceVariableInfo
{
    ShaderInterfaceVariableInfo();

    static constexpr uint32_t kInvalid = std::numeric_limits<uint32_t>::max();

    // Used for interface blocks and opaque uniforms.
    uint32_t descriptorSet = kInvalid;
    uint32_t binding       = kInvalid;
    // Used for vertex attributes, fragment shader outputs and varyings.  There could be different
    // variables that share the same name, such as a vertex attribute and a fragment output.  They
    // will share this object since they have the same name, but will find possibly different
    // locations in their respective slots.
    uint32_t location  = kInvalid;
    uint32_t component = kInvalid;
    // The stages this shader interface variable is active.
    gl::ShaderBitSet activeStages;
    // Used for transform feedback extension to decorate vertex shader output.
    uint32_t xfbBuffer = kInvalid;
    uint32_t xfbOffset = kInvalid;
    uint32_t xfbStride = kInvalid;
};

// TODO: http://anglebug.com/4524: Need a different hash key than a string, since
// that's slow to calculate.
using ShaderInterfaceVariableInfoMap = std::unordered_map<std::string, ShaderInterfaceVariableInfo>;
using ShaderMapInterfaceVariableInfoMap = gl::ShaderMap<ShaderInterfaceVariableInfoMap>;

void GlslangInitialize();
void GlslangRelease();

bool GetImageNameWithoutIndices(std::string *name);

// Get the mapped sampler name after the soure is transformed by GlslangGetShaderSource()
std::string GetMappedSamplerNameOld(const std::string &originalName);
std::string GlslangGetMappedSamplerName(const std::string &originalName);
std::string GetXfbBufferName(const uint32_t bufferIndex);

void GlslangAssignLocations(GlslangSourceOptions &options,
                            const gl::ProgramExecutable &programExecutable,
                            const gl::ShaderType shaderType,
                            GlslangProgramInterfaceInfo *programInterfaceInfo,
                            gl::ShaderMap<ShaderInterfaceVariableInfoMap> *variableInfoMapOut);

// Transform the source to include actual binding points for various shader resources (textures,
// buffers, xfb, etc).  For some variables, these values are instead output to the variableInfoMap
// to be set during a SPIR-V transformation.  This is a transitory step towards moving all variables
// to this map, at which point GlslangGetShaderSpirvCode will also be called by this function.
void GlslangGetShaderSource(GlslangSourceOptions &options,
                            const gl::ProgramState &programState,
                            const gl::ProgramLinkedResources &resources,
                            GlslangProgramInterfaceInfo *programInterfaceInfo,
                            gl::ShaderMap<std::string> *shaderSourcesOut,
                            ShaderMapInterfaceVariableInfoMap *variableInfoMapOut);

angle::Result GlslangTransformSpirvCode(const GlslangErrorCallback &callback,
                                        const gl::ShaderType shaderType,
                                        bool removeEarlyFragmentTestsOptimization,
                                        bool removeDebugInfo,
                                        const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                        const SpirvBlob &initialSpirvBlob,
                                        SpirvBlob *spirvBlobOut);

angle::Result GlslangGetShaderSpirvCode(const GlslangErrorCallback &callback,
                                        const gl::ShaderBitSet &linkedShaderStages,
                                        const gl::Caps &glCaps,
                                        const gl::ShaderMap<std::string> &shaderSources,
                                        const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
                                        gl::ShaderMap<SpirvBlob> *spirvBlobsOut);

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GLSLANG_WRAPPER_UTILS_H_
