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
#include "platform/Platform.h"
#include "third_party/trace_event/trace_event.h"

// Consts
namespace
{
const uint32_t kMockVendorID                              = 0xba5eba11;
const uint32_t kMockDeviceID                              = 0xf005ba11;
constexpr char kMockDeviceName[]                          = "Vulkan Mock Device";
constexpr size_t kInFlightCommandsLimit                   = 100u;
constexpr VkFormatFeatureFlags kInvalidFormatFeatureFlags = static_cast<VkFormatFeatureFlags>(-1);
constexpr size_t kDefaultPoolAllocatorPageSize            = 16 * 1024;
}  // anonymous namespace

namespace rx
{

namespace
{
// We currently only allocate 2 uniform buffer per descriptor set, one for the fragment shader and
// one for the vertex shader.
constexpr size_t kUniformBufferDescriptorsPerDescriptorSet = 2;
// Update the pipeline cache every this many swaps (if 60fps, this means every 10 minutes)
constexpr uint32_t kPipelineCacheVkUpdatePeriod = 10 * 60 * 60;
// Wait a maximum of 10s.  If that times out, we declare it a failure.
constexpr uint64_t kMaxFenceWaitTimeNs = 10'000'000'000llu;
// Per the Vulkan specification, as long as Vulkan 1.1+ is returned by vkEnumerateInstanceVersion,
// ANGLE must indicate the highest version of Vulkan functionality that it uses.  The Vulkan
// validation layers will issue messages for any core functionality that requires a higher version.
// This value must be increased whenever ANGLE starts using functionality from a newer core
// version of Vulkan.
constexpr uint32_t kPreferredVulkanAPIVersion = VK_API_VERSION_1_1;

bool ShouldEnableMockICD(const egl::AttributeMap &attribs)
{
#if !defined(ANGLE_PLATFORM_ANDROID)
    // Mock ICD does not currently run on Android
    return (attribs.get(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                        EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE) ==
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE);
#else
    return false;
#endif  // !defined(ANGLE_PLATFORM_ANDROID)
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
    ScopedVkLoaderEnvironment(bool enableValidationLayers, bool enableMockICD)
        : mEnableValidationLayers(enableValidationLayers),
          mEnableMockICD(enableMockICD),
          mChangedCWD(false),
          mChangedICDPath(false)
    {
// Changing CWD and setting environment variables makes no sense on Android,
// since this code is a part of Java application there.
// Android Vulkan loader doesn't need this either.
#if !defined(ANGLE_PLATFORM_ANDROID) && !defined(ANGLE_PLATFORM_FUCHSIA)
        if (enableMockICD)
        {
            // Override environment variable to use built Mock ICD
            // ANGLE_VK_ICD_JSON gets set to the built mock ICD in BUILD.gn
            mPreviousICDPath = angle::GetEnvironmentVar(g_VkICDPathEnv);
            mChangedICDPath  = angle::SetEnvironmentVar(g_VkICDPathEnv, ANGLE_VK_ICD_JSON);
            if (!mChangedICDPath)
            {
                ERR() << "Error setting Path for Mock/Null Driver.";
                mEnableMockICD = false;
            }
        }
        if (mEnableValidationLayers || mEnableMockICD)
        {
            const auto &cwd = angle::GetCWD();
            if (!cwd.valid())
            {
                ERR() << "Error getting CWD for Vulkan layers init.";
                mEnableValidationLayers = false;
                mEnableMockICD          = false;
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
                    mEnableMockICD          = false;
                }
            }
        }

        // Override environment variable to use the ANGLE layers.
        if (mEnableValidationLayers)
        {
            if (!angle::PrependPathToEnvironmentVar(g_VkLoaderLayersPathEnv, ANGLE_VK_DATA_DIR))
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
        if (mChangedICDPath)
        {
            if (mPreviousICDPath.value().empty())
            {
                angle::UnsetEnvironmentVar(g_VkICDPathEnv);
            }
            else
            {
                angle::SetEnvironmentVar(g_VkICDPathEnv, mPreviousICDPath.value().c_str());
            }
        }
    }

    bool canEnableValidationLayers() const { return mEnableValidationLayers; }

    bool canEnableMockICD() const { return mEnableMockICD; }

  private:
    bool mEnableValidationLayers;
    bool mEnableMockICD;
    bool mChangedCWD;
    Optional<std::string> mPreviousCWD;
    bool mChangedICDPath;
    Optional<std::string> mPreviousICDPath;
};

void ChoosePhysicalDevice(const std::vector<VkPhysicalDevice> &physicalDevices,
                          bool preferMockICD,
                          VkPhysicalDevice *physicalDeviceOut,
                          VkPhysicalDeviceProperties *physicalDevicePropertiesOut)
{
    ASSERT(!physicalDevices.empty());
    if (preferMockICD)
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

void InitializeSubmitInfo(VkSubmitInfo *submitInfo,
                          const vk::PrimaryCommandBuffer &commandBuffer,
                          const vk::Semaphore *waitSemaphore,
                          VkPipelineStageFlags *waitStageMask,
                          const vk::Semaphore *signalSemaphore)
{
    // Verify that the submitInfo has been zero'd out.
    ASSERT(submitInfo->waitSemaphoreCount == 0);
    ASSERT(submitInfo->signalSemaphoreCount == 0);

    submitInfo->sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo->commandBufferCount = 1;
    submitInfo->pCommandBuffers    = commandBuffer.ptr();

    if (waitSemaphore)
    {
        submitInfo->waitSemaphoreCount = 1;
        submitInfo->pWaitSemaphores    = waitSemaphore->ptr();
        submitInfo->pWaitDstStageMask  = waitStageMask;
    }

    if (signalSemaphore)
    {
        submitInfo->signalSemaphoreCount = 1;
        submitInfo->pSignalSemaphores    = signalSemaphore->ptr();
    }
}

// Initially dumping the command graphs is disabled.
constexpr bool kEnableCommandGraphDiagnostics = false;

}  // anonymous namespace

// CommandBatch implementation.
RendererVk::CommandBatch::CommandBatch() = default;

RendererVk::CommandBatch::~CommandBatch() = default;

RendererVk::CommandBatch::CommandBatch(CommandBatch &&other)
{
    *this = std::move(other);
}

RendererVk::CommandBatch &RendererVk::CommandBatch::operator=(CommandBatch &&other)
{
    std::swap(commandPool, other.commandPool);
    std::swap(fence, other.fence);
    std::swap(serial, other.serial);
    return *this;
}

void RendererVk::CommandBatch::destroy(VkDevice device)
{
    commandPool.destroy(device);
    fence.reset(device);
}

// RendererVk implementation.
RendererVk::RendererVk()
    : mDisplay(nullptr),
      mCapsInitialized(false),
      mFeaturesInitialized(false),
      mInstance(VK_NULL_HANDLE),
      mEnableValidationLayers(false),
      mEnableMockICD(false),
      mDebugUtilsMessenger(VK_NULL_HANDLE),
      mDebugReportCallback(VK_NULL_HANDLE),
      mPhysicalDevice(VK_NULL_HANDLE),
      mQueue(VK_NULL_HANDLE),
      mCurrentQueueFamilyIndex(std::numeric_limits<uint32_t>::max()),
      mMaxVertexAttribDivisor(1),
      mDevice(VK_NULL_HANDLE),
      mLastCompletedQueueSerial(mQueueSerialFactory.generate()),
      mCurrentQueueSerial(mQueueSerialFactory.generate()),
      mDeviceLost(false),
      mPipelineCacheVkUpdateTimeout(kPipelineCacheVkUpdatePeriod),
      mPoolAllocator(kDefaultPoolAllocatorPageSize, 1),
      mCommandGraph(kEnableCommandGraphDiagnostics, &mPoolAllocator),
      mGpuEventsEnabled(false),
      mGpuClockSync{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
      mGpuEventTimestampOrigin(0)
{
    VkFormatProperties invalid = {0, 0, kInvalidFormatFeatureFlags};
    mFormatProperties.fill(invalid);
}

RendererVk::~RendererVk() {}

void RendererVk::onDestroy(vk::Context *context)
{
    if (!mInFlightCommands.empty() || !mGarbage.empty())
    {
        // TODO(jmadill): Not nice to pass nullptr here, but shouldn't be a problem.
        (void)finish(context, nullptr, nullptr);
    }

    mUtils.destroy(mDevice);

    mPipelineLayoutCache.destroy(mDevice);
    mDescriptorSetLayoutCache.destroy(mDevice);

    mRenderPassCache.destroy(mDevice);
    mPipelineCache.destroy(mDevice);
    mShaderLibrary.destroy(mDevice);
    mGpuEventQueryPool.destroy(mDevice);

    GlslangWrapper::Release();

    if (mCommandPool.valid())
    {
        mCommandPool.destroy(mDevice);
    }

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

    mCommandGraph.clear();
    nextSerial();
    freeAllInFlightResources();

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
                                                ShouldEnableMockICD(attribs));
    mEnableValidationLayers = scopedEnvironment.canEnableValidationLayers();
    mEnableMockICD          = scopedEnvironment.canEnableMockICD();

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
        uint32_t previousExtensionCount      = instanceExtensionProps.size();
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

    // Enable VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME if available.
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
    instanceInfo.enabledLayerCount   = enabledInstanceLayerNames.size();
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
    ChoosePhysicalDevice(physicalDevices, mEnableMockICD, &mPhysicalDevice,
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
        uint32_t previousExtensionCount    = deviceExtensionProps.size();
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
    mFeaturesInitialized = true;

    // Selectively enable KHR_MAINTENANCE1 to support viewport flipping.
    if ((getFeatures().flipViewportY) &&
        (mPhysicalDeviceProperties.apiVersion < VK_MAKE_VERSION(1, 1, 0)))
    {
        enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    }
    if (getFeatures().supportsIncrementalPresent)
    {
        enabledDeviceExtensions.push_back(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
    }

    if (getFeatures().supportsAndroidHardwareBuffer || getFeatures().supportsExternalMemoryFd)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    }

#if defined(ANGLE_PLATFORM_ANDROID)
    if (getFeatures().supportsAndroidHardwareBuffer)
    {
        enabledDeviceExtensions.push_back(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME);
        enabledDeviceExtensions.push_back(
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
        InitExternalMemoryHardwareBufferANDROIDFunctions(mInstance);
    }
#else
    ASSERT(!getFeatures().supportsAndroidHardwareBuffer);
#endif

    if (getFeatures().supportsExternalMemoryFd)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalSemaphoreFd)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalSemaphoreFd)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    }

