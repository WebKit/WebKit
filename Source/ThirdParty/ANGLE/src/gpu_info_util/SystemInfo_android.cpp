//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_android.cpp: implementation of the Android-specific parts of SystemInfo.h

#include <dlfcn.h>
#include <vulkan/vulkan.h>
#include "gpu_info_util/SystemInfo_internal.h"

#include <sys/system_properties.h>
#include <cstring>
#include <fstream>

#include "common/angleutils.h"
#include "common/debug.h"

namespace angle
{
class VulkanLibrary final : NonCopyable
{
  public:
    VulkanLibrary() {}
    ~VulkanLibrary()
    {
        if (mInstance != VK_NULL_HANDLE)
        {
            PFN_vkDestroyInstance pfnDestroyInstance =
                reinterpret_cast<PFN_vkDestroyInstance>(dlsym(mLibVulkan, "vkDestroyInstance"));
            if (pfnDestroyInstance)
            {
                pfnDestroyInstance(mInstance, nullptr);
            }
        }
        if (mLibVulkan)
            dlclose(mLibVulkan);
    }
    VkInstance getVulkanInstance()
    {
        // Find the system's Vulkan library and open it:
        mLibVulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        if (!mLibVulkan)
        {
            // If Vulkan doesn't exist, bail-out early:
            return VK_NULL_HANDLE;
        }

        // Determine the available Vulkan instance version:
        uint32_t instanceVersion = VK_API_VERSION_1_0;
#if defined(VK_VERSION_1_1)
        PFN_vkEnumerateInstanceVersion pfnEnumerateInstanceVersion =
            reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
                dlsym(mLibVulkan, "vkEnumerateInstanceVersion"));
        if (!pfnEnumerateInstanceVersion ||
            pfnEnumerateInstanceVersion(&instanceVersion) != VK_SUCCESS)
        {
            instanceVersion = VK_API_VERSION_1_0;
        }
#endif  // VK_VERSION_1_1

        // Create a Vulkan instance:
        VkApplicationInfo appInfo;
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext              = nullptr;
        appInfo.pApplicationName   = "";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName        = "";
        appInfo.engineVersion      = 1;
        appInfo.apiVersion         = instanceVersion;

        VkInstanceCreateInfo createInstanceInfo;
        createInstanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInstanceInfo.pNext                   = nullptr;
        createInstanceInfo.flags                   = 0;
        createInstanceInfo.pApplicationInfo        = &appInfo;
        createInstanceInfo.enabledLayerCount       = 0;
        createInstanceInfo.ppEnabledLayerNames     = nullptr;
        createInstanceInfo.enabledExtensionCount   = 0;
        createInstanceInfo.ppEnabledExtensionNames = nullptr;

        PFN_vkCreateInstance pfnCreateInstance =
            reinterpret_cast<PFN_vkCreateInstance>(dlsym(mLibVulkan, "vkCreateInstance"));
        if (!pfnCreateInstance ||
            pfnCreateInstance(&createInstanceInfo, nullptr, &mInstance) != VK_SUCCESS)
        {
            return VK_NULL_HANDLE;
        }

        return mInstance;
    }
    void *gpa(std::string fn) { return dlsym(mLibVulkan, fn.c_str()); }
#define GPA(ob, type, fn) reinterpret_cast<type>(ob.gpa(fn))

  private:
    void *mLibVulkan     = nullptr;
    VkInstance mInstance = VK_NULL_HANDLE;
};

ANGLE_FORMAT_PRINTF(1, 2)
std::string FormatString(const char *fmt, ...)
{
    va_list vararg;
    va_start(vararg, fmt);

    std::vector<char> buffer(512);
    size_t len = FormatStringIntoVector(fmt, vararg, buffer);
    va_end(vararg);

    return std::string(&buffer[0], len);
}

bool GetAndroidSystemProperty(const std::string &propertyName, std::string *value)
{
    // PROP_VALUE_MAX from <sys/system_properties.h>
    std::vector<char> propertyBuf(PROP_VALUE_MAX);
    int len = __system_property_get(propertyName.c_str(), propertyBuf.data());
    if (len <= 0)
    {
        return false;
    }
    *value = std::string(propertyBuf.data());
    return true;
}

