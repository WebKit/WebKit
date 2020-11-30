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
#include "common/vulkan/vk_google_filtering_precision.h"
#include "common/vulkan/vulkan_icd.h"
#include "gpu_info_util/SystemInfo.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/renderer/driver_utils.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/CompilerVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/VertexArrayVk.h"
#include "libANGLE/renderer/vulkan/vk_caps_utils.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/trace.h"
#include "platform/PlatformMethods.h"

// Consts
namespace
{
constexpr VkFormatFeatureFlags kInvalidFormatFeatureFlags = static_cast<VkFormatFeatureFlags>(-1);
}  // anonymous namespace

namespace rx
{

namespace
{
constexpr uint32_t kMinDefaultUniformBufferSize = 16 * 1024u;
// This size is picked based on experience. Majority of devices support 64K
// maxUniformBufferSize. Since this is per context buffer, a bigger buffer size reduces the
// number of descriptor set allocations, so we picked the maxUniformBufferSize that most
// devices supports. It may needs further tuning based on specific device needs and balance
// between performance and memory usage.
constexpr uint32_t kPreferredDefaultUniformBufferSize = 64 * 1024u;

// Update the pipeline cache every this many swaps.
constexpr uint32_t kPipelineCacheVkUpdatePeriod = 60;
// Per the Vulkan specification, as long as Vulkan 1.1+ is returned by vkEnumerateInstanceVersion,
// ANGLE must indicate the highest version of Vulkan functionality that it uses.  The Vulkan
// validation layers will issue messages for any core functionality that requires a higher version.
// This value must be increased whenever ANGLE starts using functionality from a newer core
// version of Vulkan.
constexpr uint32_t kPreferredVulkanAPIVersion = VK_API_VERSION_1_1;

angle::vk::ICD ChooseICDFromAttribs(const egl::AttributeMap &attribs)
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
            return angle::vk::ICD::Mock;
        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE:
            return angle::vk::ICD::SwiftShader;
        default:
            UNREACHABLE();
            break;
    }
#endif  // !defined(ANGLE_PLATFORM_ANDROID)

    return angle::vk::ICD::Default;
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
    // http://anglebug.com/4883
    "UNASSIGNED-CoreValidation-Shader-InputNotProduced",
    // http://anglebug.com/2796
    "UNASSIGNED-CoreValidation-Shader-PointSizeMissing",
    // http://anglebug.com/3832
    "VUID-VkPipelineInputAssemblyStateCreateInfo-topology-00428",
    // http://anglebug.com/4063
    "VUID-VkDeviceCreateInfo-pNext-pNext",
    "VUID-VkPipelineRasterizationStateCreateInfo-pNext-pNext",
    "VUID_Undefined",
    // http://anglebug.com/3078
    "UNASSIGNED-CoreValidation-Shader-InterfaceTypeMismatch",
    // http://anglebug.com/4583
    "VUID-VkGraphicsPipelineCreateInfo-blendEnable-02023",
    // https://issuetracker.google.com/issues/159493191
    "VUID-vkCmdDraw-None-02690",
    "VUID-vkCmdDrawIndexed-None-02690",
    // Best Practices Skips issuetracker.google.com/156661359
    "UNASSIGNED-BestPractices-vkCreateCommandPool-command-buffer-reset",
    "UNASSIGNED-BestPractices-pipeline-stage-flags",
    "UNASSIGNED-BestPractices-Error-Result",
    "UNASSIGNED-BestPractices-vkAllocateMemory-small-allocation",
    "UNASSIGNED-BestPractices-vkBindMemory-small-dedicated-allocation",
    "UNASSIGNED-BestPractices-vkAllocateMemory-too-many-objects",
    "UNASSIGNED-BestPractices-vkCreateDevice-deprecated-extension",
    "UNASSIGNED-BestPractices-vkCreateRenderPass-image-requires-memory",
    "UNASSIGNED-BestPractices-vkCreateGraphicsPipelines-too-many-instanced-vertex-buffers",
    "UNASSIGNED-BestPractices-DrawState-ClearCmdBeforeDraw",
    "UNASSIGNED-BestPractices-vkCmdClearAttachments-clear-after-load",
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
        case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
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

bool ShouldUseValidationLayers(const egl::AttributeMap &attribs)
{
#if defined(ANGLE_ENABLE_VULKAN_VALIDATION_LAYERS_BY_DEFAULT)
    return ShouldUseDebugLayers(attribs);
#else
    EGLAttrib debugSetting =
        attribs.get(EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE, EGL_DONT_CARE);
    return debugSetting == EGL_TRUE;
#endif  // defined(ANGLE_ENABLE_VULKAN_VALIDATION_LAYERS_BY_DEFAULT)
}

gl::Version LimitVersionTo(const gl::Version &current, const gl::Version &lower)
{
    return std::min(current, lower);
}

ANGLE_MAYBE_UNUSED bool FencePropertiesCompatibleWithAndroid(
    const VkExternalFenceProperties &externalFenceProperties)
{
    // handleType here is the external fence type -
    // we want type compatible with creating and export/dup() Android FD

    // Imported handleType that can be exported - need for vkGetFenceFdKHR()
    if ((externalFenceProperties.exportFromImportedHandleTypes &
         VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR) == 0)
    {
        return false;
    }

    // HandleTypes which can be specified at creating a fence
    if ((externalFenceProperties.compatibleHandleTypes &
         VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR) == 0)
    {
        return false;
    }

    constexpr VkExternalFenceFeatureFlags kFeatureFlags =
        (VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT_KHR |
         VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT_KHR);
    if ((externalFenceProperties.externalFenceFeatures & kFeatureFlags) != kFeatureFlags)
    {
        return false;
    }

    return true;
}

ANGLE_MAYBE_UNUSED bool SemaphorePropertiesCompatibleWithAndroid(
    const VkExternalSemaphoreProperties &externalSemaphoreProperties)
{
    // handleType here is the external semaphore type -
    // we want type compatible with importing an Android FD

    constexpr VkExternalSemaphoreFeatureFlags kFeatureFlags =
        (VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT_KHR);
    if ((externalSemaphoreProperties.externalSemaphoreFeatures & kFeatureFlags) != kFeatureFlags)
    {
        return false;
    }

    return true;
}

}  // namespace

