//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLDeviceVk.cpp: Implements the class methods for CLDeviceVk.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/vulkan/CLDeviceVk.h"
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

#include "libANGLE/renderer/cl_types.h"
#include "libANGLE/renderer/driver_utils.h"

#include "libANGLE/cl_utils.h"

#include "common/mathutil.h"

namespace rx
{

CLDeviceVk::CLDeviceVk(const cl::Device &device, vk::Renderer *renderer)
    : CLDeviceImpl(device), mRenderer(renderer), mSpirvVersion(ClspvGetSpirvVersion(renderer))
{
    const VkPhysicalDeviceProperties &props = mRenderer->getPhysicalDeviceProperties();

    // Setup initial device mInfo fields
    // TODO(aannestrand) Create cl::Caps and use for device creation
    // http://anglebug.com/42266954
    mInfoString = {
        {cl::DeviceInfo::Name, std::string(props.deviceName)},
        {cl::DeviceInfo::Vendor, mRenderer->getVendorString()},
        {cl::DeviceInfo::DriverVersion, mRenderer->getVersionString(true)},
        {cl::DeviceInfo::Version, std::string("OpenCL 3.0 " + mRenderer->getVersionString(true))},
        {cl::DeviceInfo::Profile, std::string("FULL_PROFILE")},
        {cl::DeviceInfo::OpenCL_C_Version, std::string("OpenCL C 1.2 ")},
        {cl::DeviceInfo::LatestConformanceVersionPassed, std::string("FIXME")}};
    mInfoSizeT = {
        {cl::DeviceInfo::MaxWorkGroupSize, props.limits.maxComputeWorkGroupInvocations},
        {cl::DeviceInfo::MaxGlobalVariableSize, 0},
        {cl::DeviceInfo::GlobalVariablePreferredTotalSize, 0},

        // TODO(aannestrand) Update these hardcoded platform/device queries
        // http://anglebug.com/42266935
        {cl::DeviceInfo::MaxParameterSize, 1024},
        {cl::DeviceInfo::ProfilingTimerResolution, 1},
        {cl::DeviceInfo::PrintfBufferSize, 1024 * 1024},
        {cl::DeviceInfo::PreferredWorkGroupSizeMultiple, 16},
    };

    // Minimum float configs/support required
    cl_ulong halfFPConfig   = 0;
    cl_ulong singleFPConfig = CL_FP_ROUND_TO_NEAREST | CL_FP_INF_NAN | CL_FP_FMA;
    cl_ulong doubleFPConfig = 0;

    if (mRenderer->getFeatures().supportsShaderFloat16.enabled)
    {
        halfFPConfig |= CL_FP_ROUND_TO_NEAREST | CL_FP_INF_NAN;
    }

    if (mRenderer->getFeatures().supportsShaderFloat64.enabled)
    {
        doubleFPConfig |= CL_FP_FMA | CL_FP_ROUND_TO_NEAREST | CL_FP_ROUND_TO_ZERO |
                          CL_FP_ROUND_TO_INF | CL_FP_INF_NAN | CL_FP_DENORM;
    }

    mInfoULong = {
        {cl::DeviceInfo::LocalMemSize, props.limits.maxComputeSharedMemorySize},
        {cl::DeviceInfo::SVM_Capabilities, 0},
        {cl::DeviceInfo::QueueOnDeviceProperties, 0},
        {cl::DeviceInfo::PartitionAffinityDomain, 0},
        {cl::DeviceInfo::DeviceEnqueueCapabilities, 0},
        {cl::DeviceInfo::QueueOnHostProperties, CL_QUEUE_PROFILING_ENABLE},

        // TODO(aannestrand) Update these hardcoded platform/device queries
        // http://anglebug.com/42266935
        {cl::DeviceInfo::HalfFpConfig, halfFPConfig},
        {cl::DeviceInfo::DoubleFpConfig, doubleFPConfig},
        {cl::DeviceInfo::GlobalMemCacheSize, 0},
        {cl::DeviceInfo::GlobalMemSize, 1024 * 1024 * 1024},
        {cl::DeviceInfo::MaxConstantBufferSize, 64 * 1024},
        {cl::DeviceInfo::SingleFpConfig, singleFPConfig},
        {cl::DeviceInfo::AtomicMemoryCapabilities,
         CL_DEVICE_ATOMIC_ORDER_RELAXED | CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP |
             CL_DEVICE_ATOMIC_ORDER_ACQ_REL | CL_DEVICE_ATOMIC_SCOPE_DEVICE |
             CL_DEVICE_ATOMIC_ORDER_SEQ_CST},
        // TODO (http://anglebug.com/379669750) Add these based on the Vulkan features query
        {cl::DeviceInfo::AtomicFenceCapabilities, CL_DEVICE_ATOMIC_ORDER_RELAXED |
                                                      CL_DEVICE_ATOMIC_ORDER_ACQ_REL |
                                                      CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP |
                                                      // non-mandatory
                                                      CL_DEVICE_ATOMIC_SCOPE_WORK_ITEM},
    };

    cl_uint maxNumSubGroups = 0u;
    if (mRenderer->getFeatures().supportsClKhrSubgroups.enabled)
    {
        const uint32_t subgroupSize = mRenderer->getPhysicalDeviceSubgroupProperties().subgroupSize;
        ASSERT(subgroupSize > 0);
        maxNumSubGroups =
            static_cast<uint32_t>(mInfoSizeT[cl::DeviceInfo::MaxWorkGroupSize]) / subgroupSize;
    }

    mInfoUInt = {
        {cl::DeviceInfo::VendorID, props.vendorID},
        {cl::DeviceInfo::MaxReadImageArgs, cl::IMPLEMENATION_MAX_READ_IMAGES},
        {cl::DeviceInfo::MaxWriteImageArgs, cl::IMPLEMENATION_MAX_WRITE_IMAGES},
        {cl::DeviceInfo::MaxReadWriteImageArgs, cl::IMPLEMENATION_MAX_WRITE_IMAGES},
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
        // http://anglebug.com/42266935
        {cl::DeviceInfo::AddressBits,
         mRenderer->getFeatures().supportsBufferDeviceAddress.enabled ? 64 : 32},
        {cl::DeviceInfo::EndianLittle, CL_TRUE},
        {cl::DeviceInfo::LocalMemType, CL_LOCAL},
        // TODO (http://anglebug.com/379669750) Vulkan reports a big sampler count number, we dont
        // need that many and set it to minimum req for now.
        {cl::DeviceInfo::MaxSamplers, 16u},
        {cl::DeviceInfo::MaxConstantArgs, 8},
        {cl::DeviceInfo::MaxNumSubGroups, maxNumSubGroups},
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
        {cl::DeviceInfo::NativeVectorWidthDouble, mRenderer->getNativeVectorWidthDouble()},
        {cl::DeviceInfo::NativeVectorWidthHalf, mRenderer->getNativeVectorWidthHalf()},
        {cl::DeviceInfo::PartitionMaxSubDevices, 0},
        {cl::DeviceInfo::PreferredVectorWidthChar, 4},
        {cl::DeviceInfo::PreferredVectorWidthShort, 8},
        {cl::DeviceInfo::PreferredVectorWidthInt, 1},
        {cl::DeviceInfo::PreferredVectorWidthLong, 1},
        {cl::DeviceInfo::PreferredVectorWidthFloat, 1},
        {cl::DeviceInfo::PreferredVectorWidthDouble, mRenderer->getPreferredVectorWidthDouble()},
        {cl::DeviceInfo::PreferredVectorWidthHalf, mRenderer->getPreferredVectorWidthHalf()},
        {cl::DeviceInfo::PreferredLocalAtomicAlignment, 0},
        {cl::DeviceInfo::PreferredGlobalAtomicAlignment, 0},
        {cl::DeviceInfo::PreferredPlatformAtomicAlignment, 0},
        {cl::DeviceInfo::NonUniformWorkGroupSupport, CL_TRUE},
        {cl::DeviceInfo::GenericAddressSpaceSupport, CL_FALSE},
        {cl::DeviceInfo::SubGroupIndependentForwardProgress,
         maxNumSubGroups > 0 ? CL_TRUE : CL_FALSE},
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
    // http://anglebug.com/42266935
    info.maxMemAllocSize  = 1 << 30;
    info.memBaseAddrAlign = 1024;

    info.imageSupport = CL_TRUE;

    info.image2D_MaxWidth  = properties.limits.maxImageDimension2D;
    info.image2D_MaxHeight = properties.limits.maxImageDimension2D;
    info.image3D_MaxWidth  = properties.limits.maxImageDimension3D;
    info.image3D_MaxHeight = properties.limits.maxImageDimension3D;
    info.image3D_MaxDepth  = properties.limits.maxImageDimension3D;
    // Max number of pixels for a 1D image created from a buffer object.
    info.imageMaxBufferSize = properties.limits.maxTexelBufferElements;
    info.imageMaxArraySize  = properties.limits.maxImageArrayLayers;
    // The following are queried when image2d is created from buffer. We mimic its support for now
    // by doing a copy and as such dont have alignment requirements.
    info.imagePitchAlignment       = 1u;
    info.imageBaseAddressAlignment = 1u;

    info.execCapabilities     = CL_EXEC_KERNEL;
    info.queueOnDeviceMaxSize = 0u;
    info.builtInKernels       = "";
    info.version              = CL_MAKE_VERSION(3, 0, 0);
    info.versionStr           = "OpenCL 3.0 " + mRenderer->getVersionString(true);
    info.OpenCL_C_AllVersions = {{CL_MAKE_VERSION(1, 0, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(1, 1, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(1, 2, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(3, 0, 0), "OpenCL C"}};

    info.OpenCL_C_Features         = {};
    info.ILsWithVersion            = {};
    info.builtInKernelsWithVersion = {};
    info.partitionProperties       = {};
    info.partitionType             = {};
    info.IL_Version                = "";

    // Below extensions are required as of OpenCL 1.1, add their versioned strings
    NameVersionVector versionedExtensionList = {
        // Below extensions are required as of OpenCL 1.1
        cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0),
                        .name    = "cl_khr_byte_addressable_store"},
        cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0),
                        .name    = "cl_khr_global_int32_base_atomics"},
        cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0),
                        .name    = "cl_khr_global_int32_extended_atomics"},
        cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0),
                        .name    = "cl_khr_local_int32_base_atomics"},
        cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0),
                        .name    = "cl_khr_local_int32_extended_atomics"},
    };

    CLExtensions::ExternalMemoryHandleBitset supportedHandles;
    supportedHandles.set(cl::ExternalMemoryHandle::OpaqueFd, supportsExternalMemoryFd());
    supportedHandles.set(cl::ExternalMemoryHandle::DmaBuf, supportsExternalMemoryDmaBuf());

    // Populate other extensions based on feature support
    if (info.populateSupportedExternalMemoryHandleTypes(supportedHandles))
    {
        versionedExtensionList.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_khr_external_memory"});

        // cl_arm_import_memory is layered on top of cl_arm_import_memory
        bool reportBaseArmImportMemString = false;
        if (supportedHandles.test(cl::ExternalMemoryHandle::DmaBuf))
        {
            versionedExtensionList.push_back(cl_name_version{
                .version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_arm_import_memory_dma_buf"});
            reportBaseArmImportMemString = true;
        }
        if (reportBaseArmImportMemString)
        {
            versionedExtensionList.push_back(cl_name_version{.version = CL_MAKE_VERSION(1, 11, 0),
                                                             .name    = "cl_arm_import_memory"});
        }
    }
    if (mRenderer->getFeatures().supportsShaderFloat16.enabled)
    {
        versionedExtensionList.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_khr_fp16"});
    }
    if (mRenderer->getFeatures().supportsShaderFloat64.enabled)
    {
        versionedExtensionList.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_khr_fp64"});
    }
    if (info.imageSupport && info.image3D_MaxDepth > 1)
    {
        versionedExtensionList.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_khr_3d_image_writes"});
    }
    if (mRenderer->getQueueFamilyProperties().queueCount > 1)
    {
        versionedExtensionList.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_khr_priority_hints"});
    }

    info.integerDotProductCapabilities = getIntegerDotProductCapabilities();
    info.integerDotProductAccelerationProperties8Bit =
        getIntegerDotProductAccelerationProperties8Bit();
    info.integerDotProductAccelerationProperties4x8BitPacked =
        getIntegerDotProductAccelerationProperties4x8BitPacked();

    if (mRenderer->getFeatures().supportsShaderIntegerDotProduct.enabled)
    {
        versionedExtensionList.push_back(cl_name_version{.version = CL_MAKE_VERSION(2, 0, 0),
                                                         .name    = "cl_khr_integer_dot_product"});
    }

    // cl_khr_int64_base_atomics and cl_khr_int64_extended_atomics
    if (mRenderer->getFeatures().supportsShaderAtomicInt64.enabled)
    {
        versionedExtensionList.push_back(cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0),
                                                         .name    = "cl_khr_int64_base_atomics"});
        versionedExtensionList.push_back(cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0),
                                                         .name = "cl_khr_int64_extended_atomics"});
    }

    // cl_khr_depth_images
    if (setupAndReportDepthImageSupport(info))
    {
        versionedExtensionList.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_khr_depth_images"});
    }

    // cl_khr_image2d_from_buffer
    if (info.version >= CL_MAKE_VERSION(3, 0, 0) && info.imageSupport)
    {
        versionedExtensionList.push_back(cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0),
                                                         .name    = "cl_khr_image2d_from_buffer"});
    }

    // cl_khr_subgroups
    if (mRenderer->getFeatures().supportsClKhrSubgroups.enabled)
    {
        versionedExtensionList.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_khr_subgroups"});
    }

    info.initializeVersionedExtensions(std::move(versionedExtensionList));

    if (!mRenderer->getFeatures().supportsUniformBufferStandardLayout.enabled)
    {
        ERR() << "VK_KHR_uniform_buffer_standard_layout extension support is needed to properly "
                 "support uniform buffers. Otherwise, you must disable OpenCL.";
    }

    // Populate supported features
    if (info.imageSupport)
    {
        info.OpenCL_C_Features.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0), .name = "__opencl_c_images"});
        info.OpenCL_C_Features.push_back(cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0),
                                                         .name    = "__opencl_c_3d_image_writes"});
        info.OpenCL_C_Features.push_back(cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0),
                                                         .name = "__opencl_c_read_write_images"});
    }
    if (mRenderer->getEnabledFeatures().features.shaderInt64)
    {
        info.OpenCL_C_Features.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0), .name = "__opencl_c_int64"});
    }

    if (mRenderer->getFeatures().supportsShaderIntegerDotProduct.enabled)
    {
        info.OpenCL_C_Features.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0),
                            .name    = "__opencl_c_integer_dot_product_input_4x8bit"});
        info.OpenCL_C_Features.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0),
                            .name    = "__opencl_c_integer_dot_product_input_4x8bit_packed"});
    }

    info.OpenCL_C_Features.push_back(cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0),
                                                     .name    = "__opencl_c_atomic_order_acq_rel"});
    info.OpenCL_C_Features.push_back(cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0),
                                                     .name    = "__opencl_c_atomic_order_seq_cst"});
    info.OpenCL_C_Features.push_back(cl_name_version{.version = CL_MAKE_VERSION(3, 0, 0),
                                                     .name    = "__opencl_c_atomic_scope_device"});

    if (mRenderer->getFeatures().supportsClKhrSubgroups.enabled)
    {
        info.OpenCL_C_Features.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "__opencl_c_subgroups"});
    }

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

