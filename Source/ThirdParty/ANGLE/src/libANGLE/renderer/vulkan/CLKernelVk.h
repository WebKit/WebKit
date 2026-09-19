//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLKernelVk.h: Defines the class interface for CLKernelVk, implementing CLKernelImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_

#include <memory>
#include <string>
#include "common/hash_containers.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLMemoryVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/renderer/CLKernelImpl.h"

#include "vulkan/vulkan_core.h"

namespace rx
{

class CLKernelArgument
{
  public:
    CLKernelArgument(const CLContextVk *context, const ClspvKernelArgument &info);
    ~CLKernelArgument() = default;

    bool isReadOnly() const;
    cl::Memory *getMemoryHandle() const;
    uint32_t getReflectionType() const { return mCompiledInfo.type; }
    uint32_t getDescriptorBinding() const { return mCompiledInfo.descriptorBinding; }
    uint32_t getPushConstantOffset() const { return mCompiledInfo.pushConstOffset; }
    uint32_t getPushConstantSize() const { return mCompiledInfo.pushConstantSize; }
    uint32_t getPodUniformOffset() const { return mCompiledInfo.podUniformOffset; }
    uint32_t getPodStorageBufferOffset() const { return mCompiledInfo.podStorageBufferOffset; }
    uint32_t getPodStorageBufferSize() const { return mCompiledInfo.podStorageBufferSize; }
    uint32_t getWorkGroupElementSize() const { return mCompiledInfo.workgroupBufferElemSize; }
    uint32_t getWorkGroupBufferSpecId() const { return mCompiledInfo.workgroupBufferSpecId; }
    bool getUsed() const { return mCompiledInfo.used; }
    cl::Sampler *getSamplerHandle() const;
    size_t getSize() const { return mSize; }
    const std::string &getTypeName() const { return mCompiledInfo.info.typeName; }
    const std::string &getVariableName() const { return mCompiledInfo.info.name; }
    int32_t getSpecConstantIndex() const { return mSpecConstantIndex; }
    cl_kernel_arg_type_qualifier getTypeQualifier() const
    {
        return mCompiledInfo.info.typeQualifier;
    }
    cl_kernel_arg_address_qualifier getAddressQualifier() const
    {
        return mCompiledInfo.info.addressQualifier;
    }
    cl_kernel_arg_access_qualifier getAccessQualifier() const
    {
        return mCompiledInfo.info.accessQualifier;
    }
    CLKernelImpl::ArgInfo getArgInfo() const { return mArgInfo; }

    void set(size_t size, void *handle)
    {
        mSize   = size;
        mHandle = handle;
    }
    void setSpecConstantIndex(int32_t index) { mSpecConstantIndex = index; }

  private:
    ClspvKernelArgument mCompiledInfo;
    CLKernelImpl::ArgInfo mArgInfo;
    size_t mSize;
    void *mHandle;

    // For kernel arguments of type ArgumentWorkgroup, cache the kernel's spec constant index
    int32_t mSpecConstantIndex = -1;
};
// Vector of arguments used by a kernel
using CLKernelArguments = std::vector<std::shared_ptr<CLKernelArgument>>;

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

    CLKernelVk(const cl::Kernel &kernel, std::string &name, std::string &attributes);
    ~CLKernelVk() override;

    angle::Result init();

    angle::Result setArg(cl_uint argIndex, size_t argSize, const void *argValue) override;

    angle::Result createInfo(CLKernelImpl::Info *infoOut) const override;

    angle::Result initPipelineLayout();

    CLProgramVk *getProgram() { return mProgram; }
    const std::string &getKernelName() const { return mName; }
    const CLKernelArguments &getArgs() const { return mArgs; }

    const vk::PipelineLayout &getPipelineLayout() const { return *mPipelineLayout; }
    vk::DescriptorSetLayoutPointerArray &getDescriptorSetLayouts() { return mDescriptorSetLayouts; }
    cl::Kernel &getFrontendObject() { return const_cast<cl::Kernel &>(mKernel); }

    angle::Result getOrCreateComputePipeline(vk::PipelineCacheAccess *pipelineCache,
                                             const cl::NDRange &ndrange,
                                             const cl::Device &device,
                                             vk::PipelineHelper **pipelineOut);

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

    std::vector<uint8_t> &getPodArgumentPushConstantsData() { return mPodArgumentPushConstants; }

    cl::BufferPtr getPodBuffer() { return mPodBuffer; }

    bool usesPrintf() const;
    bool usesPrintfBufferPointerPushConstant() const;

    angle::Result allocateDescriptorSet(
        DescriptorSetIndex index,
        angle::EnumIterator<DescriptorSetIndex> layoutIndex,
        vk::OutsideRenderPassCommandBufferHelper *computePassCommands);

    // Initialize the descriptor pools for this kernel resources
    angle::Result initializeDescriptorPools();

    cl_ulong getLocalMemSizeUsed(const cl::Device &device) const override;
    cl_ulong getAllArgLocalMemSize() const override;
    cl_ulong getCompiledLocalMemSize(const cl::Device &device) const override;

  private:
    CLProgramVk *mProgram;
    CLContextVk *mContext;
    std::string mName;
    std::string mAttributes;
    CLKernelArguments mArgs;

    std::vector<uint8_t> mPodArgumentPushConstants;
    cl::BufferPtr mPodBuffer;

    vk::ShaderProgramHelper mShaderProgramHelper;
    ComputePipelineCache mComputePipelineCache;

    // Pipeline and DescriptorSetLayout Shared pointers
    vk::PipelineLayoutPtr mPipelineLayout;
    vk::DescriptorSetLayoutPointerArray mDescriptorSetLayouts{};

    // DescriptorSet and DescriptorPool shared pointers for this kernel resources
    vk::DescriptorSetArray<vk::DescriptorSetPointer> mDescriptorSets;
    vk::DescriptorSetArray<vk::DynamicDescriptorPoolPointer> mDynamicDescriptorPools;

    vk::DescriptorSetArray<vk::DescriptorSetLayoutDesc> mDescriptorSetLayoutDescs;
    vk::PipelineLayoutDesc mPipelineLayoutDesc;

    std::vector<size_t> mLocalMemoryArgSizes;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLKERNELVK_H_
