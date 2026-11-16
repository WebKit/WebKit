//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLDeviceVk.cpp: Implements the class methods for CLDeviceVk.
//

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

uint32_t CLDeviceVk::getNumComputeUnits() const
{
    if (mRenderer->getFeatures().supportsAmdShaderCoreProperties.enabled)
    {
        auto shaderCoreProperties = getRenderer()->getPhysicalDeviceShaderCorePropertiesAMD();
        // There are two modes of operations possible for RDNA - WGP and CU. WGP mode uses 2'CUs as
        // a single unit. Samsung configures the GPU in WGP mode by default.
        uint32_t workGroupFactor =
            IsSamsung(getRenderer()->getPhysicalDeviceProperties().vendorID) ? 2 : 1;
        return shaderCoreProperties.shaderEngineCount *
               shaderCoreProperties.shaderArraysPerEngineCount *
               shaderCoreProperties.computeUnitsPerShaderArray / workGroupFactor;
    }
    return cl::IMPLEMENTATION_NUM_COMPUTE_UNITS;
}

uint32_t CLDeviceVk::getWorkGroupSizeMultiple() const
{
    if (mRenderer->getFeatures().supportsAmdShaderCoreProperties.enabled)
    {
        return getRenderer()->getPhysicalDeviceShaderCorePropertiesAMD().wavefrontSize;
    }
    return cl::IMPLEMENTATION_PREFERRED_WORKGROUP_SIZE_MULTIPLE;
}

cl_ulong CLDeviceVk::getSingleFpConfig() const
{
    // The spec requires these as bare minimum
    cl_ulong singleFpConfig = CL_FP_INF_NAN | CL_FP_ROUND_TO_NEAREST;
    if (mRenderer->getFeatures().supportsRoundingModeRtzFp32.enabled)
    {
        singleFpConfig |= CL_FP_ROUND_TO_ZERO;
    }

    if (mRenderer->getFeatures().supportsAmdShaderCoreProperties.enabled)
    {
        // The below are known to be supported
        // TODO(http://anglebug.com/472472687) need to find appropriate query
        singleFpConfig |= CL_FP_ROUND_TO_INF | CL_FP_FMA;
    }

    return singleFpConfig;
}

cl_ulong CLDeviceVk::getHalfFpConfig() const
{
    cl_ulong halfFpConfig = 0;
    if (mRenderer->getFeatures().supportsClFp16.enabled)
    {
        halfFpConfig |= CL_FP_INF_NAN;
        if (mRenderer->getFeatures().supportsRoundingModeRteFp16.enabled)
        {
            halfFpConfig |= CL_FP_ROUND_TO_NEAREST;
        }
        if (mRenderer->getFeatures().supportsRoundingModeRtzFp16.enabled)
        {
            halfFpConfig |= CL_FP_ROUND_TO_ZERO;
        }

        if (mRenderer->getFeatures().supportsAmdShaderCoreProperties.enabled)
        {
            // The below are known to be supported
            // TODO(http://anglebug.com/472472687) need to find appropriate query
            halfFpConfig |= CL_FP_ROUND_TO_INF | CL_FP_FMA;
        }
    }
    return halfFpConfig;
}

cl_ulong CLDeviceVk::getDoubleFpConfig() const
{
    cl_ulong doubleFpConfig = 0;
    if (mRenderer->getFeatures().supportsClFp64.enabled)
    {
        doubleFpConfig |=
            CL_FP_INF_NAN | CL_FP_ROUND_TO_NEAREST | CL_FP_ROUND_TO_ZERO | CL_FP_DENORM;

        // The below are required
        // TODO(http://anglebug.com/472472687) need to find appropriate query
        doubleFpConfig |= CL_FP_ROUND_TO_INF | CL_FP_FMA;
    }
    return doubleFpConfig;
}

cl_ulong CLDeviceVk::getCacheSize() const
{
    // TODO(http://anglebug.com/472472687) need to find appropriate query
    return 1024 * 1024ULL;
}