bool CLDeviceVk::supportsExternalMemoryFd() const
{
    return mRenderer->getFeatures().supportsExternalMemoryFd.enabled;
}

bool CLDeviceVk::supportsExternalMemoryDmaBuf() const
{
    return mRenderer->getFeatures().supportsExternalMemoryDmaBuf.enabled;
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
    uint32_t subgroupSize = 0;
    uint32_t maxSize      = static_cast<uint32_t>(mInfoSizeT.at(cl::DeviceInfo::MaxWorkGroupSize));
    if (mRenderer->getFeatures().supportsClKhrSubgroups.enabled)
    {
        // query the renderer SIMD width
        subgroupSize = mRenderer->getPhysicalDeviceSubgroupProperties().subgroupSize;
    }
    // adjust max to be at least one full subgroup
    maxSize = std::max(subgroupSize, std::min(64u, maxSize));

    if (getRenderer()->getFeatures().clBestUniformFitWGS.enabled)
    {
        return CalculateUniformFitWGS(ndrange, maxSize);
    }
    else
    {
        return CalculateSimplePow2WGS(ndrange, maxSize);
    }
}

cl::WorkgroupSize CLDeviceVk::CalculateSimplePow2WGS(const cl::NDRange &ndrange,
                                                     const uint32_t maxSize)
{
    // simplest strategy - start with always-valid GWS, increase by power-of-two until limits
    bool keepIncreasing         = false;
    cl::WorkgroupSize localSize = {1, 1, 1};
    do
    {
        keepIncreasing = false;
        for (uint32_t dim = 0; dim < ndrange.workDimensions; dim++)
        {
            cl::WorkgroupSize newLocalSize = localSize;
            newLocalSize[dim] *= 2;

            uint32_t threadsInWorkgroup = newLocalSize[0] * newLocalSize[1] * newLocalSize[2];
            if (newLocalSize[dim] <= ndrange.globalWorkSize[dim] && threadsInWorkgroup <= maxSize)
            {
                localSize      = newLocalSize;
                keepIncreasing = true;
            }
        }
    } while (keepIncreasing);
    return localSize;
}

