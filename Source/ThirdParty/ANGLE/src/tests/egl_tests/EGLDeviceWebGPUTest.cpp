//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EGLDeviceWebGPUTest: Tests the WebGPU adapter and device that EGL_ANGLE_device_webgpu returns,
// through the default proc table of the WebGPU backend.

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "test_utils/ANGLETest.h"
#include "util/EGLWindow.h"

#if defined(ANGLE_ENABLE_WGPU)
#    include <dawn/native/DawnNative.h>
#    include <webgpu/webgpu.h>

#    include <algorithm>
#    include <functional>
#    include <vector>

using namespace angle;

namespace
{
constexpr char kWebGPUDeviceExt[] = "EGL_ANGLE_device_webgpu";

// The values of WGPUFeatureName: the standard features, then the Dawn ones.
std::vector<WGPUFeatureName> AllFeatureNames()
{
    std::vector<WGPUFeatureName> names;
    for (uint32_t value = WGPUFeatureName_CoreFeaturesAndLimits;
         value <= WGPUFeatureName_TextureComponentSwizzle; ++value)
    {
        names.push_back(static_cast<WGPUFeatureName>(value));
    }
    for (uint32_t value = WGPUFeatureName_DawnInternalUsages;
         value <= WGPUFeatureName_AdapterPropertiesDrm; ++value)
    {
        names.push_back(static_cast<WGPUFeatureName>(value));
    }
    return names;
}

std::vector<WGPUFeatureName> ToVector(const WGPUSupportedFeatures &features)
{
    if (features.featureCount == 0)
    {
        return {};
    }
    return {features.features, features.features + features.featureCount};
}

// A feature is in the list exactly when hasFeature() is true for it, and only once.
void ExpectMatchesHasFeature(const std::vector<WGPUFeatureName> &features,
                             const std::function<bool(WGPUFeatureName)> &hasFeature)
{
    for (WGPUFeatureName feature : features)
    {
        EXPECT_TRUE(hasFeature(feature)) << "Listed feature 0x" << std::hex << feature;
        EXPECT_EQ(std::count(features.begin(), features.end(), feature), 1)
            << "Feature 0x" << std::hex << feature;
    }
    for (WGPUFeatureName feature : AllFeatureNames())
    {
        bool listed = std::find(features.begin(), features.end(), feature) != features.end();
        EXPECT_EQ(hasFeature(feature), listed) << "Feature 0x" << std::hex << feature;
    }
}

class EGLDeviceWebGPUTest : public ANGLETest<>
{
  protected:
    const DawnProcTable &procs() const { return dawn::native::GetProcs(); }

    EGLDeviceEXT getEGLDevice()
    {
        EGLAttrib device = 0;
        EXPECT_EGL_TRUE(
            eglQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &device));
        return reinterpret_cast<EGLDeviceEXT>(device);
    }

    bool hasWebGPUDeviceExt()
    {
        return IsEGLClientExtensionEnabled("EGL_EXT_device_query") &&
               IsEGLDeviceExtensionEnabled(getEGLDevice(), kWebGPUDeviceExt);
    }

    // The caller releases the adapter.
    WGPUAdapter getAdapter()
    {
        EGLAttrib adapter = 0;
        EXPECT_EGL_TRUE(
            eglQueryDeviceAttribEXT(getEGLDevice(), EGL_WEBGPU_ADAPTER_ANGLE, &adapter));
        return reinterpret_cast<WGPUAdapter>(adapter);
    }

    // The caller releases the device.
    WGPUDevice getDevice()
    {
        EGLAttrib device = 0;
        EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(getEGLDevice(), EGL_WEBGPU_DEVICE_ANGLE, &device));
        return reinterpret_cast<WGPUDevice>(device);
    }

    void waitFor(WGPUInstance instance, WGPUFuture future)
    {
        WGPUFutureWaitInfo waitInfo = WGPU_FUTURE_WAIT_INFO_INIT;
        waitInfo.future             = future;
        EXPECT_EQ(procs().instanceWaitAny(instance, 1, &waitInfo, UINT64_MAX),
                  WGPUWaitStatus_Success);
        EXPECT_TRUE(waitInfo.completed);
    }

    std::vector<WGPUFeatureName> adapterFeatures(WGPUAdapter adapter)
    {
        WGPUSupportedFeatures features = WGPU_SUPPORTED_FEATURES_INIT;
        procs().adapterGetFeatures(adapter, &features);
        std::vector<WGPUFeatureName> result = ToVector(features);
        procs().supportedFeaturesFreeMembers(features);
        return result;
    }

    std::vector<WGPUFeatureName> deviceFeatures(WGPUDevice device)
    {
        WGPUSupportedFeatures features = WGPU_SUPPORTED_FEATURES_INIT;
        procs().deviceGetFeatures(device, &features);
        std::vector<WGPUFeatureName> result = ToVector(features);
        procs().supportedFeaturesFreeMembers(features);
        return result;
    }

    bool hasFeatureProcs() const
    {
        return procs().adapterGetFeatures && procs().deviceGetFeatures &&
               procs().supportedFeaturesFreeMembers;
    }
};

