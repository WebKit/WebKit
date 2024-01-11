//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLPlatformVk.cpp: Implements the class methods for CLPlatformVk.

#include "libANGLE/renderer/vulkan/CLPlatformVk.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"

#include "libANGLE/CLPlatform.h"
#include "libANGLE/cl_utils.h"

#include "anglebase/no_destructor.h"
#include "common/angle_version_info.h"

namespace rx
{

namespace
{

std::string CreateExtensionString(const NameVersionVector &extList)
{
    std::string extensions;
    for (const cl_name_version &ext : extList)
    {
        extensions += ext.name;
        extensions += ' ';
    }
    if (!extensions.empty())
    {
        extensions.pop_back();
    }
    return extensions;
}

}  // namespace

CLPlatformVk::~CLPlatformVk() = default;

CLPlatformImpl::Info CLPlatformVk::createInfo() const
{
    NameVersionVector extList = {
        cl_name_version{CL_MAKE_VERSION(1, 0, 0), "cl_khr_icd"},
        cl_name_version{CL_MAKE_VERSION(1, 0, 0), "cl_khr_extended_versioning"}};

    Info info;
    info.initializeExtensions(CreateExtensionString(extList));
    info.profile.assign("FULL_PROFILE");
    info.versionStr.assign(GetVersionString());
    info.version = GetVersion();
    info.name.assign("ANGLE Vulkan");
    info.extensionsWithVersion = std::move(extList);
    info.hostTimerRes          = 0u;
    return info;
}

CLDeviceImpl::CreateDatas CLPlatformVk::createDevices() const
{
    cl::DeviceType type;  // TODO(jplate) Fetch device type from Vulkan
    CLDeviceImpl::CreateDatas createDatas;
    createDatas.emplace_back(
        type, [](const cl::Device &device) { return CLDeviceVk::Ptr(new CLDeviceVk(device)); });
    return createDatas;
}

angle::Result CLPlatformVk::createContext(cl::Context &context,
                                          const cl::DevicePtrs &devices,
                                          bool userSync,
                                          CLContextImpl::Ptr *contextOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLPlatformVk::createContextFromType(cl::Context &context,
                                                  cl::DeviceType deviceType,
                                                  bool userSync,
                                                  CLContextImpl::Ptr *contextOut)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLPlatformVk::unloadCompiler()
{
    return angle::Result::Continue;
}

void CLPlatformVk::Initialize(CreateFuncs &createFuncs)
{
    createFuncs.emplace_back(
        [](const cl::Platform &platform) { return Ptr(new CLPlatformVk(platform)); });
}

const std::string &CLPlatformVk::GetVersionString()
{
    static const angle::base::NoDestructor<const std::string> sVersion(
        "OpenCL " + std::to_string(CL_VERSION_MAJOR(GetVersion())) + "." +
        std::to_string(CL_VERSION_MINOR(GetVersion())) + " ANGLE " +
        angle::GetANGLEVersionString());
    return *sVersion;
}

CLPlatformVk::CLPlatformVk(const cl::Platform &platform) : CLPlatformImpl(platform) {}

}  // namespace rx