    std::sort(enabledDeviceExtensions.begin(), enabledDeviceExtensions.end(), StrLess);
    ANGLE_VK_TRY(displayVk, VerifyExtensionsPresent(deviceExtensionNames, enabledDeviceExtensions));

    // Select additional features to be enabled
    VkPhysicalDeviceFeatures2KHR enabledFeatures = {};
    enabledFeatures.sType                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enabledFeatures.features.independentBlend    = mPhysicalDeviceFeatures.independentBlend;
    enabledFeatures.features.robustBufferAccess  = mPhysicalDeviceFeatures.robustBufferAccess;
    enabledFeatures.features.samplerAnisotropy   = mPhysicalDeviceFeatures.samplerAnisotropy;
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
    createInfo.enabledLayerCount    = enabledDeviceLayerNames.size();
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

    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames =
        enabledDeviceExtensions.empty() ? nullptr : enabledDeviceExtensions.data();

    ANGLE_VK_TRY(displayVk, vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice));

    mCurrentQueueFamilyIndex = queueFamilyIndex;

    vkGetDeviceQueue(mDevice, mCurrentQueueFamilyIndex, 0, &mQueue);

    // Initialize the command pool now that we know the queue family index.
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolInfo.queueFamilyIndex        = mCurrentQueueFamilyIndex;

