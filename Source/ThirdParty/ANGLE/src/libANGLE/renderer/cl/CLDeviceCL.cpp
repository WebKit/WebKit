//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLDeviceCL.cpp: Implements the class methods for CLDeviceCL.

#include "libANGLE/renderer/cl/CLDeviceCL.h"

#include "common/string_utils.h"
#include "libANGLE/CLCaps.h"
#include "libANGLE/renderer/cl/cl_util.h"

#include "libANGLE/CLDevice.h"
#include "libANGLE/cl_utils.h"

namespace
{

bool HasExtension(const std::string &extensions, const std::string &extension)
{
    return angle::ContainsToken(extensions, ' ', extension);
}

}  // namespace

namespace rx
{

CLDeviceCL::~CLDeviceCL()
{
    if (!mDevice.isRoot() && mNative->getDispatch().clReleaseDevice(mNative) != CL_SUCCESS)
    {
        ERR() << "Error while releasing CL device";
    }
}

CLDeviceImpl::Info CLDeviceCL::createInfo(cl::DeviceType type) const
{
    Info info(type);
    std::vector<char> valString;

    if (!GetDeviceInfo(cl::DeviceInfo::MaxWorkItemSizes, info.maxWorkItemSizes))
    {
        return Info{};
    }
    // From the OpenCL specification for info name CL_DEVICE_MAX_WORK_ITEM_SIZES:
    // "The minimum value is (1, 1, 1) for devices that are not of type CL_DEVICE_TYPE_CUSTOM."
    // https://www.khronos.org/registry/OpenCL/specs/3.0-unified/html/OpenCL_API.html#clGetDeviceInfo
    // Custom devices are currently not supported by this back end.
    if (info.maxWorkItemSizes.size() < 3u || info.maxWorkItemSizes[0] == 0u ||
        info.maxWorkItemSizes[1] == 0u || info.maxWorkItemSizes[2] == 0u)
    {
        ERR() << "Invalid CL_DEVICE_MAX_WORK_ITEM_SIZES";
        return Info{};
    }

    if (!GetDeviceInfo(cl::DeviceInfo::MaxMemAllocSize, info.maxMemAllocSize) ||
        !GetDeviceInfo(cl::DeviceInfo::ImageSupport, info.imageSupport) ||
        !GetDeviceInfo(cl::DeviceInfo::Image2D_MaxWidth, info.image2D_MaxWidth) ||
        !GetDeviceInfo(cl::DeviceInfo::Image2D_MaxHeight, info.image2D_MaxHeight) ||
        !GetDeviceInfo(cl::DeviceInfo::Image3D_MaxWidth, info.image3D_MaxWidth) ||
        !GetDeviceInfo(cl::DeviceInfo::Image3D_MaxHeight, info.image3D_MaxHeight) ||
        !GetDeviceInfo(cl::DeviceInfo::Image3D_MaxDepth, info.image3D_MaxDepth) ||
        !GetDeviceInfo(cl::DeviceInfo::MemBaseAddrAlign, info.memBaseAddrAlign) ||
        !GetDeviceInfo(cl::DeviceInfo::ExecutionCapabilities, info.execCapabilities))
    {
        return Info{};
    }

    if (!GetDeviceInfo(cl::DeviceInfo::Version, valString))
    {
        return Info{};
    }
    info.versionStr.assign(valString.data());

    if (!GetDeviceInfo(cl::DeviceInfo::Extensions, valString))
    {
        return Info{};
    }
    std::string extensionStr(valString.data());

    // TODO(jplate) Remove workaround after bug is fixed http://anglebug.com/42264583
    if (info.versionStr.compare(0u, 15u, "OpenCL 3.0 CUDA", 15u) == 0)
    {
        extensionStr.append(" cl_khr_depth_images cl_khr_image2d_from_buffer");
    }

    info.version = ExtractCLVersion(info.versionStr);
    if (info.version == 0u)
    {
        return Info{};
    }

    RemoveUnsupportedCLExtensions(extensionStr);
    info.initializeExtensions(std::move(extensionStr));

    if (info.version >= CL_MAKE_VERSION(1, 2, 0))
    {
        if (!GetDeviceInfo(cl::DeviceInfo::ImageMaxBufferSize, info.imageMaxBufferSize) ||
            !GetDeviceInfo(cl::DeviceInfo::ImageMaxArraySize, info.imageMaxArraySize) ||
            !GetDeviceInfo(cl::DeviceInfo::BuiltInKernels, valString))
        {
            return Info{};
        }
        info.builtInKernels.assign(valString.data());
        if (!GetDeviceInfo(cl::DeviceInfo::PartitionProperties, info.partitionProperties) ||
            !GetDeviceInfo(cl::DeviceInfo::PartitionType, info.partitionType))
        {
            return Info{};
        }
    }

    if (info.version >= CL_MAKE_VERSION(2, 0, 0) &&
        (!GetDeviceInfo(cl::DeviceInfo::ImagePitchAlignment, info.imagePitchAlignment) ||
         !GetDeviceInfo(cl::DeviceInfo::ImageBaseAddressAlignment,
                        info.imageBaseAddressAlignment) ||
         !GetDeviceInfo(cl::DeviceInfo::QueueOnDeviceMaxSize, info.queueOnDeviceMaxSize)))
    {
        return Info{};
    }

    if (info.version >= CL_MAKE_VERSION(2, 1, 0))
    {
        if (!GetDeviceInfo(cl::DeviceInfo::IL_Version, valString))
        {
            return Info{};
        }
        info.IL_Version.assign(valString.data());
    }

    if (info.version >= CL_MAKE_VERSION(3, 0, 0) &&
        (!GetDeviceInfo(cl::DeviceInfo::ILsWithVersion, info.ILsWithVersion) ||
         !GetDeviceInfo(cl::DeviceInfo::BuiltInKernelsWithVersion,
                        info.builtInKernelsWithVersion) ||
         !GetDeviceInfo(cl::DeviceInfo::OpenCL_C_AllVersions, info.OpenCL_C_AllVersions) ||
         !GetDeviceInfo(cl::DeviceInfo::OpenCL_C_Features, info.OpenCL_C_Features) ||
         !GetDeviceInfo(cl::DeviceInfo::ExtensionsWithVersion, info.extensionsWithVersion)))
    {
        return Info{};
    }
    RemoveUnsupportedCLExtensions(info.extensionsWithVersion);

    return info;
}

bool CLDeviceCL::getInfoString(cl::DeviceInfo name, std::string *value) const
{
    std::vector<char> buf;
    if (!GetDeviceInfo<char>(name, buf))
    {
        return false;
    }

    value->assign(buf.data());

    return true;
}

angle::Result CLDeviceCL::createSubDevices(const cl_device_partition_property *properties,
                                           cl_uint numDevices,
                                           CreateFuncs &createFuncs,
                                           cl_uint *numDevicesRet)
{
    if (numDevices == 0u)
    {
        ANGLE_CL_TRY(mNative->getDispatch().clCreateSubDevices(mNative, properties, 0u, nullptr,
                                                               numDevicesRet));
        return angle::Result::Continue;
    }

    std::vector<cl_device_id> nativeSubDevices(numDevices, nullptr);
    ANGLE_CL_TRY(mNative->getDispatch().clCreateSubDevices(mNative, properties, numDevices,
                                                           nativeSubDevices.data(), nullptr));

    for (cl_device_id nativeSubDevice : nativeSubDevices)
    {
        createFuncs.emplace_back([nativeSubDevice](const cl::Device &device) {
            return Ptr(new CLDeviceCL(device, nativeSubDevice));
        });
    }
    return angle::Result::Continue;
}

CLDeviceCL::CLDeviceCL(const cl::Device &device, cl_device_id native)
    : CLDeviceImpl(device), mNative(native)
{
    bool getInfoPassed = true;

    // Querying an info name that is missing before the device's OpenCL version, or that belongs to
    // an extension the device does not support, fails with CL_INVALID_VALUE. So gate the queries
    // below on the device version and extensions, matching table 5 of the OpenCL 3.0 spec.
    // https://registry.khronos.org/OpenCL/specs/3.0-unified/html/OpenCL_API.html#clGetDeviceInfo
    std::string extensions;
    getInfoPassed &= getInfoString(cl::DeviceInfo::Version, &mCaps.version);
    getInfoPassed &= getInfoString(cl::DeviceInfo::Extensions, &extensions);
    const cl_version version = ExtractCLVersion(mCaps.version);

    // Populate the caps supported by all versions.
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::VendorID, mCaps.vendorID);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxComputeUnits, mCaps.maxComputeUnits);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::MaxWorkItemDimensions, mCaps.maxWorkItemDimensions);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::PreferredVectorWidthChar, mCaps.preferredVectorWidthChar);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::PreferredVectorWidthShort, mCaps.preferredVectorWidthShort);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::PreferredVectorWidthInt, mCaps.preferredVectorWidthInt);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::PreferredVectorWidthLong, mCaps.preferredVectorWidthLong);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::PreferredVectorWidthFloat, mCaps.preferredVectorWidthFloat);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::PreferredVectorWidthDouble, mCaps.preferredVectorWidthDouble);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxClockFrequency, mCaps.maxClockFrequency);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::AddressBits, mCaps.addressBits);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxReadImageArgs, mCaps.maxReadImageArgs);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxWriteImageArgs, mCaps.maxWriteImageArgs);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxSamplers, mCaps.maxSamplers);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::MinDataTypeAlignSize, mCaps.minDataTypeAlignSize);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::GlobalMemCacheType, mCaps.globalMemCacheType);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::GlobalMemCachelineSize, mCaps.globalMemCachelineSize);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxConstantArgs, mCaps.maxConstantArgs);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::LocalMemType, mCaps.localMemType);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::ErrorCorrectionSupport, mCaps.errorCorrectionSupport);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::EndianLittle, mCaps.endianLittle);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::Available, mCaps.available);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::CompilerAvailable, mCaps.compilerAvailable);

    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::SingleFpConfig, mCaps.singleFpConfig);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::GlobalMemCacheSize, mCaps.globalMemCacheSize);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::GlobalMemSize, mCaps.globalMemSize);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::MaxConstantBufferSize, mCaps.maxConstantBufferSize);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::LocalMemSize, mCaps.localMemSize);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::ExecutionCapabilities, mCaps.executionCapabilities);
    // CL_DEVICE_QUEUE_ON_HOST_PROPERTIES shares its value with CL_DEVICE_QUEUE_PROPERTIES.
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::QueueOnHostProperties, mCaps.queueOnHostProperties);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxMemAllocSize, mCaps.maxMemAllocSize);

    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxWorkGroupSize, mCaps.maxWorkGroupSize);
    getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxParameterSize, mCaps.maxParameterSize);
    getInfoPassed &=
        GetDeviceInfo(cl::DeviceInfo::ProfilingTimerResolution, mCaps.profilingTimerResolution);

    getInfoPassed &= getInfoString(cl::DeviceInfo::Name, &mCaps.name);
    getInfoPassed &= getInfoString(cl::DeviceInfo::Vendor, &mCaps.vendor);
    getInfoPassed &= getInfoString(cl::DeviceInfo::DriverVersion, &mCaps.driverVersion);
    getInfoPassed &= getInfoString(cl::DeviceInfo::Profile, &mCaps.profile);

    // Populate the caps added in OpenCL 1.1.
    if (version >= CL_MAKE_VERSION(1, 1, 0))
    {
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::PreferredVectorWidthHalf, mCaps.preferredVectorWidthHalf);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::NativeVectorWidthChar, mCaps.nativeVectorWidthChar);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::NativeVectorWidthShort, mCaps.nativeVectorWidthShort);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::NativeVectorWidthInt, mCaps.nativeVectorWidthInt);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::NativeVectorWidthLong, mCaps.nativeVectorWidthLong);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::NativeVectorWidthFloat, mCaps.nativeVectorWidthFloat);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::NativeVectorWidthDouble, mCaps.nativeVectorWidthDouble);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::NativeVectorWidthHalf, mCaps.nativeVectorWidthHalf);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::HostUnifiedMemory, mCaps.hostUnifiedMemory);
        getInfoPassed &= getInfoString(cl::DeviceInfo::OpenCL_C_Version, &mCaps.openCL_C_Version);
    }

    // Populate the caps added in OpenCL 1.2.
    if (version >= CL_MAKE_VERSION(1, 2, 0))
    {
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::LinkerAvailable, mCaps.linkerAvailable);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::PreferredInteropUserSync, mCaps.preferredInteropUserSync);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::PartitionMaxSubDevices, mCaps.partitionMaxSubDevices);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::PartitionAffinityDomain, mCaps.partitionAffinityDomain);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::PrintfBufferSize, mCaps.printfBufferSize);
    }

    // Populate the caps added in OpenCL 2.0.
    if (version >= CL_MAKE_VERSION(2, 0, 0))
    {
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::MaxReadWriteImageArgs, mCaps.maxReadWriteImageArgs);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxPipeArgs, mCaps.maxPipeArgs);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::PipeMaxActiveReservations,
                                       mCaps.pipeMaxActiveReservations);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::PipeMaxPacketSize, mCaps.pipeMaxPacketSize);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::QueueOnDevicePreferredSize,
                                       mCaps.queueOnDevicePreferredSize);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::QueueOnDeviceMaxSize, mCaps.queueOnDeviceMaxSize);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxOnDeviceQueues, mCaps.maxOnDeviceQueues);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxOnDeviceEvents, mCaps.maxOnDeviceEvents);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::PreferredPlatformAtomicAlignment,
                                       mCaps.preferredPlatformAtomicAlignment);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::PreferredGlobalAtomicAlignment,
                                       mCaps.preferredGlobalAtomicAlignment);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::PreferredLocalAtomicAlignment,
                                       mCaps.preferredLocalAtomicAlignment);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::QueueOnDeviceProperties, mCaps.queueOnDeviceProperties);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::SVM_Capabilities, mCaps.sVM_Capabilities);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::MaxGlobalVariableSize, mCaps.maxGlobalVariableSize);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::GlobalVariablePreferredTotalSize,
                                       mCaps.globalVariablePreferredTotalSize);
    }

    // Populate the caps added in OpenCL 2.1.
    if (version >= CL_MAKE_VERSION(2, 1, 0))
    {
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::MaxNumSubGroups, mCaps.maxNumSubGroups);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::SubGroupIndependentForwardProgress,
                                       mCaps.subGroupIndependentForwardProgress);
    }

    // Populate the caps added in OpenCL 3.0.
    if (version >= CL_MAKE_VERSION(3, 0, 0))
    {
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::NonUniformWorkGroupSupport,
                                       mCaps.nonUniformWorkGroupSupport);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::WorkGroupCollectiveFunctionsSupport,
                                       mCaps.workGroupCollectiveFunctionsSupport);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::GenericAddressSpaceSupport,
                                       mCaps.genericAddressSpaceSupport);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::PipeSupport, mCaps.pipeSupport);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::AtomicMemoryCapabilities, mCaps.atomicMemoryCapabilities);
        getInfoPassed &=
            GetDeviceInfo(cl::DeviceInfo::AtomicFenceCapabilities, mCaps.atomicFenceCapabilities);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::DeviceEnqueueCapabilities,
                                       mCaps.deviceEnqueueCapabilities);
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::PreferredWorkGroupSizeMultiple,
                                       mCaps.preferredWorkGroupSizeMultiple);
        getInfoPassed &= getInfoString(cl::DeviceInfo::LatestConformanceVersionPassed,
                                       &mCaps.latestConformanceVersionPassed);
    }

    // Populate the caps that depend on extensions.
    // CL_DEVICE_DOUBLE_FP_CONFIG was part of cl_khr_fp64 before becoming core in OpenCL 1.2.
    if (version >= CL_MAKE_VERSION(1, 2, 0) || HasExtension(extensions, "cl_khr_fp64"))
    {
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::DoubleFpConfig, mCaps.doubleFpConfig);
    }
    if (HasExtension(extensions, "cl_khr_fp16"))
    {
        getInfoPassed &= GetDeviceInfo(cl::DeviceInfo::HalfFpConfig, mCaps.halfFpConfig);
    }

    if (!getInfoPassed)
    {
        WARN() << "failure(s) detected for some getInfo queries!";
    }
}

}  // namespace rx
