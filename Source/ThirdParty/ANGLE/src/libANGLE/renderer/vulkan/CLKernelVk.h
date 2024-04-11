//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLKernelVk.h: Defines the class interface for CLKernelVk, implementing CLKernelImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_

#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/renderer/CLKernelImpl.h"

namespace rx
{

struct CLKernelArgument
{
    CLKernelImpl::ArgInfo info{};
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
        uint32_t workgroupSpecId;
    };
    union
    {
        uint32_t op4;
        uint32_t descriptorBinding;
        uint32_t pushConstantSize;
        uint32_t workgroupSize;
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
using CLKernelArguments = std::vector<CLKernelArgument>;
using CLKernelArgsMap   = angle::HashMap<std::string, CLKernelArguments>;

class CLKernelVk : public CLKernelImpl
{
  public:
    using Ptr = std::unique_ptr<CLKernelVk>;
    CLKernelVk(const cl::Kernel &kernel,
               std::string &name,
               std::string &attributes,
               CLKernelArguments &args);
    ~CLKernelVk() override;

    angle::Result setArg(cl_uint argIndex, size_t argSize, const void *argValue) override;

    angle::Result createInfo(CLKernelImpl::Info *infoOut) const override;

    const CLProgramVk *getProgram() { return mProgram; }
    const std::string &getKernelName() { return mName; }
    const CLKernelArguments &getArgs() { return mArgs; }
    VkDescriptorSet &getDescriptorSet() { return mDescriptorSet; }
    vk::AtomicBindingPointer<vk::PipelineLayout> &getPipelineLayout() { return mPipelineLayout; }
    vk::DescriptorSetLayoutPointerArray &getDescriptorSetLayouts() { return mDescriptorSetLayouts; }

  private:
    CLProgramVk *mProgram;
    CLContextVk *mContext;
    std::string mName;
    std::string mAttributes;
    CLKernelArguments mArgs;
    VkDescriptorSet mDescriptorSet{VK_NULL_HANDLE};
    vk::AtomicBindingPointer<vk::PipelineLayout> mPipelineLayout;
    vk::DescriptorSetLayoutPointerArray mDescriptorSetLayouts{};
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_
