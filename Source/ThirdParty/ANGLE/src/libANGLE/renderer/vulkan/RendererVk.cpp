//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RendererVk.cpp:
//    Implements the class methods for RendererVk.
//

#include "libANGLE/renderer/vulkan/RendererVk.h"

// Placing this first seems to solve an intellisense bug.
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include <EGL/eglext.h>

#include "common/debug.h"
#include "common/platform.h"
#include "common/system_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/CompilerVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/GlslangWrapper.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"
#include "libANGLE/renderer/vulkan/vk_caps_utils.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/trace.h"
#include "platform/Platform.h"

// Consts
namespace
{
const uint32_t kMockVendorID                              = 0xba5eba11;
const uint32_t kMockDeviceID                              = 0xf005ba11;
constexpr char kMockDeviceName[]                          = "Vulkan Mock Device";
constexpr VkFormatFeatureFlags kInvalidFormatFeatureFlags = static_cast<VkFormatFeatureFlags>(-1);
}  // anonymous namespace

namespace rx
{

namespace
{
// Update the pipeline cache every this many swaps.
constexpr uint32_t kPipelineCacheVkUpdatePeriod = 60;
// Wait a maximum of 10s.  If that times out, we declare it a failure.
constexpr uint64_t kMaxFenceWaitTimeNs = 10'000'000'000llu;
// Per the Vulkan specification, as long as Vulkan 1.1+ is returned by vkEnumerateInstanceVersion,
// ANGLE must indicate the highest version of Vulkan functionality that it uses.  The Vulkan
// validation layers will issue messages for any core functionality that requires a higher version.
// This value must be increased whenever ANGLE starts using functionality from a newer core
// version of Vulkan.
constexpr uint32_t kPreferredVulkanAPIVersion = VK_API_VERSION_1_1;

vk::ICD ChooseICDFromAttribs(const egl::AttributeMap &attribs)
{
#if !defined(ANGLE_PLATFORM_ANDROID)
    // Mock ICD does not currently run on Android
    EGLAttrib deviceType = attribs.get(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                                       EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);

    switch (deviceType)
    {
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE:
            break;
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
            return vk::ICD::Mock;
        default:
            UNREACHABLE();
            break;
    }
#endif  // !defined(ANGLE_PLATFORM_ANDROID)

    return vk::ICD::Default;
}

bool StrLess(const char *a, const char *b)
{
    return strcmp(a, b) < 0;
}

bool ExtensionFound(const char *needle, const RendererVk::ExtensionNameList &haystack)
{
    // NOTE: The list must be sorted.
    return std::binary_search(haystack.begin(), haystack.end(), needle, StrLess);
}

VkResult VerifyExtensionsPresent(const RendererVk::ExtensionNameList &haystack,
                                 const RendererVk::ExtensionNameList &needles)
{
    // NOTE: The lists must be sorted.
    if (std::includes(haystack.begin(), haystack.end(), needles.begin(), needles.end(), StrLess))
    {
        return VK_SUCCESS;
    }
    for (const char *needle : needles)
    {
        if (!ExtensionFound(needle, haystack))
        {
            ERR() << "Extension not supported: " << needle;
        }
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

// Array of Validation error/warning messages that will be ignored, should include bugID
constexpr const char *kSkippedMessages[] = {
    // http://anglebug.com/2866
    "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed",
    // http://anglebug.com/2796
    "UNASSIGNED-CoreValidation-Shader-PointSizeMissing",
    // http://anglebug.com/3832
    "VUID-VkPipelineInputAssemblyStateCreateInfo-topology-00428",
};

// Suppress validation errors that are known
//  return "true" if given code/prefix/message is known, else return "false"
bool IsIgnoredDebugMessage(const char *message)
{
    if (!message)
    {
        return false;
    }
    for (const char *msg : kSkippedMessages)
    {
        if (strstr(message, msg) != nullptr)
        {
            return true;
        }
    }
    return false;
}

const char *GetVkObjectTypeName(VkObjectType type)
{
    switch (type)
    {
        case VK_OBJECT_TYPE_UNKNOWN:
            return "Unknown";
        case VK_OBJECT_TYPE_INSTANCE:
            return "Instance";
        case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
            return "Physical Device";
        case VK_OBJECT_TYPE_DEVICE:
            return "Device";
        case VK_OBJECT_TYPE_QUEUE:
            return "Queue";
        case VK_OBJECT_TYPE_SEMAPHORE:
            return "Semaphore";
        case VK_OBJECT_TYPE_COMMAND_BUFFER:
            return "Command Buffer";
        case VK_OBJECT_TYPE_FENCE:
            return "Fence";
        case VK_OBJECT_TYPE_DEVICE_MEMORY:
            return "Device Memory";
        case VK_OBJECT_TYPE_BUFFER:
            return "Buffer";
        case VK_OBJECT_TYPE_IMAGE:
            return "Image";
        case VK_OBJECT_TYPE_EVENT:
            return "Event";
        case VK_OBJECT_TYPE_QUERY_POOL:
            return "Query Pool";
        case VK_OBJECT_TYPE_BUFFER_VIEW:
            return "Buffer View";
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            return "Image View";
        case VK_OBJECT_TYPE_SHADER_MODULE:
            return "Shader Module";
        case VK_OBJECT_TYPE_PIPELINE_CACHE:
            return "Pipeline Cache";
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            return "Pipeline Layout";
        case VK_OBJECT_TYPE_RENDER_PASS:
            return "Render Pass";
        case VK_OBJECT_TYPE_PIPELINE:
            return "Pipeline";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            return "Descriptor Set Layout";
        case VK_OBJECT_TYPE_SAMPLER:
            return "Sampler";
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            return "Descriptor Pool";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            return "Descriptor Set";
        case VK_OBJECT_TYPE_FRAMEBUFFER:
            return "Framebuffer";
        case VK_OBJECT_TYPE_COMMAND_POOL:
            return "Command Pool";
        case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
            return "Sampler YCbCr Conversion";
        case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
            return "Descriptor Update Template";
        case VK_OBJECT_TYPE_SURFACE_KHR:
            return "Surface";
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
            return "Swapchain";
        case VK_OBJECT_TYPE_DISPLAY_KHR:
            return "Display";
        case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
            return "Display Mode";
        case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
            return "Debug Report Callback";
        case VK_OBJECT_TYPE_OBJECT_TABLE_NVX:
            return "Object Table";
        case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX:
            return "Indirect Commands Layout";
        case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
            return "Debug Utils Messenger";
        case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
            return "Validation Cache";
        case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
            return "Acceleration Structure";
        default:
            return "<Unrecognized>";
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugUtilsMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                    const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                    void *userData)
{
    // See if it's an issue we are aware of and don't want to be spammed about.
    if (IsIgnoredDebugMessage(callbackData->pMessageIdName))
    {
        return VK_FALSE;
    }

    std::ostringstream log;
    if (callbackData->pMessageIdName)
    {
        log << "[ " << callbackData->pMessageIdName << " ] ";
    }
    log << callbackData->pMessage << std::endl;

    // Aesthetic value based on length of the function name, line number, etc.
    constexpr size_t kStartIndent = 28;

    // Output the debug marker hierarchy under which this error has occured.
    size_t indent = kStartIndent;
    if (callbackData->queueLabelCount > 0)
    {
        log << std::string(indent++, ' ') << "<Queue Label Hierarchy:>" << std::endl;
        for (uint32_t i = 0; i < callbackData->queueLabelCount; ++i)
        {
            log << std::string(indent++, ' ') << callbackData->pQueueLabels[i].pLabelName
                << std::endl;
        }
    }
    if (callbackData->cmdBufLabelCount > 0)
    {
        log << std::string(indent++, ' ') << "<Command Buffer Label Hierarchy:>" << std::endl;
        for (uint32_t i = 0; i < callbackData->cmdBufLabelCount; ++i)
        {
            log << std::string(indent++, ' ') << callbackData->pCmdBufLabels[i].pLabelName
                << std::endl;
        }
    }
    // Output the objects involved in this error message.
    if (callbackData->objectCount > 0)
    {
        for (uint32_t i = 0; i < callbackData->objectCount; ++i)
        {
            const char *objectName = callbackData->pObjects[i].pObjectName;
            const char *objectType = GetVkObjectTypeName(callbackData->pObjects[i].objectType);
            uint64_t objectHandle  = callbackData->pObjects[i].objectHandle;
            log << std::string(indent, ' ') << "Object: ";
            if (objectHandle == 0)
            {
                log << "VK_NULL_HANDLE";
            }
            else
            {
                log << "0x" << std::hex << objectHandle << std::dec;
            }
            log << " (type = " << objectType << "(" << callbackData->pObjects[i].objectType << "))";
            if (objectName)
            {
                log << " [" << objectName << "]";
            }
            log << std::endl;
        }
    }

    bool isError    = (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    std::string msg = log.str();

    RendererVk *rendererVk = static_cast<RendererVk *>(userData);
    rendererVk->onNewValidationMessage(msg);

    if (isError)
    {
        ERR() << msg;
    }
    else
    {
        WARN() << msg;
    }

    return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(VkDebugReportFlagsEXT flags,
                                                   VkDebugReportObjectTypeEXT objectType,
                                                   uint64_t object,
                                                   size_t location,
                                                   int32_t messageCode,
                                                   const char *layerPrefix,
                                                   const char *message,
                                                   void *userData)
{
    if (IsIgnoredDebugMessage(message))
    {
        return VK_FALSE;
    }
    if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0)
    {
        ERR() << message;
#if !defined(NDEBUG)
        // Abort the call in Debug builds.
        return VK_TRUE;
#endif
    }
    else if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0)
    {
        WARN() << message;
    }
    else
    {
        // Uncomment this if you want Vulkan spam.
        // WARN() << message;
    }

    return VK_FALSE;
}

// If we're loading the validation layers, we could be running from any random directory.
// Change to the executable directory so we can find the layers, then change back to the
// previous directory to be safe we don't disrupt the application.
class ScopedVkLoaderEnvironment : angle::NonCopyable
{
  public:
    ScopedVkLoaderEnvironment(bool enableValidationLayers, vk::ICD icd)
        : mEnableValidationLayers(enableValidationLayers),
          mICD(icd),
          mChangedCWD(false),
          mChangedICDEnv(false)
    {
// Changing CWD and setting environment variables makes no sense on Android,
// since this code is a part of Java application there.
// Android Vulkan loader doesn't need this either.
#if !defined(ANGLE_PLATFORM_ANDROID) && !defined(ANGLE_PLATFORM_FUCHSIA)
        if (icd == vk::ICD::Mock)
        {
            if (!setICDEnvironment(ANGLE_VK_MOCK_ICD_JSON))
            {
                ERR() << "Error setting environment for Mock/Null Driver.";
            }
        }
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
            if (!angle::PrependPathToEnvironmentVar(vk::gLoaderLayersPathEnv, ANGLE_VK_LAYERS_DIR))
            {
                ERR() << "Error setting environment for Vulkan layers init.";
                mEnableValidationLayers = false;
            }
        }
#endif  // !defined(ANGLE_PLATFORM_ANDROID)
    }

    ~ScopedVkLoaderEnvironment()
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
            if (mPreviousICDEnv.value().empty())
            {
                angle::UnsetEnvironmentVar(vk::gLoaderICDFilenamesEnv);
            }
            else
            {
                angle::SetEnvironmentVar(vk::gLoaderICDFilenamesEnv,
                                         mPreviousICDEnv.value().c_str());
            }
        }
    }

    bool canEnableValidationLayers() const { return mEnableValidationLayers; }
    vk::ICD getEnabledICD() const { return mICD; }

  private:
    bool setICDEnvironment(const char *icd)
    {
        // Override environment variable to use built Mock ICD
        // ANGLE_VK_ICD_JSON gets set to the built mock ICD in BUILD.gn
        mPreviousICDEnv = angle::GetEnvironmentVar(vk::gLoaderICDFilenamesEnv);
        mChangedICDEnv  = angle::SetEnvironmentVar(vk::gLoaderICDFilenamesEnv, icd);

        if (!mChangedICDEnv)
        {
            mICD = vk::ICD::Default;
        }
        return mChangedICDEnv;
    }

    bool mEnableValidationLayers;
    vk::ICD mICD;
    bool mChangedCWD;
    Optional<std::string> mPreviousCWD;
    bool mChangedICDEnv;
    Optional<std::string> mPreviousICDEnv;
};

void ChoosePhysicalDevice(const std::vector<VkPhysicalDevice> &physicalDevices,
                          vk::ICD preferredICD,
                          VkPhysicalDevice *physicalDeviceOut,
                          VkPhysicalDeviceProperties *physicalDevicePropertiesOut)
{
    ASSERT(!physicalDevices.empty());
    if (preferredICD == vk::ICD::Mock)
    {
        for (const VkPhysicalDevice &physicalDevice : physicalDevices)
        {
            vkGetPhysicalDeviceProperties(physicalDevice, physicalDevicePropertiesOut);
            if ((kMockVendorID == physicalDevicePropertiesOut->vendorID) &&
                (kMockDeviceID == physicalDevicePropertiesOut->deviceID) &&
                (strcmp(kMockDeviceName, physicalDevicePropertiesOut->deviceName) == 0))
            {
                *physicalDeviceOut = physicalDevice;
                return;
            }
        }
        WARN() << "Vulkan Mock Driver was requested but Mock Device was not found. Using default "
                  "physicalDevice instead.";
    }

    // Fall back to first device.
    *physicalDeviceOut = physicalDevices[0];
    vkGetPhysicalDeviceProperties(*physicalDeviceOut, physicalDevicePropertiesOut);
}

angle::Result WaitFences(vk::Context *context,
                         std::vector<vk::Shared<vk::Fence>> *fences,
                         bool block)
{
    uint64_t timeout = block ? kMaxFenceWaitTimeNs : 0;

    // Iterate backwards over the fences, removing them from the list in constant time when they are
    // complete.
    while (!fences->empty())
    {
        VkResult result = fences->back().get().wait(context->getDevice(), timeout);
        if (result == VK_TIMEOUT)
        {
            return angle::Result::Continue;
        }
        ANGLE_VK_TRY(context, result);

        context->getRenderer()->resetSharedFence(&fences->back());
        fences->pop_back();
    }

    return angle::Result::Continue;
}

}  // anonymous namespace

// RendererVk implementation.
RendererVk::RendererVk()
    : mDisplay(nullptr),
      mCapsInitialized(false),
      mFeaturesInitialized(false),
      mInstance(VK_NULL_HANDLE),
      mEnableValidationLayers(false),
      mEnabledICD(vk::ICD::Default),
      mDebugUtilsMessenger(VK_NULL_HANDLE),
      mDebugReportCallback(VK_NULL_HANDLE),
      mPhysicalDevice(VK_NULL_HANDLE),
      mPhysicalDeviceSubgroupProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES},
      mQueue(VK_NULL_HANDLE),
      mCurrentQueueFamilyIndex(std::numeric_limits<uint32_t>::max()),
      mMaxVertexAttribDivisor(1),
      mDevice(VK_NULL_HANDLE),
      mDeviceLost(false),
      mPipelineCacheVkUpdateTimeout(kPipelineCacheVkUpdatePeriod),
      mPipelineCacheDirty(false),
      mPipelineCacheInitialized(false)
{
    VkFormatProperties invalid = {0, 0, kInvalidFormatFeatureFlags};
    mFormatProperties.fill(invalid);
}