cl::WorkgroupSize CLDeviceVk::CalculateUniformFitWGS(const cl::NDRange &ndrange,
                                                     const uint32_t maxSize)
{
    // uniform-fit strategy: prioritizes on ensuring calculated WGS is uniform to the given GWS.
    // this tries to avoid non-uniform case which can be costly due to "chunking" dispatches into
    // uniform regions where each new/unique WGS leads to a creation of a new compute pipeline
    // (i.e. due to WGS being treated as a VK spec-constant)
    cl::WorkgroupSize localSize = {std::min(ndrange.globalWorkSize[0], maxSize),
                                   std::min(ndrange.globalWorkSize[1], maxSize),
                                   std::min(ndrange.globalWorkSize[2], maxSize)};
    uint32_t threadsInWorkgroup = localSize[2] * localSize[1] * localSize[0];

    // 1st-pass: iterate the WGS (each dim) to ensure they each evenly divide into their GWS
    while (threadsInWorkgroup > maxSize)
    {
        // check for dim with largest WGS on each try
        uint32_t maxDim = 0;
        for (uint32_t dim = 1; dim < ndrange.workDimensions; ++dim)
        {
            if (localSize[dim] > localSize[maxDim])
            {
                maxDim = dim;
            }
        }

        if (localSize[maxDim] > 1)
        {
            --localSize[maxDim];  // back-off by one initially
            while (localSize[maxDim] > 1 &&
                   (ndrange.globalWorkSize[maxDim] % localSize[maxDim]) != 0)
            {
                --localSize[maxDim];
            }
        }
        threadsInWorkgroup = localSize[2] * localSize[1] * localSize[0];
    }

    // 2nd-pass: if nothing worked so far, try pinning dim with the most threads (ignore others)
    if (localSize == cl::WorkgroupSize{1, 1, 1})
    {
        uint32_t dimWithMostThreads = 0, currentSize = 1;
        for (uint32_t dim = 0; dim < ndrange.workDimensions; ++dim)
        {
            if (currentSize < ndrange.globalWorkSize[dim])
            {
                currentSize        = ndrange.globalWorkSize[dim];
                dimWithMostThreads = dim;
            }
        }
        if (currentSize > maxSize)
        {
            // we tried our best to fit our WGS evenly into GWS, but it's not feasible - fall
            // back to simpler pow2 generator
            WARN() << "could not perform even-fit for WGS, falling back to simple pow2 WGS";
            return CalculateSimplePow2WGS(ndrange, maxSize);
        }
        localSize[dimWithMostThreads] = currentSize;
    }

    return localSize;
}