    ANGLE_VK_TRY(displayVk, mCommandPool.init(mDevice, commandPoolInfo));

    // Initialize the vulkan pipeline cache.
    ANGLE_TRY(initPipelineCache(displayVk));

#if ANGLE_ENABLE_VULKAN_GPU_TRACE_EVENTS
    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // GPU tracing workaround for anglebug.com/2927.  The renderer should not emit gpu events during
    // platform discovery.
    const unsigned char *gpuEventsEnabled =
        platform->getTraceCategoryEnabledFlag(platform, "gpu.angle.gpu");
    mGpuEventsEnabled = gpuEventsEnabled && *gpuEventsEnabled;
#endif

    if (mGpuEventsEnabled)
    {
        // Calculate the difference between CPU and GPU clocks for GPU event reporting.
        ANGLE_TRY(mGpuEventQueryPool.init(displayVk, VK_QUERY_TYPE_TIMESTAMP,
                                          vk::kDefaultTimestampQueryPoolSize));
        ANGLE_TRY(synchronizeCpuGpuTime(displayVk, nullptr, nullptr));
    }

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
    gl::Version maxVersion = gl::Version(3, 0);

#if ANGLE_VULKAN_CONFORMANT_CONFIGS_ONLY
    // TODO: Disallow ES 3.0 until supported. http://crbug.com/angleproject/2950
    maxVersion = gl::Version(2, 0);
#endif

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

    return maxVersion;
}