RendererVk::~RendererVk()
{
    ASSERT(mFencedGarbage.empty());
}

void RendererVk::onDestroy(vk::Context *context)
{
    (void)cleanupGarbage(context, true);

    mFenceRecycler.destroy(mDevice);

    mPipelineLayoutCache.destroy(mDevice);
    mDescriptorSetLayoutCache.destroy(mDevice);

    mPipelineCache.destroy(mDevice);

    GlslangWrapper::Release();

    if (mDevice)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    if (mDebugUtilsMessenger)
    {
        ASSERT(mInstance && vkDestroyDebugUtilsMessengerEXT);
        vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugUtilsMessenger, nullptr);

        ASSERT(mDebugReportCallback == VK_NULL_HANDLE);
    }
    else if (mDebugReportCallback)
    {
        ASSERT(mInstance && vkDestroyDebugReportCallbackEXT);
        vkDestroyDebugReportCallbackEXT(mInstance, mDebugReportCallback, nullptr);
    }

    if (mInstance)
    {
        vkDestroyInstance(mInstance, nullptr);
        mInstance = VK_NULL_HANDLE;
    }

    mMemoryProperties.destroy();
    mPhysicalDevice = VK_NULL_HANDLE;
}

void RendererVk::notifyDeviceLost()
{
    mDeviceLost = true;
    mDisplay->notifyDeviceLost();
}

