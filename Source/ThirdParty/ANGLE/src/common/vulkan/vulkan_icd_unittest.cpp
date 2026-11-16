//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vulkan_icd_unittest.cpp: Unit tests for ChoosePhysicalDevice.
//
// ChoosePhysicalDevice takes the properties query as a function pointer and
// treats the device handles as opaque, so these tests run without a Vulkan
// loader, an ICD or a GPU.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string_view>
#include <vector>

#include "common/vulkan/vulkan_icd.h"

namespace angle
{
namespace
{
// All MIG slices of one GPU report the same PCI IDs, which is what makes UUID
// matching the only way to tell them apart.
constexpr uint32_t kVendorID = 0x10DE;
constexpr uint32_t kDeviceID = 0x2900;

constexpr uint32_t kOtherVendorID = 0x8086;
constexpr uint32_t kOtherDeviceID = 0x9A49;

// With ICD::Default and ANGLE_PREFERRED_DEVICE unset, the ICD filter compares
// the empty string against the device name.  An empty name would therefore
// match every device and the first device would always win.
constexpr char kDeviceName[] = "ANGLE Test Device";

// What GetFilterForICD looks for when ICD::SwiftShader is requested.  Repeated
// here because those constants are private to vulkan_icd.cpp.
constexpr uint32_t kSwiftShaderVendorID = 0x1AE0;
constexpr uint32_t kSwiftShaderDeviceID = 0xC0DE;
constexpr char kSwiftShaderDeviceName[] = "SwiftShader Device";

constexpr size_t kNoDevice = std::numeric_limits<size_t>::max();

// A physical device as reported by the stub properties function.
struct TestDevice
{
    uint32_t vendorID               = kVendorID;
    uint32_t deviceID               = kDeviceID;
    VkPhysicalDeviceType deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    // Every byte of the UUIDs is set to the seed, so distinct seeds give
    // distinct UUIDs.
    uint8_t deviceUUIDSeed = 0;
    uint8_t driverUUIDSeed = 0;
    const char *deviceName = kDeviceName;
};

// vkGetPhysicalDeviceProperties2 carries no user data, so the device table has
// to be reachable from file scope.
const std::vector<TestDevice> *gDevices = nullptr;

// Handles are opaque to ChoosePhysicalDevice, so an index biased by one serves
// as a handle; the bias keeps them distinct from VK_NULL_HANDLE.
VkPhysicalDevice HandleForIndex(size_t index)
{
    return reinterpret_cast<VkPhysicalDevice>(index + 1);
}

size_t IndexForHandle(VkPhysicalDevice handle)
{
    return handle == VK_NULL_HANDLE ? kNoDevice : reinterpret_cast<uintptr_t>(handle) - 1;
}

void FillUUID(uint8_t (&uuid)[VK_UUID_SIZE], uint8_t seed)
{
    std::fill(std::begin(uuid), std::end(uuid), seed);
}

VKAPI_ATTR void VKAPI_CALL GetPhysicalDeviceProperties2Stub(VkPhysicalDevice physicalDevice,
                                                            VkPhysicalDeviceProperties2 *properties)
{
    const TestDevice &device = (*gDevices)[IndexForHandle(physicalDevice)];

    properties->properties.apiVersion = vk::kMinimumVulkanAPIVersion;
    properties->properties.vendorID   = device.vendorID;
    properties->properties.deviceID   = device.deviceID;
    properties->properties.deviceType = device.deviceType;
    const std::string_view deviceName(device.deviceName);
    std::fill(std::begin(properties->properties.deviceName),
              std::end(properties->properties.deviceName), '\0');
    std::copy(deviceName.begin(), deviceName.end(), std::begin(properties->properties.deviceName));

    for (VkBaseOutStructure *next = static_cast<VkBaseOutStructure *>(properties->pNext);
         next != nullptr; next    = next->pNext)
    {
        if (next->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES)
        {
            VkPhysicalDeviceIDProperties *idProperties =
                reinterpret_cast<VkPhysicalDeviceIDProperties *>(next);
            FillUUID(idProperties->deviceUUID, device.deviceUUIDSeed);
            FillUUID(idProperties->driverUUID, device.driverUUIDSeed);
        }
    }
}

class ChoosePhysicalDeviceTest : public testing::Test
{
  protected:
    void TearDown() override { gDevices = nullptr; }