// RendererVk implementation.
RendererVk::RendererVk()
    : mDisplay(nullptr),
      mCapsInitialized(false),
      mInstance(VK_NULL_HANDLE),
      mEnableValidationLayers(false),
      mEnableDebugUtils(false),
      mEnabledICD(angle::vk::ICD::Default),
      mDebugUtilsMessenger(VK_NULL_HANDLE),
      mDebugReportCallback(VK_NULL_HANDLE),
      mPhysicalDevice(VK_NULL_HANDLE),
      mCurrentQueueFamilyIndex(std::numeric_limits<uint32_t>::max()),
      mMaxVertexAttribDivisor(1),
      mMaxVertexAttribStride(0),
      mMinImportedHostPointerAlignment(1),
      mDefaultUniformBufferSize(kPreferredDefaultUniformBufferSize),
      mDevice(VK_NULL_HANDLE),
      mLastCompletedQueueSerial(mQueueSerialFactory.generate()),
      mCurrentQueueSerial(mQueueSerialFactory.generate()),
      mDeviceLost(false),
      mPipelineCacheVkUpdateTimeout(kPipelineCacheVkUpdatePeriod),
      mPipelineCacheDirty(false),
      mPipelineCacheInitialized(false),
      mGlslangInitialized(false)
{
    VkFormatProperties invalid = {0, 0, kInvalidFormatFeatureFlags};
    mFormatProperties.fill(invalid);

    // We currently don't have any big-endian devices in the list of supported platforms.  There are
    // a number of places in the Vulkan backend that make this assumption.  This assertion is made
    // early to fail immediately on big-endian platforms.
    ASSERT(IsLittleEndian());
}

RendererVk::~RendererVk()
{
    mAllocator.release();
    mPipelineCache.release();
    ASSERT(mSharedGarbage.empty());
}