bool RendererVk::isDeviceLost() const
{
    return mDeviceLost;
}

angle::Result RendererVk::initialize(DisplayVk *displayVk,
                                     egl::Display *display,
                                     const char *wsiExtension,
                                     const char *wsiLayer)
{
    mDisplay                         = display;
    const egl::AttributeMap &attribs = mDisplay->getAttributeMap();
    ScopedVkLoaderEnvironment scopedEnvironment(ShouldUseDebugLayers(attribs),
                                                ChooseICDFromAttribs(attribs));
    mEnableValidationLayers = scopedEnvironment.canEnableValidationLayers();
    mEnabledICD             = scopedEnvironment.getEnabledICD();

    // Gather global layer properties.
    uint32_t instanceLayerCount = 0;
    ANGLE_VK_TRY(displayVk, vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

    std::vector<VkLayerProperties> instanceLayerProps(instanceLayerCount);
    if (instanceLayerCount > 0)
    {
        ANGLE_VK_TRY(displayVk, vkEnumerateInstanceLayerProperties(&instanceLayerCount,
                                                                   instanceLayerProps.data()));
    }

    VulkanLayerVector enabledInstanceLayerNames;
    if (mEnableValidationLayers)
    {
        bool layersRequested =
            (attribs.get(EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE, EGL_DONT_CARE) == EGL_TRUE);
        mEnableValidationLayers = GetAvailableValidationLayers(instanceLayerProps, layersRequested,
                                                               &enabledInstanceLayerNames);
    }

    if (wsiLayer)
    {
        enabledInstanceLayerNames.push_back(wsiLayer);
    }

    // Enumerate instance extensions that are provided by the vulkan
    // implementation and implicit layers.
    uint32_t instanceExtensionCount = 0;
    ANGLE_VK_TRY(displayVk,
                 vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));

    std::vector<VkExtensionProperties> instanceExtensionProps(instanceExtensionCount);
    if (instanceExtensionCount > 0)
    {
        ANGLE_VK_TRY(displayVk,
                     vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount,
                                                            instanceExtensionProps.data()));
    }

    // Enumerate instance extensions that are provided by explicit layers.
    for (const char *layerName : enabledInstanceLayerNames)
    {
        uint32_t previousExtensionCount      = static_cast<uint32_t>(instanceExtensionProps.size());
        uint32_t instanceLayerExtensionCount = 0;
        ANGLE_VK_TRY(displayVk, vkEnumerateInstanceExtensionProperties(
                                    layerName, &instanceLayerExtensionCount, nullptr));
        instanceExtensionProps.resize(previousExtensionCount + instanceLayerExtensionCount);
        ANGLE_VK_TRY(displayVk, vkEnumerateInstanceExtensionProperties(
                                    layerName, &instanceLayerExtensionCount,
                                    instanceExtensionProps.data() + previousExtensionCount));
    }

    ExtensionNameList instanceExtensionNames;
    if (!instanceExtensionProps.empty())
    {
        for (const VkExtensionProperties &i : instanceExtensionProps)
        {
            instanceExtensionNames.push_back(i.extensionName);
        }
        std::sort(instanceExtensionNames.begin(), instanceExtensionNames.end(), StrLess);
    }

    ExtensionNameList enabledInstanceExtensions;
    enabledInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    enabledInstanceExtensions.push_back(wsiExtension);

    bool enableDebugUtils =
        mEnableValidationLayers &&
        ExtensionFound(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instanceExtensionNames);
    bool enableDebugReport =
        mEnableValidationLayers && !enableDebugUtils &&
        ExtensionFound(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instanceExtensionNames);

    if (enableDebugUtils)
    {
        enabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    else if (enableDebugReport)
    {
        enabledInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    // Verify the required extensions are in the extension names set. Fail if not.
    std::sort(enabledInstanceExtensions.begin(), enabledInstanceExtensions.end(), StrLess);
    ANGLE_VK_TRY(displayVk,
                 VerifyExtensionsPresent(instanceExtensionNames, enabledInstanceExtensions));

    // Enable VK_KHR_get_physical_device_properties_2 if available.
    if (ExtensionFound(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                       instanceExtensionNames))
    {
        enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    VkApplicationInfo applicationInfo  = {};
    applicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName   = "ANGLE";
    applicationInfo.applicationVersion = 1;
    applicationInfo.pEngineName        = "ANGLE";
    applicationInfo.engineVersion      = 1;

    auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(mInstance, "vkEnumerateInstanceVersion"));
    if (!enumerateInstanceVersion)
    {
        applicationInfo.apiVersion = VK_API_VERSION_1_0;
    }
    else
    {
        uint32_t apiVersion = VK_API_VERSION_1_0;
        ANGLE_VK_TRY(displayVk, enumerateInstanceVersion(&apiVersion));
        if ((VK_VERSION_MAJOR(apiVersion) > 1) || (VK_VERSION_MINOR(apiVersion) >= 1))
        {
            // This is the highest version of core Vulkan functionality that ANGLE uses.
            applicationInfo.apiVersion = kPreferredVulkanAPIVersion;
        }
        else
        {
            // Since only 1.0 instance-level functionality is available, this must set to 1.0.
            applicationInfo.apiVersion = VK_API_VERSION_1_0;
        }
    }

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.flags                = 0;
    instanceInfo.pApplicationInfo     = &applicationInfo;

    // Enable requested layers and extensions.
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size());
    instanceInfo.ppEnabledExtensionNames =
        enabledInstanceExtensions.empty() ? nullptr : enabledInstanceExtensions.data();
    instanceInfo.enabledLayerCount   = static_cast<uint32_t>(enabledInstanceLayerNames.size());
    instanceInfo.ppEnabledLayerNames = enabledInstanceLayerNames.data();

    ANGLE_VK_TRY(displayVk, vkCreateInstance(&instanceInfo, nullptr, &mInstance));

    if (enableDebugUtils)
    {
        // Try to use the newer EXT_debug_utils if it exists.
        InitDebugUtilsEXTFunctions(mInstance);

        // Create the messenger callback.
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};

        constexpr VkDebugUtilsMessageSeverityFlagsEXT kSeveritiesToLog =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

        constexpr VkDebugUtilsMessageTypeFlagsEXT kMessagesToLog =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        messengerInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerInfo.messageSeverity = kSeveritiesToLog;
        messengerInfo.messageType     = kMessagesToLog;
        messengerInfo.pfnUserCallback = &DebugUtilsMessenger;
        messengerInfo.pUserData       = this;

        ANGLE_VK_TRY(displayVk, vkCreateDebugUtilsMessengerEXT(mInstance, &messengerInfo, nullptr,
                                                               &mDebugUtilsMessenger));
    }
    else if (enableDebugReport)
    {
        // Fallback to EXT_debug_report.
        InitDebugReportEXTFunctions(mInstance);

        VkDebugReportCallbackCreateInfoEXT debugReportInfo = {};

        debugReportInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        debugReportInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        debugReportInfo.pfnCallback = &DebugReportCallback;
        debugReportInfo.pUserData   = this;

        ANGLE_VK_TRY(displayVk, vkCreateDebugReportCallbackEXT(mInstance, &debugReportInfo, nullptr,
                                                               &mDebugReportCallback));
    }

    if (std::find(enabledInstanceExtensions.begin(), enabledInstanceExtensions.end(),
                  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) !=
        enabledInstanceExtensions.end())
    {
        InitGetPhysicalDeviceProperties2KHRFunctions(mInstance);
        ASSERT(vkGetPhysicalDeviceProperties2KHR);
    }

    uint32_t physicalDeviceCount = 0;
    ANGLE_VK_TRY(displayVk, vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, nullptr));
    ANGLE_VK_CHECK(displayVk, physicalDeviceCount > 0, VK_ERROR_INITIALIZATION_FAILED);

    // TODO(jmadill): Handle multiple physical devices. For now, use the first device.
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    ANGLE_VK_TRY(displayVk, vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount,
                                                       physicalDevices.data()));
    ChoosePhysicalDevice(physicalDevices, mEnabledICD, &mPhysicalDevice,
                         &mPhysicalDeviceProperties);

    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mPhysicalDeviceFeatures);

    // Ensure we can find a graphics queue family.
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueCount, nullptr);

    ANGLE_VK_CHECK(displayVk, queueCount > 0, VK_ERROR_INITIALIZATION_FAILED);

    mQueueFamilyProperties.resize(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueCount,
                                             mQueueFamilyProperties.data());

    size_t graphicsQueueFamilyCount            = false;
    uint32_t firstGraphicsQueueFamily          = 0;
    constexpr VkQueueFlags kGraphicsAndCompute = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    for (uint32_t familyIndex = 0; familyIndex < queueCount; ++familyIndex)
    {
        const auto &queueInfo = mQueueFamilyProperties[familyIndex];
        if ((queueInfo.queueFlags & kGraphicsAndCompute) == kGraphicsAndCompute)
        {
            ASSERT(queueInfo.queueCount > 0);
            graphicsQueueFamilyCount++;
            if (firstGraphicsQueueFamily == 0)
            {
                firstGraphicsQueueFamily = familyIndex;
            }
            break;
        }
    }

    ANGLE_VK_CHECK(displayVk, graphicsQueueFamilyCount > 0, VK_ERROR_INITIALIZATION_FAILED);

    // If only one queue family, go ahead and initialize the device. If there is more than one
    // queue, we'll have to wait until we see a WindowSurface to know which supports present.
    if (graphicsQueueFamilyCount == 1)
    {
        ANGLE_TRY(initializeDevice(displayVk, firstGraphicsQueueFamily));
    }

    // Store the physical device memory properties so we can find the right memory pools.
    mMemoryProperties.init(mPhysicalDevice);

    GlslangWrapper::Initialize();

    // Initialize the format table.
    mFormatTable.initialize(this, &mNativeTextureCaps, &mNativeCaps.compressedTextureFormats);

    return angle::Result::Continue;
}