cl_device_integer_dot_product_capabilities_khr CLDeviceVk::getIntegerDotProductCapabilities() const
{
    cl_device_integer_dot_product_capabilities_khr integerDotProductCapabilities = {};

    if (mRenderer->getFeatures().supportsShaderIntegerDotProduct.enabled)
    {
        // If the VK extension is supported, then all the caps mentioned in the CL spec are
        // supported by default.
        integerDotProductCapabilities = (CL_DEVICE_INTEGER_DOT_PRODUCT_INPUT_4x8BIT_PACKED_KHR |
                                         CL_DEVICE_INTEGER_DOT_PRODUCT_INPUT_4x8BIT_KHR);
    }

    return integerDotProductCapabilities;
}

cl_device_integer_dot_product_acceleration_properties_khr
CLDeviceVk::getIntegerDotProductAccelerationProperties8Bit() const
{

    cl_device_integer_dot_product_acceleration_properties_khr
        integerDotProductAccelerationProperties = {};
    const VkPhysicalDeviceShaderIntegerDotProductProperties &integerDotProductProps =
        mRenderer->getPhysicalDeviceShaderIntegerDotProductProperties();

    integerDotProductAccelerationProperties.signed_accelerated =
        integerDotProductProps.integerDotProduct8BitSignedAccelerated;
    integerDotProductAccelerationProperties.unsigned_accelerated =
        integerDotProductProps.integerDotProduct8BitUnsignedAccelerated;
    integerDotProductAccelerationProperties.mixed_signedness_accelerated =
        integerDotProductProps.integerDotProduct8BitMixedSignednessAccelerated;
    integerDotProductAccelerationProperties.accumulating_saturating_signed_accelerated =
        integerDotProductProps.integerDotProductAccumulatingSaturating8BitSignedAccelerated;
    integerDotProductAccelerationProperties.accumulating_saturating_unsigned_accelerated =
        integerDotProductProps.integerDotProductAccumulatingSaturating8BitUnsignedAccelerated;
    integerDotProductAccelerationProperties.accumulating_saturating_mixed_signedness_accelerated =
        integerDotProductProps
            .integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated;

    return integerDotProductAccelerationProperties;
}

