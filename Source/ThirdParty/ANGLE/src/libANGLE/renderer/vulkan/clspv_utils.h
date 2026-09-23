//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
//
// clspv_utils:
//     Utilities to map clspv interface variables to OpenCL and Vulkan mappings.
//

#ifndef LIBANGLE_RENDERER_VULKAN_CLSPV_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_CLSPV_UTILS_H_

#include <libANGLE/renderer/vulkan/CLDeviceVk.h>

#include "clspv/Compiler.h"
#include "clspv/Sampler.h"

#include "spirv-tools/libspirv.h"

#include <vulkan/vulkan_core.h>

#include <string>
#include <vector>

namespace rx
{
struct ClspvPrintfBufferStorage
{
    uint32_t descriptorSet = 0;
    uint32_t binding       = 0;
    uint32_t pcOffset      = 0;
    uint32_t size          = 0;
};

struct ClspvPrintfInfo
{
    uint32_t id = 0;
    std::string formatSpecifier;
    std::vector<uint32_t> argSizes;
};

struct ClspvLiteralSampler
{
    uint32_t descriptorSet;
    uint32_t binding;
    cl_bool normalizedCoords;
    cl::AddressingMode addressingMode;
    cl::FilterMode filterMode;
};

struct ClspvConstantDataBufferInfo
{
    uint32_t set;
    uint32_t binding;
    uint32_t pcOffset;
    std::vector<uint8_t> bufferData;
};

struct ClspvWorkgroupVariableSize
{
    uint32_t size = 0;
};

// TODO: Look into moving this information in CLKernelArgument
// https://anglebug.com/378514267
struct ClspvImagePushConstant
{
    VkPushConstantRange pcRange;
    uint32_t ordinal;
};

// Information as encoded by the clspv ArgumentInfo reflection instruction
// https://github.khronos.org/SPIRV-Registry/nonsemantic/NonSemantic.ClspvReflection.html#ArgumentInfo
struct ClspvArgumentInfo
{
    std::string name;
    std::string typeName;

    cl_kernel_arg_address_qualifier addressQualifier = 0u;
    cl_kernel_arg_access_qualifier accessQualifier   = 0u;
    cl_kernel_arg_type_qualifier typeQualifier       = 0u;
};

// This captures all in information that is available from the clspv compiled binary related to a
// kernel argument
// - ClspvArgumentInfo
// - Reflection type
// - Descriptor set/push constant offset
// - Descriptor binding/push constant size
// - pod buffer offset
// - pod buffer size
struct ClspvKernelArgument
{
    ClspvArgumentInfo info{};

    uint32_t type     = 0;
    uint32_t ordinal  = 0;
    size_t handleSize = 0;
    void *handle      = nullptr;
    bool used         = false;

    // Shared operand words/regions for "OpExtInst" type spv instructions
    // (starts from spv word index/offset 7 and onward)
    // https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpExtInst
    // https://github.com/google/clspv/blob/main/docs/OpenCLCOnVulkan.md#kernels
    union
    {
        uint32_t op3;
        uint32_t descriptorSet;
        uint32_t pushConstOffset;
        uint32_t workgroupBufferSpecId;
    };
    union
    {
        uint32_t op4;
        uint32_t descriptorBinding;
        uint32_t pushConstantSize;
        uint32_t workgroupBufferElemSize;
    };
    union
    {
        uint32_t op5;
        uint32_t podStorageBufferOffset;
        uint32_t podUniformOffset;
        uint32_t pointerUniformOffset;
    };
    union
    {
        uint32_t op6;
        uint32_t podStorageBufferSize;
        uint32_t podUniformSize;
        uint32_t pointerUniformSize;
    };
};
using ClspvKernelArguments = std::vector<ClspvKernelArgument>;
using ClspvKernelArgsMap   = angle::HashMap<std::string, ClspvKernelArguments>;

struct ClspvReflectionData
{
    angle::HashMap<uint32_t, uint32_t> spvIntLookup;
    angle::HashMap<uint32_t, std::string> spvStrLookup;

    // The ArgumentInfo is setup as a separate reflection instruction
    // %a = OpExtInst %b %c ArgumentInfo %d [ %e %f %g %h ]
    // The instruction itself doesn't encode the variable information
    // instead this is attached as a tag to the appropriate resource
    // Here we store the map of <a, ClspvArgumentInfo> for later processing
    angle::HashMap<uint32_t, ClspvArgumentInfo> kernelArgInfos;
    // A map of kernel name to kernel arguments
    ClspvKernelArgsMap kernelArgsMap;
    // Temporary store for any out of order processing on the kernel arguments
    // map of kernel name to single kernel argument
    angle::HashMap<std::string, ClspvKernelArgument> kernelArgMap;

    angle::HashMap<std::string, uint32_t> kernelFlags;
    angle::HashMap<std::string, std::string> kernelAttributes;
    angle::HashMap<std::string, std::array<uint32_t, 3>> kernelCompileWorkgroupSize;
    angle::HashMap<uint32_t, VkPushConstantRange> pushConstants;
    angle::PackedEnumMap<SpecConstantType, uint32_t> specConstantIDs;
    angle::PackedEnumBitSet<SpecConstantType, uint32_t> specConstantsUsed;
    angle::HashMap<uint32_t, std::vector<ClspvImagePushConstant>> imagePushConstants;
    angle::HashSet<uint32_t> kernelIDs;

    // Printf buffer related info
    ClspvPrintfBufferStorage printfBufferStorage;
    angle::HashMap<uint32_t, ClspvPrintfInfo> printfInfoMap;

    std::vector<ClspvLiteralSampler> literalSamplers;
    ClspvConstantDataBufferInfo constantDataBufferInfo;
    ClspvWorkgroupVariableSize workgroupVariableSize;
};

namespace clspv_cl
{

cl::AddressingMode GetAddressingMode(uint32_t mask);

cl::FilterMode GetFilterMode(uint32_t mask);

inline bool IsNormalizedCoords(uint32_t mask)
{
    return (mask & clspv::kSamplerNormalizedCoordsMask) == clspv::CLK_NORMALIZED_COORDS_TRUE;
}

}  // namespace clspv_cl

angle::Result ClspvProcessPrintfBuffer(unsigned char *buffer,
                                       const size_t bufferSize,
                                       const angle::HashMap<uint32_t, ClspvPrintfInfo> *infoMap);

// Populate a list of options that can be supported by clspv based on the features supported by the
// vulkan renderer.
std::string ClspvGetCompilerOptions(const CLDeviceVk *device);

ClspvError ClspvCompileSource(const size_t programCount,
                              const size_t *programSizes,
                              const char **programs,
                              const char *options,
                              char **outputBinary,
                              size_t *outputBinarySize,
                              char **outputLog);

spv_target_env ClspvGetSpirvVersion(const vk::Renderer *renderer);

bool ClspvValidate(vk::Renderer *rendererVk, const angle::spirv::Blob &blob);

bool ClspvParseReflection(vk::Renderer *rendererVk,
                          const angle::spirv::Blob &blob,
                          ClspvReflectionData &reflectionDataOut);

bool ClspvStripReflection(vk::Renderer *rendererVk,
                          const angle::spirv::Blob &inBlob,
                          angle::spirv::Blob &outBlob);

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLSPV_UTILS_H_
