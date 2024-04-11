//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLDeviceVk.cpp: Implements the class methods for CLDeviceVk.

#include "libANGLE/renderer/vulkan/CLDeviceVk.h"
#include "libANGLE/renderer/vulkan/CLPlatformVk.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

#include "libANGLE/Display.h"
#include "libANGLE/cl_utils.h"

namespace rx
{

CLDeviceVk::CLDeviceVk(const cl::Device &device, vk::Renderer *renderer)
    : CLDeviceImpl(device), mRenderer(renderer)
{
    const VkPhysicalDeviceProperties &props = mRenderer->getPhysicalDeviceProperties();

    // Setup initial device mInfo fields
    // TODO(aannestrand) Create cl::Caps and use for device creation
    // http://anglebug.com/8529
    mInfoString = {
        {cl::DeviceInfo::Name, std::string(props.deviceName)},
        {cl::DeviceInfo::Vendor, mRenderer->getVendorString()},
        {cl::DeviceInfo::DriverVersion, mRenderer->getVersionString(true)},
        {cl::DeviceInfo::Version, std::string("OpenCL 3.0 " + mRenderer->getVersionString(true))},
        {cl::DeviceInfo::Profile, std::string("FULL_PROFILE")},
        {cl::DeviceInfo::OpenCL_C_Version, std::string("OpenCL C 3.0 ")},
        {cl::DeviceInfo::LatestConformanceVersionPassed, std::string("FIXME")}};
    mInfoSizeT = {
        {cl::DeviceInfo::MaxWorkGroupSize, props.limits.maxComputeWorkGroupInvocations},
        {cl::DeviceInfo::MaxGlobalVariableSize, 0},
        {cl::DeviceInfo::GlobalVariablePreferredTotalSize, 0},

        // TODO(aannestrand) Update these hardcoded platform/device queries
        // http://anglebug.com/8511
        {cl::DeviceInfo::MaxParameterSize, 1024},
        {cl::DeviceInfo::ProfilingTimerResolution, 1},
        {cl::DeviceInfo::PrintfBufferSize, 1024 * 1024},
        {cl::DeviceInfo::PreferredWorkGroupSizeMultiple, 16},
    };
    mInfoULong = {
        {cl::DeviceInfo::LocalMemSize, props.limits.maxComputeSharedMemorySize},
        {cl::DeviceInfo::SVM_Capabilities, 0},
        {cl::DeviceInfo::QueueOnDeviceProperties, 0},
        {cl::DeviceInfo::PartitionAffinityDomain, 0},
        {cl::DeviceInfo::DeviceEnqueueCapabilities, 0},
        {cl::DeviceInfo::QueueOnHostProperties, CL_QUEUE_PROFILING_ENABLE},

        // TODO(aannestrand) Update these hardcoded platform/device queries
        // http://anglebug.com/8511
        {cl::DeviceInfo::HalfFpConfig, 0},
        {cl::DeviceInfo::DoubleFpConfig, 0},
        {cl::DeviceInfo::GlobalMemCacheSize, 0},
        {cl::DeviceInfo::GlobalMemSize, 1024 * 1024 * 1024},
        {cl::DeviceInfo::MaxConstantBufferSize, 64 * 1024},
        {cl::DeviceInfo::SingleFpConfig, CL_FP_ROUND_TO_NEAREST | CL_FP_INF_NAN | CL_FP_FMA},
        {cl::DeviceInfo::AtomicMemoryCapabilities,
         CL_DEVICE_ATOMIC_ORDER_RELAXED | CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP},
        {cl::DeviceInfo::AtomicFenceCapabilities, CL_DEVICE_ATOMIC_ORDER_RELAXED |
                                                      CL_DEVICE_ATOMIC_SCOPE_WORK_ITEM |
                                                      CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP},
    };
    mInfoUInt = {
        {cl::DeviceInfo::VendorID, props.vendorID},
        {cl::DeviceInfo::MaxReadImageArgs, props.limits.maxPerStageDescriptorSampledImages},
        {cl::DeviceInfo::MaxWriteImageArgs, props.limits.maxPerStageDescriptorStorageImages},
        {cl::DeviceInfo::MaxReadWriteImageArgs, props.limits.maxPerStageDescriptorStorageImages},
        {cl::DeviceInfo::GlobalMemCachelineSize,
         static_cast<cl_uint>(props.limits.nonCoherentAtomSize)},
        {cl::DeviceInfo::Available, CL_TRUE},
        {cl::DeviceInfo::LinkerAvailable, CL_TRUE},
        {cl::DeviceInfo::CompilerAvailable, CL_TRUE},
        {cl::DeviceInfo::MaxOnDeviceQueues, 0},
        {cl::DeviceInfo::MaxOnDeviceEvents, 0},
        {cl::DeviceInfo::QueueOnDeviceMaxSize, 0},
        {cl::DeviceInfo::QueueOnDevicePreferredSize, 0},
        {cl::DeviceInfo::MaxPipeArgs, 0},
        {cl::DeviceInfo::PipeMaxPacketSize, 0},
        {cl::DeviceInfo::PipeSupport, CL_FALSE},
        {cl::DeviceInfo::PipeMaxActiveReservations, 0},
        {cl::DeviceInfo::ErrorCorrectionSupport, CL_FALSE},
        {cl::DeviceInfo::PreferredInteropUserSync, CL_TRUE},
        {cl::DeviceInfo::ExecutionCapabilities, CL_EXEC_KERNEL},

        // TODO(aannestrand) Update these hardcoded platform/device queries
        // http://anglebug.com/8511
        {cl::DeviceInfo::AddressBits, 64},
        {cl::DeviceInfo::EndianLittle, CL_TRUE},
        {cl::DeviceInfo::LocalMemType, CL_LOCAL},
        {cl::DeviceInfo::MaxSamplers, 0},
        {cl::DeviceInfo::MaxConstantArgs, 8},
        {cl::DeviceInfo::MaxNumSubGroups, 0},
        {cl::DeviceInfo::MaxComputeUnits, 4},
        {cl::DeviceInfo::MaxClockFrequency, 555},
        {cl::DeviceInfo::MaxWorkItemDimensions, 3},
        {cl::DeviceInfo::MinDataTypeAlignSize, 128},
        {cl::DeviceInfo::GlobalMemCacheType, CL_NONE},
        {cl::DeviceInfo::HostUnifiedMemory, CL_TRUE},
        {cl::DeviceInfo::NativeVectorWidthChar, 4},
        {cl::DeviceInfo::NativeVectorWidthShort, 2},
        {cl::DeviceInfo::NativeVectorWidthInt, 1},
        {cl::DeviceInfo::NativeVectorWidthLong, 1},
        {cl::DeviceInfo::NativeVectorWidthFloat, 1},
        {cl::DeviceInfo::NativeVectorWidthDouble, 1},
        {cl::DeviceInfo::NativeVectorWidthHalf, 0},
        {cl::DeviceInfo::PartitionMaxSubDevices, 0},
        {cl::DeviceInfo::PreferredVectorWidthInt, 1},
        {cl::DeviceInfo::PreferredVectorWidthLong, 1},
        {cl::DeviceInfo::PreferredVectorWidthChar, 4},
        {cl::DeviceInfo::PreferredVectorWidthHalf, 0},
        {cl::DeviceInfo::PreferredVectorWidthShort, 2},
        {cl::DeviceInfo::PreferredVectorWidthFloat, 1},
        {cl::DeviceInfo::PreferredVectorWidthDouble, 0},
        {cl::DeviceInfo::PreferredLocalAtomicAlignment, 0},
        {cl::DeviceInfo::PreferredGlobalAtomicAlignment, 0},
        {cl::DeviceInfo::PreferredPlatformAtomicAlignment, 0},
        {cl::DeviceInfo::NonUniformWorkGroupSupport, CL_TRUE},
        {cl::DeviceInfo::GenericAddressSpaceSupport, CL_FALSE},
        {cl::DeviceInfo::SubGroupIndependentForwardProgress, CL_FALSE},
        {cl::DeviceInfo::WorkGroupCollectiveFunctionsSupport, CL_FALSE},
    };
}

CLDeviceVk::~CLDeviceVk() = default;

CLDeviceImpl::Info CLDeviceVk::createInfo(cl::DeviceType type) const
{
    Info info(type);

    const VkPhysicalDeviceProperties &properties = mRenderer->getPhysicalDeviceProperties();

    info.maxWorkItemSizes.push_back(properties.limits.maxComputeWorkGroupSize[0]);
    info.maxWorkItemSizes.push_back(properties.limits.maxComputeWorkGroupSize[1]);
    info.maxWorkItemSizes.push_back(properties.limits.maxComputeWorkGroupSize[2]);

    // TODO(aannestrand) Update these hardcoded platform/device queries
    // http://anglebug.com/8511
    info.maxMemAllocSize  = 1 << 30;
    info.memBaseAddrAlign = 1024;

    // TODO(aannestrand) Add image and sampler support later
    // http://anglebug.com/8512
    info.imageSupport = CL_FALSE;

    info.image2D_MaxWidth          = properties.limits.maxImageDimension2D;
    info.image2D_MaxHeight         = properties.limits.maxImageDimension2D;
    info.image3D_MaxWidth          = properties.limits.maxImageDimension3D;
    info.image3D_MaxHeight         = properties.limits.maxImageDimension3D;
    info.image3D_MaxDepth          = properties.limits.maxImageDimension3D;
    info.imageMaxBufferSize        = properties.limits.maxImageDimension1D;
    info.imageMaxArraySize         = properties.limits.maxImageArrayLayers;
    info.imagePitchAlignment       = 0u;
    info.imageBaseAddressAlignment = 0u;

    info.execCapabilities     = CL_EXEC_KERNEL;
    info.queueOnDeviceMaxSize = 0u;
    info.builtInKernels       = "";
    info.version              = CL_MAKE_VERSION(3, 0, 0);
    info.versionStr           = "OpenCL 3.0 " + mRenderer->getVersionString(true);
    info.OpenCL_C_AllVersions = {{CL_MAKE_VERSION(3, 0, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(2, 0, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(1, 2, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(1, 1, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(1, 0, 0), "OpenCL C"}};

    info.OpenCL_C_Features         = {};
    info.extensionsWithVersion     = {};
    info.ILsWithVersion            = {};
    info.builtInKernelsWithVersion = {};
    info.partitionProperties       = {};
    info.partitionType             = {};
    info.IL_Version                = "";

    // Below extensions are required as of OpenCL 1.1
    info.extensions =
        "cl_khr_byte_addressable_store "
        "cl_khr_global_int32_base_atomics "
        "cl_khr_global_int32_extended_atomics "
        "cl_khr_local_int32_base_atomics "
        "cl_khr_local_int32_extended_atomics ";

    return info;
}

angle::Result CLDeviceVk::getInfoUInt(cl::DeviceInfo name, cl_uint *value) const
{
    if (mInfoUInt.count(name))
    {
        *value = mInfoUInt.at(name);
        return angle::Result::Continue;
    }
    ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
}

angle::Result CLDeviceVk::getInfoULong(cl::DeviceInfo name, cl_ulong *value) const
{
    if (mInfoULong.count(name))
    {
        *value = mInfoULong.at(name);
        return angle::Result::Continue;
    }
    ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
}

angle::Result CLDeviceVk::getInfoSizeT(cl::DeviceInfo name, size_t *value) const
{
    if (mInfoSizeT.count(name))
    {
        *value = mInfoSizeT.at(name);
        return angle::Result::Continue;
    }
    ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
}

angle::Result CLDeviceVk::getInfoStringLength(cl::DeviceInfo name, size_t *value) const
{
    if (mInfoString.count(name))
    {
        *value = mInfoString.at(name).length() + 1;
        return angle::Result::Continue;
    }
    ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
}

angle::Result CLDeviceVk::getInfoString(cl::DeviceInfo name, size_t size, char *value) const
{
    if (mInfoString.count(name))
    {
        std::strcpy(value, mInfoString.at(name).c_str());
        return angle::Result::Continue;
    }
    ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
}

angle::Result CLDeviceVk::createSubDevices(const cl_device_partition_property *properties,
                                           cl_uint numDevices,
                                           CreateFuncs &subDevices,
                                           cl_uint *numDevicesRet)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

cl::WorkgroupSize CLDeviceVk::selectWorkGroupSize(const cl::NDRange &ndrange) const
{
    // Limit total work-group size to the Vulkan device's limit
    const VkPhysicalDeviceProperties &props = mRenderer->getPhysicalDeviceProperties();
    uint32_t maxSize = static_cast<uint32_t>(mInfoSizeT.at(cl::DeviceInfo::MaxWorkGroupSize));
    maxSize          = std::min(maxSize, 64u);

    bool keepIncreasing         = false;
    cl::WorkgroupSize localSize = {1, 1, 1};
    do
    {
        keepIncreasing = false;
        for (cl_uint i = 0; i < ndrange.workDimensions; i++)
        {
            cl::WorkgroupSize newLocalSize = localSize;
            newLocalSize[i] *= 2;

            // TODO: Add support for non-uniform WGS
            // http://anglebug.com/8631
            if (ndrange.globalWorkSize[i] % newLocalSize[i] == 0 &&
                newLocalSize[i] <= props.limits.maxComputeWorkGroupCount[i] &&
                newLocalSize[0] * newLocalSize[1] * newLocalSize[2] <= maxSize)
            {
                localSize      = newLocalSize;
                keepIncreasing = true;
            }
        }
    } while (keepIncreasing);

    return localSize;
}

}  // namespace rx
