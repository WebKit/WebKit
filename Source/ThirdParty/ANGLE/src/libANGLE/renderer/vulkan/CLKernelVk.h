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
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/renderer/CLKernelImpl.h"
#include "vulkan/vulkan_core.h"

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

    struct KernelSpecConstant
    {
        uint32_t ID;
        uint32_t data;
    };
    // Setting a reasonable initial value
    // https://registry.khronos.org/OpenCL/specs/3.0-unified/html/OpenCL_API.html#CL_DEVICE_MAX_PARAMETER_SIZE
    using KernelSpecConstants = angle::FastVector<KernelSpecConstant, 128>;

    CLKernelVk(const cl::Kernel &kernel,
               std::string &name,
               std::string &attributes,
               CLKernelArguments &args);
    ~CLKernelVk() override;

    angle::Result init();

    angle::Result setArg(cl_uint argIndex, size_t argSize, const void *argValue) override;

    angle::Result createInfo(CLKernelImpl::Info *infoOut) const override;

    CLProgramVk *getProgram() { return mProgram; }
    const std::string &getKernelName() { return mName; }
    const CLKernelArguments &getArgs() { return mArgs; }
    vk::AtomicBindingPointer<vk::PipelineLayout> &getPipelineLayout() { return mPipelineLayout; }
    vk::DescriptorSetLayoutPointerArray &getDescriptorSetLayouts() { return mDescriptorSetLayouts; }
    cl::Kernel &getFrontendObject() { return const_cast<cl::Kernel &>(mKernel); }

    angle::Result getOrCreateComputePipeline(vk::PipelineCacheAccess *pipelineCache,
                                             const cl::NDRange &ndrange,
                                             const cl::Device &device,
                                             vk::PipelineHelper **pipelineOut,
                                             cl::WorkgroupCount *workgroupCountOut);

    const vk::DescriptorSetLayoutDesc &getDescriptorSetLayoutDesc(DescriptorSetIndex index) const
    {
        return mDescriptorSetLayoutDescs[index];
    }
    const vk::DescriptorSetLayoutDesc &getKernelArgDescriptorSetDesc() const
    {
        return getDescriptorSetLayoutDesc(DescriptorSetIndex::KernelArguments);
    }
    const vk::DescriptorSetLayoutDesc &getLiteralSamplerDescriptorSetDesc() const
    {
        return getDescriptorSetLayoutDesc(DescriptorSetIndex::LiteralSampler);
    }
    const vk::DescriptorSetLayoutDesc &getPrintfDescriptorSetDesc() const
    {
        return getDescriptorSetLayoutDesc(DescriptorSetIndex::Printf);
    }

    const vk::PipelineLayoutDesc &getPipelineLayoutDesc() { return mPipelineLayoutDesc; }

    VkDescriptorSet getDescriptorSet(DescriptorSetIndex index)
    {
        return mDescriptorSets[index]->getDescriptorSet();
    }

    std::vector<uint8_t> &getPodArgumentsData() { return mPodArgumentsData; }

    bool usesPrintf() const;

    angle::Result allocateDescriptorSet(
        DescriptorSetIndex index,
        angle::EnumIterator<DescriptorSetIndex> layoutIndex,
        vk::OutsideRenderPassCommandBufferHelper *computePassCommands);

  private:
    static constexpr std::array<size_t, 3> kEmptyWorkgroupSize = {0, 0, 0};

    CLProgramVk *mProgram;
    CLContextVk *mContext;
    std::string mName;
    std::string mAttributes;
    CLKernelArguments mArgs;

    // Copy of the pod data
    std::vector<uint8_t> mPodArgumentsData;

    vk::ShaderProgramHelper mShaderProgramHelper;
    vk::ComputePipelineCache mComputePipelineCache;
    KernelSpecConstants mSpecConstants;
    vk::AtomicBindingPointer<vk::PipelineLayout> mPipelineLayout;
    vk::DescriptorSetLayoutPointerArray mDescriptorSetLayouts{};

    vk::DescriptorSetArray<vk::DescriptorSetPointer> mDescriptorSets;

    vk::DescriptorSetArray<vk::DescriptorSetLayoutDesc> mDescriptorSetLayoutDescs;
    vk::PipelineLayoutDesc mPipelineLayoutDesc;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_
