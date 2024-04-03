//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLPlatformVk.cpp: Implements the class methods for CLPlatformVk.

#include "libANGLE/renderer/vulkan/CLPlatformVk.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

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

angle::Result InitBackendRenderer(egl::Display *display)
{
    // Initialize the backend vk::Renderer by initializing a dummy/default EGL display object
    // TODO(aannestrand) Implement display-less vk::Renderer init
    // http://anglebug.com/8515
    // TODO(aannestrand) Add CL and EGL context testing
    // http://anglebug.com/8514
    if (display == nullptr || IsError(display->initialize()))
    {
        ERR() << "Failed to init renderer!";
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_HOST_MEMORY);
    }
    return angle::Result::Continue;
}

}  // namespace

CLPlatformVk::~CLPlatformVk()
{
    mDisplay->getImplementation()->terminate();
}

CLPlatformImpl::Info CLPlatformVk::createInfo() const
{
    NameVersionVector extList = {
        cl_name_version{CL_MAKE_VERSION(3, 0, 0), "cl_khr_icd"},
        cl_name_version{CL_MAKE_VERSION(3, 0, 0), "cl_khr_extended_versioning"}};

    Info info;
    info.name.assign("ANGLE Vulkan");
    info.profile.assign("FULL_PROFILE");
    info.versionStr.assign(GetVersionString());
    info.hostTimerRes          = 0u;
    info.extensionsWithVersion = std::move(extList);
    info.version               = GetVersion();
    info.initializeExtensions(CreateExtensionString(extList));
    return info;
}

CLDeviceImpl::CreateDatas CLPlatformVk::createDevices() const
{
    ASSERT(mDisplay);
    ASSERT(mDisplay->isInitialized());

    CLDeviceImpl::CreateDatas createDatas;

    // Convert Vk device type to CL equivalent
    cl_device_type type = CL_DEVICE_TYPE_DEFAULT;
    switch (GetImplAs<DisplayVk>(mDisplay)->getRenderer()->getPhysicalDeviceProperties().deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            type |= CL_DEVICE_TYPE_CPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            type |= CL_DEVICE_TYPE_GPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
            // The default OpenCL device must not be a CL_DEVICE_TYPE_CUSTOM device.
            // Thus, we override type bitfield to custom only.
            type = CL_DEVICE_TYPE_CUSTOM;
            break;
    }

    createDatas.emplace_back(type, [this](const cl::Device &device) {
        return CLDeviceVk::Ptr(
            new CLDeviceVk(device, GetImplAs<DisplayVk>(mDisplay)->getRenderer()));
    });
    return createDatas;
}

angle::Result CLPlatformVk::createContext(cl::Context &context,
                                          const cl::DevicePtrs &devices,
                                          bool userSync,
                                          CLContextImpl::Ptr *contextOut)
{
    *contextOut = CLContextImpl::Ptr(new (std::nothrow) CLContextVk(context, mDisplay, devices));
    if (*contextOut == nullptr)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_HOST_MEMORY);
    }
    return angle::Result::Continue;
}

angle::Result CLPlatformVk::createContextFromType(cl::Context &context,
                                                  cl::DeviceType deviceType,
                                                  bool userSync,
                                                  CLContextImpl::Ptr *contextOut)
{
    ASSERT(mDisplay);
    ASSERT(mDisplay->isInitialized());

    const VkPhysicalDeviceType &vkPhysicalDeviceType =
        GetImplAs<DisplayVk>(mDisplay)->getRenderer()->getPhysicalDeviceProperties().deviceType;

    if (deviceType.isSet(CL_DEVICE_TYPE_CPU) && vkPhysicalDeviceType != VK_PHYSICAL_DEVICE_TYPE_CPU)
    {
        ANGLE_CL_RETURN_ERROR(CL_DEVICE_NOT_FOUND);
    }
    else if (deviceType.isSet(CL_DEVICE_TYPE_GPU))
    {
        switch (vkPhysicalDeviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                break;
            default:
                ANGLE_CL_RETURN_ERROR(CL_DEVICE_NOT_FOUND);
        }
    }
    else
    {
        ANGLE_CL_RETURN_ERROR(CL_DEVICE_NOT_FOUND);
    }

    cl::DevicePtrs devices;
    for (const auto &platformDevice : mPlatform.getDevices())
    {
        const auto &platformDeviceInfo = platformDevice->getInfo();
        if (platformDeviceInfo.type.isSet(deviceType))
        {
            devices.push_back(platformDevice);
        }
    }

    *contextOut = CLContextImpl::Ptr(new (std::nothrow) CLContextVk(context, mDisplay, devices));
    if (*contextOut == nullptr)
    {
        ANGLE_CL_RETURN_ERROR(CL_OUT_OF_HOST_MEMORY);
    }
    return angle::Result::Continue;
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

CLPlatformVk::CLPlatformVk(const cl::Platform &platform) : CLPlatformImpl(platform)
{
    mDisplay = egl::Display::GetDisplayFromNativeDisplay(EGL_PLATFORM_ANGLE_ANGLE,
                                                         EGL_DEFAULT_DISPLAY, egl::AttributeMap{});
    ANGLE_CL_IMPL_TRY(InitBackendRenderer(mDisplay));
}

}  // namespace rx