angle::Result RendererVk::initializeDevice(DisplayVk *displayVk, uint32_t queueFamilyIndex)
{
    uint32_t deviceLayerCount = 0;
    ANGLE_VK_TRY(displayVk,
                 vkEnumerateDeviceLayerProperties(mPhysicalDevice, &deviceLayerCount, nullptr));

    std::vector<VkLayerProperties> deviceLayerProps(deviceLayerCount);
    if (deviceLayerCount > 0)
    {
        ANGLE_VK_TRY(displayVk, vkEnumerateDeviceLayerProperties(mPhysicalDevice, &deviceLayerCount,
                                                                 deviceLayerProps.data()));
    }

    VulkanLayerVector enabledDeviceLayerNames;
    if (mEnableValidationLayers)
    {
        mEnableValidationLayers =
            GetAvailableValidationLayers(deviceLayerProps, false, &enabledDeviceLayerNames);
    }

    const char *wsiLayer = displayVk->getWSILayer();
    if (wsiLayer)
    {
        enabledDeviceLayerNames.push_back(wsiLayer);
    }

    // Enumerate device extensions that are provided by the vulkan
    // implementation and implicit layers.
    uint32_t deviceExtensionCount = 0;
    ANGLE_VK_TRY(displayVk, vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr,
                                                                 &deviceExtensionCount, nullptr));

    std::vector<VkExtensionProperties> deviceExtensionProps(deviceExtensionCount);
    if (deviceExtensionCount > 0)
    {
        ANGLE_VK_TRY(displayVk, vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr,
                                                                     &deviceExtensionCount,
                                                                     deviceExtensionProps.data()));
    }

    // Enumerate device extensions that are provided by explicit layers.
    for (const char *layerName : enabledDeviceLayerNames)
    {
        uint32_t previousExtensionCount    = static_cast<uint32_t>(deviceExtensionProps.size());
        uint32_t deviceLayerExtensionCount = 0;
        ANGLE_VK_TRY(displayVk,
                     vkEnumerateDeviceExtensionProperties(mPhysicalDevice, layerName,
                                                          &deviceLayerExtensionCount, nullptr));
        deviceExtensionProps.resize(previousExtensionCount + deviceLayerExtensionCount);
        ANGLE_VK_TRY(displayVk, vkEnumerateDeviceExtensionProperties(
                                    mPhysicalDevice, layerName, &deviceLayerExtensionCount,
                                    deviceExtensionProps.data() + previousExtensionCount));
    }

    ExtensionNameList deviceExtensionNames;
    if (!deviceExtensionProps.empty())
    {
        ASSERT(deviceExtensionNames.size() <= deviceExtensionProps.size());
        for (const VkExtensionProperties &prop : deviceExtensionProps)
        {
            deviceExtensionNames.push_back(prop.extensionName);
        }
        std::sort(deviceExtensionNames.begin(), deviceExtensionNames.end(), StrLess);
    }

    ExtensionNameList enabledDeviceExtensions;
    enabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    initFeatures(deviceExtensionNames);
    OverrideFeaturesWithDisplayState(&mFeatures, displayVk->getState());
    mFeaturesInitialized = true;

    // Selectively enable KHR_MAINTENANCE1 to support viewport flipping.
    if ((getFeatures().flipViewportY.enabled) &&
        (mPhysicalDeviceProperties.apiVersion < VK_MAKE_VERSION(1, 1, 0)))
    {
        enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    }
    if (getFeatures().supportsIncrementalPresent.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
    }

    if (getFeatures().supportsAndroidHardwareBuffer.enabled ||
        getFeatures().supportsExternalMemoryFd.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    }

