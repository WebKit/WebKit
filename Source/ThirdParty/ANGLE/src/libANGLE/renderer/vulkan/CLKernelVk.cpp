//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLKernelVk.cpp: Implements the class methods for CLKernelVk.

#include "libANGLE/renderer/vulkan/CLKernelVk.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"
#include "libANGLE/renderer/vulkan/CLProgramVk.h"

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

angle::Result CLKernelVk::setArg(cl_uint argIndex, size_t argSize, const void *argValue)
{
    auto &arg = mArgs.at(argIndex);
    if (arg.used)
    {
        arg.handle     = const_cast<void *>(argValue);
        arg.handleSize = argSize;
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
    uint32_t constantDataOffset = 0;
    angle::FixedVector<size_t, 3> specConstantData;
    angle::FixedVector<VkSpecializationMapEntry, 3> mapEntries;
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

        // If at least one of the kernels does not use the reqd_work_group_size attribute, the
        // Vulkan SPIR-V produced by the compiler will contain specialization constants
        const std::array<uint32_t, 3> &specConstantWorkgroupSizeIDs =
            devProgramData->reflectionData.specConstantWorkgroupSizeIDs;
        ASSERT(ndrange.workDimensions <= 3);
        for (cl_uint i = 0; i < ndrange.workDimensions; ++i)
        {
            mapEntries.push_back(
                VkSpecializationMapEntry{.constantID = specConstantWorkgroupSizeIDs.at(i),
                                         .offset     = constantDataOffset,
                                         .size       = sizeof(uint32_t)});
            constantDataOffset += sizeof(uint32_t);
            specConstantData.push_back(workgroupSize[i]);
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

    VkSpecializationInfo computeSpecializationInfo{
        .mapEntryCount = static_cast<uint32_t>(mapEntries.size()),
        .pMapEntries   = mapEntries.data(),
        .dataSize      = specConstantData.size() * sizeof(specConstantData[0]),
        .pData         = specConstantData.data(),
    };

    // Now get or create (on compute pipeline cache miss) compute pipeline and return it
    return mShaderProgramHelper.getOrCreateComputePipeline(
        mContext, &mComputePipelineCache, pipelineCache, getPipelineLayout().get(),
        vk::ComputePipelineOptions{}, PipelineSource::Draw, pipelineOut, mName.c_str(),
        &computeSpecializationInfo);
}

}  // namespace rx