void RendererVk::initFeatures(const ExtensionNameList &deviceExtensionNames)
{
// Use OpenGL line rasterization rules by default.
// TODO(jmadill): Fix Android support. http://anglebug.com/2830
#if defined(ANGLE_PLATFORM_ANDROID)
    mFeatures.basicGLLineRasterization = false;
#else
    mFeatures.basicGLLineRasterization = true;
#endif  // defined(ANGLE_PLATFORM_ANDROID)

    if ((mPhysicalDeviceProperties.apiVersion >= VK_MAKE_VERSION(1, 1, 0)) ||
        ExtensionFound(VK_KHR_MAINTENANCE1_EXTENSION_NAME, deviceExtensionNames))
    {
        // TODO(lucferron): Currently disabled on Intel only since many tests are failing and need
        // investigation. http://anglebug.com/2728
        mFeatures.flipViewportY = !IsIntel(mPhysicalDeviceProperties.vendorID);
    }

#ifdef ANGLE_PLATFORM_WINDOWS
    // http://anglebug.com/2838
    mFeatures.extraCopyBufferRegion = IsIntel(mPhysicalDeviceProperties.vendorID);

    // http://anglebug.com/3055
    mFeatures.forceCpuPathForCubeMapCopy = IsIntel(mPhysicalDeviceProperties.vendorID);
#endif

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    platform->overrideFeaturesVk(platform, &mFeatures);

    // Work around incorrect NVIDIA point size range clamping.
    // TODO(jmadill): Narrow driver range once fixed. http://anglebug.com/2970
    if (IsNvidia(mPhysicalDeviceProperties.vendorID))
    {
        mFeatures.clampPointSize = true;
    }

#if defined(ANGLE_PLATFORM_ANDROID)
    // Work around ineffective compute-graphics barriers on Nexus 5X.
    // TODO(syoussefi): Figure out which other vendors and driver versions are affected.
    // http://anglebug.com/3019
    mFeatures.flushAfterVertexConversion =
        IsNexus5X(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID);
#endif

    if (ExtensionFound(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME, deviceExtensionNames))
    {
        mFeatures.supportsIncrementalPresent = true;
    }

#if defined(ANGLE_PLATFORM_ANDROID)
    mFeatures.supportsAndroidHardwareBuffer =
        ExtensionFound(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
                       deviceExtensionNames) &&
        ExtensionFound(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, deviceExtensionNames);
#endif

    if (ExtensionFound(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, deviceExtensionNames))
    {
        mFeatures.supportsExternalMemoryFd = true;
    }

    if (ExtensionFound(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, deviceExtensionNames))
    {
        mFeatures.supportsExternalSemaphoreFd = true;
    }

    if (IsLinux() && IsIntel(mPhysicalDeviceProperties.vendorID))
    {
        mFeatures.disableFifoPresentMode = true;
    }

    if (vk::CommandBuffer::ExecutesInline())
    {
        if (IsAndroid() && IsQualcomm(mPhysicalDeviceProperties.vendorID))
        {
            mFeatures.restartRenderPassAfterLoadOpClear = true;
        }
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

angle::Result RendererVk::initPipelineCache(DisplayVk *display)
{
    initPipelineCacheVkKey();

    egl::BlobCache::Value initialData;
    bool success = display->getBlobCache()->get(display->getScratchBuffer(),
                                                mPipelineCacheVkBlobKey, &initialData);

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};

    pipelineCacheCreateInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheCreateInfo.flags           = 0;
    pipelineCacheCreateInfo.initialDataSize = success ? initialData.size() : 0;
    pipelineCacheCreateInfo.pInitialData    = success ? initialData.data() : nullptr;

    ANGLE_VK_TRY(display, mPipelineCache.init(mDevice, pipelineCacheCreateInfo));
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

uint32_t RendererVk::getMaxActiveTextures()
{
    // TODO(lucferron): expose this limitation to GL in Context Caps
    return std::min<uint32_t>(mPhysicalDeviceProperties.limits.maxPerStageDescriptorSamplers,
                              gl::IMPLEMENTATION_MAX_ACTIVE_TEXTURES);
}

const vk::CommandPool &RendererVk::getCommandPool() const
{
    return mCommandPool;
}

angle::Result RendererVk::finish(vk::Context *context,
                                 const vk::Semaphore *waitSemaphore,
                                 const vk::Semaphore *signalSemaphore)
{
    if (!mCommandGraph.empty())
    {
        TRACE_EVENT0("gpu.angle", "RendererVk::finish");

        vk::Scoped<vk::PrimaryCommandBuffer> commandBatch(mDevice);
        ANGLE_TRY(flushCommandGraph(context, &commandBatch.get()));

        VkSubmitInfo submitInfo       = {};
        VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        InitializeSubmitInfo(&submitInfo, commandBatch.get(), waitSemaphore, &waitMask,
                             signalSemaphore);

        ANGLE_TRY(submitFrame(context, submitInfo, std::move(commandBatch.get())));
    }
    else
    {
        // If the sempahores were non-null, there will be breaks in the semaphore chain by not
        // submitting them.
        // TODO(geofflang): We can request the semaphores from the surface just before submission
        // once contexts hold the command graph.  http://anglebug.com/2464
        ASSERT(waitSemaphore == nullptr);
        ASSERT(signalSemaphore == nullptr);
    }

    ASSERT(mQueue != VK_NULL_HANDLE);
    ANGLE_VK_TRY(context, vkQueueWaitIdle(mQueue));
    freeAllInFlightResources();

    if (mGpuEventsEnabled)
    {
        // This loop should in practice execute once since the queue is already idle.
        while (mInFlightGpuEventQueries.size() > 0)
        {
            ANGLE_TRY(checkCompletedGpuEvents(context));
        }
        // Recalculate the CPU/GPU time difference to account for clock drifting.  Avoid unnecessary
        // synchronization if there is no event to be adjusted (happens when finish() gets called
        // multiple times towards the end of the application).
        if (mGpuEvents.size() > 0)
        {
            ANGLE_TRY(synchronizeCpuGpuTime(context, nullptr, nullptr));
        }
    }

    return angle::Result::Continue;
}

void RendererVk::freeAllInFlightResources()
{
    for (CommandBatch &batch : mInFlightCommands)
    {
        // On device loss we need to wait for fence to be signaled before destroying it
        if (mDeviceLost)
        {
            VkResult status = batch.fence.get().wait(mDevice, kMaxFenceWaitTimeNs);
            // If wait times out, it is probably not possible to recover from lost device
            ASSERT(status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST);
        }
        batch.commandPool.destroy(mDevice);
        batch.fence.reset(mDevice);
    }
    mInFlightCommands.clear();

    for (auto &garbage : mGarbage)
    {
        garbage.destroy(mDevice);
    }
    mGarbage.clear();

    mLastCompletedQueueSerial = mLastSubmittedQueueSerial;
}

angle::Result RendererVk::checkCompletedCommands(vk::Context *context)
{
    int finishedCount = 0;

    for (CommandBatch &batch : mInFlightCommands)
    {
        VkResult result = batch.fence.get().getStatus(mDevice);
        if (result == VK_NOT_READY)
        {
            break;
        }
        ANGLE_VK_TRY(context, result);

        ASSERT(batch.serial > mLastCompletedQueueSerial);
        mLastCompletedQueueSerial = batch.serial;

        batch.fence.reset(mDevice);
        TRACE_EVENT0("gpu.angle", "commandPool.destroy");
        batch.commandPool.destroy(mDevice);
        ++finishedCount;
    }

    mInFlightCommands.erase(mInFlightCommands.begin(), mInFlightCommands.begin() + finishedCount);

    size_t freeIndex = 0;
    for (; freeIndex < mGarbage.size(); ++freeIndex)
    {
        if (!mGarbage[freeIndex].destroyIfComplete(mDevice, mLastCompletedQueueSerial))
            break;
    }

    // Remove the entries from the garbage list - they should be ready to go.
    if (freeIndex > 0)
    {
        mGarbage.erase(mGarbage.begin(), mGarbage.begin() + freeIndex);
    }

    return angle::Result::Continue;
}

angle::Result RendererVk::submitFrame(vk::Context *context,
                                      const VkSubmitInfo &submitInfo,
                                      vk::PrimaryCommandBuffer &&commandBuffer)
{
    TRACE_EVENT0("gpu.angle", "RendererVk::submitFrame");

    vk::Scoped<CommandBatch> scopedBatch(mDevice);
    CommandBatch &batch = scopedBatch.get();
    ANGLE_TRY(getSubmitFence(context, &batch.fence));

    ANGLE_VK_TRY(context, vkQueueSubmit(mQueue, 1, &submitInfo, batch.fence.get().getHandle()));

    // Store this command buffer in the in-flight list.
    batch.commandPool = std::move(mCommandPool);
    batch.serial      = mCurrentQueueSerial;

    mInFlightCommands.emplace_back(scopedBatch.release());

    // Make sure a new fence is created for the next submission.
    mSubmitFence.reset(mDevice);

    // CPU should be throttled to avoid mInFlightCommands from growing too fast.  That is done on
    // swap() though, and there could be multiple submissions in between (through glFlush() calls),
    // so the limit is larger than the expected number of images.
    ASSERT(mInFlightCommands.size() <= kInFlightCommandsLimit);

    nextSerial();

    ANGLE_TRY(checkCompletedCommands(context));

    if (mGpuEventsEnabled)
    {
        ANGLE_TRY(checkCompletedGpuEvents(context));
    }

    // Simply null out the command buffer here - it was allocated using the command pool.
    commandBuffer.releaseHandle();

    // Reallocate the command pool for next frame.
    // TODO(jmadill): Consider reusing command pools.
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex        = mCurrentQueueFamilyIndex;

    ANGLE_VK_TRY(context, mCommandPool.init(mDevice, poolInfo));
    return angle::Result::Continue;
}

void RendererVk::nextSerial()
{
    // Increment the queue serial. If this fails, we should restart ANGLE.
    mLastSubmittedQueueSerial = mCurrentQueueSerial;
    mCurrentQueueSerial       = mQueueSerialFactory.generate();

    // Notify the Contexts that they should be starting new command buffers.
    // We use one command pool per serial/submit associated with this VkQueue. We can also
    // have multiple Contexts sharing one VkQueue. In ContextVk::setupDraw we don't explicitly
    // check for a new serial when starting a new command buffer. We just check that the current
    // recording command buffer is valid. Thus we need to explicitly notify every other Context
    // using this VkQueue that they their current command buffer is no longer valid.
    for (gl::Context *context : mDisplay->getContextSet())
    {
        ContextVk *contextVk = vk::GetImpl(context);
        contextVk->onCommandBufferFinished();
    }
}

bool RendererVk::isSerialInUse(Serial serial) const
{
    return serial > mLastCompletedQueueSerial;
}

angle::Result RendererVk::finishToSerial(vk::Context *context, Serial serial)
{
    if (!isSerialInUse(serial) || mInFlightCommands.empty())
    {
        return angle::Result::Continue;
    }

    // Find the first batch with serial equal to or bigger than given serial (note that
    // the batch serials are unique, otherwise upper-bound would have been necessary).
    size_t batchIndex = mInFlightCommands.size() - 1;
    for (size_t i = 0; i < mInFlightCommands.size(); ++i)
    {
        if (mInFlightCommands[i].serial >= serial)
        {
            batchIndex = i;
            break;
        }
    }
    const CommandBatch &batch = mInFlightCommands[batchIndex];

    // Wait for it finish
    VkResult status = batch.fence.get().wait(mDevice, kMaxFenceWaitTimeNs);

    // Don't tolerate timeout.  If such a large wait time results in timeout, something's wrong.
    ANGLE_VK_TRY(context, status);

    // Clean up finished batches.
    return checkCompletedCommands(context);
}

angle::Result RendererVk::getCompatibleRenderPass(vk::Context *context,
                                                  const vk::RenderPassDesc &desc,
                                                  vk::RenderPass **renderPassOut)
{
    return mRenderPassCache.getCompatibleRenderPass(context, mCurrentQueueSerial, desc,
                                                    renderPassOut);
}

angle::Result RendererVk::getRenderPassWithOps(vk::Context *context,
                                               const vk::RenderPassDesc &desc,
                                               const vk::AttachmentOpsArray &ops,
                                               vk::RenderPass **renderPassOut)
{
    return mRenderPassCache.getRenderPassWithOps(context, mCurrentQueueSerial, desc, ops,
                                                 renderPassOut);
}

vk::CommandGraph *RendererVk::getCommandGraph()
{
    return &mCommandGraph;
}

angle::Result RendererVk::flushCommandGraph(vk::Context *context,
                                            vk::PrimaryCommandBuffer *commandBatch)
{
    return mCommandGraph.submitCommands(context, mCurrentQueueSerial, &mRenderPassCache,
                                        &mCommandPool, commandBatch);
}

angle::Result RendererVk::flush(vk::Context *context,
                                const vk::Semaphore *waitSemaphore,
                                const vk::Semaphore *signalSemaphore)
{
    if (mCommandGraph.empty())
    {
        // If the sempahores were non-null, there will be breaks in the semaphore chain by not
        // submitting them.
        // TODO(geofflang): We can request the semaphores from the surface just before submission
        // once contexts hold the command graph.  http://anglebug.com/2464
        ASSERT(waitSemaphore == nullptr);
        ASSERT(signalSemaphore == nullptr);
        return angle::Result::Continue;
    }

    TRACE_EVENT0("gpu.angle", "RendererVk::flush");

    vk::Scoped<vk::PrimaryCommandBuffer> commandBatch(mDevice);
    ANGLE_TRY(flushCommandGraph(context, &commandBatch.get()));

    VkSubmitInfo submitInfo       = {};
    VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    InitializeSubmitInfo(&submitInfo, commandBatch.get(), waitSemaphore, &waitMask,
                         signalSemaphore);

    ANGLE_TRY(submitFrame(context, submitInfo, commandBatch.release()));

    return angle::Result::Continue;
}

Serial RendererVk::issueShaderSerial()
{
    return mShaderSerialFactory.generate();
}

angle::Result RendererVk::getDescriptorSetLayout(
    vk::Context *context,
    const vk::DescriptorSetLayoutDesc &desc,
    vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut)
{
    return mDescriptorSetLayoutCache.getDescriptorSetLayout(context, desc, descriptorSetLayoutOut);
}

angle::Result RendererVk::getPipelineLayout(
    vk::Context *context,
    const vk::PipelineLayoutDesc &desc,
    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut)
{
    return mPipelineLayoutCache.getPipelineLayout(context, desc, descriptorSetLayouts,
                                                  pipelineLayoutOut);
}

angle::Result RendererVk::syncPipelineCacheVk(DisplayVk *displayVk)
{
    ASSERT(mPipelineCache.valid());

    if (--mPipelineCacheVkUpdateTimeout > 0)
    {
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

    return angle::Result::Continue;
}

angle::Result RendererVk::getSubmitFence(vk::Context *context,
                                         vk::Shared<vk::Fence> *sharedFenceOut)
{
    if (!mSubmitFence.isReferenced())
    {
        vk::Fence fence;

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags             = 0;

        ANGLE_VK_TRY(context, fence.init(mDevice, fenceCreateInfo));

        mSubmitFence.assign(mDevice, std::move(fence));
    }

    sharedFenceOut->copy(mDevice, mSubmitFence);
    return angle::Result::Continue;
}

angle::Result RendererVk::getTimestamp(vk::Context *context, uint64_t *timestampOut)
{
    // The intent of this function is to query the timestamp without stalling the GPU.  Currently,
    // that seems impossible, so instead, we are going to make a small submission with just a
    // timestamp query.  First, the disjoint timer query extension says:
    //
    // > This will return the GL time after all previous commands have reached the GL server but
    // have not yet necessarily executed.
    //
    // The previous commands are stored in the command graph at the moment and are not yet flushed.
    // The wording allows us to make a submission to get the timestamp without performing a flush.
    //
    // Second:
    //
    // > By using a combination of this synchronous get command and the asynchronous timestamp query
    // object target, applications can measure the latency between when commands reach the GL server
    // and when they are realized in the framebuffer.
    //
    // This fits with the above strategy as well, although inevitably we are possibly introducing a
    // GPU bubble.  This function directly generates a command buffer and submits it instead of
    // using the other member functions.  This is to avoid changing any state, such as the queue
    // serial.

    // Create a query used to receive the GPU timestamp
    vk::Scoped<vk::DynamicQueryPool> timestampQueryPool(mDevice);
    vk::QueryHelper timestampQuery;
    ANGLE_TRY(timestampQueryPool.get().init(context, VK_QUERY_TYPE_TIMESTAMP, 1));
    ANGLE_TRY(timestampQueryPool.get().allocateQuery(context, &timestampQuery));

    // Record the command buffer
    vk::Scoped<vk::PrimaryCommandBuffer> commandBatch(mDevice);
    vk::PrimaryCommandBuffer &commandBuffer = commandBatch.get();

    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.commandPool                 = mCommandPool.getHandle();
    commandBufferInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferInfo.commandBufferCount          = 1;

    ANGLE_VK_TRY(context, commandBuffer.init(mDevice, commandBufferInfo));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = 0;
    beginInfo.pInheritanceInfo         = nullptr;

    ANGLE_VK_TRY(context, commandBuffer.begin(beginInfo));

    commandBuffer.resetQueryPool(timestampQuery.getQueryPool()->getHandle(),
                                 timestampQuery.getQuery(), 1);
    commandBuffer.writeTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 timestampQuery.getQueryPool()->getHandle(),
                                 timestampQuery.getQuery());

    ANGLE_VK_TRY(context, commandBuffer.end());

    // Create fence for the submission
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = 0;

    vk::Scoped<vk::Fence> fence(mDevice);
    ANGLE_VK_TRY(context, fence.get().init(mDevice, fenceInfo));

    // Submit the command buffer
    VkSubmitInfo submitInfo = {};
    InitializeSubmitInfo(&submitInfo, commandBatch.get(), nullptr, nullptr, nullptr);

    ANGLE_VK_TRY(context, vkQueueSubmit(mQueue, 1, &submitInfo, fence.get().getHandle()));

    // Wait for the submission to finish.  Given no semaphores, there is hope that it would execute
    // in parallel with what's already running on the GPU.
    ANGLE_VK_TRY(context, fence.get().wait(mDevice, kMaxFenceWaitTimeNs));

    // Get the query results
    constexpr VkQueryResultFlags queryFlags = VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT;

    ANGLE_VK_TRY(context, timestampQuery.getQueryPool()->getResults(
                              mDevice, timestampQuery.getQuery(), 1, sizeof(*timestampOut),
                              timestampOut, sizeof(*timestampOut), queryFlags));

    timestampQueryPool.get().freeQuery(context, &timestampQuery);

    // Convert results to nanoseconds.
    *timestampOut = static_cast<uint64_t>(
        *timestampOut * static_cast<double>(mPhysicalDeviceProperties.limits.timestampPeriod));

    return angle::Result::Continue;
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

void RendererVk::insertDebugMarker(GLenum source, GLuint id, std::string &&marker)
{
    mCommandGraph.insertDebugMarker(source, std::move(marker));
}

void RendererVk::pushDebugMarker(GLenum source, GLuint id, std::string &&marker)
{
    mCommandGraph.pushDebugMarker(source, std::move(marker));
}

void RendererVk::popDebugMarker()
{
    mCommandGraph.popDebugMarker();
}

angle::Result RendererVk::synchronizeCpuGpuTime(vk::Context *context,
                                                const vk::Semaphore *waitSemaphore,
                                                const vk::Semaphore *signalSemaphore)
{
    ASSERT(mGpuEventsEnabled);

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // To synchronize CPU and GPU times, we need to get the CPU timestamp as close as possible to
    // the GPU timestamp.  The process of getting the GPU timestamp is as follows:
    //
    //             CPU                            GPU
    //
    //     Record command buffer
    //     with timestamp query
    //
    //     Submit command buffer
    //
    //     Post-submission work             Begin execution
    //
    //            ????                    Write timestamp Tgpu
    //
    //            ????                       End execution
    //
    //            ????                    Return query results
    //
    //            ????
    //
    //       Get query results
    //
    // The areas of unknown work (????) on the CPU indicate that the CPU may or may not have
    // finished post-submission work while the GPU is executing in parallel. With no further work,
    // querying CPU timestamps before submission and after getting query results give the bounds to
    // Tgpu, which could be quite large.
    //
    // Using VkEvents, the GPU can be made to wait for the CPU and vice versa, in an effort to
    // reduce this range. This function implements the following procedure:
    //
    //             CPU                            GPU
    //
    //     Record command buffer
    //     with timestamp query
    //
    //     Submit command buffer
    //
    //     Post-submission work             Begin execution
    //
    //            ????                    Set Event GPUReady
    //
    //    Wait on Event GPUReady         Wait on Event CPUReady
    //
    //       Get CPU Time Ts             Wait on Event CPUReady
    //
    //      Set Event CPUReady           Wait on Event CPUReady
    //
    //      Get CPU Time Tcpu              Get GPU Time Tgpu
    //
    //    Wait on Event GPUDone            Set Event GPUDone
    //
    //       Get CPU Time Te                 End Execution
    //
    //            Idle                    Return query results
    //
    //      Get query results
    //
    // If Te-Ts > epsilon, a GPU or CPU interruption can be assumed and the operation can be
    // retried.  Once Te-Ts < epsilon, Tcpu can be taken to presumably match Tgpu.  Finding an
    // epsilon that's valid for all devices may be difficult, so the loop can be performed only a
    // limited number of times and the Tcpu,Tgpu pair corresponding to smallest Te-Ts used for
    // calibration.
    //
    // Note: Once VK_EXT_calibrated_timestamps is ubiquitous, this should be redone.

    // Make sure nothing is running
    ASSERT(mCommandGraph.empty());

    TRACE_EVENT0("gpu.angle", "RendererVk::synchronizeCpuGpuTime");

    // Create a query used to receive the GPU timestamp
    vk::QueryHelper timestampQuery;
    ANGLE_TRY(mGpuEventQueryPool.allocateQuery(context, &timestampQuery));

    // Create the three events
    VkEventCreateInfo eventCreateInfo = {};
    eventCreateInfo.sType             = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    eventCreateInfo.flags             = 0;

    vk::Scoped<vk::Event> cpuReady(mDevice), gpuReady(mDevice), gpuDone(mDevice);
    ANGLE_VK_TRY(context, cpuReady.get().init(mDevice, eventCreateInfo));
    ANGLE_VK_TRY(context, gpuReady.get().init(mDevice, eventCreateInfo));
    ANGLE_VK_TRY(context, gpuDone.get().init(mDevice, eventCreateInfo));

    constexpr uint32_t kRetries = 10;

    // Time suffixes used are S for seconds and Cycles for cycles
    double tightestRangeS = 1e6f;
    double TcpuS          = 0;
    uint64_t TgpuCycles   = 0;
    for (uint32_t i = 0; i < kRetries; ++i)
    {
        // Reset the events
        ANGLE_VK_TRY(context, cpuReady.get().reset(mDevice));
        ANGLE_VK_TRY(context, gpuReady.get().reset(mDevice));
        ANGLE_VK_TRY(context, gpuDone.get().reset(mDevice));

        // Record the command buffer
        vk::Scoped<vk::PrimaryCommandBuffer> commandBatch(mDevice);
        vk::PrimaryCommandBuffer &commandBuffer = commandBatch.get();

        VkCommandBufferAllocateInfo commandBufferInfo = {};
        commandBufferInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandPool        = mCommandPool.getHandle();
        commandBufferInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferInfo.commandBufferCount = 1;

        ANGLE_VK_TRY(context, commandBuffer.init(mDevice, commandBufferInfo));

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = 0;
        beginInfo.pInheritanceInfo         = nullptr;

        ANGLE_VK_TRY(context, commandBuffer.begin(beginInfo));

        commandBuffer.setEvent(gpuReady.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
        commandBuffer.waitEvents(1, cpuReady.get().ptr(), VK_PIPELINE_STAGE_HOST_BIT,
                                 VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, nullptr, 0, nullptr, 0,
                                 nullptr);

        commandBuffer.resetQueryPool(timestampQuery.getQueryPool()->getHandle(),
                                     timestampQuery.getQuery(), 1);
        commandBuffer.writeTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                     timestampQuery.getQueryPool()->getHandle(),
                                     timestampQuery.getQuery());

        commandBuffer.setEvent(gpuDone.get().getHandle(), VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        ANGLE_VK_TRY(context, commandBuffer.end());

        // Submit the command buffer
        VkSubmitInfo submitInfo       = {};
        VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        InitializeSubmitInfo(&submitInfo, commandBatch.get(), waitSemaphore, &waitMask,
                             signalSemaphore);

        ANGLE_TRY(submitFrame(context, submitInfo, std::move(commandBuffer)));

        // Wait for GPU to be ready.  This is a short busy wait.
        VkResult result = VK_EVENT_RESET;
        do
        {
            result = gpuReady.get().getStatus(mDevice);
            if (result != VK_EVENT_SET && result != VK_EVENT_RESET)
            {
                ANGLE_VK_TRY(context, result);
            }
        } while (result == VK_EVENT_RESET);

        double TsS = platform->monotonicallyIncreasingTime(platform);

        // Tell the GPU to go ahead with the timestamp query.
        ANGLE_VK_TRY(context, cpuReady.get().set(mDevice));
        double cpuTimestampS = platform->monotonicallyIncreasingTime(platform);

        // Wait for GPU to be done.  Another short busy wait.
        do
        {
            result = gpuDone.get().getStatus(mDevice);
            if (result != VK_EVENT_SET && result != VK_EVENT_RESET)
            {
                ANGLE_VK_TRY(context, result);
            }
        } while (result == VK_EVENT_RESET);

        double TeS = platform->monotonicallyIncreasingTime(platform);

        // Get the query results
        ANGLE_TRY(finishToSerial(context, getLastSubmittedQueueSerial()));

        constexpr VkQueryResultFlags queryFlags = VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT;

        uint64_t gpuTimestampCycles = 0;
        ANGLE_VK_TRY(context, timestampQuery.getQueryPool()->getResults(
                                  mDevice, timestampQuery.getQuery(), 1, sizeof(gpuTimestampCycles),
                                  &gpuTimestampCycles, sizeof(gpuTimestampCycles), queryFlags));

        // Use the first timestamp queried as origin.
        if (mGpuEventTimestampOrigin == 0)
        {
            mGpuEventTimestampOrigin = gpuTimestampCycles;
        }

        // Take these CPU and GPU timestamps if there is better confidence.
        double confidenceRangeS = TeS - TsS;
        if (confidenceRangeS < tightestRangeS)
        {
            tightestRangeS = confidenceRangeS;
            TcpuS          = cpuTimestampS;
            TgpuCycles     = gpuTimestampCycles;
        }
    }

    mGpuEventQueryPool.freeQuery(context, &timestampQuery);

    // timestampPeriod gives nanoseconds/cycle.
    double TgpuS = (TgpuCycles - mGpuEventTimestampOrigin) *
                   static_cast<double>(mPhysicalDeviceProperties.limits.timestampPeriod) /
                   1'000'000'000.0;

    flushGpuEvents(TgpuS, TcpuS);

    mGpuClockSync.gpuTimestampS = TgpuS;
    mGpuClockSync.cpuTimestampS = TcpuS;

    return angle::Result::Continue;
}

angle::Result RendererVk::traceGpuEventImpl(vk::Context *context,
                                            vk::PrimaryCommandBuffer *commandBuffer,
                                            char phase,
                                            const char *name)
{
    ASSERT(mGpuEventsEnabled);

    GpuEventQuery event;

    event.name   = name;
    event.phase  = phase;
    event.serial = mCurrentQueueSerial;

    ANGLE_TRY(mGpuEventQueryPool.allocateQuery(context, &event.queryPoolIndex, &event.queryIndex));

    commandBuffer->resetQueryPool(
        mGpuEventQueryPool.getQueryPool(event.queryPoolIndex)->getHandle(), event.queryIndex, 1);
    commandBuffer->writeTimestamp(
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        mGpuEventQueryPool.getQueryPool(event.queryPoolIndex)->getHandle(), event.queryIndex);

    mInFlightGpuEventQueries.push_back(std::move(event));

    return angle::Result::Continue;
}

angle::Result RendererVk::checkCompletedGpuEvents(vk::Context *context)
{
    ASSERT(mGpuEventsEnabled);

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    int finishedCount = 0;

    for (GpuEventQuery &eventQuery : mInFlightGpuEventQueries)
    {
        // Only check the timestamp query if the submission has finished.
        if (eventQuery.serial > mLastCompletedQueueSerial)
        {
            break;
        }

        // See if the results are available.
        uint64_t gpuTimestampCycles = 0;
        VkResult result             = mGpuEventQueryPool.getQueryPool(eventQuery.queryPoolIndex)
                              ->getResults(mDevice, eventQuery.queryIndex, 1,
                                           sizeof(gpuTimestampCycles), &gpuTimestampCycles,
                                           sizeof(gpuTimestampCycles), VK_QUERY_RESULT_64_BIT);
        if (result == VK_NOT_READY)
        {
            break;
        }
        ANGLE_VK_TRY(context, result);

        mGpuEventQueryPool.freeQuery(context, eventQuery.queryPoolIndex, eventQuery.queryIndex);

        GpuEvent event;
        event.gpuTimestampCycles = gpuTimestampCycles;
        event.name               = eventQuery.name;
        event.phase              = eventQuery.phase;

        mGpuEvents.emplace_back(event);

        ++finishedCount;
    }

    mInFlightGpuEventQueries.erase(mInFlightGpuEventQueries.begin(),
                                   mInFlightGpuEventQueries.begin() + finishedCount);

    return angle::Result::Continue;
}

void RendererVk::flushGpuEvents(double nextSyncGpuTimestampS, double nextSyncCpuTimestampS)
{
    if (mGpuEvents.size() == 0)
    {
        return;
    }

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    // Find the slope of the clock drift for adjustment
    double lastGpuSyncTimeS  = mGpuClockSync.gpuTimestampS;
    double lastGpuSyncDiffS  = mGpuClockSync.cpuTimestampS - mGpuClockSync.gpuTimestampS;
    double gpuSyncDriftSlope = 0;

    double nextGpuSyncTimeS = nextSyncGpuTimestampS;
    double nextGpuSyncDiffS = nextSyncCpuTimestampS - nextSyncGpuTimestampS;

    // No gpu trace events should have been generated before the clock sync, so if there is no
    // "previous" clock sync, there should be no gpu events (i.e. the function early-outs above).
    ASSERT(mGpuClockSync.gpuTimestampS != std::numeric_limits<double>::max() &&
           mGpuClockSync.cpuTimestampS != std::numeric_limits<double>::max());

    gpuSyncDriftSlope =
        (nextGpuSyncDiffS - lastGpuSyncDiffS) / (nextGpuSyncTimeS - lastGpuSyncTimeS);

    for (const GpuEvent &event : mGpuEvents)
    {
        double gpuTimestampS =
            (event.gpuTimestampCycles - mGpuEventTimestampOrigin) *
            static_cast<double>(mPhysicalDeviceProperties.limits.timestampPeriod) * 1e-9;

        // Account for clock drift.
        gpuTimestampS += lastGpuSyncDiffS + gpuSyncDriftSlope * (gpuTimestampS - lastGpuSyncTimeS);

        // Generate the trace now that the GPU timestamp is available and clock drifts are accounted
        // for.
        static long long eventId = 1;
        static const unsigned char *categoryEnabled =
            TRACE_EVENT_API_GET_CATEGORY_ENABLED("gpu.angle.gpu");
        platform->addTraceEvent(platform, event.phase, categoryEnabled, event.name, eventId++,
                                gpuTimestampS, 0, nullptr, nullptr, nullptr, TRACE_EVENT_FLAG_NONE);
    }

    mGpuEvents.clear();
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
    }

    return deviceProperties.*features & featureBits;
}

template <VkFormatFeatureFlags VkFormatProperties::*features>
bool RendererVk::hasFormatFeatureBits(VkFormat format, const VkFormatFeatureFlags featureBits)
{
    return IsMaskFlagSet(getFormatFeatureBits<features>(format, featureBits), featureBits);
}

uint32_t GetUniformBufferDescriptorCount()
{
    return kUniformBufferDescriptorsPerDescriptorSet;
}

}  // namespace rx