#if defined(ANGLE_PLATFORM_ANDROID)
    if (getFeatures().supportsAndroidHardwareBuffer.enabled)
    {
        enabledDeviceExtensions.push_back(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME);
        enabledDeviceExtensions.push_back(
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
        InitExternalMemoryHardwareBufferANDROIDFunctions(mInstance);
    }
#else
    ASSERT(!getFeatures().supportsAndroidHardwareBuffer.enabled);
#endif

    if (getFeatures().supportsExternalMemoryFd.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalSemaphoreFd.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
        InitExternalSemaphoreFdFunctions(mInstance);
    }

    if (getFeatures().supportsExternalSemaphoreFd.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    }

    if (getFeatures().supportsShaderStencilExport.enabled)
    {
        enabledDeviceExtensions.push_back(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);
    }

    std::sort(enabledDeviceExtensions.begin(), enabledDeviceExtensions.end(), StrLess);
    ANGLE_VK_TRY(displayVk, VerifyExtensionsPresent(deviceExtensionNames, enabledDeviceExtensions));

    // Select additional features to be enabled
    VkPhysicalDeviceFeatures2KHR enabledFeatures = {};
    enabledFeatures.sType                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enabledFeatures.features.independentBlend    = mPhysicalDeviceFeatures.independentBlend;
    enabledFeatures.features.robustBufferAccess  = mPhysicalDeviceFeatures.robustBufferAccess;
    enabledFeatures.features.samplerAnisotropy   = mPhysicalDeviceFeatures.samplerAnisotropy;
    enabledFeatures.features.vertexPipelineStoresAndAtomics =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics;
    enabledFeatures.features.fragmentStoresAndAtomics =
        mPhysicalDeviceFeatures.fragmentStoresAndAtomics;
    if (!vk::CommandBuffer::ExecutesInline())
    {
        enabledFeatures.features.inheritedQueries = mPhysicalDeviceFeatures.inheritedQueries;
    }

    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT divisorFeatures = {};
    divisorFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
    divisorFeatures.vertexAttributeInstanceRateDivisor = true;

    float zeroPriority                      = 0.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.flags                   = 0;
    queueCreateInfo.queueFamilyIndex        = queueFamilyIndex;
    queueCreateInfo.queueCount              = 1;
    queueCreateInfo.pQueuePriorities        = &zeroPriority;

    // Initialize the device
    VkDeviceCreateInfo createInfo = {};

    createInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.flags                = 0;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos    = &queueCreateInfo;
    createInfo.enabledLayerCount    = static_cast<uint32_t>(enabledDeviceLayerNames.size());
    createInfo.ppEnabledLayerNames  = enabledDeviceLayerNames.data();

    if (vkGetPhysicalDeviceProperties2KHR &&
        ExtensionFound(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME, deviceExtensionNames))
    {
        enabledDeviceExtensions.push_back(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
        enabledFeatures.pNext = &divisorFeatures;

        VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT divisorProperties = {};
        divisorProperties.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;

        VkPhysicalDeviceProperties2 deviceProperties = {};
        deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties.pNext = &divisorProperties;

        vkGetPhysicalDeviceProperties2KHR(mPhysicalDevice, &deviceProperties);
        mMaxVertexAttribDivisor = divisorProperties.maxVertexAttribDivisor;

        createInfo.pNext = &enabledFeatures;
    }
    else
    {
        createInfo.pEnabledFeatures = &enabledFeatures.features;
    }

    if (vkGetPhysicalDeviceProperties2KHR)
    {
        VkPhysicalDeviceProperties2 deviceProperties = {};
        deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties.pNext = &mPhysicalDeviceSubgroupProperties;

        vkGetPhysicalDeviceProperties2KHR(mPhysicalDevice, &deviceProperties);
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames =
        enabledDeviceExtensions.empty() ? nullptr : enabledDeviceExtensions.data();

    ANGLE_VK_TRY(displayVk, vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice));

    mCurrentQueueFamilyIndex = queueFamilyIndex;

    vkGetDeviceQueue(mDevice, mCurrentQueueFamilyIndex, 0, &mQueue);

    // Initialize the vulkan pipeline cache.
    bool success = false;
    ANGLE_TRY(initPipelineCache(displayVk, &mPipelineCache, &success));

    return angle::Result::Continue;
}

angle::Result RendererVk::selectPresentQueueForSurface(DisplayVk *displayVk,
                                                       VkSurfaceKHR surface,
                                                       uint32_t *presentQueueOut)
{
    // We've already initialized a device, and can't re-create it unless it's never been used.
    // TODO(jmadill): Handle the re-creation case if necessary.
    if (mDevice != VK_NULL_HANDLE)
    {
        ASSERT(mCurrentQueueFamilyIndex != std::numeric_limits<uint32_t>::max());

        // Check if the current device supports present on this surface.
        VkBool32 supportsPresent = VK_FALSE;
        ANGLE_VK_TRY(displayVk,
                     vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, mCurrentQueueFamilyIndex,
                                                          surface, &supportsPresent));

        if (supportsPresent == VK_TRUE)
        {
            *presentQueueOut = mCurrentQueueFamilyIndex;
            return angle::Result::Continue;
        }
    }

    // Find a graphics and present queue.
    Optional<uint32_t> newPresentQueue;
    uint32_t queueCount = static_cast<uint32_t>(mQueueFamilyProperties.size());
    constexpr VkQueueFlags kGraphicsAndCompute = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    for (uint32_t queueIndex = 0; queueIndex < queueCount; ++queueIndex)
    {
        const auto &queueInfo = mQueueFamilyProperties[queueIndex];
        if ((queueInfo.queueFlags & kGraphicsAndCompute) == kGraphicsAndCompute)
        {
            VkBool32 supportsPresent = VK_FALSE;
            ANGLE_VK_TRY(displayVk, vkGetPhysicalDeviceSurfaceSupportKHR(
                                        mPhysicalDevice, queueIndex, surface, &supportsPresent));

            if (supportsPresent == VK_TRUE)
            {
                newPresentQueue = queueIndex;
                break;
            }
        }
    }

    ANGLE_VK_CHECK(displayVk, newPresentQueue.valid(), VK_ERROR_INITIALIZATION_FAILED);
    ANGLE_TRY(initializeDevice(displayVk, newPresentQueue.value()));

    *presentQueueOut = newPresentQueue.value();
    return angle::Result::Continue;
}

