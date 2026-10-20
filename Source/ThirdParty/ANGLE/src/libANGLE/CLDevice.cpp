//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLDevice.cpp: Implements the cl::Device class.
//

#include "common/unsafe_buffers.h"

#include <angle_cl.h>

#include "libANGLE/CLBitField.h"
#include "libANGLE/CLDevice.h"
#include "libANGLE/CLPlatform.h"
#include "libANGLE/cl_types.h"
#include "libANGLE/cl_utils.h"

#include <cstring>
#include <string>

namespace cl
{

angle::Result Device::getInfo(DeviceInfo name,
                              size_t valueSize,
                              void *value,
                              size_t *valueSizeRet) const
{
    static_assert(std::is_same<cl_uint, cl_bool>::value &&
                      std::is_same<cl_uint, cl_device_mem_cache_type>::value &&
                      std::is_same<cl_uint, cl_device_local_mem_type>::value &&
                      std::is_same<cl_uint, cl_version>::value &&
                      std::is_same<cl_ulong, cl_device_type>::value &&
                      std::is_same<cl_ulong, cl_device_fp_config>::value &&
                      std::is_same<cl_ulong, cl_device_exec_capabilities>::value &&
                      std::is_same<cl_ulong, cl_command_queue_properties>::value &&
                      std::is_same<cl_ulong, cl_device_affinity_domain>::value &&
                      std::is_same<cl_ulong, cl_device_svm_capabilities>::value &&
                      std::is_same<cl_ulong, cl_device_atomic_capabilities>::value &&
                      std::is_same<cl_ulong, cl_device_device_enqueue_capabilities>::value,
                  "OpenCL type mismatch");

    cl_uint valUInt   = 0u;
    cl_ulong valULong = 0u;
    size_t valSizeT   = 0u;
    void *valPointer  = nullptr;
    std::vector<char> valString;

    const void *copyValue      = nullptr;
    size_t copySize            = 0u;
    const cl::DeviceCaps &caps = mImpl->getCaps();

    // The info names are sorted within their type group in the order they appear in the OpenCL
    // specification, so it is easier to compare them side-by-side when looking for changes.
    // https://www.khronos.org/registry/OpenCL/specs/3.0-unified/html/OpenCL_API.html#clGetDeviceInfo
    switch (name)
    {
        // Handle all cl_uint and aliased types
        case DeviceInfo::VendorID:
            copyValue = &caps.vendorID;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxComputeUnits:
            copyValue = &caps.maxComputeUnits;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredVectorWidthChar:
            copyValue = &caps.preferredVectorWidthChar;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredVectorWidthShort:
            copyValue = &caps.preferredVectorWidthShort;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredVectorWidthInt:
            copyValue = &caps.preferredVectorWidthInt;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredVectorWidthLong:
            copyValue = &caps.preferredVectorWidthLong;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredVectorWidthFloat:
            copyValue = &caps.preferredVectorWidthFloat;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredVectorWidthDouble:
            copyValue = &caps.preferredVectorWidthDouble;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredVectorWidthHalf:
            copyValue = &caps.preferredVectorWidthHalf;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::NativeVectorWidthChar:
            copyValue = &caps.nativeVectorWidthChar;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::NativeVectorWidthShort:
            copyValue = &caps.nativeVectorWidthShort;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::NativeVectorWidthInt:
            copyValue = &caps.nativeVectorWidthInt;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::NativeVectorWidthLong:
            copyValue = &caps.nativeVectorWidthLong;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::NativeVectorWidthFloat:
            copyValue = &caps.nativeVectorWidthFloat;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::NativeVectorWidthDouble:
            copyValue = &caps.nativeVectorWidthDouble;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::NativeVectorWidthHalf:
            copyValue = &caps.nativeVectorWidthHalf;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxClockFrequency:
            copyValue = &caps.maxClockFrequency;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::AddressBits:
            copyValue = &caps.addressBits;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxReadImageArgs:
            copyValue = &caps.maxReadImageArgs;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxWriteImageArgs:
            copyValue = &caps.maxWriteImageArgs;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxReadWriteImageArgs:
            copyValue = &caps.maxReadWriteImageArgs;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxSamplers:
            copyValue = &caps.maxSamplers;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxPipeArgs:
            copyValue = &caps.maxPipeArgs;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PipeMaxActiveReservations:
            copyValue = &caps.pipeMaxActiveReservations;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PipeMaxPacketSize:
            copyValue = &caps.pipeMaxPacketSize;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MinDataTypeAlignSize:
            copyValue = &caps.minDataTypeAlignSize;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::GlobalMemCacheType:
            copyValue = &caps.globalMemCacheType;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::GlobalMemCachelineSize:
            copyValue = &caps.globalMemCachelineSize;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxConstantArgs:
            copyValue = &caps.maxConstantArgs;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::LocalMemType:
            copyValue = &caps.localMemType;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::ErrorCorrectionSupport:
            copyValue = &caps.errorCorrectionSupport;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::HostUnifiedMemory:
            copyValue = &caps.hostUnifiedMemory;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::EndianLittle:
            copyValue = &caps.endianLittle;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::Available:
            copyValue = &caps.available;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::CompilerAvailable:
            copyValue = &caps.compilerAvailable;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::LinkerAvailable:
            copyValue = &caps.linkerAvailable;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::QueueOnDevicePreferredSize:
            copyValue = &caps.queueOnDevicePreferredSize;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxOnDeviceQueues:
            copyValue = &caps.maxOnDeviceQueues;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxOnDeviceEvents:
            copyValue = &caps.maxOnDeviceEvents;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredInteropUserSync:
            copyValue = &caps.preferredInteropUserSync;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PartitionMaxSubDevices:
            copyValue = &caps.partitionMaxSubDevices;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredPlatformAtomicAlignment:
            copyValue = &caps.preferredPlatformAtomicAlignment;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredGlobalAtomicAlignment:
            copyValue = &caps.preferredGlobalAtomicAlignment;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PreferredLocalAtomicAlignment:
            copyValue = &caps.preferredLocalAtomicAlignment;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxNumSubGroups:
            copyValue = &caps.maxNumSubGroups;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::SubGroupIndependentForwardProgress:
            copyValue = &caps.subGroupIndependentForwardProgress;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::NonUniformWorkGroupSupport:
            copyValue = &caps.nonUniformWorkGroupSupport;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::WorkGroupCollectiveFunctionsSupport:
            copyValue = &caps.workGroupCollectiveFunctionsSupport;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::GenericAddressSpaceSupport:
            copyValue = &caps.genericAddressSpaceSupport;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::PipeSupport:
            copyValue = &caps.pipeSupport;
            copySize  = sizeof(valUInt);
            break;

        // Handle all cl_ulong and aliased types
        case DeviceInfo::SingleFpConfig:
            copyValue = &caps.singleFpConfig;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::DoubleFpConfig:
            copyValue = &caps.doubleFpConfig;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::GlobalMemCacheSize:
            copyValue = &caps.globalMemCacheSize;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::GlobalMemSize:
            copyValue = &caps.globalMemSize;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::MaxConstantBufferSize:
            copyValue = &caps.maxConstantBufferSize;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::LocalMemSize:
            copyValue = &caps.localMemSize;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::QueueOnHostProperties:
            copyValue = &caps.queueOnHostProperties;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::QueueOnDeviceProperties:
            copyValue = &caps.queueOnDeviceProperties;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::PartitionAffinityDomain:
            copyValue = &caps.partitionAffinityDomain;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::SVM_Capabilities:
            copyValue = &caps.sVM_Capabilities;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::AtomicMemoryCapabilities:
            copyValue = &caps.atomicMemoryCapabilities;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::AtomicFenceCapabilities:
            copyValue = &caps.atomicFenceCapabilities;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::DeviceEnqueueCapabilities:
            copyValue = &caps.deviceEnqueueCapabilities;
            copySize  = sizeof(valULong);
            break;
        case DeviceInfo::HalfFpConfig:
            copyValue = &caps.halfFpConfig;
            copySize  = sizeof(valULong);
            break;

        // Handle all size_t and aliased types
        case DeviceInfo::MaxWorkGroupSize:
            copyValue = &caps.maxWorkGroupSize;
            copySize  = sizeof(valSizeT);
            break;
        case DeviceInfo::MaxParameterSize:
            copyValue = &caps.maxParameterSize;
            copySize  = sizeof(valSizeT);
            break;
        case DeviceInfo::MaxGlobalVariableSize:
            copyValue = &caps.maxGlobalVariableSize;
            copySize  = sizeof(valSizeT);
            break;
        case DeviceInfo::GlobalVariablePreferredTotalSize:
            copyValue = &caps.globalVariablePreferredTotalSize;
            copySize  = sizeof(valSizeT);
            break;
        case DeviceInfo::ProfilingTimerResolution:
            copyValue = &caps.profilingTimerResolution;
            copySize  = sizeof(valSizeT);
            break;
        case DeviceInfo::PrintfBufferSize:
            copyValue = &caps.printfBufferSize;
            copySize  = sizeof(valSizeT);
            break;
        case DeviceInfo::PreferredWorkGroupSizeMultiple:
            copyValue = &caps.preferredWorkGroupSizeMultiple;
            copySize  = sizeof(valSizeT);
            break;

        // Handle all string types
        case DeviceInfo::Name:
        {
            const std::string &nameStr = caps.name;
            copyValue                  = nameStr.c_str();
            copySize                   = nameStr.size() + 1;
            break;
        }
        case DeviceInfo::Vendor:
        {
            const std::string &vendorStr = caps.vendor;
            copyValue                    = vendorStr.c_str();
            copySize                     = vendorStr.size() + 1;
            break;
        }
        case DeviceInfo::DriverVersion:
        {
            const std::string &driverVersionStr = caps.driverVersion;
            copyValue                           = driverVersionStr.c_str();
            copySize                            = driverVersionStr.size() + 1;
            break;
        }
        case DeviceInfo::Profile:
        {
            const std::string &profileStr = caps.profile;
            copyValue                     = profileStr.c_str();
            copySize                      = profileStr.size() + 1;
            break;
        }
        case DeviceInfo::OpenCL_C_Version:
        {
            const std::string &openCL_C_VersionStr = caps.openCL_C_Version;
            copyValue                              = openCL_C_VersionStr.c_str();
            copySize                               = openCL_C_VersionStr.size() + 1;
            break;
        }
        case DeviceInfo::LatestConformanceVersionPassed:
        {
            const std::string &latestConformanceVersionPassedStr =
                caps.latestConformanceVersionPassed;
            copyValue = latestConformanceVersionPassedStr.c_str();
            copySize  = latestConformanceVersionPassedStr.size() + 1;
            break;
        }
        case DeviceInfo::ExternalMemoryImportHandleTypes:
            copyValue = mInfo.externalMemoryHandleSupportList.data();
            copySize  = mInfo.externalMemoryHandleSupportList.size() *
                        sizeof(*mInfo.externalMemoryHandleSupportList.data());
            break;
        case DeviceInfo::ExternalMemoryLinearImagesHandleTypes:
            copyValue = mInfo.externalMemoryLinearImagesHandleSupportList.data();
            copySize  = mInfo.externalMemoryLinearImagesHandleSupportList.size() *
                        sizeof(cl_external_memory_handle_type_khr);
            break;
        // Handle all cached values
        case DeviceInfo::Type:
            copyValue = &mInfo.type;
            copySize  = sizeof(mInfo.type);
            break;
        case DeviceInfo::MaxWorkItemDimensions:
            valUInt   = static_cast<cl_uint>(mInfo.maxWorkItemSizes.size());
            copyValue = &valUInt;
            copySize  = sizeof(valUInt);
            break;
        case DeviceInfo::MaxWorkItemSizes:
            copyValue = mInfo.maxWorkItemSizes.data();
            copySize  = mInfo.maxWorkItemSizes.size() *
                        sizeof(decltype(mInfo.maxWorkItemSizes)::value_type);
            break;
        case DeviceInfo::MaxMemAllocSize:
            copyValue = &mInfo.maxMemAllocSize;
            copySize  = sizeof(mInfo.maxMemAllocSize);
            break;
        case DeviceInfo::ImageSupport:
            copyValue = &mInfo.imageSupport;
            copySize  = sizeof(mInfo.imageSupport);
            break;
        case DeviceInfo::IL_Version:
            copyValue = mInfo.IL_Version.c_str();
            copySize  = mInfo.IL_Version.length() + 1u;
            break;
        case DeviceInfo::ILsWithVersion:
            copyValue = mInfo.ILsWithVersion.data();
            copySize =
                mInfo.ILsWithVersion.size() * sizeof(decltype(mInfo.ILsWithVersion)::value_type);
            break;
        case DeviceInfo::Image2D_MaxWidth:
            copyValue = &mInfo.image2D_MaxWidth;
            copySize  = sizeof(mInfo.image2D_MaxWidth);
            break;
        case DeviceInfo::Image2D_MaxHeight:
            copyValue = &mInfo.image2D_MaxHeight;
            copySize  = sizeof(mInfo.image2D_MaxHeight);
            break;
        case DeviceInfo::Image3D_MaxWidth:
            copyValue = &mInfo.image3D_MaxWidth;
            copySize  = sizeof(mInfo.image3D_MaxWidth);
            break;
        case DeviceInfo::Image3D_MaxHeight:
            copyValue = &mInfo.image3D_MaxHeight;
            copySize  = sizeof(mInfo.image3D_MaxHeight);
            break;
        case DeviceInfo::Image3D_MaxDepth:
            copyValue = &mInfo.image3D_MaxDepth;
            copySize  = sizeof(mInfo.image3D_MaxDepth);
            break;
        case DeviceInfo::ImageMaxBufferSize:
            copyValue = &mInfo.imageMaxBufferSize;
            copySize  = sizeof(mInfo.imageMaxBufferSize);
            break;
        case DeviceInfo::ImageMaxArraySize:
            copyValue = &mInfo.imageMaxArraySize;
            copySize  = sizeof(mInfo.imageMaxArraySize);
            break;
        case DeviceInfo::ImagePitchAlignment:
            copyValue = &mInfo.imagePitchAlignment;
            copySize  = sizeof(mInfo.imagePitchAlignment);
            break;
        case DeviceInfo::ImageBaseAddressAlignment:
            copyValue = &mInfo.imageBaseAddressAlignment;
            copySize  = sizeof(mInfo.imageBaseAddressAlignment);
            break;
        case DeviceInfo::MemBaseAddrAlign:
            copyValue = &mInfo.memBaseAddrAlign;
            copySize  = sizeof(mInfo.memBaseAddrAlign);
            break;
        case DeviceInfo::ExecutionCapabilities:
            copyValue = &mInfo.execCapabilities;
            copySize  = sizeof(mInfo.execCapabilities);
            break;
        case DeviceInfo::QueueOnDeviceMaxSize:
            copyValue = &mInfo.queueOnDeviceMaxSize;
            copySize  = sizeof(mInfo.queueOnDeviceMaxSize);
            break;
        case DeviceInfo::BuiltInKernels:
            copyValue = mInfo.builtInKernels.c_str();
            copySize  = mInfo.builtInKernels.length() + 1u;
            break;
        case DeviceInfo::BuiltInKernelsWithVersion:
            copyValue = mInfo.builtInKernelsWithVersion.data();
            copySize  = mInfo.builtInKernelsWithVersion.size() *
                        sizeof(decltype(mInfo.builtInKernelsWithVersion)::value_type);
            break;
        case DeviceInfo::Version:
            copyValue = mInfo.versionStr.c_str();
            copySize  = mInfo.versionStr.length() + 1u;
            break;
        case DeviceInfo::NumericVersion:
            copyValue = &mInfo.version;
            copySize  = sizeof(mInfo.version);
            break;
        case DeviceInfo::OpenCL_C_AllVersions:
            copyValue = mInfo.OpenCL_C_AllVersions.data();
            copySize  = mInfo.OpenCL_C_AllVersions.size() *
                        sizeof(decltype(mInfo.OpenCL_C_AllVersions)::value_type);
            break;
        case DeviceInfo::OpenCL_C_Features:
            copyValue = mInfo.OpenCL_C_Features.data();
            copySize  = mInfo.OpenCL_C_Features.size() *
                        sizeof(decltype(mInfo.OpenCL_C_Features)::value_type);
            break;
        case DeviceInfo::Extensions:
            copyValue = mInfo.extensions.c_str();
            copySize  = mInfo.extensions.length() + 1u;
            break;
        case DeviceInfo::ExtensionsWithVersion:
            copyValue = mInfo.extensionsWithVersion.data();
            copySize  = mInfo.extensionsWithVersion.size() *
                        sizeof(decltype(mInfo.extensionsWithVersion)::value_type);
            break;
        case DeviceInfo::PartitionProperties:
            copyValue = mInfo.partitionProperties.data();
            copySize  = mInfo.partitionProperties.size() *
                        sizeof(decltype(mInfo.partitionProperties)::value_type);
            break;
        case DeviceInfo::PartitionType:
            copyValue = mInfo.partitionType.data();
            copySize =
                mInfo.partitionType.size() * sizeof(decltype(mInfo.partitionType)::value_type);
            break;
        case DeviceInfo::IntegerDotProductCapabilities:
            copyValue = &mInfo.integerDotProductCapabilities;
            copySize  = sizeof(mInfo.integerDotProductCapabilities);
            break;
        case DeviceInfo::IntegerDotProductAccelerationProperties8bit:
            copyValue = &mInfo.integerDotProductAccelerationProperties8Bit;
            copySize  = sizeof(mInfo.integerDotProductAccelerationProperties8Bit);
            break;

        case DeviceInfo::IntegerDotProductAccelerationProperties4x8bitPacked:
            copyValue = &mInfo.integerDotProductAccelerationProperties4x8BitPacked;
            copySize  = sizeof(mInfo.integerDotProductAccelerationProperties4x8BitPacked);
            break;

        // Handle all mapped values
        case DeviceInfo::Platform:
            valPointer = mPlatform.getNative();
            copyValue  = &valPointer;
            copySize   = sizeof(valPointer);
            break;
        case DeviceInfo::ParentDevice:
            valPointer = Device::CastNative(mParent.get());
            copyValue  = &valPointer;
            copySize   = sizeof(valPointer);
            break;
        case DeviceInfo::ReferenceCount:
            valUInt   = isRoot() ? 1u : getRefCount();
            copyValue = &valUInt;
            copySize  = sizeof(valUInt);
            break;

        default:
            ASSERT(false);
            ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
    }

    if (value != nullptr)
    {
        // CL_INVALID_VALUE if size in bytes specified by param_value_size is < size of return
        // type as specified in the Device Queries table and param_value is not a NULL value
        if (valueSize < copySize)
        {
            ANGLE_CL_RETURN_ERROR(CL_INVALID_VALUE);
        }
        if (copyValue != nullptr)
        {
            ANGLE_UNSAFE_TODO(std::memcpy(value, copyValue, copySize));
        }
    }
    if (valueSizeRet != nullptr)
    {
        *valueSizeRet = copySize;
    }
    return angle::Result::Continue;
}

angle::Result Device::createSubDevices(const cl_device_partition_property *properties,
                                       cl_uint numDevices,
                                       cl_device_id *subDevices,
                                       cl_uint *numDevicesRet)
{
    if (subDevices == nullptr)
    {
        numDevices = 0u;
    }
    rx::CLDeviceImpl::CreateFuncs subDeviceCreateFuncs;
    ANGLE_TRY(mImpl->createSubDevices(properties, numDevices, subDeviceCreateFuncs, numDevicesRet));
    cl::DeviceType type = mInfo.type;
    type.clear(CL_DEVICE_TYPE_DEFAULT);
    DevicePtrs devices;
    devices.reserve(subDeviceCreateFuncs.size());
    while (!subDeviceCreateFuncs.empty())
    {
        devices.emplace_back(
            DevicePtr::Create(mPlatform, this, type, subDeviceCreateFuncs.front()));

        if (!devices.back()->mInfo.isValid())
        {
            return angle::Result::Stop;
        }
        subDeviceCreateFuncs.pop_front();
    }
    for (DevicePtr &subDevice : devices)
    {
        *ANGLE_UNSAFE_TODO(subDevices++) = subDevice.release();
    }
    return angle::Result::Continue;
}

Device::~Device() = default;

bool Device::supportsBuiltInKernel(const std::string &name) const
{
    return angle::ContainsToken(mInfo.builtInKernels, ';', name);
}

bool Device::supportsNativeImageDimensions(const cl_image_desc &desc) const
{
    switch (FromCLenum<MemObjectType>(desc.image_type))
    {
        case MemObjectType::Image1D:
            return desc.image_width <= mInfo.image2D_MaxWidth;
        case MemObjectType::Image2D:
            return desc.image_width <= mInfo.image2D_MaxWidth &&
                   desc.image_height <= mInfo.image2D_MaxHeight;
        case MemObjectType::Image3D:
            return desc.image_width <= mInfo.image3D_MaxWidth &&
                   desc.image_height <= mInfo.image3D_MaxHeight &&
                   desc.image_depth <= mInfo.image3D_MaxDepth;
        case MemObjectType::Image1D_Array:
            return desc.image_width <= mInfo.image2D_MaxWidth &&
                   desc.image_array_size <= mInfo.imageMaxArraySize;
        case MemObjectType::Image2D_Array:
            return desc.image_width <= mInfo.image2D_MaxWidth &&
                   desc.image_height <= mInfo.image2D_MaxHeight &&
                   desc.image_array_size <= mInfo.imageMaxArraySize;
        case MemObjectType::Image1D_Buffer:
            return desc.image_width <= mInfo.imageMaxBufferSize;
        default:
            ASSERT(false);
            break;
    }
    return false;
}

bool Device::supportsImageDimensions(const ImageDescriptor &desc) const
{
    switch (desc.type)
    {
        case MemObjectType::Image1D:
            return desc.width <= mInfo.image2D_MaxWidth;
        case MemObjectType::Image2D:
            return desc.width <= mInfo.image2D_MaxWidth && desc.height <= mInfo.image2D_MaxHeight;
        case MemObjectType::Image3D:
            return desc.width <= mInfo.image3D_MaxWidth && desc.height <= mInfo.image3D_MaxHeight &&
                   desc.depth <= mInfo.image3D_MaxDepth;
        case MemObjectType::Image1D_Array:
            return desc.width <= mInfo.image2D_MaxWidth &&
                   desc.arraySize <= mInfo.imageMaxArraySize;
        case MemObjectType::Image2D_Array:
            return desc.width <= mInfo.image2D_MaxWidth && desc.height <= mInfo.image2D_MaxHeight &&
                   desc.arraySize <= mInfo.imageMaxArraySize;
        case MemObjectType::Image1D_Buffer:
            return desc.width <= mInfo.imageMaxBufferSize;
        default:
            ASSERT(false);
            break;
    }
    return false;
}

bool Device::hasDeviceEnqueueCaps() const
{
    return mInfo.queueOnDeviceMaxSize > 0;
}

bool Device::supportsNonUniformWorkGroups() const
{
    if (getPlatform().isVersionOrNewer(3, 0))
    {
        return mImpl->getCaps().nonUniformWorkGroupSupport;
    }
    // Check older platforms support via device extension
    return getInfo().armNonUniformWorkGroupSize;
}

Device::Device(Platform &platform,
               Device *parent,
               DeviceType type,
               const rx::CLDeviceImpl::CreateFunc &createFunc)
    : mPlatform(platform), mParent(parent), mImpl(createFunc(*this)), mInfo(mImpl->createInfo(type))
{}

}  // namespace cl
