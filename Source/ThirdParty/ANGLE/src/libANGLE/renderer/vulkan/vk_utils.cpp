//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_utils:
//    Helper functions for the Vulkan Renderer.
//

#include "libANGLE/renderer/vulkan/vk_utils.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace
{
VkImageUsageFlags GetStagingBufferUsageFlags(rx::vk::StagingUsage usage)
{
    switch (usage)
    {
        case rx::vk::StagingUsage::Read:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case rx::vk::StagingUsage::Write:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case rx::vk::StagingUsage::Both:
            return (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        default:
            UNREACHABLE();
            return 0;
    }
}
}  // anonymous namespace

namespace angle
{
egl::Error ToEGL(Result result, rx::DisplayVk *displayVk, EGLint errorCode)
{
    if (result != angle::Result::Continue)
    {
        return displayVk->getEGLError(errorCode);
    }
    else
    {
        return egl::NoError();
    }
}
}  // namespace angle

namespace rx
{
// Mirrors std_validation_str in loader.c
const char *g_VkStdValidationLayerName = "VK_LAYER_LUNARG_standard_validation";
const char *g_VkValidationLayerNames[] = {
    "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation",
    "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation",
    "VK_LAYER_GOOGLE_unique_objects"};

bool HasValidationLayer(const std::vector<VkLayerProperties> &layerProps, const char *layerName)
{
    for (const auto &layerProp : layerProps)
    {
        if (std::string(layerProp.layerName) == layerName)
        {
            return true;
        }
    }

    return false;
}

bool HasStandardValidationLayer(const std::vector<VkLayerProperties> &layerProps)
{
    return HasValidationLayer(layerProps, g_VkStdValidationLayerName);
}

bool HasValidationLayers(const std::vector<VkLayerProperties> &layerProps)
{
    for (const char *layerName : g_VkValidationLayerNames)
    {
        if (!HasValidationLayer(layerProps, layerName))
        {
            return false;
        }
    }

    return true;
}

angle::Result FindAndAllocateCompatibleMemory(vk::Context *context,
                                              const vk::MemoryProperties &memoryProperties,
                                              VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                              VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                              const VkMemoryRequirements &memoryRequirements,
                                              const void *extraAllocationInfo,
                                              vk::DeviceMemory *deviceMemoryOut)
{
    uint32_t memoryTypeIndex = 0;
    ANGLE_TRY(memoryProperties.findCompatibleMemoryIndex(context, memoryRequirements,
                                                         requestedMemoryPropertyFlags,
                                                         memoryPropertyFlagsOut, &memoryTypeIndex));

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext                = extraAllocationInfo;
    allocInfo.memoryTypeIndex      = memoryTypeIndex;
    allocInfo.allocationSize       = memoryRequirements.size;

    ANGLE_VK_TRY(context, deviceMemoryOut->allocate(context->getDevice(), allocInfo));
    return angle::Result::Continue;
}

template <typename T>
angle::Result AllocateAndBindBufferOrImageMemory(vk::Context *context,
                                                 VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                                 VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                                 const VkMemoryRequirements &memoryRequirements,
                                                 const void *extraAllocationInfo,
                                                 T *bufferOrImage,
                                                 vk::DeviceMemory *deviceMemoryOut)
{
    const vk::MemoryProperties &memoryProperties = context->getRenderer()->getMemoryProperties();

    ANGLE_TRY(FindAndAllocateCompatibleMemory(
        context, memoryProperties, requestedMemoryPropertyFlags, memoryPropertyFlagsOut,
        memoryRequirements, extraAllocationInfo, deviceMemoryOut));
    ANGLE_VK_TRY(context, bufferOrImage->bindMemory(context->getDevice(), *deviceMemoryOut));
    return angle::Result::Continue;
}

template <typename T>
angle::Result AllocateBufferOrImageMemory(vk::Context *context,
                                          VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                          VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                          const void *extraAllocationInfo,
                                          T *bufferOrImage,
                                          vk::DeviceMemory *deviceMemoryOut)
{
    // Call driver to determine memory requirements.
    VkMemoryRequirements memoryRequirements;
    bufferOrImage->getMemoryRequirements(context->getDevice(), &memoryRequirements);

    ANGLE_TRY(AllocateAndBindBufferOrImageMemory(
        context, requestedMemoryPropertyFlags, memoryPropertyFlagsOut, memoryRequirements,
        extraAllocationInfo, bufferOrImage, deviceMemoryOut));

    return angle::Result::Continue;
}

const char *g_VkLoaderLayersPathEnv = "VK_LAYER_PATH";
const char *g_VkICDPathEnv          = "VK_ICD_FILENAMES";

const char *VulkanResultString(VkResult result)
{
    switch (result)
    {
        case VK_SUCCESS:
            return "Command successfully completed.";
        case VK_NOT_READY:
            return "A fence or query has not yet completed.";
        case VK_TIMEOUT:
            return "A wait operation has not completed in the specified time.";
        case VK_EVENT_SET:
            return "An event is signaled.";
        case VK_EVENT_RESET:
            return "An event is unsignaled.";
        case VK_INCOMPLETE:
            return "A return array was too small for the result.";
        case VK_SUBOPTIMAL_KHR:
            return "A swapchain no longer matches the surface properties exactly, but can still be "
                   "used to present to the surface successfully.";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "A host memory allocation has failed.";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "A device memory allocation has failed.";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "Initialization of an object could not be completed for implementation-specific "
                   "reasons.";
        case VK_ERROR_DEVICE_LOST:
            return "The logical or physical device has been lost.";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "Mapping of a memory object has failed.";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "A requested layer is not present or could not be loaded.";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "A requested extension is not supported.";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "A requested feature is not supported.";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "The requested version of Vulkan is not supported by the driver or is otherwise "
                   "incompatible for implementation-specific reasons.";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "Too many objects of the type have already been created.";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "A requested format is not supported on this device.";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "A surface is no longer available.";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "The requested window is already connected to a VkSurfaceKHR, or to some other "
                   "non-Vulkan API.";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "A surface has changed in such a way that it is no longer compatible with the "
                   "swapchain.";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "The display used by a swapchain does not use the same presentable image "
                   "layout, or is incompatible in a way that prevents sharing an image.";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "The validation layers detected invalid API usage.";
        default:
            return "Unknown vulkan error code.";
    }
}

bool GetAvailableValidationLayers(const std::vector<VkLayerProperties> &layerProps,
                                  bool mustHaveLayers,
                                  VulkanLayerVector *enabledLayerNames)
{
    if (HasStandardValidationLayer(layerProps))
    {
        enabledLayerNames->push_back(g_VkStdValidationLayerName);
    }
    else if (HasValidationLayers(layerProps))
    {
        for (const char *layerName : g_VkValidationLayerNames)
        {
            enabledLayerNames->push_back(layerName);
        }
    }
    else
    {
        // Generate an error if the layers were explicitly requested, warning otherwise.
        if (mustHaveLayers)
        {
            ERR() << "Vulkan validation layers are missing.";
        }
        else
        {
            WARN() << "Vulkan validation layers are missing.";
        }

        return false;
    }

    return true;
}

namespace vk
{
VkImageAspectFlags GetDepthStencilAspectFlags(const angle::Format &format)
{
    return (format.depthBits > 0 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
           (format.stencilBits > 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
}

VkImageAspectFlags GetFormatAspectFlags(const angle::Format &format)
{
    VkImageAspectFlags dsAspect = GetDepthStencilAspectFlags(format);
    // If the image is not depth stencil, assume color aspect.  Note that detecting color formats
    // is less trivial than depth/stencil, e.g. as block formats don't indicate any bits for RGBA
    // channels.
    return dsAspect != 0 ? dsAspect : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageAspectFlags GetDepthStencilAspectFlagsForCopy(bool copyDepth, bool copyStencil)
{
    return copyDepth ? VK_IMAGE_ASPECT_DEPTH_BIT
                     : 0 | copyStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
}

// Context implementation.
Context::Context(RendererVk *renderer) : mRenderer(renderer) {}

Context::~Context() {}

VkDevice Context::getDevice() const
{
    return mRenderer->getDevice();
}

// MemoryProperties implementation.
MemoryProperties::MemoryProperties() : mMemoryProperties{0} {}

void MemoryProperties::init(VkPhysicalDevice physicalDevice)
{
    ASSERT(mMemoryProperties.memoryTypeCount == 0);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mMemoryProperties);
    ASSERT(mMemoryProperties.memoryTypeCount > 0);
}

void MemoryProperties::destroy()
{
    mMemoryProperties = {0};
}

angle::Result MemoryProperties::findCompatibleMemoryIndex(
    Context *context,
    const VkMemoryRequirements &memoryRequirements,
    VkMemoryPropertyFlags requestedMemoryPropertyFlags,
    VkMemoryPropertyFlags *memoryPropertyFlagsOut,
    uint32_t *typeIndexOut) const
{
    ASSERT(mMemoryProperties.memoryTypeCount > 0 && mMemoryProperties.memoryTypeCount <= 32);

    // Find a compatible memory pool index. If the index doesn't change, we could cache it.
    // Not finding a valid memory pool means an out-of-spec driver, or internal error.
    // TODO(jmadill): Determine if it is possible to cache indexes.
    // TODO(jmadill): More efficient memory allocation.
    for (size_t memoryIndex : angle::BitSet32<32>(memoryRequirements.memoryTypeBits))
    {
        ASSERT(memoryIndex < mMemoryProperties.memoryTypeCount);

        if ((mMemoryProperties.memoryTypes[memoryIndex].propertyFlags &
             requestedMemoryPropertyFlags) == requestedMemoryPropertyFlags)
        {
            *memoryPropertyFlagsOut = mMemoryProperties.memoryTypes[memoryIndex].propertyFlags;
            *typeIndexOut           = static_cast<uint32_t>(memoryIndex);
            return angle::Result::Continue;
        }
    }

    // TODO(jmadill): Add error message to error.
    context->handleError(VK_ERROR_INCOMPATIBLE_DRIVER, __FILE__, ANGLE_FUNCTION, __LINE__);
    return angle::Result::Stop;
}

// StagingBuffer implementation.
StagingBuffer::StagingBuffer() : mSize(0) {}

void StagingBuffer::destroy(VkDevice device)
{
    mBuffer.destroy(device);
    mDeviceMemory.destroy(device);
    mSize = 0;
}

angle::Result StagingBuffer::init(Context *context, VkDeviceSize size, StagingUsage usage)
{
    VkBufferCreateInfo createInfo    = {};
    createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.flags                 = 0;
    createInfo.size                  = size;
    createInfo.usage                 = GetStagingBufferUsageFlags(usage);
    createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;

    VkMemoryPropertyFlags flags =
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    ANGLE_VK_TRY(context, mBuffer.init(context->getDevice(), createInfo));
    VkMemoryPropertyFlags flagsOut = 0;
    ANGLE_TRY(AllocateBufferMemory(context, flags, &flagsOut, nullptr, &mBuffer, &mDeviceMemory));
    mSize = static_cast<size_t>(size);
    return angle::Result::Continue;
}

void StagingBuffer::dumpResources(Serial serial, std::vector<vk::GarbageObject> *garbageQueue)
{
    mBuffer.dumpResources(serial, garbageQueue);
    mDeviceMemory.dumpResources(serial, garbageQueue);
}

angle::Result AllocateBufferMemory(vk::Context *context,
                                   VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                   VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                   const void *extraAllocationInfo,
                                   Buffer *buffer,
                                   DeviceMemory *deviceMemoryOut)
{
    return AllocateBufferOrImageMemory(context, requestedMemoryPropertyFlags,
                                       memoryPropertyFlagsOut, extraAllocationInfo, buffer,
                                       deviceMemoryOut);
}

angle::Result AllocateImageMemory(vk::Context *context,
                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                  const void *extraAllocationInfo,
                                  Image *image,
                                  DeviceMemory *deviceMemoryOut)
{
    VkMemoryPropertyFlags memoryPropertyFlagsOut = 0;
    return AllocateBufferOrImageMemory(context, memoryPropertyFlags, &memoryPropertyFlagsOut,
                                       extraAllocationInfo, image, deviceMemoryOut);
}

angle::Result AllocateImageMemoryWithRequirements(vk::Context *context,
                                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                                  const VkMemoryRequirements &memoryRequirements,
                                                  const void *extraAllocationInfo,
                                                  Image *image,
                                                  DeviceMemory *deviceMemoryOut)
{
    VkMemoryPropertyFlags memoryPropertyFlagsOut = 0;
    return AllocateAndBindBufferOrImageMemory(context, memoryPropertyFlags, &memoryPropertyFlagsOut,
                                              memoryRequirements, extraAllocationInfo, image,
                                              deviceMemoryOut);
}

angle::Result InitShaderAndSerial(Context *context,
                                  ShaderAndSerial *shaderAndSerial,
                                  const uint32_t *shaderCode,
                                  size_t shaderCodeSize)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.flags                    = 0;
    createInfo.codeSize                 = shaderCodeSize;
    createInfo.pCode                    = shaderCode;

    ANGLE_VK_TRY(context, shaderAndSerial->get().init(context->getDevice(), createInfo));
    shaderAndSerial->updateSerial(context->getRenderer()->issueShaderSerial());
    return angle::Result::Continue;
}

// GarbageObject implementation.
GarbageObject::GarbageObject()
    : mSerial(), mHandleType(HandleType::Invalid), mHandle(VK_NULL_HANDLE)
{}

GarbageObject::GarbageObject(const GarbageObject &other) = default;

GarbageObject &GarbageObject::operator=(const GarbageObject &other) = default;

bool GarbageObject::destroyIfComplete(VkDevice device, Serial completedSerial)
{
    if (completedSerial >= mSerial)
    {
        destroy(device);
        return true;
    }

    return false;
}

void GarbageObject::destroy(VkDevice device)
{
    switch (mHandleType)
    {
        case HandleType::Semaphore:
            vkDestroySemaphore(device, reinterpret_cast<VkSemaphore>(mHandle), nullptr);
            break;
        case HandleType::CommandBuffer:
            // Command buffers are pool allocated.
            UNREACHABLE();
            break;
        case HandleType::Event:
            vkDestroyEvent(device, reinterpret_cast<VkEvent>(mHandle), nullptr);
            break;
        case HandleType::Fence:
            vkDestroyFence(device, reinterpret_cast<VkFence>(mHandle), nullptr);
            break;
        case HandleType::DeviceMemory:
            vkFreeMemory(device, reinterpret_cast<VkDeviceMemory>(mHandle), nullptr);
            break;
        case HandleType::Buffer:
            vkDestroyBuffer(device, reinterpret_cast<VkBuffer>(mHandle), nullptr);
            break;
        case HandleType::BufferView:
            vkDestroyBufferView(device, reinterpret_cast<VkBufferView>(mHandle), nullptr);
            break;
        case HandleType::Image:
            vkDestroyImage(device, reinterpret_cast<VkImage>(mHandle), nullptr);
            break;
        case HandleType::ImageView:
            vkDestroyImageView(device, reinterpret_cast<VkImageView>(mHandle), nullptr);
            break;
        case HandleType::ShaderModule:
            vkDestroyShaderModule(device, reinterpret_cast<VkShaderModule>(mHandle), nullptr);
            break;
        case HandleType::PipelineLayout:
            vkDestroyPipelineLayout(device, reinterpret_cast<VkPipelineLayout>(mHandle), nullptr);
            break;
        case HandleType::RenderPass:
            vkDestroyRenderPass(device, reinterpret_cast<VkRenderPass>(mHandle), nullptr);
            break;
        case HandleType::Pipeline:
            vkDestroyPipeline(device, reinterpret_cast<VkPipeline>(mHandle), nullptr);
            break;
        case HandleType::DescriptorSetLayout:
            vkDestroyDescriptorSetLayout(device, reinterpret_cast<VkDescriptorSetLayout>(mHandle),
                                         nullptr);
            break;
        case HandleType::Sampler:
            vkDestroySampler(device, reinterpret_cast<VkSampler>(mHandle), nullptr);
            break;
        case HandleType::DescriptorPool:
            vkDestroyDescriptorPool(device, reinterpret_cast<VkDescriptorPool>(mHandle), nullptr);
            break;
        case HandleType::Framebuffer:
            vkDestroyFramebuffer(device, reinterpret_cast<VkFramebuffer>(mHandle), nullptr);
            break;
        case HandleType::CommandPool:
            vkDestroyCommandPool(device, reinterpret_cast<VkCommandPool>(mHandle), nullptr);
            break;
        case HandleType::QueryPool:
            vkDestroyQueryPool(device, reinterpret_cast<VkQueryPool>(mHandle), nullptr);
            break;
        default:
            UNREACHABLE();
            break;
    }
}
}  // namespace vk

// VK_EXT_debug_utils
PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT   = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT       = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT           = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT     = nullptr;

// VK_EXT_debug_report
PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT   = nullptr;
PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = nullptr;

// VK_KHR_get_physical_device_properties2
PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR = nullptr;

#if defined(ANGLE_PLATFORM_FUCHSIA)
// VK_FUCHSIA_imagepipe_surface
PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA = nullptr;
#endif

#define GET_FUNC(vkName)                                                                   \
    do                                                                                     \
    {                                                                                      \
        vkName = reinterpret_cast<PFN_##vkName>(vkGetInstanceProcAddr(instance, #vkName)); \
        ASSERT(vkName);                                                                    \
    } while (0)

void InitDebugUtilsEXTFunctions(VkInstance instance)
{
    GET_FUNC(vkCreateDebugUtilsMessengerEXT);
    GET_FUNC(vkDestroyDebugUtilsMessengerEXT);
    GET_FUNC(vkCmdBeginDebugUtilsLabelEXT);
    GET_FUNC(vkCmdEndDebugUtilsLabelEXT);
    GET_FUNC(vkCmdInsertDebugUtilsLabelEXT);
}

void InitDebugReportEXTFunctions(VkInstance instance)
{
    GET_FUNC(vkCreateDebugReportCallbackEXT);
    GET_FUNC(vkDestroyDebugReportCallbackEXT);
}

void InitGetPhysicalDeviceProperties2KHRFunctions(VkInstance instance)
{
    GET_FUNC(vkGetPhysicalDeviceProperties2KHR);
}

#if defined(ANGLE_PLATFORM_FUCHSIA)
void InitImagePipeSurfaceFUCHSIAFunctions(VkInstance instance)
{
    GET_FUNC(vkCreateImagePipeSurfaceFUCHSIA);
}
#endif

#if defined(ANGLE_PLATFORM_ANDROID)
PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID =
    nullptr;
PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID = nullptr;
void InitExternalMemoryHardwareBufferANDROIDFunctions(VkInstance instance)
{
    GET_FUNC(vkGetAndroidHardwareBufferPropertiesANDROID);
    GET_FUNC(vkGetMemoryAndroidHardwareBufferANDROID);
}
#endif

#undef GET_FUNC

namespace gl_vk
{

VkFilter GetFilter(const GLenum filter)
{
    switch (filter)
    {
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR:
            return VK_FILTER_LINEAR;
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST:
            return VK_FILTER_NEAREST;
        default:
            UNIMPLEMENTED();
            return VK_FILTER_MAX_ENUM;
    }
}

VkSamplerMipmapMode GetSamplerMipmapMode(const GLenum filter)
{
    switch (filter)
    {
        case GL_LINEAR:
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case GL_NEAREST:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        default:
            UNIMPLEMENTED();
            return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}

VkSamplerAddressMode GetSamplerAddressMode(const GLenum wrap)
{
    switch (wrap)
    {
        case GL_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case GL_MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case GL_CLAMP_TO_BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case GL_CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        default:
            UNIMPLEMENTED();
            return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }
}

VkRect2D GetRect(const gl::Rectangle &source)
{
    return {{source.x, source.y},
            {static_cast<uint32_t>(source.width), static_cast<uint32_t>(source.height)}};
}

VkPrimitiveTopology GetPrimitiveTopology(gl::PrimitiveMode mode)
{
    switch (mode)
    {
        case gl::PrimitiveMode::Triangles:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case gl::PrimitiveMode::Points:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case gl::PrimitiveMode::Lines:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case gl::PrimitiveMode::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case gl::PrimitiveMode::TriangleFan:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case gl::PrimitiveMode::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case gl::PrimitiveMode::LineLoop:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        default:
            UNREACHABLE();
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
}

VkCullModeFlags GetCullMode(const gl::RasterizerState &rasterState)
{
    if (!rasterState.cullFace)
    {
        return VK_CULL_MODE_NONE;
    }

    switch (rasterState.cullMode)
    {
        case gl::CullFaceMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case gl::CullFaceMode::Back:
            return VK_CULL_MODE_BACK_BIT;
        case gl::CullFaceMode::FrontAndBack:
            return VK_CULL_MODE_FRONT_AND_BACK;
        default:
            UNREACHABLE();
            return VK_CULL_MODE_NONE;
    }
}

VkFrontFace GetFrontFace(GLenum frontFace, bool invertCullFace)
{
    // Invert CW and CCW to have the same behavior as OpenGL.
    switch (frontFace)
    {
        case GL_CW:
            return invertCullFace ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case GL_CCW:
            return invertCullFace ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        default:
            UNREACHABLE();
            return VK_FRONT_FACE_CLOCKWISE;
    }
}

VkSampleCountFlagBits GetSamples(GLint sampleCount)
{
    switch (sampleCount)
    {
        case 0:
        case 1:
            return VK_SAMPLE_COUNT_1_BIT;
        case 2:
            return VK_SAMPLE_COUNT_2_BIT;
        case 4:
            return VK_SAMPLE_COUNT_4_BIT;
        case 8:
            return VK_SAMPLE_COUNT_8_BIT;
        case 16:
            return VK_SAMPLE_COUNT_16_BIT;
        case 32:
            return VK_SAMPLE_COUNT_32_BIT;
        default:
            UNREACHABLE();
            return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    }
}

VkComponentSwizzle GetSwizzle(const GLenum swizzle)
{
    switch (swizzle)
    {
        case GL_ALPHA:
            return VK_COMPONENT_SWIZZLE_A;
        case GL_RED:
            return VK_COMPONENT_SWIZZLE_R;
        case GL_GREEN:
            return VK_COMPONENT_SWIZZLE_G;
        case GL_BLUE:
            return VK_COMPONENT_SWIZZLE_B;
        case GL_ZERO:
            return VK_COMPONENT_SWIZZLE_ZERO;
        case GL_ONE:
            return VK_COMPONENT_SWIZZLE_ONE;
        default:
            UNREACHABLE();
            return VK_COMPONENT_SWIZZLE_IDENTITY;
    }
}

void GetOffset(const gl::Offset &glOffset, VkOffset3D *vkOffset)
{
    vkOffset->x = glOffset.x;
    vkOffset->y = glOffset.y;
    vkOffset->z = glOffset.z;
}

void GetExtent(const gl::Extents &glExtent, VkExtent3D *vkExtent)
{
    vkExtent->width  = glExtent.width;
    vkExtent->height = glExtent.height;
    vkExtent->depth  = glExtent.depth;
}

VkImageType GetImageType(gl::TextureType textureType)
{
    switch (textureType)
    {
        case gl::TextureType::_2D:
        case gl::TextureType::_2DArray:
        case gl::TextureType::_2DMultisample:
        case gl::TextureType::_2DMultisampleArray:
        case gl::TextureType::CubeMap:
        case gl::TextureType::External:
            return VK_IMAGE_TYPE_2D;
        case gl::TextureType::_3D:
            return VK_IMAGE_TYPE_3D;
        default:
            // We will need to implement all the texture types for ES3+.
            UNIMPLEMENTED();
            return VK_IMAGE_TYPE_MAX_ENUM;
    }
}

VkImageViewType GetImageViewType(gl::TextureType textureType)
{
    switch (textureType)
    {
        case gl::TextureType::_2D:
        case gl::TextureType::_2DMultisample:
        case gl::TextureType::External:
            return VK_IMAGE_VIEW_TYPE_2D;
        case gl::TextureType::_2DArray:
        case gl::TextureType::_2DMultisampleArray:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case gl::TextureType::_3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case gl::TextureType::CubeMap:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        default:
            // We will need to implement all the texture types for ES3+.
            UNIMPLEMENTED();
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }
}

VkColorComponentFlags GetColorComponentFlags(bool red, bool green, bool blue, bool alpha)
{
    return (red ? VK_COLOR_COMPONENT_R_BIT : 0) | (green ? VK_COLOR_COMPONENT_G_BIT : 0) |
           (blue ? VK_COLOR_COMPONENT_B_BIT : 0) | (alpha ? VK_COLOR_COMPONENT_A_BIT : 0);
}

void GetViewport(const gl::Rectangle &viewport,
                 float nearPlane,
                 float farPlane,
                 bool invertViewport,
                 GLint renderAreaHeight,
                 VkViewport *viewportOut)
{
    viewportOut->x        = static_cast<float>(viewport.x);
    viewportOut->y        = static_cast<float>(viewport.y);
    viewportOut->width    = static_cast<float>(viewport.width);
    viewportOut->height   = static_cast<float>(viewport.height);
    viewportOut->minDepth = gl::clamp01(nearPlane);
    viewportOut->maxDepth = gl::clamp01(farPlane);

    if (invertViewport)
    {
        viewportOut->y      = static_cast<float>(renderAreaHeight - viewport.y);
        viewportOut->height = -viewportOut->height;
    }
}
}  // namespace gl_vk
}  // namespace rx