bool GetSystemInfo(SystemInfo *info)
{
    bool isFullyPopulated = true;

    isFullyPopulated =
        GetAndroidSystemProperty("ro.product.manufacturer", &info->machineManufacturer) &&
        isFullyPopulated;
    isFullyPopulated =
        GetAndroidSystemProperty("ro.product.model", &info->machineModelName) && isFullyPopulated;

    // This implementation builds on top of the Vulkan API, but cannot assume the existence of the
    // Vulkan library.  ANGLE can be installed on versions of Android as old as Ice Cream Sandwich.
    // Therefore, we need to use dlopen()/dlsym() in order to see if Vulkan is installed on the
    // system, and if so, to use it:
    VulkanLibrary vkLibrary;
    VkInstance instance = vkLibrary.getVulkanInstance();
    if (instance == VK_NULL_HANDLE)
    {
        // If Vulkan doesn't exist, bail-out early:
        return false;
    }

    // Enumerate the Vulkan physical devices, which are ANGLE gpus:
    PFN_vkEnumeratePhysicalDevices pfnEnumeratePhysicalDevices =
        GPA(vkLibrary, PFN_vkEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties pfnGetPhysicalDeviceProperties =
        GPA(vkLibrary, PFN_vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
    uint32_t physicalDeviceCount = 0;
    if (!pfnEnumeratePhysicalDevices ||
        pfnEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr) != VK_SUCCESS)
    {
        return false;
    }
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    if (pfnEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()) !=
        VK_SUCCESS)
    {
        return false;
    }

    // If we get to here, we will likely provide a valid answer (unless an unknown vendorID):
    info->gpus.resize(physicalDeviceCount);

    for (uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        VkPhysicalDeviceProperties properties;
        pfnGetPhysicalDeviceProperties(physicalDevices[i], &properties);
        // Fill in data for a given physical device (a.k.a. gpu):
        GPUDeviceInfo &gpu = info->gpus[i];
        gpu.vendorId       = properties.vendorID;
        gpu.deviceId       = properties.deviceID;
        // Need to parse/re-format properties.driverVersion.
        //
        // TODO(ianelliott): Determine the formatting used for each vendor
        // (http://anglebug.com/2677)
        switch (properties.vendorID)
        {
            case kVendorID_AMD:
                gpu.driverVendor                = "Advanced Micro Devices, Inc";
                gpu.driverVersion               = FormatString("0x%x", properties.driverVersion);
                gpu.detailedDriverVersion.major = properties.driverVersion;
                break;
            case kVendorID_ARM:
                gpu.driverVendor                = "Arm Holdings";
                gpu.driverVersion               = FormatString("0x%x", properties.driverVersion);
                gpu.detailedDriverVersion.major = properties.driverVersion;
                break;
            case kVendorID_Broadcom:
                gpu.driverVendor                = "Broadcom";
                gpu.driverVersion               = FormatString("0x%x", properties.driverVersion);
                gpu.detailedDriverVersion.major = properties.driverVersion;
                break;
            case kVendorID_ImgTec:
                gpu.driverVendor                = "Imagination Technologies Limited";
                gpu.driverVersion               = FormatString("0x%x", properties.driverVersion);
                gpu.detailedDriverVersion.major = properties.driverVersion;
                break;
            case kVendorID_Intel:
                gpu.driverVendor                = "Intel Corporation";
                gpu.driverVersion               = FormatString("0x%x", properties.driverVersion);
                gpu.detailedDriverVersion.major = properties.driverVersion;
                break;
            case kVendorID_Kazan:
                gpu.driverVendor                = "Kazan Software";
                gpu.driverVersion               = FormatString("0x%x", properties.driverVersion);
                gpu.detailedDriverVersion.major = properties.driverVersion;
                break;
            case kVendorID_NVIDIA:
                gpu.driverVendor  = "NVIDIA Corporation";
                gpu.driverVersion = FormatString("%d.%d.%d.%d", properties.driverVersion >> 22,
                                                 (properties.driverVersion >> 14) & 0XFF,
                                                 (properties.driverVersion >> 6) & 0XFF,
                                                 properties.driverVersion & 0x3F);
                gpu.detailedDriverVersion.major    = properties.driverVersion >> 22;
                gpu.detailedDriverVersion.minor    = (properties.driverVersion >> 14) & 0xFF;
                gpu.detailedDriverVersion.subMinor = (properties.driverVersion >> 6) & 0xFF;
                gpu.detailedDriverVersion.patch    = properties.driverVersion & 0x3F;
                break;
            case kVendorID_Qualcomm:
                gpu.driverVendor = "Qualcomm Technologies, Inc";
                if (properties.driverVersion & 0x80000000)
                {
                    gpu.driverVersion = FormatString("%d.%d.%d", properties.driverVersion >> 22,
                                                     (properties.driverVersion >> 12) & 0X3FF,
                                                     properties.driverVersion & 0xFFF);
                    gpu.detailedDriverVersion.major    = properties.driverVersion >> 22;
                    gpu.detailedDriverVersion.minor    = (properties.driverVersion >> 12) & 0x3FF;
                    gpu.detailedDriverVersion.subMinor = properties.driverVersion & 0xFFF;
                }
                else
                {
                    gpu.driverVersion = FormatString("0x%x", properties.driverVersion);
                    gpu.detailedDriverVersion.major = properties.driverVersion;
                }
                break;
            case kVendorID_VeriSilicon:
                gpu.driverVendor                = "VeriSilicon";
                gpu.driverVersion               = FormatString("0x%x", properties.driverVersion);
                gpu.detailedDriverVersion.major = properties.driverVersion;
                break;
            case kVendorID_Vivante:
                gpu.driverVendor                = "Vivante";
                gpu.driverVersion               = FormatString("0x%x", properties.driverVersion);
                gpu.detailedDriverVersion.major = properties.driverVersion;
                break;
            default:
                return false;
        }
        gpu.driverDate = "";
    }

    return isFullyPopulated;
}

}  // namespace angle