void RendererVk::onDestroy()
{
    if (getFeatures().enableCommandProcessingThread.enabled)
    {
        // Shutdown worker thread
        mCommandProcessor.shutdown(&mCommandProcessorThread);
    }

    // Force all commands to finish by flushing all queues.
    for (VkQueue queue : mQueues)
    {
        if (queue != VK_NULL_HANDLE)
        {
            vkQueueWaitIdle(queue);
        }
    }

    // Then assign an infinite "last completed" serial to force garbage to delete.
    mLastCompletedQueueSerial = Serial::Infinite();
    (void)cleanupGarbage(true);
    ASSERT(mSharedGarbage.empty());

    for (PendingOneOffCommands &pending : mPendingOneOffCommands)
    {
        pending.commandBuffer.releaseHandle();
    }

    mOneOffCommandPool.destroy(mDevice);

    mFenceRecycler.destroy(mDevice);

    mPipelineLayoutCache.destroy(mDevice);
    mDescriptorSetLayoutCache.destroy(mDevice);

    mPipelineCache.destroy(mDevice);
    mSamplerCache.destroy(this);
    mYuvConversionCache.destroy(this);

    mAllocator.destroy();

    if (mGlslangInitialized)
    {
        GlslangRelease();
        mGlslangInitialized = false;
    }

    if (mDevice)
    {
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
    }

    if (mDebugUtilsMessenger)
    {
        vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugUtilsMessenger, nullptr);

        ASSERT(mDebugReportCallback == VK_NULL_HANDLE);
    }
    else if (mDebugReportCallback)
    {
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
    mLastCompletedQueueSerial = mLastSubmittedQueueSerial;
    mDeviceLost               = true;
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
#if defined(ANGLE_SHARED_LIBVULKAN)
    // Set all vk* function ptrs
    ANGLE_VK_TRY(displayVk, volkInitialize());
#endif  // defined(ANGLE_SHARED_LIBVULKAN)

    mDisplay                         = display;
    const egl::AttributeMap &attribs = mDisplay->getAttributeMap();
    angle::vk::ScopedVkLoaderEnvironment scopedEnvironment(ShouldUseValidationLayers(attribs),
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
    mEnableDebugUtils = mEnableValidationLayers &&
                        ExtensionFound(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instanceExtensionNames);

    bool enableDebugReport =
        mEnableValidationLayers && !mEnableDebugUtils &&
        ExtensionFound(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instanceExtensionNames);

    if (mEnableDebugUtils)
    {
        enabledInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    else if (enableDebugReport)
    {
        enabledInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    if (ExtensionFound(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, instanceExtensionNames))
    {
        enabledInstanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsSwapchainColorspace, true);
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

    if (mEnableValidationLayers)
    {
        // Enable best practices output which includes perfdoc layer
        VkValidationFeatureEnableEXT enabledFeatures[] = {
            VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT};
        VkValidationFeaturesEXT validationFeatures = {};
        validationFeatures.sType                   = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validationFeatures.enabledValidationFeatureCount = 1;
        validationFeatures.pEnabledValidationFeatures    = enabledFeatures;
        vk::AddToPNextChain(&instanceInfo, &validationFeatures);
    }

    ANGLE_VK_TRY(displayVk, vkCreateInstance(&instanceInfo, nullptr, &mInstance));
#if defined(ANGLE_SHARED_LIBVULKAN)
    // Load volk if we are linking dynamically
    volkLoadInstance(mInstance);
#endif  // defined(ANGLE_SHARED_LIBVULKAN)

    if (mEnableDebugUtils)
    {
        // Use the newer EXT_debug_utils if it exists.
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitDebugUtilsEXTFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)

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
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitDebugReportEXTFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)

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
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitGetPhysicalDeviceProperties2KHRFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
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

    mGarbageCollectionFlushThreshold =
        static_cast<uint32_t>(mPhysicalDeviceProperties.limits.maxMemoryAllocationCount *
                              kPercentMaxMemoryAllocationCount);

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

    // Create VMA allocator
    ANGLE_VK_TRY(displayVk,
                 mAllocator.init(mPhysicalDevice, mDevice, mInstance, applicationInfo.apiVersion));

    // Store the physical device memory properties so we can find the right memory pools.
    mMemoryProperties.init(mPhysicalDevice);

    if (!mGlslangInitialized)
    {
        GlslangInitialize();
        mGlslangInitialized = true;
    }

    // Initialize the format table.
    mFormatTable.initialize(this, &mNativeTextureCaps, &mNativeCaps.compressedTextureFormats);

    if (getFeatures().enableCommandProcessingThread.enabled)
    {
        mCommandProcessorThread =
            std::thread(&CommandProcessor::processCommandProcessorTasks, &mCommandProcessor);
    }
    return angle::Result::Continue;
}

void RendererVk::queryDeviceExtensionFeatures(const ExtensionNameList &deviceExtensionNames)
{
    // Default initialize all extension features to false.
    mLineRasterizationFeatures = {};
    mLineRasterizationFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;

    mProvokingVertexFeatures = {};
    mProvokingVertexFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT;

    mVertexAttributeDivisorFeatures = {};
    mVertexAttributeDivisorFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;

    mVertexAttributeDivisorProperties = {};
    mVertexAttributeDivisorProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;

    mTransformFeedbackFeatures = {};
    mTransformFeedbackFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;

    mIndexTypeUint8Features       = {};
    mIndexTypeUint8Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;

    mSubgroupProperties       = {};
    mSubgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

    mExternalMemoryHostProperties = {};
    mExternalMemoryHostProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;

    mShaderFloat16Int8Features = {};
    mShaderFloat16Int8Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;

    mExternalFenceProperties       = {};
    mExternalFenceProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES;

    mExternalSemaphoreProperties       = {};
    mExternalSemaphoreProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;

    mSamplerYcbcrConversionFeatures = {};
    mSamplerYcbcrConversionFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;

    if (!vkGetPhysicalDeviceProperties2KHR || !vkGetPhysicalDeviceFeatures2KHR)
    {
        return;
    }

    // Query features and properties.
    VkPhysicalDeviceFeatures2KHR deviceFeatures = {};
    deviceFeatures.sType                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceProperties2 deviceProperties = {};
    deviceProperties.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    // Query line rasterization features
    if (ExtensionFound(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mLineRasterizationFeatures);
    }

    // Query provoking vertex features
    if (ExtensionFound(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mProvokingVertexFeatures);
    }

    // Query attribute divisor features and properties
    if (ExtensionFound(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mVertexAttributeDivisorFeatures);
        vk::AddToPNextChain(&deviceProperties, &mVertexAttributeDivisorProperties);
    }

    // Query transform feedback features
    if (ExtensionFound(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mTransformFeedbackFeatures);
    }

    // Query uint8 index type features
    if (ExtensionFound(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mIndexTypeUint8Features);
    }

    // Query external memory host properties
    if (ExtensionFound(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceProperties, &mExternalMemoryHostProperties);
    }

    // Query Ycbcr conversion properties
    if (ExtensionFound(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mSamplerYcbcrConversionFeatures);
    }

    // Query float16/int8 features
    if (ExtensionFound(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, deviceExtensionNames))
    {
        vk::AddToPNextChain(&deviceFeatures, &mShaderFloat16Int8Features);
    }

    // Query subgroup properties
    vk::AddToPNextChain(&deviceProperties, &mSubgroupProperties);

    vkGetPhysicalDeviceFeatures2KHR(mPhysicalDevice, &deviceFeatures);
    vkGetPhysicalDeviceProperties2KHR(mPhysicalDevice, &deviceProperties);

    // Fence properties
    if (mFeatures.supportsExternalFenceCapabilities.enabled)
    {
        VkPhysicalDeviceExternalFenceInfo externalFenceInfo = {};
        externalFenceInfo.sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO;
        externalFenceInfo.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR;

        vkGetPhysicalDeviceExternalFencePropertiesKHR(mPhysicalDevice, &externalFenceInfo,
                                                      &mExternalFenceProperties);
    }

    // Semaphore properties
    if (mFeatures.supportsExternalSemaphoreCapabilities.enabled)
    {
        VkPhysicalDeviceExternalSemaphoreInfo externalSemaphoreInfo = {};
        externalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
        externalSemaphoreInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR;

        vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(mPhysicalDevice, &externalSemaphoreInfo,
                                                          &mExternalSemaphoreProperties);
    }

    // Clean up pNext chains
    mLineRasterizationFeatures.pNext        = nullptr;
    mProvokingVertexFeatures.pNext          = nullptr;
    mVertexAttributeDivisorFeatures.pNext   = nullptr;
    mVertexAttributeDivisorProperties.pNext = nullptr;
    mTransformFeedbackFeatures.pNext        = nullptr;
    mIndexTypeUint8Features.pNext           = nullptr;
    mSubgroupProperties.pNext               = nullptr;
    mExternalMemoryHostProperties.pNext     = nullptr;
    mLineRasterizationFeatures.pNext        = nullptr;
    mProvokingVertexFeatures.pNext          = nullptr;
    mVertexAttributeDivisorFeatures.pNext   = nullptr;
    mVertexAttributeDivisorProperties.pNext = nullptr;
    mTransformFeedbackFeatures.pNext        = nullptr;
    mIndexTypeUint8Features.pNext           = nullptr;
    mSamplerYcbcrConversionFeatures.pNext   = nullptr;
    mShaderFloat16Int8Features.pNext        = nullptr;
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

    // Queues: map low, med, high priority to whatever is supported up to 3 queues
    uint32_t queueCount = std::min(mQueueFamilyProperties[queueFamilyIndex].queueCount,
                                   static_cast<uint32_t>(egl::ContextPriority::EnumCount));

    constexpr float kVulkanQueuePriorityLow    = 0.0;
    constexpr float kVulkanQueuePriorityMedium = 0.4;
    constexpr float kVulkanQueuePriorityHigh   = 1.0;

    // Index order: Low, High, Medium - so no need to rearrange according to count:
    // If we have 1 queue - all same, if 2 - Low and High, if 3 Low, High and Medium.
    constexpr uint32_t kQueueIndexLow    = 0;
    constexpr uint32_t kQueueIndexHigh   = 1;
    constexpr uint32_t kQueueIndexMedium = 2;

    constexpr float queuePriorities[static_cast<uint32_t>(egl::ContextPriority::EnumCount)] = {
        kVulkanQueuePriorityMedium, kVulkanQueuePriorityHigh, kVulkanQueuePriorityLow};

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.flags                   = 0;
    queueCreateInfo.queueFamilyIndex        = queueFamilyIndex;
    queueCreateInfo.queueCount              = queueCount;
    queueCreateInfo.pQueuePriorities        = queuePriorities;

    // Query extensions and their features.
    queryDeviceExtensionFeatures(deviceExtensionNames);

    // Initialize features and workarounds.
    initFeatures(displayVk, deviceExtensionNames);

    // Enable VK_KHR_get_memory_requirements2, if supported
    if (ExtensionFound(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, deviceExtensionNames))
    {
        enabledDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    }

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

#if defined(ANGLE_PLATFORM_ANDROID)
    if (getFeatures().supportsAndroidHardwareBuffer.enabled)
    {
        enabledDeviceExtensions.push_back(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME);
        enabledDeviceExtensions.push_back(
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
#    if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalMemoryHardwareBufferANDROIDFunctions(mInstance);
#    endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }
#else
    ASSERT(!getFeatures().supportsAndroidHardwareBuffer.enabled);
#endif

#if defined(ANGLE_PLATFORM_GGP)
    if (getFeatures().supportsGGPFrameToken.enabled)
    {
        enabledDeviceExtensions.push_back(VK_GGP_FRAME_TOKEN_EXTENSION_NAME);
    }
    ANGLE_VK_CHECK(displayVk, getFeatures().supportsGGPFrameToken.enabled,
                   VK_ERROR_EXTENSION_NOT_PRESENT);
#else
    ASSERT(!getFeatures().supportsGGPFrameToken.enabled);
#endif

    if (getFeatures().supportsAndroidHardwareBuffer.enabled ||
        getFeatures().supportsExternalMemoryFd.enabled ||
        getFeatures().supportsExternalMemoryFuchsia.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalMemoryFd.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalMemoryFuchsia.enabled)
    {
        enabledDeviceExtensions.push_back(VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalSemaphoreFd.enabled ||
        getFeatures().supportsExternalSemaphoreFuchsia.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalSemaphoreFdFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsExternalSemaphoreCapabilities.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalFenceCapabilities.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalSemaphoreFd.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    }

    if (getFeatures().supportsExternalSemaphoreCapabilities.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME);
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalSemaphoreCapabilitiesFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsExternalFenceFd.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalFenceFdFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsExternalFenceCapabilities.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalFenceCapabilitiesFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsExternalSemaphoreFuchsia.enabled)
    {
        enabledDeviceExtensions.push_back(VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
    }

    if (getFeatures().supportsShaderStencilExport.enabled)
    {
        enabledDeviceExtensions.push_back(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);
    }

    std::sort(enabledDeviceExtensions.begin(), enabledDeviceExtensions.end(), StrLess);
    ANGLE_VK_TRY(displayVk, VerifyExtensionsPresent(deviceExtensionNames, enabledDeviceExtensions));

    // Select additional features to be enabled.
    VkPhysicalDeviceFeatures2KHR enabledFeatures = {};
    enabledFeatures.sType                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    // Used to support framebuffers with multiple attachments:
    enabledFeatures.features.independentBlend = mPhysicalDeviceFeatures.independentBlend;
    // Used to support robust buffer access:
    enabledFeatures.features.robustBufferAccess = mPhysicalDeviceFeatures.robustBufferAccess;
    // Used to support Anisotropic filtering:
    enabledFeatures.features.samplerAnisotropy = mPhysicalDeviceFeatures.samplerAnisotropy;
    // Used to support wide lines:
    enabledFeatures.features.wideLines = mPhysicalDeviceFeatures.wideLines;
    // Used to emulate transform feedback:
    enabledFeatures.features.vertexPipelineStoresAndAtomics =
        mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics;
    // Used to implement storage buffers and images in the fragment shader:
    enabledFeatures.features.fragmentStoresAndAtomics =
        mPhysicalDeviceFeatures.fragmentStoresAndAtomics;
    // Used to support geometry shaders:
    enabledFeatures.features.geometryShader = mPhysicalDeviceFeatures.geometryShader;
    // Used to support EXT_gpu_shader5:
    enabledFeatures.features.shaderImageGatherExtended =
        mPhysicalDeviceFeatures.shaderImageGatherExtended;
    // Used to support EXT_gpu_shader5:
    enabledFeatures.features.shaderUniformBufferArrayDynamicIndexing =
        mPhysicalDeviceFeatures.shaderUniformBufferArrayDynamicIndexing;
    // Used to support EXT_gpu_shader5 and sampler array of array emulation:
    enabledFeatures.features.shaderSampledImageArrayDynamicIndexing =
        mPhysicalDeviceFeatures.shaderSampledImageArrayDynamicIndexing;
    // Used to support atomic counter emulation:
    enabledFeatures.features.shaderStorageBufferArrayDynamicIndexing =
        mPhysicalDeviceFeatures.shaderStorageBufferArrayDynamicIndexing;
    // Used to support APPLE_clip_distance
    enabledFeatures.features.shaderClipDistance = mPhysicalDeviceFeatures.shaderClipDistance;

    if (!vk::CommandBuffer::ExecutesInline())
    {
        enabledFeatures.features.inheritedQueries = mPhysicalDeviceFeatures.inheritedQueries;
    }

    // Setup device initialization struct
    VkDeviceCreateInfo createInfo = {};

    // Based on available extension features, decide on which extensions and features to enable.

    if (mLineRasterizationFeatures.bresenhamLines)
    {
        enabledDeviceExtensions.push_back(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);
        vk::AddToPNextChain(&createInfo, &mLineRasterizationFeatures);
    }

    if (mProvokingVertexFeatures.provokingVertexLast)
    {
        enabledDeviceExtensions.push_back(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
        vk::AddToPNextChain(&createInfo, &mProvokingVertexFeatures);
    }

    if (mVertexAttributeDivisorFeatures.vertexAttributeInstanceRateDivisor)
    {
        enabledDeviceExtensions.push_back(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
        vk::AddToPNextChain(&createInfo, &mVertexAttributeDivisorFeatures);

        // We only store 8 bit divisor in GraphicsPipelineDesc so capping value & we emulate if
        // exceeded
        mMaxVertexAttribDivisor =
            std::min(mVertexAttributeDivisorProperties.maxVertexAttribDivisor,
                     static_cast<uint32_t>(std::numeric_limits<uint8_t>::max()));
    }

    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        enabledDeviceExtensions.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
        vk::AddToPNextChain(&createInfo, &mTransformFeedbackFeatures);
    }

    if (getFeatures().supportsIndexTypeUint8.enabled)
    {
        enabledDeviceExtensions.push_back(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);
        vk::AddToPNextChain(&createInfo, &mIndexTypeUint8Features);
    }

    if (getFeatures().supportsExternalMemoryHost.enabled)
    {
        enabledDeviceExtensions.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
        mMinImportedHostPointerAlignment =
            mExternalMemoryHostProperties.minImportedHostPointerAlignment;
#if !defined(ANGLE_SHARED_LIBVULKAN)
        InitExternalMemoryHostFunctions(mInstance);
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
    }

    if (getFeatures().supportsYUVSamplerConversion.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
        vk::AddToPNextChain(&createInfo, &mSamplerYcbcrConversionFeatures);
    }

    if (getFeatures().supportsShaderFloat16.enabled)
    {
        enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
        vk::AddToPNextChain(&createInfo, &mShaderFloat16Int8Features);
    }

    createInfo.sType                 = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.flags                 = 0;
    createInfo.queueCreateInfoCount  = 1;
    createInfo.pQueueCreateInfos     = &queueCreateInfo;
    createInfo.enabledLayerCount     = static_cast<uint32_t>(enabledDeviceLayerNames.size());
    createInfo.ppEnabledLayerNames   = enabledDeviceLayerNames.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames =
        enabledDeviceExtensions.empty() ? nullptr : enabledDeviceExtensions.data();
    // Enable core features without assuming VkPhysicalDeviceFeatures2KHR is accepted in the pNext
    // chain of VkDeviceCreateInfo.
    createInfo.pEnabledFeatures = &enabledFeatures.features;

    ANGLE_VK_TRY(displayVk, vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice));
#if defined(ANGLE_SHARED_LIBVULKAN)
    // Load volk if we are loading dynamically
    volkLoadDevice(mDevice);
#endif  // defined(ANGLE_SHARED_LIBVULKAN)

    mCurrentQueueFamilyIndex = queueFamilyIndex;

    // When only 1 Queue, use same for all, Low index. Identify as Medium, since it's default.
    VkQueue queue;
    vkGetDeviceQueue(mDevice, mCurrentQueueFamilyIndex, kQueueIndexLow, &queue);
    mQueues[egl::ContextPriority::Low]        = queue;
    mQueues[egl::ContextPriority::Medium]     = queue;
    mQueues[egl::ContextPriority::High]       = queue;
    mPriorities[egl::ContextPriority::Low]    = egl::ContextPriority::Medium;
    mPriorities[egl::ContextPriority::Medium] = egl::ContextPriority::Medium;
    mPriorities[egl::ContextPriority::High]   = egl::ContextPriority::Medium;

    // If at least 2 queues, High has its own queue
    if (queueCount > 1)
    {
        vkGetDeviceQueue(mDevice, mCurrentQueueFamilyIndex, kQueueIndexHigh,
                         &mQueues[egl::ContextPriority::High]);
        mPriorities[egl::ContextPriority::High] = egl::ContextPriority::High;
    }
    // If at least 3 queues, Medium has its own queue. Adjust Low priority.
    if (queueCount > 2)
    {
        vkGetDeviceQueue(mDevice, mCurrentQueueFamilyIndex, kQueueIndexMedium,
                         &mQueues[egl::ContextPriority::Medium]);
        mPriorities[egl::ContextPriority::Low] = egl::ContextPriority::Low;
    }

#if !defined(ANGLE_SHARED_LIBVULKAN)
    if (getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        InitTransformFeedbackEXTFunctions(mDevice);
    }
    if (getFeatures().supportsYUVSamplerConversion.enabled)
    {
        InitSamplerYcbcrKHRFunctions(mDevice);
    }
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)

    if (getFeatures().forceMaxUniformBufferSize16KB.enabled)
    {
        mDefaultUniformBufferSize = kMinDefaultUniformBufferSize;
    }
    // Cap it with the driver limit
    mDefaultUniformBufferSize = std::min(
        mDefaultUniformBufferSize, getPhysicalDeviceProperties().limits.maxUniformBufferRange);

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

    // Early out without downgrading ES version if mock ICD enabled.
    // Mock ICD doesn't expose sufficient capabilities yet.
    // https://github.com/KhronosGroup/Vulkan-Tools/issues/84
    if (isMockICDEnabled())
    {
        return maxVersion;
    }

    // Limit to ES3.1 if there are any blockers for 3.2.
    if (!vk::CanSupportGPUShader5EXT(mPhysicalDeviceFeatures))
    {
        maxVersion = LimitVersionTo(maxVersion, {3, 1});
    }

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
        maxVersion = LimitVersionTo(maxVersion, {3, 0});
    }

    // ES3.1 requires at least a maximum offset of at least 2047.
    // If the Vulkan implementation can't support that, we cannot support 3.1.
    if (mPhysicalDeviceProperties.limits.maxVertexInputAttributeOffset < 2047)
    {
        maxVersion = LimitVersionTo(maxVersion, {3, 0});
    }

    // Limit to ES2.0 if there are any blockers for 3.0.
    // TODO: http://anglebug.com/3972 Limit to GLES 2.0 if flat shading can't be emulated

    // Multisample textures (ES3.1) and multisample renderbuffers (ES3.0) require the Vulkan driver
    // to support the standard sample locations (in order to pass dEQP tests that check these
    // locations).  If the Vulkan implementation can't support that, we cannot support 3.0/3.1.
    if (mPhysicalDeviceProperties.limits.standardSampleLocations != VK_TRUE)
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    // If the command buffer doesn't support queries, we can't support ES3.
    if (!vk::CommandBuffer::SupportsQueries(mPhysicalDeviceFeatures))
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    // If independentBlend is not supported, we can't have a mix of has-alpha and emulated-alpha
    // render targets in a framebuffer.  We also cannot perform masked clears of multiple render
    // targets.
    if (!mPhysicalDeviceFeatures.independentBlend)
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    // If the Vulkan transform feedback extension is not present, we use an emulation path that
    // requires the vertexPipelineStoresAndAtomics feature. Without the extension or this feature,
    // we can't currently support transform feedback.
    if (!mFeatures.supportsTransformFeedbackExtension.enabled &&
        !mFeatures.emulateTransformFeedback.enabled)
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    // Limit to GLES 2.0 if maxPerStageDescriptorUniformBuffers is too low.
    // Table 6.31 MAX_VERTEX_UNIFORM_BLOCKS minimum value = 12
    // Table 6.32 MAX_FRAGMENT_UNIFORM_BLOCKS minimum value = 12
    // NOTE: We reserve some uniform buffers for emulation, so use the NativeCaps which takes this
    // into account, rather than the physical device maxPerStageDescriptorUniformBuffers limits.
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        if (static_cast<GLuint>(getNativeCaps().maxShaderUniformBlocks[shaderType]) <
            gl::limits::kMinimumShaderUniformBlocks)
        {
            maxVersion = LimitVersionTo(maxVersion, {2, 0});
        }
    }

    // Limit to GLES 2.0 if maxVertexOutputComponents is too low.
    // Table 6.31 MAX VERTEX OUTPUT COMPONENTS minimum value = 64
    // NOTE: We reserve some vertex output components for emulation, so use the NativeCaps which
    // takes this into account, rather than the physical device maxVertexOutputComponents limits.
    if (static_cast<GLuint>(getNativeCaps().maxVertexOutputComponents) <
        gl::limits::kMinimumVertexOutputComponents)
    {
        maxVersion = LimitVersionTo(maxVersion, {2, 0});
    }

    return maxVersion;
}

gl::Version RendererVk::getMaxConformantESVersion() const
{
    return LimitVersionTo(getMaxSupportedESVersion(), {3, 1});
}

void RendererVk::initFeatures(DisplayVk *displayVk, const ExtensionNameList &deviceExtensionNames)
{
    if (displayVk->getState().featuresAllDisabled)
    {
        ApplyFeatureOverrides(&mFeatures, displayVk->getState());
        return;
    }

    bool isAMD      = IsAMD(mPhysicalDeviceProperties.vendorID);
    bool isIntel    = IsIntel(mPhysicalDeviceProperties.vendorID);
    bool isNvidia   = IsNvidia(mPhysicalDeviceProperties.vendorID);
    bool isQualcomm = IsQualcomm(mPhysicalDeviceProperties.vendorID);
    bool isARM      = IsARM(mPhysicalDeviceProperties.vendorID);
    bool isSwS =
        IsSwiftshader(mPhysicalDeviceProperties.vendorID, mPhysicalDeviceProperties.deviceID);

    if (mLineRasterizationFeatures.bresenhamLines == VK_TRUE)
    {
        ASSERT(mLineRasterizationFeatures.sType ==
               VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT);
        ANGLE_FEATURE_CONDITION(&mFeatures, bresenhamLineRasterization, true);
    }
    else
    {
        // Use OpenGL line rasterization rules if extension not available by default.
        // TODO(jmadill): Fix Android support. http://anglebug.com/2830
        ANGLE_FEATURE_CONDITION(&mFeatures, basicGLLineRasterization, !IsAndroid());
    }

    if (mProvokingVertexFeatures.provokingVertexLast == VK_TRUE)
    {
        ASSERT(mProvokingVertexFeatures.sType ==
               VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT);
        ANGLE_FEATURE_CONDITION(&mFeatures, provokingVertex, true);
    }

    // TODO(lucferron): Currently disabled on Intel only since many tests are failing and need
    // investigation. http://anglebug.com/2728
    ANGLE_FEATURE_CONDITION(
        &mFeatures, flipViewportY,
        !IsIntel(mPhysicalDeviceProperties.vendorID) &&
                (mPhysicalDeviceProperties.apiVersion >= VK_MAKE_VERSION(1, 1, 0)) ||
            ExtensionFound(VK_KHR_MAINTENANCE1_EXTENSION_NAME, deviceExtensionNames));

    // http://anglebug.com/2838
    ANGLE_FEATURE_CONDITION(&mFeatures, extraCopyBufferRegion, IsWindows() && isIntel);

    // http://anglebug.com/3055
    ANGLE_FEATURE_CONDITION(&mFeatures, forceCPUPathForCubeMapCopy, IsWindows() && isIntel);

    // Work around incorrect NVIDIA point size range clamping.
    // http://anglebug.com/2970#c10
    // Clamp if driver version is:
    //   < 430 on Windows
    //   < 421 otherwise
    angle::VersionInfo nvidiaVersion;
    if (isNvidia)
    {
        nvidiaVersion =
            angle::ParseNvidiaDriverVersion(this->mPhysicalDeviceProperties.driverVersion);
    }
    ANGLE_FEATURE_CONDITION(&mFeatures, clampPointSize,
                            isNvidia && nvidiaVersion.major < uint32_t(IsWindows() ? 430 : 421));

    // Work around ineffective compute-graphics barriers on Nexus 5X.
    // TODO(syoussefi): Figure out which other vendors and driver versions are affected.
    // http://anglebug.com/3019
    ANGLE_FEATURE_CONDITION(&mFeatures, flushAfterVertexConversion,
                            IsAndroid() && IsNexus5X(mPhysicalDeviceProperties.vendorID,
                                                     mPhysicalDeviceProperties.deviceID));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsIncrementalPresent,
        ExtensionFound(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME, deviceExtensionNames));

#if defined(ANGLE_PLATFORM_ANDROID)
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsAndroidHardwareBuffer,
        IsAndroid() &&
            ExtensionFound(VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
                           deviceExtensionNames) &&
            ExtensionFound(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, deviceExtensionNames));
#endif

#if defined(ANGLE_PLATFORM_GGP)
    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsGGPFrameToken,
        ExtensionFound(VK_GGP_FRAME_TOKEN_EXTENSION_NAME, deviceExtensionNames));