// The features of the adapter are the ones it has.
TEST_P(EGLDeviceWebGPUTest, AdapterGetFeaturesMatchesHasFeature)
{
    ANGLE_SKIP_TEST_IF(!hasWebGPUDeviceExt());
    ASSERT_TRUE(hasFeatureProcs())
        << "No adapterGetFeatures, deviceGetFeatures or supportedFeaturesFreeMembers proc";

    WGPUAdapter adapter = getAdapter();
    ASSERT_NE(adapter, nullptr);
    std::vector<WGPUFeatureName> features = adapterFeatures(adapter);
    // A core adapter has core-features-and-limits.
    EXPECT_TRUE(procs().adapterHasFeature(adapter, WGPUFeatureName_CoreFeaturesAndLimits));
    ExpectMatchesHasFeature(features, [&](WGPUFeatureName feature) {
        return procs().adapterHasFeature(adapter, feature);
    });
    procs().adapterRelease(adapter);
}

// The features of the device are the ones it has. The device of the display may have no features;
// DeviceGetFeaturesHasRequiredFeatures covers a device that has some.
TEST_P(EGLDeviceWebGPUTest, DeviceGetFeaturesMatchesHasFeature)
{
    ANGLE_SKIP_TEST_IF(!hasWebGPUDeviceExt());
    ASSERT_TRUE(hasFeatureProcs())
        << "No adapterGetFeatures, deviceGetFeatures or supportedFeaturesFreeMembers proc";

    WGPUDevice device = getDevice();
    ASSERT_NE(device, nullptr);
    std::vector<WGPUFeatureName> features = deviceFeatures(device);
    ExpectMatchesHasFeature(features, [&](WGPUFeatureName feature) {
        return procs().deviceHasFeature(device, feature);
    });
    procs().deviceRelease(device);
}

// A device that requires every feature of its adapter has them all.
TEST_P(EGLDeviceWebGPUTest, DeviceGetFeaturesHasRequiredFeatures)
{
    ANGLE_SKIP_TEST_IF(!hasWebGPUDeviceExt());
    ASSERT_TRUE(hasFeatureProcs())
        << "No adapterGetFeatures, deviceGetFeatures or supportedFeaturesFreeMembers proc";

    // The adapter of the display has a device already, so request another adapter.
    static constexpr WGPUInstanceFeatureName kTimedWaitAny = WGPUInstanceFeatureName_TimedWaitAny;
    WGPUInstanceDescriptor instanceDescriptor              = WGPU_INSTANCE_DESCRIPTOR_INIT;
    instanceDescriptor.requiredFeatureCount                = 1;
    instanceDescriptor.requiredFeatures                    = &kTimedWaitAny;
    WGPUInstance instance = procs().createInstance(&instanceDescriptor);
    ASSERT_NE(instance, nullptr);

    WGPUAdapter adapter                            = nullptr;
    WGPURequestAdapterCallbackInfo adapterCallback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapterCallback.mode                           = WGPUCallbackMode_WaitAnyOnly;
    adapterCallback.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                  WGPUStringView, void *userdata1, void *) {
        EXPECT_EQ(status, WGPURequestAdapterStatus_Success);
        *static_cast<WGPUAdapter *>(userdata1) = adapter;
    };
    adapterCallback.userdata1 = &adapter;
    waitFor(instance, procs().instanceRequestAdapter(instance, nullptr, adapterCallback));
    if (!adapter)
    {
        procs().instanceRelease(instance);
        FAIL() << "No adapter";
    }

    std::vector<WGPUFeatureName> requiredFeatures = adapterFeatures(adapter);
    EXPECT_FALSE(requiredFeatures.empty());

    WGPUDeviceDescriptor deviceDescriptor        = WGPU_DEVICE_DESCRIPTOR_INIT;
    deviceDescriptor.requiredFeatureCount        = requiredFeatures.size();
    deviceDescriptor.requiredFeatures            = requiredFeatures.data();
    WGPUDevice device                            = nullptr;
    WGPURequestDeviceCallbackInfo deviceCallback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    deviceCallback.mode                          = WGPUCallbackMode_WaitAnyOnly;
    deviceCallback.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView,
                                 void *userdata1, void *) {
        EXPECT_EQ(status, WGPURequestDeviceStatus_Success);
        *static_cast<WGPUDevice *>(userdata1) = device;
    };
    deviceCallback.userdata1 = &device;
    waitFor(instance, procs().adapterRequestDevice(adapter, &deviceDescriptor, deviceCallback));
    if (!device)
    {
        procs().adapterRelease(adapter);
        procs().instanceRelease(instance);
        FAIL() << "No device";
    }

    std::vector<WGPUFeatureName> features = deviceFeatures(device);
    for (WGPUFeatureName feature : requiredFeatures)
    {
        EXPECT_NE(std::find(features.begin(), features.end(), feature), features.end())
            << "Required feature 0x" << std::hex << feature;
    }
    ExpectMatchesHasFeature(features, [&](WGPUFeatureName feature) {
        return procs().deviceHasFeature(device, feature);
    });

    procs().deviceRelease(device);
    procs().adapterRelease(adapter);
    procs().instanceRelease(instance);
}

}  // anonymous namespace

ANGLE_INSTANTIATE_TEST(EGLDeviceWebGPUTest, ES2_WEBGPU());

#endif  // defined(ANGLE_ENABLE_WGPU)