cl_device_integer_dot_product_acceleration_properties_khr
CLDeviceVk::getIntegerDotProductAccelerationProperties4x8BitPacked() const
{

    cl_device_integer_dot_product_acceleration_properties_khr
        integerDotProductAccelerationProperties = {};
    const VkPhysicalDeviceShaderIntegerDotProductProperties &integerDotProductProps =
        mRenderer->getPhysicalDeviceShaderIntegerDotProductProperties();

    integerDotProductAccelerationProperties.signed_accelerated =
        integerDotProductProps.integerDotProduct4x8BitPackedSignedAccelerated;
    integerDotProductAccelerationProperties.unsigned_accelerated =
        integerDotProductProps.integerDotProduct4x8BitPackedUnsignedAccelerated;
    integerDotProductAccelerationProperties.mixed_signedness_accelerated =
        integerDotProductProps.integerDotProduct4x8BitPackedMixedSignednessAccelerated;
    integerDotProductAccelerationProperties.accumulating_saturating_signed_accelerated =
        integerDotProductProps.integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated;
    integerDotProductAccelerationProperties.accumulating_saturating_unsigned_accelerated =
        integerDotProductProps
            .integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated;
    integerDotProductAccelerationProperties.accumulating_saturating_mixed_signedness_accelerated =
        integerDotProductProps
            .integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated;

    return integerDotProductAccelerationProperties;
}