#endif

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalMemoryFd,
        ExtensionFound(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalMemoryFuchsia,
        ExtensionFound(VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsFilteringPrecision,
        ExtensionFound(VK_GOOGLE_SAMPLER_FILTERING_PRECISION_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalFenceCapabilities,
        ExtensionFound(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsExternalSemaphoreCapabilities,
                            ExtensionFound(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
                                           deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalSemaphoreFd,
        ExtensionFound(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalSemaphoreFuchsia,
        ExtensionFound(VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalFenceFd,
        ExtensionFound(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME, deviceExtensionNames));

#if defined(ANGLE_PLATFORM_ANDROID)
    if (mFeatures.supportsExternalFenceCapabilities.enabled &&
        mFeatures.supportsExternalSemaphoreCapabilities.enabled)
    {
        ANGLE_FEATURE_CONDITION(
            &mFeatures, supportsAndroidNativeFenceSync,
            (mFeatures.supportsExternalFenceFd.enabled &&
             FencePropertiesCompatibleWithAndroid(mExternalFenceProperties) &&
             mFeatures.supportsExternalSemaphoreFd.enabled &&
             SemaphorePropertiesCompatibleWithAndroid(mExternalSemaphoreProperties)));
    }
    else
    {
        ANGLE_FEATURE_CONDITION(&mFeatures, supportsAndroidNativeFenceSync,
                                (mFeatures.supportsExternalFenceFd.enabled &&
                                 mFeatures.supportsExternalSemaphoreFd.enabled));
    }
#endif  // defined(ANGLE_PLATFORM_ANDROID)

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsShaderStencilExport,
        ExtensionFound(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME, deviceExtensionNames));

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsTransformFeedbackExtension,
                            mTransformFeedbackFeatures.transformFeedback == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsIndexTypeUint8,
                            mIndexTypeUint8Features.indexTypeUint8 == VK_TRUE);

    ANGLE_FEATURE_CONDITION(&mFeatures, emulateTransformFeedback,
                            (!mFeatures.supportsTransformFeedbackExtension.enabled &&
                             mPhysicalDeviceFeatures.vertexPipelineStoresAndAtomics == VK_TRUE));

    ANGLE_FEATURE_CONDITION(&mFeatures, disableFifoPresentMode, IsLinux() && isIntel);

    ANGLE_FEATURE_CONDITION(&mFeatures, bindEmptyForUnusedDescriptorSets,
                            IsAndroid() && isQualcomm);

    ANGLE_FEATURE_CONDITION(&mFeatures, forceOldRewriteStructSamplers, IsAndroid() && !isSwS);

    ANGLE_FEATURE_CONDITION(&mFeatures, perFrameWindowSizeQuery,
                            isIntel || (IsWindows() && isAMD) || IsFuchsia() || isARM);

    // Disabled on AMD/windows due to buggy behavior.
    ANGLE_FEATURE_CONDITION(&mFeatures, disallowSeamfulCubeMapEmulation, IsWindows() && isAMD);

    ANGLE_FEATURE_CONDITION(&mFeatures, padBuffersToMaxVertexAttribStride, isAMD);
    mMaxVertexAttribStride = std::min(static_cast<uint32_t>(gl::limits::kMaxVertexAttribStride),
                                      mPhysicalDeviceProperties.limits.maxVertexInputBindingStride);

    ANGLE_FEATURE_CONDITION(&mFeatures, forceD16TexFilter, IsAndroid() && isQualcomm);

    ANGLE_FEATURE_CONDITION(&mFeatures, disableFlippingBlitWithCommand, IsAndroid() && isQualcomm);

    // Allocation sanitization disabled by default because of a heaveyweight implementation
    // that can cause OOM and timeouts.
    ANGLE_FEATURE_CONDITION(&mFeatures, allocateNonZeroMemory, false);

    ANGLE_FEATURE_CONDITION(&mFeatures, shadowBuffers, true);

    ANGLE_FEATURE_CONDITION(&mFeatures, persistentlyMappedBuffers, true);

    ANGLE_FEATURE_CONDITION(
        &mFeatures, supportsExternalMemoryHost,
        ExtensionFound(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME, deviceExtensionNames));

    // Android pre-rotation support can be disabled.
    ANGLE_FEATURE_CONDITION(&mFeatures, enablePreRotateSurfaces, IsAndroid());

    // Currently disable FramebufferVk cache on Apple: http://anglebug.com/4442
    ANGLE_FEATURE_CONDITION(&mFeatures, enableFramebufferVkCache, !IsApple());

    // Currently disabled by default: http://anglebug.com/3078
    ANGLE_FEATURE_CONDITION(&mFeatures, enablePrecisionQualifiers, false);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportDepthStencilRenderingFeedbackLoops, true);

    ANGLE_FEATURE_CONDITION(&mFeatures, preferAggregateBarrierCalls, isNvidia || isAMD || isIntel);

    // Currently disabled by default: http://anglebug.com/4324
    ANGLE_FEATURE_CONDITION(&mFeatures, enableCommandProcessingThread, false);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsYUVSamplerConversion,
                            mSamplerYcbcrConversionFeatures.samplerYcbcrConversion != VK_FALSE);

    ANGLE_FEATURE_CONDITION(&mFeatures, supportsShaderFloat16,
                            mShaderFloat16Int8Features.shaderFloat16 == VK_TRUE);

    // The compute shader used to generate mipmaps uses a 256-wide workgroup.  This path is only
    // enabled on devices that meet this minimum requirement.  Furthermore,
    // VK_IMAGE_USAGE_STORAGE_BIT is detrimental to performance on many platforms, on which this
    // path is not enabled.  Platforms that are known to have better performance with this path are:
    //
    // - Nvidia
    // - AMD
    //
    // Additionally, this path is disabled on buggy drivers:
    //
    // - AMD/Windows: Unfortunately the trybots use ancient AMD cards and drivers.
    const uint32_t maxComputeWorkGroupInvocations =
        mPhysicalDeviceProperties.limits.maxComputeWorkGroupInvocations;
    ANGLE_FEATURE_CONDITION(
        &mFeatures, allowGenerateMipmapWithCompute,
        maxComputeWorkGroupInvocations >= 256 && (isNvidia || (isAMD && !IsWindows())));

    bool isAdreno540 = mPhysicalDeviceProperties.deviceID == angle::kDeviceID_Adreno540;
    ANGLE_FEATURE_CONDITION(&mFeatures, forceMaxUniformBufferSize16KB, isQualcomm && isAdreno540);

    angle::PlatformMethods *platform = ANGLEPlatformCurrent();
    platform->overrideFeaturesVk(platform, &mFeatures);

    ApplyFeatureOverrides(&mFeatures, displayVk->getState());
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
    size_t dataSize = 0;
    *success = display->getBlobCache()->get(display->getScratchBuffer(), mPipelineCacheVkBlobKey,
                                            &initialData, &dataSize);

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};

    pipelineCacheCreateInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheCreateInfo.flags           = 0;
    pipelineCacheCreateInfo.initialDataSize = *success ? dataSize : 0;
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

angle::Result RendererVk::getPipelineCacheSize(DisplayVk *displayVk, size_t *pipelineCacheSizeOut)
{
    VkResult result = mPipelineCache.getCacheData(mDevice, pipelineCacheSizeOut, nullptr);
    ANGLE_VK_TRY(displayVk, result);

    return angle::Result::Continue;
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

    size_t pipelineCacheSize = 0;
    ANGLE_TRY(getPipelineCacheSize(displayVk, &pipelineCacheSize));
    // Make sure we will receive enough data to hold the pipeline cache header
    // Table 7. Layout for pipeline cache header version VK_PIPELINE_CACHE_HEADER_VERSION_ONE
    const size_t kPipelineCacheHeaderSize = 16 + VK_UUID_SIZE;
    if (pipelineCacheSize < kPipelineCacheHeaderSize)
    {
        // No pipeline cache data to read, so return
        return angle::Result::Continue;
    }

    angle::MemoryBuffer *pipelineCacheData = nullptr;
    ANGLE_VK_CHECK_ALLOC(displayVk,
                         displayVk->getScratchBuffer(pipelineCacheSize, &pipelineCacheData));

    size_t oldPipelineCacheSize = pipelineCacheSize;
    VkResult result =
        mPipelineCache.getCacheData(mDevice, &pipelineCacheSize, pipelineCacheData->data());
    // We don't need all of the cache data, so just make sure we at least got the header
    // Vulkan Spec 9.6. Pipeline Cache
    // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap9.html#pipelines-cache
    // If pDataSize is less than what is necessary to store this header, nothing will be written to
    // pData and zero will be written to pDataSize.
    // Any data written to pData is valid and can be provided as the pInitialData member of the
    // VkPipelineCacheCreateInfo structure passed to vkCreatePipelineCache.
    if (ANGLE_UNLIKELY(pipelineCacheSize < kPipelineCacheHeaderSize))
    {
        WARN() << "Not enough pipeline cache data read.";
        return angle::Result::Continue;
    }
    else if (ANGLE_UNLIKELY(result == VK_INCOMPLETE))
    {
        WARN() << "Received VK_INCOMPLETE: Old: " << oldPipelineCacheSize
               << ", New: " << pipelineCacheSize;
    }
    else
    {
        ANGLE_VK_TRY(displayVk, result);
    }

    // If vkGetPipelineCacheData ends up writing fewer bytes than requested, zero out the rest of
    // the buffer to avoid leaking garbage memory.
    ASSERT(pipelineCacheSize <= pipelineCacheData->size());
    if (pipelineCacheSize < pipelineCacheData->size())
    {
        memset(pipelineCacheData->data() + pipelineCacheSize, 0,
               pipelineCacheData->size() - pipelineCacheSize);
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
                                      egl::ContextPriority priority,
                                      const VkSubmitInfo &submitInfo,
                                      const vk::Fence *fence,
                                      Serial *serialOut)
{
    if (getFeatures().enableCommandProcessingThread.enabled)
    {
        // For initial threading phase 1 code make sure any outstanding command processing
        //  is complete.
        // TODO: b/153666475 For phase2 investigate if this is required as most submits will take
        //  place through worker thread except for one-off submits below.
        mCommandProcessor.waitForWorkComplete();
    }
    {
        std::lock_guard<decltype(mQueueMutex)> lock(mQueueMutex);
        VkFence handle = fence ? fence->getHandle() : VK_NULL_HANDLE;
        ANGLE_VK_TRY(context, vkQueueSubmit(mQueues[priority], 1, &submitInfo, handle));
    }

    ANGLE_TRY(cleanupGarbage(false));

    *serialOut                = mCurrentQueueSerial;
    mLastSubmittedQueueSerial = mCurrentQueueSerial;
    mCurrentQueueSerial       = mQueueSerialFactory.generate();

    return angle::Result::Continue;
}

angle::Result RendererVk::queueSubmitOneOff(vk::Context *context,
                                            vk::PrimaryCommandBuffer &&primary,
                                            egl::ContextPriority priority,
                                            const vk::Fence *fence,
                                            Serial *serialOut)
{
    VkSubmitInfo submitInfo       = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = primary.ptr();

    ANGLE_TRY(queueSubmit(context, priority, submitInfo, fence, serialOut));

    mPendingOneOffCommands.push_back({*serialOut, std::move(primary)});

    return angle::Result::Continue;
}

angle::Result RendererVk::queueWaitIdle(vk::Context *context, egl::ContextPriority priority)
{
    if (getFeatures().enableCommandProcessingThread.enabled)
    {
        // First make sure command processor is complete when waiting for queue idle.
        mCommandProcessor.waitForWorkComplete();
    }
    {
        std::lock_guard<decltype(mQueueMutex)> lock(mQueueMutex);
        ANGLE_VK_TRY(context, vkQueueWaitIdle(mQueues[priority]));
    }

    ANGLE_TRY(cleanupGarbage(false));

    return angle::Result::Continue;
}

angle::Result RendererVk::deviceWaitIdle(vk::Context *context)
{
    if (getFeatures().enableCommandProcessingThread.enabled)
    {
        // First make sure command processor is complete when waiting for device idle.
        mCommandProcessor.waitForWorkComplete();
    }
    {
        std::lock_guard<decltype(mQueueMutex)> lock(mQueueMutex);
        ANGLE_VK_TRY(context, vkDeviceWaitIdle(mDevice));
    }

    ANGLE_TRY(cleanupGarbage(false));

    return angle::Result::Continue;
}

VkResult RendererVk::queuePresent(egl::ContextPriority priority,
                                  const VkPresentInfoKHR &presentInfo)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::queuePresent");

    if (getFeatures().enableCommandProcessingThread.enabled)
    {
        // First make sure command processor is complete before queue present as
        //  present may have dependencies on that thread.
        mCommandProcessor.waitForWorkComplete();
    }

    std::lock_guard<decltype(mQueueMutex)> lock(mQueueMutex);

    {
        ANGLE_TRACE_EVENT0("gpu.angle", "vkQueuePresentKHR");
        return vkQueuePresentKHR(mQueues[priority], &presentInfo);
    }
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

angle::Result RendererVk::cleanupGarbage(bool block)
{
    std::lock_guard<decltype(mGarbageMutex)> lock(mGarbageMutex);

    for (auto garbageIter = mSharedGarbage.begin(); garbageIter != mSharedGarbage.end();)
    {
        // Possibly 'counter' should be always zero when we add the object to garbage.
        vk::SharedGarbage &garbage = *garbageIter;
        if (garbage.destroyIfComplete(this, mLastCompletedQueueSerial))
        {
            garbageIter = mSharedGarbage.erase(garbageIter);
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

uint64_t RendererVk::getMaxFenceWaitTimeNs() const
{
    constexpr uint64_t kMaxFenceWaitTimeNs = 120'000'000'000llu;

    return kMaxFenceWaitTimeNs;
}

void RendererVk::onCompletedSerial(Serial serial)
{
    if (serial > mLastCompletedQueueSerial)
    {
        mLastCompletedQueueSerial = serial;
    }
}

void RendererVk::reloadVolkIfNeeded() const
{
#if defined(ANGLE_SHARED_LIBVULKAN)
    if ((mInstance != VK_NULL_HANDLE) && (volkGetLoadedInstance() != mInstance))
    {
        volkLoadInstance(mInstance);
    }

    if ((mDevice != VK_NULL_HANDLE) && (volkGetLoadedDevice() != mDevice))
    {
        volkLoadDevice(mDevice);
    }
#endif  // defined(ANGLE_SHARED_LIBVULKAN)
}

angle::Result RendererVk::getCommandBufferOneOff(vk::Context *context,
                                                 vk::PrimaryCommandBuffer *commandBufferOut)
{
    if (!mOneOffCommandPool.valid())
    {
        VkCommandPoolCreateInfo createInfo = {};
        createInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        ANGLE_VK_TRY(context, mOneOffCommandPool.init(mDevice, createInfo));
    }

    if (!mPendingOneOffCommands.empty() &&
        mPendingOneOffCommands.front().serial < mLastCompletedQueueSerial)
    {
        *commandBufferOut = std::move(mPendingOneOffCommands.front().commandBuffer);
        mPendingOneOffCommands.pop_front();
        ANGLE_VK_TRY(context, commandBufferOut->reset());
    }
    else
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount          = 1;
        allocInfo.commandPool                 = mOneOffCommandPool.getHandle();

        ANGLE_VK_TRY(context, commandBufferOut->init(context->getDevice(), allocInfo));
    }

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;
    ANGLE_VK_TRY(context, commandBufferOut->begin(beginInfo));

    return angle::Result::Continue;
}
}  // namespace rx