std::string RendererVk::getVendorString() const
{
    return GetVendorString(mPhysicalDeviceProperties.vendorID);
}

std::string RendererVk::getRendererDescription() const
{
    std::stringstream strstr;

    uint32_t apiVersion = mPhysicalDeviceProperties.apiVersion;

    strstr << "Vulkan ";
    strstr << VK_VERSION_MAJOR(apiVersion) << ".";
    strstr << VK_VERSION_MINOR(apiVersion) << ".";
    strstr << VK_VERSION_PATCH(apiVersion);

    strstr << "(";

    // In the case of NVIDIA, deviceName does not necessarily contain "NVIDIA". Add "NVIDIA" so that
    // Vulkan end2end tests can be selectively disabled on NVIDIA. TODO(jmadill): should not be
    // needed after http://anglebug.com/1874 is fixed and end2end_tests use more sophisticated
    // driver detection.
    if (mPhysicalDeviceProperties.vendorID == VENDOR_ID_NVIDIA)
    {
        strstr << GetVendorString(mPhysicalDeviceProperties.vendorID) << " ";
    }

    strstr << mPhysicalDeviceProperties.deviceName;
    strstr << " (" << gl::FmtHex(mPhysicalDeviceProperties.deviceID) << ")";

    strstr << ")";

    return strstr.str();
}

gl::Version RendererVk::getMaxSupportedESVersion() const
{
    // Current highest supported version
    gl::Version maxVersion = gl::Version(3, 1);

    // Limit to ES3.0 if there are any blockers for 3.1.

    // ES3.1 requires at least one atomic counter buffer and four storage buffers in compute.
    // Atomic counter buffers are emulated with storage buffers.  For simplicity, we always support
    // either none or IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS atomic counter buffers.  So if
    // Vulkan doesn't support at least that many storage buffers in compute, we don't support 3.1.
    const uint32_t kMinimumStorageBuffersForES31 =
        gl::limits::kMinimumComputeStorageBuffers + gl::IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS;
    if (mPhysicalDeviceProperties.limits.maxPerStageDescriptorStorageBuffers <
        kMinimumStorageBuffersForES31)
    {
        maxVersion = std::min(maxVersion, gl::Version(3, 0));
    }

    // ES3.1 requires at least a maximum offset of at least 2047.
    // If the Vulkan implementation can't support that, we cannot support 3.1.
    if (mPhysicalDeviceProperties.limits.maxVertexInputAttributeOffset < 2047)
    {
        maxVersion = std::min(maxVersion, gl::Version(3, 0));
    }

    // Limit to ES2.0 if there are any blockers for 3.0.

    // If the command buffer doesn't support queries, we can't support ES3.
    if (!vk::CommandBuffer::SupportsQueries(mPhysicalDeviceFeatures))
    {
        maxVersion = std::max(maxVersion, gl::Version(2, 0));
    }

    // If independentBlend is not supported, we can't have a mix of has-alpha and emulated-alpha
    // render targets in a framebuffer.  We also cannot perform masked clears of multiple render
    // targets.
    if (!mPhysicalDeviceFeatures.independentBlend)
    {
        maxVersion = std::max(maxVersion, gl::Version(2, 0));
    }

    // If vertexPipelineStoresAndAtomics is not supported, we can't currently support transform
    // feedback.  TODO(syoussefi): this should be conditioned to the extension not being present as
    // well, when that code path is implemented.  http://anglebug.com/3206
    if (!mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics)
    {
        maxVersion = std::max(maxVersion, gl::Version(2, 0));
    }

    return maxVersion;
}

gl::Version RendererVk::getMaxConformantESVersion() const
{
    return std::min(getMaxSupportedESVersion(), gl::Version(2, 0));
}

void RendererVk::initFeatures(const ExtensionNameList &deviceExtensionNames)
{
    // Use OpenGL line rasterization rules by default.
    mFeatures.basicGLLineRasterization.enabled = true;

    if ((mPhysicalDeviceProperties.apiVersion >= VK_MAKE_VERSION(1, 1, 0)) ||
        ExtensionFound(VK_KHR_MAINTENANCE1_EXTENSION_NAME, deviceExtensionNames))
    {
        // TODO(lucferron): Currently disabled on Intel only since many tests are failing and need
        // investigation. http://anglebug.com/2728
        mFeatures.flipViewportY.enabled = !IsIntel(mPhysicalDeviceProperties.vendorID);
    }

#ifdef ANGLE_PLATFORM_WINDOWS
    // http://anglebug.com/2838
    mFeatures.extraCopyBufferRegion.enabled = IsIntel(mPhysicalDeviceProperties.vendorID);

    // http://anglebug.com/3055
    mFeatures.forceCPUPathForCubeMapCopy.enabled = IsIntel(mPhysicalDeviceProperties.vendorID);
#endif

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    platform->overrideFeaturesVk(platform, &mFeatures);

    // Work around incorrect NVIDIA point size range clamping.
    // TODO(jmadill): Narrow driver range once fixed. http://anglebug.com/2970
    if (IsNvidia(mPhysicalDeviceProperties.vendorID))
    {
        mFeatures.clampPointSize.enabled = true;
    }

#if defined(ANGLE_PLATFORM_ANDROID)
    // Work around ineffective compute-graphics barriers on Nexus 5X.
    // TODO(syoussefi): Figure out which other vendors and driver versions are affected.
    // http://anglebug.com/3019
    mFeatures.flushAfterVertexConversion.enabled =
        IsNexus5X(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID);
#endif

    if (ExtensionFound(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME, deviceExtensionNames))
    {
        mFeatures.supportsIncrementalPresent.enabled = true;
    }

#if defined(ANGLE_PLATFORM_ANDROID)
    mFeatures.supportsAndroidHardwareBuffer.enabled =
        ExtensionFound(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
                       deviceExtensionNames) &&
        ExtensionFound(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, deviceExtensionNames);
#endif

    if (ExtensionFound(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, deviceExtensionNames))
    {
        mFeatures.supportsExternalMemoryFd.enabled = true;
    }

    if (ExtensionFound(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, deviceExtensionNames))
    {
        mFeatures.supportsExternalSemaphoreFd.enabled = true;
    }

    if (ExtensionFound(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME, deviceExtensionNames))
    {
        mFeatures.supportsShaderStencilExport.enabled = true;
    }

    // TODO(syoussefi): when the code path using the extension is implemented, this should be
    // conditioned to the extension not being present as well.  http://anglebug.com/3206
    if (mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics)
    {
        mFeatures.emulateTransformFeedback.enabled = true;
    }

    if (IsLinux() && IsIntel(mPhysicalDeviceProperties.vendorID))
    {
        mFeatures.disableFifoPresentMode.enabled = true;
    }

    if (IsAndroid() && IsQualcomm(mPhysicalDeviceProperties.vendorID))
    {
        if (vk::CommandBuffer::ExecutesInline())
        {
            mFeatures.restartRenderPassAfterLoadOpClear.enabled = true;
        }

        mFeatures.bindEmptyForUnusedDescriptorSets.enabled = true;

        mFeatures.forceOldRewriteStructSamplers.enabled = true;
    }

    if (IsWindows() && IsIntel(mPhysicalDeviceProperties.vendorID))
    {
        mFeatures.forceNonZeroScissor.enabled = true;
    }

    if (IsIntel(mPhysicalDeviceProperties.vendorID) ||
        (IsWindows() && IsAMD(mPhysicalDeviceProperties.vendorID)))
    {
        mFeatures.perFrameWindowSizeQuery.enabled = true;
    }

    if (IsWindows() && IsAMD(mPhysicalDeviceProperties.vendorID))
    {
        // Disabled on AMD/windows due to buggy behavior.
        mFeatures.disallowSeamfulCubeMapEmulation.enabled = true;
    }

    if (IsAndroid() && IsQualcomm(mPhysicalDeviceProperties.vendorID))
    {
        mFeatures.forceD16TexFilter.enabled = true;
    }

    if (IsAndroid() && IsQualcomm(mPhysicalDeviceProperties.vendorID))
    {
        mFeatures.disableFlippingBlitWithCommand.enabled = true;
    }

    if (IsPixel2(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID) ||
        IsPixel1XL(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID))
    {
        mFeatures.transientCommandBuffer.enabled = true;
    }
}