bool CLDeviceVk::setupAndReportDepthImageSupport(Info &info) const
{
    if (IsNvidia(getRenderer()->getPhysicalDeviceProperties().vendorID))
    {
        // TODO(aannestrand) CTS validation issue with (cl_copy_images.2D use_pitches) on nvidia
        // platform, disable its cl_khr_depth_images support for now
        // http://anglebug.com/472472687
        return false;
    }

    constexpr VkFlags kDepthFeatures =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    // for reporting the extension string, we only need CL_FLOAT and CL_UNORM_INT16
    // https://registry.khronos.org/OpenCL/specs/3.0-unified/html/OpenCL_API.html#minimum-list-of-supported-image-formats
    CLExtensions::SupportedDepthOrderTypes minimumDepthOrderTypeSupport;
    minimumDepthOrderTypeSupport.set(cl::ImageChannelType::Float);
    minimumDepthOrderTypeSupport.set(cl::ImageChannelType::UnormInt16);

    for (const cl::ImageChannelType imageChannelType : angle::AllEnums<cl::ImageChannelType>())
    {
        angle::FormatID format = angle::Format::CLDEPTHFormatToID(cl::ToCLenum(imageChannelType));
        if (format != angle::FormatID::NONE &&
            mRenderer->hasImageFormatFeatureBits(format, kDepthFeatures))
        {
            info.supportedDepthOrderTypes.set(imageChannelType);
        }
    }

    // check/return-true if the minimum support is there
    return (info.supportedDepthOrderTypes & minimumDepthOrderTypeSupport) ==
           minimumDepthOrderTypeSupport;
}

}  // namespace rx
