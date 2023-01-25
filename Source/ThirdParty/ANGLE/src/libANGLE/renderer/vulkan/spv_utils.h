//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities to map shader interface variables to Vulkan mappings, and transform the SPIR-V
// accordingly.
//

#ifndef LIBANGLE_RENDERER_VULKAN_SPV_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_SPV_UTILS_H_

#include <functional>

#include "common/spirv/spirv_types.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "platform/FeaturesVk_autogen.h"

namespace rx
{
class ShaderInterfaceVariableInfoMap;
constexpr gl::ShaderMap<const char *> kDefaultUniformNames = {
    {gl::ShaderType::Vertex, sh::vk::kDefaultUniformsNameVS},
    {gl::ShaderType::TessControl, sh::vk::kDefaultUniformsNameTCS},
    {gl::ShaderType::TessEvaluation, sh::vk::kDefaultUniformsNameTES},
    {gl::ShaderType::Geometry, sh::vk::kDefaultUniformsNameGS},
    {gl::ShaderType::Fragment, sh::vk::kDefaultUniformsNameFS},
    {gl::ShaderType::Compute, sh::vk::kDefaultUniformsNameCS},
};

struct SpvProgramInterfaceInfo
{
    uint32_t currentUniformBindingIndex        = 0;
    uint32_t currentTextureBindingIndex        = 0;
    uint32_t currentShaderResourceBindingIndex = 0;

    uint32_t locationsUsedForXfbExtension = 0;
};

struct SpvSourceOptions
{
    bool supportsTransformFeedbackExtension = false;
    bool supportsTransformFeedbackEmulation = false;
    bool enableTransformFeedbackEmulation   = false;
};

SpvSourceOptions SpvCreateSourceOptions(const angle::FeaturesVk &features);

struct SpvTransformOptions
{
    gl::ShaderType shaderType           = gl::ShaderType::InvalidEnum;
    bool negativeViewportSupported      = false;
    bool removeDebugInfo                = false;
    bool isLastPreFragmentStage         = false;
    bool isTransformFeedbackStage       = false;
    bool isTransformFeedbackEmulated    = false;
    bool isMultisampledFramebufferFetch = false;
    bool validate                       = true;
    bool useSpirvVaryingPrecisionFixer  = false;
};

struct UniformBindingInfo final
{
    UniformBindingInfo();
    UniformBindingInfo(uint32_t bindingIndex,
                       gl::ShaderBitSet shaderBitSet,
                       gl::ShaderType frontShaderType);
    uint32_t bindingIndex          = 0;
    gl::ShaderBitSet shaderBitSet  = gl::ShaderBitSet();
    gl::ShaderType frontShaderType = gl::ShaderType::InvalidEnum;
};

using UniformBindingIndexMap = angle::HashMap<std::string, UniformBindingInfo>;

struct ShaderInterfaceVariableXfbInfo
{
    static constexpr uint32_t kInvalid = std::numeric_limits<uint32_t>::max();

    // Used by both extension and emulation
    uint32_t buffer = kInvalid;
    uint32_t offset = kInvalid;
    uint32_t stride = kInvalid;

    // Used only by emulation (array index support is missing from VK_EXT_transform_feedback)
    uint32_t arraySize   = kInvalid;
    uint32_t columnCount = kInvalid;
    uint32_t rowCount    = kInvalid;
    uint32_t arrayIndex  = kInvalid;
    GLenum componentType = GL_FLOAT;
    // If empty, the whole array is captured.  Otherwise only the specified members are captured.
    std::vector<ShaderInterfaceVariableXfbInfo> arrayElements;
};

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
    uint32_t index     = kInvalid;
    // The stages this shader interface variable is active.
    gl::ShaderBitSet activeStages;
    // Used for transform feedback extension to decorate vertex shader output.
    ShaderInterfaceVariableXfbInfo xfb;
    std::vector<ShaderInterfaceVariableXfbInfo> fieldXfb;
    // Indicates that the precision needs to be modified in the generated SPIR-V
    // to support only transferring medium precision data when there's a precision
    // mismatch between the shaders. For example, either the VS casts highp->mediump
    // or the FS casts mediump->highp.
    bool useRelaxedPrecision = false;
    // Indicate if varying is input or output, or both (in case of for example gl_Position in a
    // geometry shader)
    bool varyingIsInput  = false;
    bool varyingIsOutput = false;
    // For vertex attributes, this is the number of components / locations.  These are used by the
    // vertex attribute aliasing transformation only.
    uint8_t attributeComponentCount = 0;
    uint8_t attributeLocationCount  = 0;
    // Indicate if this variable has been deduplicated.
    bool isDuplicate = false;
};

bool GetImageNameWithoutIndices(std::string *name);

// Get the mapped sampler name.
std::string SpvGetMappedSamplerName(const std::string &originalName);
std::string SpvGetXfbBufferName(const uint32_t bufferIndex);

void SpvAssignLocations(const SpvSourceOptions &options,
                        const gl::ProgramExecutable &programExecutable,
                        const gl::ProgramVaryingPacking &varyingPacking,
                        const gl::ShaderType shaderType,
                        const gl::ShaderType frontShaderType,
                        bool isTransformFeedbackStage,
                        SpvProgramInterfaceInfo *programInterfaceInfo,
                        UniformBindingIndexMap *uniformBindingIndexMapOut,
                        ShaderInterfaceVariableInfoMap *variableInfoMapOut);

void SpvAssignTransformFeedbackLocations(gl::ShaderType shaderType,
                                         const gl::ProgramExecutable &programExecutable,
                                         bool isTransformFeedbackStage,
                                         SpvProgramInterfaceInfo *programInterfaceInfo,
                                         ShaderInterfaceVariableInfoMap *variableInfoMapOut);

// Retrieves the compiled SPIR-V code for each shader stage, and calls |SpvAssignLocations|.
void SpvGetShaderSpirvCode(const gl::Context *context,
                           const SpvSourceOptions &options,
                           const gl::ProgramState &programState,
                           const gl::ProgramLinkedResources &resources,
                           SpvProgramInterfaceInfo *programInterfaceInfo,
                           gl::ShaderMap<const angle::spirv::Blob *> *spirvBlobsOut,
                           ShaderInterfaceVariableInfoMap *variableInfoMapOut);

angle::Result SpvTransformSpirvCode(const SpvTransformOptions &options,
                                    const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                    const angle::spirv::Blob &initialSpirvBlob,
                                    angle::spirv::Blob *spirvBlobOut);

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SPV_UTILS_H_