    // Returns the index of the device ChoosePhysicalDevice settled on.
    size_t choose(uint32_t preferredVendorID,
                  uint32_t preferredDeviceID,
                  const uint8_t *preferredDeviceUUID)
    {
        gDevices = &mDevices;

        std::vector<VkPhysicalDevice> handles;
        for (size_t index = 0; index < mDevices.size(); ++index)
        {
            handles.push_back(HandleForIndex(index));
        }

        VkPhysicalDevice chosen                           = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties2 properties            = {};
        VkPhysicalDeviceIDProperties idProperties         = {};
        VkPhysicalDeviceDriverProperties driverProperties = {};

        vk::ChoosePhysicalDevice(GetPhysicalDeviceProperties2Stub, handles, mICD, preferredVendorID,
                                 preferredDeviceID, preferredDeviceUUID, nullptr,
                                 static_cast<VkDriverId>(0), &chosen, &properties, &idProperties,
                                 &driverProperties);

        return IndexForHandle(chosen);
    }

    std::vector<TestDevice> mDevices;
    vk::ICD mICD = vk::ICD::Default;
};

// A UUID names one exact device, so a PCI match on an earlier device must not
// preempt it.  This is the MIG case: every slice carries the same PCI IDs.
TEST_F(ChoosePhysicalDeviceTest, UUIDMatchIsNotPreemptedByEarlierPCIMatch)
{
    mDevices = {
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 1, 0},
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 2, 0},
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 3, 0},
    };

    uint8_t requestedUUID[VK_UUID_SIZE];
    FillUUID(requestedUUID, 3);

    EXPECT_EQ(2u, choose(kVendorID, kDeviceID, requestedUUID));
}

// The ICD filter names one device too, whether through ANGLE_PREFERRED_DEVICE
// or through a request for the mock or SwiftShader device, so a PCI match on
// an earlier device must not preempt it either.
TEST_F(ChoosePhysicalDeviceTest, ICDFilterMatchIsNotPreemptedByEarlierPCIMatch)
{
    mDevices = {
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 1, 0, kDeviceName},
        TestDevice{kSwiftShaderVendorID, kSwiftShaderDeviceID, VK_PHYSICAL_DEVICE_TYPE_CPU, 2, 0,
                   kSwiftShaderDeviceName},
    };
    mICD = vk::ICD::SwiftShader;

    EXPECT_EQ(1u, choose(kVendorID, kDeviceID, nullptr));
}

// Without a UUID, the PCI IDs still select the first device that matches them.
TEST_F(ChoosePhysicalDeviceTest, PCIIDsSelectTheFirstMatchingDevice)
{
    mDevices = {
        TestDevice{kOtherVendorID, kOtherDeviceID, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 1, 0},
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 2, 0},
    };

    EXPECT_EQ(1u, choose(kVendorID, kDeviceID, nullptr));
}

// Without PCI IDs, the UUID selects its device wherever it sits.
TEST_F(ChoosePhysicalDeviceTest, UUIDSelectsItsDevice)
{
    mDevices = {
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 1, 0},
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 2, 0},
    };

    uint8_t requestedUUID[VK_UUID_SIZE];
    FillUUID(requestedUUID, 2);

    EXPECT_EQ(1u, choose(0, 0, requestedUUID));
}

// An unmatched UUID falls back to the first discrete device, as the extension
// permits.  Pinned here so that the precedence change above does not quietly
// turn into a policy change.
TEST_F(ChoosePhysicalDeviceTest, UnmatchedUUIDFallsBackToTheFirstDiscreteDevice)
{
    mDevices = {
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 1, 0},
        TestDevice{kVendorID, kDeviceID, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 2, 0},
    };

    uint8_t requestedUUID[VK_UUID_SIZE];
    FillUUID(requestedUUID, 9);

    EXPECT_EQ(1u, choose(0, 0, requestedUUID));
}
}  // namespace
}  // namespace angle
