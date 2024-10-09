//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLKernelVk.cpp: Implements the class methods for CLKernelVk.

#include "common/PackedEnums.h"

#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"
#include "libANGLE/renderer/vulkan/CLKernelVk.h"
#include "libANGLE/renderer/vulkan/CLProgramVk.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

#include "libANGLE/CLContext.h"
#include "libANGLE/CLKernel.h"
#include "libANGLE/CLProgram.h"
#include "libANGLE/cl_utils.h"

namespace rx
{

CLKernelVk::CLKernelVk(const cl::Kernel &kernel,
                       std::string &name,
                       std::string &attributes,
                       CLKernelArguments &args)
    : CLKernelImpl(kernel),
      mProgram(&kernel.getProgram().getImpl<CLProgramVk>()),
      mContext(&kernel.getProgram().getContext().getImpl<CLContextVk>()),
      mName(name),
      mAttributes(attributes),
      mArgs(args)
{
    mShaderProgramHelper.setShader(gl::ShaderType::Compute,
                                   mKernel.getProgram().getImpl<CLProgramVk>().getShaderModule());
    for (DescriptorSetIndex index : angle::AllEnums<DescriptorSetIndex>())
    {
        mDescriptorSets[index] = VK_NULL_HANDLE;
    }
}

CLKernelVk::~CLKernelVk()
{
    for (auto &dsLayouts : mDescriptorSetLayouts)
    {
        dsLayouts.reset();
    }

    mPipelineLayout.reset();
    for (auto &pipelineHelper : mComputePipelineCache)
    {
        pipelineHelper.destroy(mContext->getDevice());
    }
    mShaderProgramHelper.destroy(mContext->getRenderer());
}

angle::Result CLKernelVk::init()
{
    vk::DescriptorSetLayoutDesc &descriptorSetLayoutDesc =
        mDescriptorSetLayoutDescs[DescriptorSetIndex::KernelArguments];
    VkPushConstantRange pcRange = mProgram->getDeviceProgramData(mName.c_str())->pushConstRange;
    for (const auto &arg : getArgs())
    {
        VkDescriptorType descType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        switch (arg.type)
        {
            case NonSemanticClspvReflectionArgumentStorageBuffer:
            case NonSemanticClspvReflectionArgumentPodStorageBuffer:
                descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;
            case NonSemanticClspvReflectionArgumentUniform:
            case NonSemanticClspvReflectionArgumentPodUniform:
            case NonSemanticClspvReflectionArgumentPointerUniform:
                descType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;
            case NonSemanticClspvReflectionArgumentPodPushConstant:
                // Get existing push constant range and see if we need to update
                if (arg.pushConstOffset + arg.pushConstantSize > pcRange.offset + pcRange.size)
                {
                    pcRange.size = arg.pushConstOffset + arg.pushConstantSize - pcRange.offset;
                }
                continue;
            default:
                continue;
        }
        descriptorSetLayoutDesc.addBinding(arg.descriptorBinding, descType, 1,
                                           VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
    }

    if (usesPrintf())
    {
        mDescriptorSetLayoutDescs[DescriptorSetIndex::Printf].addBinding(
            mProgram->getDeviceProgramData(mName.c_str())
                ->reflectionData.printfBufferStorage.binding,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
    }

    // Get pipeline layout from cache (creates if missed)
    // A given kernel need not have resulted in use of all the descriptor sets. Unless the
    // graphicsPipelineLibrary extension is supported, the pipeline layout need all the descriptor
    // set layouts to be valide. So set them up in the order of their occurrence.
    mPipelineLayoutDesc = {};
    for (DescriptorSetIndex index : angle::AllEnums<DescriptorSetIndex>())
    {
        if (!mDescriptorSetLayoutDescs[index].empty())
        {
            mPipelineLayoutDesc.updateDescriptorSetLayout(index, mDescriptorSetLayoutDescs[index]);
        }
    }

    // push constant setup
    // push constant size must be multiple of 4
    pcRange.size = roundUpPow2(pcRange.size, 4u);
    // push constant offset must be multiple of 4, round down to ensure this
    pcRange.offset = roundDownPow2(pcRange.offset, 4u);
    mPipelineLayoutDesc.updatePushConstantRange(pcRange.stageFlags, pcRange.offset, pcRange.size);

    return angle::Result::Continue;
}

angle::Result CLKernelVk::setArg(cl_uint argIndex, size_t argSize, const void *argValue)
{
    auto &arg = mArgs.at(argIndex);
    if (arg.used)
    {
        arg.handle     = const_cast<void *>(argValue);
        arg.handleSize = argSize;

        if (arg.type == NonSemanticClspvReflectionArgumentWorkgroup)
        {
            mSpecConstants.push_back(
                KernelSpecConstant{.ID   = arg.workgroupSpecId,
                                   .data = static_cast<uint32_t>(argSize / arg.workgroupSize)});
        }
    }

    return angle::Result::Continue;
}

angle::Result CLKernelVk::createInfo(CLKernelImpl::Info *info) const
{
    info->functionName = mName;
    info->attributes   = mAttributes;
    info->numArgs      = static_cast<cl_uint>(mArgs.size());
    for (const auto &arg : mArgs)
    {
        ArgInfo argInfo;
        argInfo.name             = arg.info.name;
        argInfo.typeName         = arg.info.typeName;
        argInfo.accessQualifier  = arg.info.accessQualifier;
        argInfo.addressQualifier = arg.info.addressQualifier;
        argInfo.typeQualifier    = arg.info.typeQualifier;
        info->args.push_back(std::move(argInfo));
    }

    auto &ctx = mKernel.getProgram().getContext();
    info->workGroups.resize(ctx.getDevices().size());
    const CLProgramVk::DeviceProgramData *deviceProgramData = nullptr;
    for (auto i = 0u; i < ctx.getDevices().size(); ++i)
    {
        auto &workGroup     = info->workGroups[i];
        const auto deviceVk = &ctx.getDevices()[i]->getImpl<CLDeviceVk>();
        deviceProgramData   = mProgram->getDeviceProgramData(ctx.getDevices()[i]->getNative());
        if (deviceProgramData == nullptr)
        {
            continue;
        }

        // TODO: http://anglebug.com/42267005
        ANGLE_TRY(
            deviceVk->getInfoSizeT(cl::DeviceInfo::MaxWorkGroupSize, &workGroup.workGroupSize));

        // TODO: http://anglebug.com/42267004
        workGroup.privateMemSize = 0;
        workGroup.localMemSize   = 0;

        workGroup.prefWorkGroupSizeMultiple = 16u;
        workGroup.globalWorkSize            = {0, 0, 0};
        if (deviceProgramData->reflectionData.kernelCompileWorkgroupSize.contains(mName))
        {
            workGroup.compileWorkGroupSize = {
                deviceProgramData->reflectionData.kernelCompileWorkgroupSize.at(mName)[0],
                deviceProgramData->reflectionData.kernelCompileWorkgroupSize.at(mName)[1],
                deviceProgramData->reflectionData.kernelCompileWorkgroupSize.at(mName)[2]};
        }
        else
        {
            workGroup.compileWorkGroupSize = {0, 0, 0};
        }
    }

    return angle::Result::Continue;
}

angle::Result CLKernelVk::getOrCreateComputePipeline(vk::PipelineCacheAccess *pipelineCache,
                                                     const cl::NDRange &ndrange,
                                                     const cl::Device &device,
                                                     vk::PipelineHelper **pipelineOut,
                                                     cl::WorkgroupCount *workgroupCountOut)
{
    const CLProgramVk::DeviceProgramData *devProgramData =
        getProgram()->getDeviceProgramData(device.getNative());
    ASSERT(devProgramData != nullptr);

    // Start with Workgroup size (WGS) from kernel attribute (if available)
    cl::WorkgroupSize workgroupSize = devProgramData->getCompiledWorkgroupSize(getKernelName());

    if (workgroupSize == kEmptyWorkgroupSize)
    {
        if (ndrange.nullLocalWorkSize)
        {
            // NULL value was passed, in which case the OpenCL implementation will determine
            // how to be break the global work-items into appropriate work-group instances.
            workgroupSize = device.getImpl<CLDeviceVk>().selectWorkGroupSize(ndrange);
        }
        else
        {
            // Local work size (LWS) was valid, use that as WGS
            workgroupSize = ndrange.localWorkSize;
        }
    }

    // Calculate the workgroup count
    // TODO: Add support for non-uniform WGS
    // http://angleproject:8631
    ASSERT(workgroupSize[0] != 0);
    ASSERT(workgroupSize[1] != 0);
    ASSERT(workgroupSize[2] != 0);
    (*workgroupCountOut)[0] = static_cast<uint32_t>((ndrange.globalWorkSize[0] / workgroupSize[0]));
    (*workgroupCountOut)[1] = static_cast<uint32_t>((ndrange.globalWorkSize[1] / workgroupSize[1]));
    (*workgroupCountOut)[2] = static_cast<uint32_t>((ndrange.globalWorkSize[2] / workgroupSize[2]));

    // Populate program specialization constants (if any)
    uint32_t constantDataOffset = 0;
    std::vector<uint32_t> specConstantData;
    std::vector<VkSpecializationMapEntry> mapEntries;
    for (const auto specConstantUsed : devProgramData->reflectionData.specConstantsUsed)
    {
        switch (specConstantUsed)
        {
            case SpecConstantType::WorkDimension:
                specConstantData.push_back(ndrange.workDimensions);
                break;
            case SpecConstantType::WorkgroupSizeX:
                specConstantData.push_back(static_cast<uint32_t>(workgroupSize[0]));
                break;
            case SpecConstantType::WorkgroupSizeY:
                specConstantData.push_back(static_cast<uint32_t>(workgroupSize[1]));
                break;
            case SpecConstantType::WorkgroupSizeZ:
                specConstantData.push_back(static_cast<uint32_t>(workgroupSize[2]));
                break;
            case SpecConstantType::GlobalOffsetX:
                specConstantData.push_back(static_cast<uint32_t>(ndrange.globalWorkOffset[0]));
                break;
            case SpecConstantType::GlobalOffsetY:
                specConstantData.push_back(static_cast<uint32_t>(ndrange.globalWorkOffset[1]));
                break;
            case SpecConstantType::GlobalOffsetZ:
                specConstantData.push_back(static_cast<uint32_t>(ndrange.globalWorkOffset[2]));
                break;
            default:
                UNIMPLEMENTED();
                continue;
        }
        mapEntries.push_back(VkSpecializationMapEntry{
            .constantID = devProgramData->reflectionData.specConstantIDs[specConstantUsed],
            .offset     = constantDataOffset,
            .size       = sizeof(uint32_t)});
        constantDataOffset += sizeof(uint32_t);
    }
    // Populate kernel specialization constants (if any)
    for (const auto &specConstant : mSpecConstants)
    {
        specConstantData.push_back(specConstant.data);
        mapEntries.push_back(VkSpecializationMapEntry{
            .constantID = specConstant.ID, .offset = constantDataOffset, .size = sizeof(uint32_t)});
        constantDataOffset += sizeof(uint32_t);
    }
    VkSpecializationInfo computeSpecializationInfo{
        .mapEntryCount = static_cast<uint32_t>(mapEntries.size()),
        .pMapEntries   = mapEntries.data(),
        .dataSize      = specConstantData.size() * sizeof(uint32_t),
        .pData         = specConstantData.data(),
    };

    // Now get or create (on compute pipeline cache miss) compute pipeline and return it
    return mShaderProgramHelper.getOrCreateComputePipeline(
        mContext, &mComputePipelineCache, pipelineCache, getPipelineLayout().get(),
        vk::ComputePipelineOptions{}, PipelineSource::Draw, pipelineOut, mName.c_str(),
        &computeSpecializationInfo);
}

bool CLKernelVk::usesPrintf() const
{
    return mProgram->getDeviceProgramData(mName.c_str())->getKernelFlags(mName) &
           NonSemanticClspvReflectionMayUsePrintf;
}

}  // namespace rx