void RendererVk::initPipelineCacheVkKey()
{
    std::ostringstream hashStream("ANGLE Pipeline Cache: ", std::ios_base::ate);
    // Add the pipeline cache UUID to make sure the blob cache always gives a compatible pipeline
    // cache.  It's not particularly necessary to write it as a hex number as done here, so long as
    // there is no '\0' in the result.
    for (const uint32_t c : mPhysicalDeviceProperties.pipelineCacheUUID)
    {
        hashStream << std::hex << c;
    }
    // Add the vendor and device id too for good measure.
    hashStream << std::hex << mPhysicalDeviceProperties.vendorID;
    hashStream << std::hex << mPhysicalDeviceProperties.deviceID;

    const std::string &hashString = hashStream.str();
    angle::base::SHA1HashBytes(reinterpret_cast<const unsigned char *>(hashString.c_str()),
                               hashString.length(), mPipelineCacheVkBlobKey.data());
}

angle::Result RendererVk::initPipelineCache(DisplayVk *display,
                                            vk::PipelineCache *pipelineCache,
                                            bool *success)
{
    initPipelineCacheVkKey();

    egl::BlobCache::Value initialData;
    *success = display->getBlobCache()->get(display->getScratchBuffer(), mPipelineCacheVkBlobKey,
                                            &initialData);

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};

    pipelineCacheCreateInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheCreateInfo.flags           = 0;
    pipelineCacheCreateInfo.initialDataSize = *success ? initialData.size() : 0;
    pipelineCacheCreateInfo.pInitialData    = *success ? initialData.data() : nullptr;

    ANGLE_VK_TRY(display, pipelineCache->init(mDevice, pipelineCacheCreateInfo));

    return angle::Result::Continue;
}

angle::Result RendererVk::getPipelineCache(vk::PipelineCache **pipelineCache)
{
    if (mPipelineCacheInitialized)
    {
        *pipelineCache = &mPipelineCache;
        return angle::Result::Continue;
    }

    // We should now recreate the pipeline cache with the blob cache pipeline data.
    vk::PipelineCache pCache;
    bool success = false;
    ANGLE_TRY(initPipelineCache(vk::GetImpl(mDisplay), &pCache, &success));
    if (success)
    {
        // Merge the newly created pipeline cache into the existing one.
        mPipelineCache.merge(mDevice, mPipelineCache.getHandle(), 1, pCache.ptr());
    }
    mPipelineCacheInitialized = true;
    pCache.destroy(mDevice);

    *pipelineCache = &mPipelineCache;
    return angle::Result::Continue;
}

const gl::Caps &RendererVk::getNativeCaps() const
{
    ensureCapsInitialized();
    return mNativeCaps;
}

const gl::TextureCapsMap &RendererVk::getNativeTextureCaps() const
{
    ensureCapsInitialized();
    return mNativeTextureCaps;
}

const gl::Extensions &RendererVk::getNativeExtensions() const
{
    ensureCapsInitialized();
    return mNativeExtensions;
}

const gl::Limitations &RendererVk::getNativeLimitations() const
{
    ensureCapsInitialized();
    return mNativeLimitations;
}

angle::Result RendererVk::getDescriptorSetLayout(
    vk::Context *context,
    const vk::DescriptorSetLayoutDesc &desc,
    vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut)
{
    std::lock_guard<decltype(mDescriptorSetLayoutCacheMutex)> lock(mDescriptorSetLayoutCacheMutex);
    return mDescriptorSetLayoutCache.getDescriptorSetLayout(context, desc, descriptorSetLayoutOut);
}

angle::Result RendererVk::getPipelineLayout(
    vk::Context *context,
    const vk::PipelineLayoutDesc &desc,
    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut)
{
    std::lock_guard<decltype(mPipelineLayoutCacheMutex)> lock(mPipelineLayoutCacheMutex);
    return mPipelineLayoutCache.getPipelineLayout(context, desc, descriptorSetLayouts,
                                                  pipelineLayoutOut);
}

angle::Result RendererVk::syncPipelineCacheVk(DisplayVk *displayVk)
{
    // TODO: Synchronize access to the pipeline/blob caches?
    ASSERT(mPipelineCache.valid());

    if (--mPipelineCacheVkUpdateTimeout > 0)
    {
        return angle::Result::Continue;
    }
    if (!mPipelineCacheDirty)
    {
        mPipelineCacheVkUpdateTimeout = kPipelineCacheVkUpdatePeriod;
        return angle::Result::Continue;
    }

    mPipelineCacheVkUpdateTimeout = kPipelineCacheVkUpdatePeriod;

    // Get the size of the cache.
    size_t pipelineCacheSize = 0;
    VkResult result          = mPipelineCache.getCacheData(mDevice, &pipelineCacheSize, nullptr);
    if (result != VK_INCOMPLETE)
    {
        ANGLE_VK_TRY(displayVk, result);
    }

    angle::MemoryBuffer *pipelineCacheData = nullptr;
    ANGLE_VK_CHECK_ALLOC(displayVk,
                         displayVk->getScratchBuffer(pipelineCacheSize, &pipelineCacheData));

    size_t originalPipelineCacheSize = pipelineCacheSize;
    result = mPipelineCache.getCacheData(mDevice, &pipelineCacheSize, pipelineCacheData->data());
    // Note: currently we don't accept incomplete as we don't expect it (the full size of cache
    // was determined just above), so receiving it hints at an implementation bug we would want
    // to know about early.
    ASSERT(result != VK_INCOMPLETE);
    ANGLE_VK_TRY(displayVk, result);

    // If vkGetPipelineCacheData ends up writing fewer bytes than requested, zero out the rest of
    // the buffer to avoid leaking garbage memory.
    ASSERT(pipelineCacheSize <= originalPipelineCacheSize);
    if (pipelineCacheSize < originalPipelineCacheSize)
    {
        memset(pipelineCacheData->data() + pipelineCacheSize, 0,
               originalPipelineCacheSize - pipelineCacheSize);
    }

    displayVk->getBlobCache()->putApplication(mPipelineCacheVkBlobKey, *pipelineCacheData);
    mPipelineCacheDirty = false;

    return angle::Result::Continue;
}