cl_ulong CLDeviceVk::getHeapSizeForResource(const VkMemoryPropertyFlags supportedProperties,
                                            const VkMemoryPropertyFlags avoidedProperties) const
{
    const vk::MemoryProperties &memoryProperties = mRenderer->getMemoryProperties();
    for (uint32_t memTypeIdx = 0; memTypeIdx < memoryProperties.getMemoryTypeCount(); memTypeIdx++)
    {
        bool support = (memoryProperties.getMemoryType(memTypeIdx).propertyFlags &
                        supportedProperties) == supportedProperties;
        bool avoid =
            (memoryProperties.getMemoryType(memTypeIdx).propertyFlags & avoidedProperties) != 0;
        if (support && !avoid)
        {
            return memoryProperties.getHeapSizeForMemoryType(memTypeIdx);
        }
    }
    return 0;
}

cl_ulong CLDeviceVk::getGlobalMemSize() const
{
    // Memory-property sets that can back CL buffers (host-visible), with varying cache/coherency.
    cl_ulong bufferSize                                            = 0;
    static constexpr VkMemoryPropertyFlags kBufferSupportedFlags[] = {
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    for (const VkMemoryPropertyFlags &supportedFlags : kBufferSupportedFlags)
    {
        bufferSize = std::max(bufferSize, getHeapSizeForResource(supportedFlags, 0));
    }

    // Memory-property sets that can back CL images (device-local, non-host-coherent).
    cl_ulong imageSize                                            = 0;
    static constexpr VkMemoryPropertyFlags kImageSupportedFlags[] = {
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
    for (const VkMemoryPropertyFlags &supportedFlags : kImageSupportedFlags)
    {
        imageSize = std::max(imageSize, getHeapSizeForResource(
                                            supportedFlags, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    }

    // Return the size of the smallest memory heap
    if (bufferSize == 0)
    {
        return imageSize;
    }
    if (imageSize == 0)
    {
        return bufferSize;
    }
    return std::min(bufferSize, imageSize);
}

cl_ulong CLDeviceVk::getMaxMemAllocSize() const
{
    constexpr cl_ulong MB = 1024 * 1024UL;
    constexpr cl_ulong GB = 1024 * MB;

    const cl_ulong globalMemorySize = getGlobalMemSize();
    const cl_ulong quarterGlobalMem = globalMemorySize >> 2;
    const cl_ulong maxAllocSize     = mRenderer->getMaxMemoryAllocationSize();
    const cl_ulong specMinimum      = gl::clamp(quarterGlobalMem, 32 * MB, 1 * GB);

    if (maxAllocSize < specMinimum)
    {  // vulkan device is not conformant
        ERR() << "vk device (0x" << this
              << ") CL_DEVICE_MAX_MEM_ALLOC_SIZE is less than CL spec minimum (i.e. "
              << maxAllocSize << " < " << specMinimum << ")!";
        return cl::kMaxAllocSentinel;
    }

    return specMinimum;
}

size_t CLDeviceVk::getImageMaxBufferSize() const
{
    const VkPhysicalDeviceProperties &properties = mRenderer->getPhysicalDeviceProperties();
    const VkDeviceSize maxBufferSize             = std::min(
        static_cast<cl_ulong>(properties.limits.maxTexelBufferElements), getMaxMemAllocSize());

    // Reserve headroom for the vertex-attribute stride padding that
    // padVertexAttribBufferSizeIfNeeded() would later append. padVertexAttribBufferSizeIfNeeded(0)
    // yields that padding amount (0 when the padBuffersToMaxVertexAttribStride feature is
    // disabled).
    const VkDeviceSize maxVertexAttribStride = mRenderer->padVertexAttribBufferSizeIfNeeded(0);
    ASSERT(maxBufferSize > maxVertexAttribStride);

    return static_cast<size_t>(maxBufferSize - maxVertexAttribStride);
}

CLDeviceVk::CLDeviceVk(const cl::Device &device, vk::Renderer *renderer)
    : CLDeviceImpl(device), mRenderer(renderer), mSpirvVersion(ClspvGetSpirvVersion(renderer))
{
    const VkPhysicalDeviceProperties &props = mRenderer->getPhysicalDeviceProperties();

    // Setup initial device caps
    // Populate all the string values.
    mCaps.name             = std::string(props.deviceName);
    mCaps.vendor           = mRenderer->getVendorString();
    mCaps.driverVersion    = mRenderer->getVersionString(true);
    mCaps.version          = std::string("OpenCL 3.0 " + mRenderer->getVersionString(true));
    mCaps.profile          = std::string("FULL_PROFILE");
    mCaps.openCL_C_Version = std::string("OpenCL C 1.2 ");
    mCaps.latestConformanceVersionPassed = std::string("FIXME");

    // Populate all the size_t values.
    // Below caps are retrieved from Vulkan queries
    mCaps.maxWorkGroupSize         = props.limits.maxComputeWorkGroupInvocations;
    mCaps.profilingTimerResolution = static_cast<size_t>(props.limits.timestampPeriod);

    // Below caps are retrieved from vendor specific extensions if present
    mCaps.preferredWorkGroupSizeMultiple = getWorkGroupSizeMultiple();

    // Below caps are not supported by current implementation
    mCaps.maxGlobalVariableSize            = 0;
    mCaps.globalVariablePreferredTotalSize = 0;

    // Below caps are set to some known good values
    // TODO(aannestrand) Update these hardcoded platform/device queries
    // http://anglebug.com/42266935
    mCaps.maxParameterSize = 1024;
    mCaps.printfBufferSize = 1024 * 1024;

    // Populate all the ulong values.
    // Below caps are retrieved from Vulkan queries
    mCaps.maxMemAllocSize = getMaxMemAllocSize();
    mCaps.globalMemSize   = getGlobalMemSize();
    mCaps.localMemSize    = props.limits.maxComputeSharedMemorySize;

    // Below caps are retrieved from vendor specific extensions if present
    mCaps.singleFpConfig     = getSingleFpConfig();
    mCaps.halfFpConfig       = getHalfFpConfig();
    mCaps.doubleFpConfig     = getDoubleFpConfig();
    mCaps.globalMemCacheSize = getCacheSize();

    // Below are Vulkan backend implementation details
    mCaps.queueOnHostProperties    = CL_QUEUE_PROFILING_ENABLE;
    mCaps.atomicMemoryCapabilities = CL_DEVICE_ATOMIC_ORDER_RELAXED |
                                     CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP |
                                     CL_DEVICE_ATOMIC_ORDER_ACQ_REL |
                                     CL_DEVICE_ATOMIC_SCOPE_DEVICE | CL_DEVICE_ATOMIC_ORDER_SEQ_CST;
    // TODO (http://anglebug.com/379669750) Add these based on the Vulkan features query
    mCaps.atomicFenceCapabilities = CL_DEVICE_ATOMIC_ORDER_RELAXED |
                                    CL_DEVICE_ATOMIC_ORDER_ACQ_REL |
                                    CL_DEVICE_ATOMIC_SCOPE_WORK_GROUP |
                                    // non-mandatory
                                    CL_DEVICE_ATOMIC_SCOPE_WORK_ITEM;

    // Below caps are not supported by current implementation
    mCaps.sVM_Capabilities          = 0ULL;
    mCaps.queueOnDeviceProperties   = 0ULL;
    mCaps.partitionAffinityDomain   = 0ULL;
    mCaps.deviceEnqueueCapabilities = 0ULL;

    // Below caps are set to some known good values
    // TODO(aannestrand) Update these hardcoded platform/device queries
    // http://anglebug.com/42266935
    // Constant buffer size is same as global variable size in SGPU
    mCaps.maxConstantBufferSize = 1024 * 1024 * 1024ULL;

    cl_uint maxNumSubGroups = 0u;
    if (mRenderer->getFeatures().supportsClKhrSubgroups.enabled)
    {
        const uint32_t subgroupSize = mRenderer->getPhysicalDeviceSubgroupProperties().subgroupSize;
        ASSERT(subgroupSize > 0);
        maxNumSubGroups =
            UnsignedCeilDivide(static_cast<uint32_t>(mCaps.maxWorkGroupSize), subgroupSize);
    }

    // Populate all the uint values.
    // Below caps are retrieved from Vulkan queries
    mCaps.vendorID               = props.vendorID;
    mCaps.globalMemCachelineSize = static_cast<cl_uint>(props.limits.nonCoherentAtomSize);
    mCaps.maxNumSubGroups        = maxNumSubGroups;
    mCaps.subGroupIndependentForwardProgress = maxNumSubGroups > 0 ? CL_TRUE : CL_FALSE;
    mCaps.addressBits = mRenderer->getFeatures().supportsBufferDeviceAddress.enabled ? 64 : 32;

    // Below caps are retrieved from vendor specific extensions if present
    mCaps.maxComputeUnits = getNumComputeUnits();
    // Frequency is reported in MHz
    mCaps.maxClockFrequency = 555;
    // Report the number of CU's as max sub devices for now
    mCaps.partitionMaxSubDevices = getNumComputeUnits();

    // Below are currently setup as Vulkan backend implementation details
    mCaps.maxReadImageArgs         = cl::IMPLEMENTATION_MAX_READ_IMAGES;
    mCaps.maxWriteImageArgs        = cl::IMPLEMENTATION_MAX_WRITE_IMAGES;
    mCaps.maxReadWriteImageArgs    = cl::IMPLEMENTATION_MAX_WRITE_IMAGES;
    mCaps.available                = CL_TRUE;
    mCaps.linkerAvailable          = CL_TRUE;
    mCaps.compilerAvailable        = CL_TRUE;
    mCaps.executionCapabilities    = CL_EXEC_KERNEL;
    mCaps.preferredInteropUserSync = CL_TRUE;
    mCaps.globalMemCacheType       = CL_READ_WRITE_CACHE;
    mCaps.hostUnifiedMemory        = CL_TRUE;
    // TODO(aannestrand) Update these hardcoded platform/device queries
    // http://anglebug.com/42266935
    mCaps.endianLittle               = CL_TRUE;
    mCaps.localMemType               = CL_LOCAL;
    mCaps.maxWorkItemDimensions      = 3;
    mCaps.nonUniformWorkGroupSupport = CL_TRUE;

    // Below caps are not supported by current implementation
    mCaps.maxOnDeviceQueues                   = 0;
    mCaps.maxOnDeviceEvents                   = 0;
    mCaps.queueOnDeviceMaxSize                = 0;
    mCaps.queueOnDevicePreferredSize          = 0;
    mCaps.maxPipeArgs                         = 0;
    mCaps.pipeMaxPacketSize                   = 0;
    mCaps.pipeSupport                         = CL_FALSE;
    mCaps.pipeMaxActiveReservations           = 0;
    mCaps.errorCorrectionSupport              = CL_FALSE;
    mCaps.genericAddressSpaceSupport          = CL_FALSE;
    mCaps.workGroupCollectiveFunctionsSupport = CL_FALSE;

    // Below caps are set to some known good values
    // TODO (http://anglebug.com/379669750) Vulkan reports a big sampler count number, we dont
    // need that many and set it to minimum req for now.
    mCaps.maxSamplers          = 16u;
    mCaps.maxConstantArgs      = 8;
    mCaps.minDataTypeAlignSize = 128;

    // TODO: vulkan currently does not have a way to query vector widths. we can just
    // emulate/return "1" for now, but come back to this if that changes
    // https://anglebug.com/561370394
    mCaps.nativeVectorWidthChar      = 1;
    mCaps.nativeVectorWidthShort     = 1;
    mCaps.nativeVectorWidthInt       = 1;
    mCaps.nativeVectorWidthLong      = 1;
    mCaps.nativeVectorWidthFloat     = 1;
    mCaps.nativeVectorWidthDouble    = mRenderer->getFeatures().supportsClFp64.enabled ? 1 : 0;
    mCaps.nativeVectorWidthHalf      = mRenderer->getFeatures().supportsClFp16.enabled ? 1 : 0;
    mCaps.preferredVectorWidthChar   = 1;
    mCaps.preferredVectorWidthShort  = 1;
    mCaps.preferredVectorWidthInt    = 1;
    mCaps.preferredVectorWidthLong   = 1;
    mCaps.preferredVectorWidthFloat  = 1;
    mCaps.preferredVectorWidthDouble = mRenderer->getFeatures().supportsClFp64.enabled ? 1 : 0;
    mCaps.preferredVectorWidthHalf   = mRenderer->getFeatures().supportsClFp16.enabled ? 1 : 0;

    mCaps.preferredLocalAtomicAlignment    = 0;
    mCaps.preferredGlobalAtomicAlignment   = 0;
    mCaps.preferredPlatformAtomicAlignment = 0;
}  // namespace rx

CLDeviceVk::~CLDeviceVk() = default;

CLDeviceImpl::Info CLDeviceVk::createInfo(cl::DeviceType type) const
{
    Info info(type);

    const VkPhysicalDeviceProperties &properties = mRenderer->getPhysicalDeviceProperties();

    info.maxWorkItemSizes.push_back(properties.limits.maxComputeWorkGroupSize[0]);
    info.maxWorkItemSizes.push_back(properties.limits.maxComputeWorkGroupSize[1]);
    info.maxWorkItemSizes.push_back(properties.limits.maxComputeWorkGroupSize[2]);

    info.maxMemAllocSize = mCaps.maxMemAllocSize;
    if (info.maxMemAllocSize == cl::kMaxAllocSentinel)
    {  // got sentinel - skips/removes device via CLDeviceImpl::Info::isValid check
        return info;
    }

    // TODO(aannestrand) Update these hardcoded platform/device queries
    // http://anglebug.com/42266935
    info.memBaseAddrAlign = 1024;

    info.imageSupport = CL_TRUE;

    info.image2D_MaxWidth  = properties.limits.maxImageDimension2D;
    info.image2D_MaxHeight = properties.limits.maxImageDimension2D;
    info.image3D_MaxWidth  = properties.limits.maxImageDimension3D;
    info.image3D_MaxHeight = properties.limits.maxImageDimension3D;
    info.image3D_MaxDepth  = properties.limits.maxImageDimension3D;
    // Max number of pixels for a 1D image created from a buffer object.
    info.imageMaxBufferSize = getImageMaxBufferSize();
    info.imageMaxArraySize  = properties.limits.maxImageArrayLayers;
    // The following are queried when image2d is created from buffer. We mimic its support for now
    // by doing a copy and as such dont have alignment requirements.
    info.imagePitchAlignment       = 1u;
    info.imageBaseAddressAlignment = 1u;

    info.execCapabilities     = mCaps.executionCapabilities;
    info.queueOnDeviceMaxSize = 0u;
    info.builtInKernels       = "";
    info.version              = CL_MAKE_VERSION(3, 0, 0);
    info.versionStr           = mCaps.version;
    info.OpenCL_C_AllVersions = {{CL_MAKE_VERSION(1, 0, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(1, 1, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(1, 2, 0), "OpenCL C"},
                                 {CL_MAKE_VERSION(3, 0, 0), "OpenCL C"}};

    info.OpenCL_C_Features         = {};
    info.ILsWithVersion            = {};
    info.builtInKernelsWithVersion = {};
    // Device partition is not supported for now
    info.partitionProperties = {CL_NONE};
    info.partitionType       = {CL_NONE};
    info.IL_Version          = "";

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
    // Check for fp16 and fp64 support.
    if (mRenderer->getFeatures().supportsClFp16.enabled)
    {
        versionedExtensionList.push_back(
            cl_name_version{.version = CL_MAKE_VERSION(1, 0, 0), .name = "cl_khr_fp16"});
    }
    if (mRenderer->getFeatures().supportsClFp64.enabled)
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
    uint32_t maxSize      = static_cast<uint32_t>(mCaps.maxWorkGroupSize);
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
