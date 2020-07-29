//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// vulkan_icd.cpp : Helper for creating vulkan instances & selecting physical device.

#include "common/vulkan/vulkan_icd.h"

#include <functional>
#include <vector>

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/system_utils.h"

#include "common/vulkan/vk_ext_provoking_vertex.h"
#include "common/vulkan/vk_google_filtering_precision.h"

namespace
{
void ResetEnvironmentVar(const char *variableName, const Optional<std::string> &value)
{
    if (!value.valid())
    {
        return;
    }

    if (value.value().empty())
    {
        angle::UnsetEnvironmentVar(variableName);
    }
    else
    {
        angle::SetEnvironmentVar(variableName, value.value().c_str());
    }
}
}  // namespace

namespace angle
{

namespace vk
{

namespace
{

// This function is unused on Android/Fuschia/GGP
#if !defined(ANGLE_PLATFORM_ANDROID) && !defined(ANGLE_PLATFORM_FUCHSIA) && \
    !defined(ANGLE_PLATFORM_GGP)
const std::string WrapICDEnvironment(const char *icdEnvironment)
{
#    if defined(ANGLE_PLATFORM_APPLE)
    // On MacOS the libraries are bundled into the application directory
    std::string ret = angle::GetHelperExecutableDir() + icdEnvironment;
    return ret;
#    endif  // defined(ANGLE_PLATFORM_APPLE)
    return icdEnvironment;
}

constexpr char kLoaderLayersPathEnv[] = "VK_LAYER_PATH";
#endif

constexpr char kLoaderICDFilenamesEnv[]              = "VK_ICD_FILENAMES";
constexpr char kANGLEPreferredDeviceEnv[]            = "ANGLE_PREFERRED_DEVICE";
constexpr char kValidationLayersCustomSTypeListEnv[] = "VK_LAYER_CUSTOM_STYPE_LIST";

constexpr uint32_t kMockVendorID = 0xba5eba11;
constexpr uint32_t kMockDeviceID = 0xf005ba11;
constexpr char kMockDeviceName[] = "Vulkan Mock Device";

constexpr uint32_t kGoogleVendorID      = 0x1AE0;
constexpr uint32_t kSwiftShaderDeviceID = 0xC0DE;
constexpr char kSwiftShaderDeviceName[] = "SwiftShader Device";

using ICDFilterFunc = std::function<bool(const VkPhysicalDeviceProperties &)>;

ICDFilterFunc GetFilterForICD(vk::ICD preferredICD)
{
    switch (preferredICD)
    {
        case vk::ICD::Mock:
            return [](const VkPhysicalDeviceProperties &deviceProperties) {
                return ((deviceProperties.vendorID == kMockVendorID) &&
                        (deviceProperties.deviceID == kMockDeviceID) &&
                        (strcmp(deviceProperties.deviceName, kMockDeviceName) == 0));
            };
        case vk::ICD::SwiftShader:
            return [](const VkPhysicalDeviceProperties &deviceProperties) {
                return ((deviceProperties.vendorID == kGoogleVendorID) &&
                        (deviceProperties.deviceID == kSwiftShaderDeviceID) &&
                        (strncmp(deviceProperties.deviceName, kSwiftShaderDeviceName,
                                 strlen(kSwiftShaderDeviceName)) == 0));
            };
        default:
            const std::string anglePreferredDevice =
                angle::GetEnvironmentVar(kANGLEPreferredDeviceEnv);
            return [anglePreferredDevice](const VkPhysicalDeviceProperties &deviceProperties) {
                return (anglePreferredDevice.empty() ||
                        anglePreferredDevice == deviceProperties.deviceName);
            };
    }
}

}  // namespace

// If we're loading the validation layers, we could be running from any random directory.
// Change to the executable directory so we can find the layers, then change back to the
// previous directory to be safe we don't disrupt the application.
ScopedVkLoaderEnvironment::ScopedVkLoaderEnvironment(bool enableValidationLayers, vk::ICD icd)
    : mEnableValidationLayers(enableValidationLayers),
      mICD(icd),
      mChangedCWD(false),
      mChangedICDEnv(false)
{
// Changing CWD and setting environment variables makes no sense on Android,
// since this code is a part of Java application there.
// Android Vulkan loader doesn't need this either.
#if !defined(ANGLE_PLATFORM_ANDROID) && !defined(ANGLE_PLATFORM_FUCHSIA) && \
    !defined(ANGLE_PLATFORM_GGP)
    if (icd == vk::ICD::Mock)
    {
        if (!setICDEnvironment(WrapICDEnvironment(ANGLE_VK_MOCK_ICD_JSON).c_str()))
        {
            ERR() << "Error setting environment for Mock/Null Driver.";
        }
    }
#    if defined(ANGLE_VK_SWIFTSHADER_ICD_JSON)
    else if (icd == vk::ICD::SwiftShader)
    {
        if (!setICDEnvironment(WrapICDEnvironment(ANGLE_VK_SWIFTSHADER_ICD_JSON).c_str()))
        {
            ERR() << "Error setting environment for SwiftShader.";
        }
    }
#    endif  // defined(ANGLE_VK_SWIFTSHADER_ICD_JSON)
    if (mEnableValidationLayers || icd != vk::ICD::Default)
    {
        const auto &cwd = angle::GetCWD();
        if (!cwd.valid())
        {
            ERR() << "Error getting CWD for Vulkan layers init.";
            mEnableValidationLayers = false;
            mICD                    = vk::ICD::Default;
        }
        else
        {
            mPreviousCWD       = cwd.value();
            std::string exeDir = angle::GetExecutableDirectory();
            mChangedCWD        = angle::SetCWD(exeDir.c_str());
            if (!mChangedCWD)
            {
                ERR() << "Error setting CWD for Vulkan layers init.";
                mEnableValidationLayers = false;
                mICD                    = vk::ICD::Default;
            }
        }
    }

    // Override environment variable to use the ANGLE layers.
    if (mEnableValidationLayers)
    {
        if (!angle::PrependPathToEnvironmentVar(kLoaderLayersPathEnv, ANGLE_VK_LAYERS_DIR))
        {
            ERR() << "Error setting environment for Vulkan layers init.";
            mEnableValidationLayers = false;
        }

        if (!setCustomExtensionsEnvironment())
        {
            ERR() << "Error setting custom list for custom extensions for Vulkan layers init.";
            mEnableValidationLayers = false;
        }
    }
#endif  // !defined(ANGLE_PLATFORM_ANDROID)
}

ScopedVkLoaderEnvironment::~ScopedVkLoaderEnvironment()
{
    if (mChangedCWD)
    {
#if !defined(ANGLE_PLATFORM_ANDROID)
        ASSERT(mPreviousCWD.valid());
        angle::SetCWD(mPreviousCWD.value().c_str());
#endif  // !defined(ANGLE_PLATFORM_ANDROID)
    }
    if (mChangedICDEnv)
    {
        ResetEnvironmentVar(kLoaderICDFilenamesEnv, mPreviousICDEnv);
    }

    ResetEnvironmentVar(kValidationLayersCustomSTypeListEnv, mPreviousCustomExtensionsEnv);
}

bool ScopedVkLoaderEnvironment::setICDEnvironment(const char *icd)
{
    // Override environment variable to use built Mock ICD
    // ANGLE_VK_ICD_JSON gets set to the built mock ICD in BUILD.gn
    mPreviousICDEnv = angle::GetEnvironmentVar(kLoaderICDFilenamesEnv);
    mChangedICDEnv  = angle::SetEnvironmentVar(kLoaderICDFilenamesEnv, icd);

    if (!mChangedICDEnv)
    {
        mICD = vk::ICD::Default;
    }
    return mChangedICDEnv;
}

bool ScopedVkLoaderEnvironment::setCustomExtensionsEnvironment()
{
    struct CustomExtension
    {
        VkStructureType type;
        size_t size;
    };

    CustomExtension customExtensions[] = {

        {VK_STRUCTURE_TYPE_SAMPLER_FILTERING_PRECISION_GOOGLE,
         sizeof(VkSamplerFilteringPrecisionGOOGLE)},

        {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT,
         sizeof(VkPhysicalDeviceProvokingVertexFeaturesEXT)},
    };

    mPreviousCustomExtensionsEnv = angle::GetEnvironmentVar(kValidationLayersCustomSTypeListEnv);

    std::stringstream strstr;
    for (CustomExtension &extension : customExtensions)
    {
        if (strstr.tellp() != std::streampos(0))
        {
            strstr << angle::GetPathSeparatorForEnvironmentVar();
        }

        strstr << extension.type << angle::GetPathSeparatorForEnvironmentVar() << extension.size;
    }

    return angle::PrependPathToEnvironmentVar(kValidationLayersCustomSTypeListEnv,
                                              strstr.str().c_str());
}

void ChoosePhysicalDevice(const std::vector<VkPhysicalDevice> &physicalDevices,
                          vk::ICD preferredICD,
                          VkPhysicalDevice *physicalDeviceOut,
                          VkPhysicalDeviceProperties *physicalDevicePropertiesOut)
{
    ASSERT(!physicalDevices.empty());

    ICDFilterFunc filter = GetFilterForICD(preferredICD);

    for (const VkPhysicalDevice &physicalDevice : physicalDevices)
    {
        vkGetPhysicalDeviceProperties(physicalDevice, physicalDevicePropertiesOut);
        if (filter(*physicalDevicePropertiesOut))
        {
            *physicalDeviceOut = physicalDevice;
            return;
        }
    }
    WARN() << "Preferred device ICD not found. Using default physicalDevice instead.";

    // Fall back to first device.
    *physicalDeviceOut = physicalDevices[0];
    vkGetPhysicalDeviceProperties(*physicalDeviceOut, physicalDevicePropertiesOut);
}

}  // namespace vk

}  // namespace angle