Serial RendererVk::issueShaderSerial()
{
    return mShaderSerialFactory.generate();
}

// These functions look at the mandatory format for support, and fallback to querying the device (if
// necessary) to test the availability of the bits.
bool RendererVk::hasLinearImageFormatFeatureBits(VkFormat format,
                                                 const VkFormatFeatureFlags featureBits)
{
    return hasFormatFeatureBits<&VkFormatProperties::linearTilingFeatures>(format, featureBits);
}

VkFormatFeatureFlags RendererVk::getImageFormatFeatureBits(VkFormat format,
                                                           const VkFormatFeatureFlags featureBits)
{
    return getFormatFeatureBits<&VkFormatProperties::optimalTilingFeatures>(format, featureBits);
}

bool RendererVk::hasImageFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits)
{
    return hasFormatFeatureBits<&VkFormatProperties::optimalTilingFeatures>(format, featureBits);
}

bool RendererVk::hasBufferFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits)
{
    return hasFormatFeatureBits<&VkFormatProperties::bufferFeatures>(format, featureBits);
}

angle::Result RendererVk::queueSubmit(vk::Context *context,
                                      const VkSubmitInfo &submitInfo,
                                      const vk::Fence &fence)
{
    {
        std::lock_guard<decltype(mQueueMutex)> lock(mQueueMutex);
        ANGLE_VK_TRY(context, vkQueueSubmit(mQueue, 1, &submitInfo, fence.getHandle()));
    }

    ANGLE_TRY(cleanupGarbage(context, false));

    return angle::Result::Continue;
}

angle::Result RendererVk::queueWaitIdle(vk::Context *context)
{
    {
        std::lock_guard<decltype(mQueueMutex)> lock(mQueueMutex);
        ANGLE_VK_TRY(context, vkQueueWaitIdle(mQueue));
    }

    ANGLE_TRY(cleanupGarbage(context, false));

    return angle::Result::Continue;
}

VkResult RendererVk::queuePresent(const VkPresentInfoKHR &presentInfo)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::queuePresent");

    std::lock_guard<decltype(mQueueMutex)> lock(mQueueMutex);

    {
        ANGLE_TRACE_EVENT0("gpu.angle", "vkQueuePresentKHR");
        return vkQueuePresentKHR(mQueue, &presentInfo);
    }
}

Serial RendererVk::nextSerial()
{
    return mQueueSerialFactory.generate();
}

angle::Result RendererVk::newSharedFence(vk::Context *context,
                                         vk::Shared<vk::Fence> *sharedFenceOut)
{
    vk::Fence fence;
    if (mFenceRecycler.empty())
    {
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags             = 0;
        ANGLE_VK_TRY(context, fence.init(mDevice, fenceCreateInfo));
    }
    else
    {
        mFenceRecycler.fetch(&fence);
        ANGLE_VK_TRY(context, fence.reset(mDevice));
    }
    sharedFenceOut->assign(mDevice, std::move(fence));
    return angle::Result::Continue;
}

void RendererVk::addGarbage(vk::Shared<vk::Fence> &&fence,
                            std::vector<vk::GarbageObjectBase> &&garbage)
{
    std::vector<vk::Shared<vk::Fence>> fences;
    fences.push_back(std::move(fence));
    addGarbage(std::move(fences), std::move(garbage));
}

void RendererVk::addGarbage(std::vector<vk::Shared<vk::Fence>> &&fences,
                            std::vector<vk::GarbageObjectBase> &&garbage)
{
    std::lock_guard<decltype(mGarbageMutex)> lock(mGarbageMutex);
    mFencedGarbage.emplace_back(std::move(fences), std::move(garbage));
}

template <VkFormatFeatureFlags VkFormatProperties::*features>
VkFormatFeatureFlags RendererVk::getFormatFeatureBits(VkFormat format,
                                                      const VkFormatFeatureFlags featureBits)
{
    ASSERT(static_cast<uint32_t>(format) < vk::kNumVkFormats);
    VkFormatProperties &deviceProperties = mFormatProperties[format];

    if (deviceProperties.bufferFeatures == kInvalidFormatFeatureFlags)
    {
        // If we don't have the actual device features, see if the requested features are mandatory.
        // If so, there's no need to query the device.
        const VkFormatProperties &mandatoryProperties = vk::GetMandatoryFormatSupport(format);
        if (IsMaskFlagSet(mandatoryProperties.*features, featureBits))
        {
            return featureBits;
        }

        // Otherwise query the format features and cache it.
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &deviceProperties);
        // Workaround for some Android devices that don't indicate filtering
        // support on D16_UNORM and they should.
        if (mFeatures.forceD16TexFilter.enabled && format == VK_FORMAT_D16_UNORM)
        {
            deviceProperties.*features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        }
    }

    return deviceProperties.*features & featureBits;
}

template <VkFormatFeatureFlags VkFormatProperties::*features>
bool RendererVk::hasFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits)
{
    return IsMaskFlagSet(getFormatFeatureBits<features>(format, featureBits), featureBits);
}

angle::Result RendererVk::cleanupGarbage(vk::Context *context, bool block)
{
    std::lock_guard<decltype(mGarbageMutex)> lock(mGarbageMutex);

    auto garbageIter = mFencedGarbage.begin();
    while (garbageIter != mFencedGarbage.end())
    {
        ANGLE_TRY(WaitFences(context, &garbageIter->first, block));
        if (garbageIter->first.empty())
        {
            for (vk::GarbageObjectBase &garbageObject : garbageIter->second)
            {
                garbageObject.destroy(mDevice);
            }
            garbageIter = mFencedGarbage.erase(garbageIter);
        }
        else
        {
            garbageIter++;
        }
    }

    return angle::Result::Continue;
}

void RendererVk::onNewValidationMessage(const std::string &message)
{
    mLastValidationMessage = message;
    ++mValidationMessageCount;
}

std::string RendererVk::getAndClearLastValidationMessage(uint32_t *countSinceLastClear)
{
    *countSinceLastClear    = mValidationMessageCount;
    mValidationMessageCount = 0;

    return std::move(mLastValidationMessage);
}
}  // namespace rx
