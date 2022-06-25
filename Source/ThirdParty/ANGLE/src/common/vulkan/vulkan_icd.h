//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vulkan_icd.h : Helper for creating vulkan instances & selecting physical device.

#ifndef COMMON_VULKAN_VULKAN_ICD_H_
#define COMMON_VULKAN_VULKAN_ICD_H_

#include <string>

#include "common/Optional.h"
#include "common/angleutils.h"
#include "common/vulkan/vk_headers.h"

namespace angle
{

namespace vk
{

enum class ICD
{
    Default,
    Mock,
    SwiftShader,
};

struct SimpleDisplayWindow
{
    uint16_t width;
    uint16_t height;
};

class ANGLE_NO_DISCARD ScopedVkLoaderEnvironment : angle::NonCopyable
{
  public:
    ScopedVkLoaderEnvironment(bool enableValidationLayers, vk::ICD icd);
    ~ScopedVkLoaderEnvironment();

    bool canEnableValidationLayers() const { return mEnableValidationLayers; }
    vk::ICD getEnabledICD() const { return mICD; }

  private:
    bool setICDEnvironment(const char *icd);
    bool setCustomExtensionsEnvironment();

    bool mEnableValidationLayers;
    vk::ICD mICD;
    bool mChangedCWD;
    Optional<std::string> mPreviousCWD;
    bool mChangedICDEnv;
    Optional<std::string> mPreviousICDEnv;
    Optional<std::string> mPreviousCustomExtensionsEnv;
    bool mChangedNoDeviceSelect;
    Optional<std::string> mPreviousNoDeviceSelectEnv;
};

void ChoosePhysicalDevice(PFN_vkGetPhysicalDeviceProperties pGetPhysicalDeviceProperties,
                          const std::vector<VkPhysicalDevice> &physicalDevices,
                          vk::ICD preferredICD,
                          VkPhysicalDevice *physicalDeviceOut,
                          VkPhysicalDeviceProperties *physicalDevicePropertiesOut);

}  // namespace vk

}  // namespace angle

#endif  // COMMON_VULKAN_VULKAN_ICD_H_
