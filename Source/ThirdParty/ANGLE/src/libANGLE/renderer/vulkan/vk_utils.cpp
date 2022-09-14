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
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/android/vk_android_utils.h"
#include "libANGLE/renderer/vulkan/vk_mem_alloc_wrapper.h"

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
namespace
{
// Pick an arbitrary value to initialize non-zero memory for sanitization.  Note that 0x3F3F3F3F
// as float is about 0.75.
constexpr int kNonZeroInitValue = 0x3F;

VkImageUsageFlags GetStagingBufferUsageFlags(vk::StagingUsage usage)
{
    switch (usage)
    {
        case vk::StagingUsage::Read:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case vk::StagingUsage::Write:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case vk::StagingUsage::Both:
            return (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        default:
            UNREACHABLE();
            return 0;
    }
}

bool FindCompatibleMemory(const VkPhysicalDeviceMemoryProperties &memoryProperties,
                          const VkMemoryRequirements &memoryRequirements,
                          VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                          VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                          uint32_t *typeIndexOut)
{
    for (size_t memoryIndex : angle::BitSet32<32>(memoryRequirements.memoryTypeBits))
    {
        ASSERT(memoryIndex < memoryProperties.memoryTypeCount);

        if ((memoryProperties.memoryTypes[memoryIndex].propertyFlags &
             requestedMemoryPropertyFlags) == requestedMemoryPropertyFlags)
        {
            *memoryPropertyFlagsOut = memoryProperties.memoryTypes[memoryIndex].propertyFlags;
            *typeIndexOut           = static_cast<uint32_t>(memoryIndex);
            return true;
        }
    }

    return false;
}

angle::Result FindAndAllocateCompatibleMemory(vk::Context *context,
                                              const vk::MemoryProperties &memoryProperties,
                                              VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                              VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                              const VkMemoryRequirements &memoryRequirements,
                                              const void *extraAllocationInfo,
                                              vk::DeviceMemory *deviceMemoryOut)
{
    VkDevice device = context->getDevice();

    uint32_t memoryTypeIndex = 0;
    ANGLE_TRY(memoryProperties.findCompatibleMemoryIndex(
        context, memoryRequirements, requestedMemoryPropertyFlags, (extraAllocationInfo != nullptr),
        memoryPropertyFlagsOut, &memoryTypeIndex));

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext                = extraAllocationInfo;
    allocInfo.memoryTypeIndex      = memoryTypeIndex;
    allocInfo.allocationSize       = memoryRequirements.size;

    ANGLE_VK_TRY(context, deviceMemoryOut->allocate(device, allocInfo));

    // Wipe memory to an invalid value when the 'allocateNonZeroMemory' feature is enabled. The
    // invalid values ensures our testing doesn't assume zero-initialized memory.
    RendererVk *renderer = context->getRenderer();
    if (renderer->getFeatures().allocateNonZeroMemory.enabled)
    {
        if ((*memoryPropertyFlagsOut & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
        {
            // Can map the memory.
            ANGLE_TRY(vk::InitMappableDeviceMemory(context, deviceMemoryOut,
                                                   memoryRequirements.size, kNonZeroInitValue,
                                                   *memoryPropertyFlagsOut));
        }
    }

    return angle::Result::Continue;
}

template <typename T>
angle::Result AllocateAndBindBufferOrImageMemory(vk::Context *context,
                                                 VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                                 VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                                 const VkMemoryRequirements &memoryRequirements,
                                                 const void *extraAllocationInfo,
                                                 const VkBindImagePlaneMemoryInfoKHR *extraBindInfo,
                                                 T *bufferOrImage,
                                                 vk::DeviceMemory *deviceMemoryOut);

template <>
angle::Result AllocateAndBindBufferOrImageMemory(vk::Context *context,
                                                 VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                                 VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                                 const VkMemoryRequirements &memoryRequirements,
                                                 const void *extraAllocationInfo,
                                                 const VkBindImagePlaneMemoryInfoKHR *extraBindInfo,
                                                 vk::Image *image,
                                                 vk::DeviceMemory *deviceMemoryOut)
{
    const vk::MemoryProperties &memoryProperties = context->getRenderer()->getMemoryProperties();

    ANGLE_TRY(FindAndAllocateCompatibleMemory(
        context, memoryProperties, requestedMemoryPropertyFlags, memoryPropertyFlagsOut,
        memoryRequirements, extraAllocationInfo, deviceMemoryOut));

    if (extraBindInfo)
    {
        VkBindImageMemoryInfoKHR bindInfo = {};
        bindInfo.sType                    = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
        bindInfo.pNext                    = extraBindInfo;
        bindInfo.image                    = image->getHandle();
        bindInfo.memory                   = deviceMemoryOut->getHandle();
        bindInfo.memoryOffset             = 0;

        ANGLE_VK_TRY(context, image->bindMemory2(context->getDevice(), bindInfo));
    }
    else
    {
        ANGLE_VK_TRY(context, image->bindMemory(context->getDevice(), *deviceMemoryOut));
    }

    return angle::Result::Continue;
}

template <>
angle::Result AllocateAndBindBufferOrImageMemory(vk::Context *context,
                                                 VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                                 VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                                 const VkMemoryRequirements &memoryRequirements,
                                                 const void *extraAllocationInfo,
                                                 const VkBindImagePlaneMemoryInfoKHR *extraBindInfo,
                                                 vk::Buffer *buffer,
                                                 vk::DeviceMemory *deviceMemoryOut)
{
    ASSERT(extraBindInfo == nullptr);

    const vk::MemoryProperties &memoryProperties = context->getRenderer()->getMemoryProperties();

    ANGLE_TRY(FindAndAllocateCompatibleMemory(
        context, memoryProperties, requestedMemoryPropertyFlags, memoryPropertyFlagsOut,
        memoryRequirements, extraAllocationInfo, deviceMemoryOut));
    ANGLE_VK_TRY(context, buffer->bindMemory(context->getDevice(), *deviceMemoryOut, 0));
    return angle::Result::Continue;
}

template <typename T>
angle::Result AllocateBufferOrImageMemory(vk::Context *context,
                                          VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                          VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                          const void *extraAllocationInfo,
                                          T *bufferOrImage,
                                          vk::DeviceMemory *deviceMemoryOut,
                                          VkDeviceSize *sizeOut)
{
    // Call driver to determine memory requirements.
    VkMemoryRequirements memoryRequirements;
    bufferOrImage->getMemoryRequirements(context->getDevice(), &memoryRequirements);

    ANGLE_TRY(AllocateAndBindBufferOrImageMemory(
        context, requestedMemoryPropertyFlags, memoryPropertyFlagsOut, memoryRequirements,
        extraAllocationInfo, nullptr, bufferOrImage, deviceMemoryOut));

    *sizeOut = memoryRequirements.size;

    return angle::Result::Continue;
}

// Unified layer that includes full validation layer stack
constexpr char kVkKhronosValidationLayerName[]  = "VK_LAYER_KHRONOS_validation";
constexpr char kVkStandardValidationLayerName[] = "VK_LAYER_LUNARG_standard_validation";
const char *kVkValidationLayerNames[]           = {
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

bool HasKhronosValidationLayer(const std::vector<VkLayerProperties> &layerProps)
{
    return HasValidationLayer(layerProps, kVkKhronosValidationLayerName);
}

bool HasStandardValidationLayer(const std::vector<VkLayerProperties> &layerProps)
{
    return HasValidationLayer(layerProps, kVkStandardValidationLayerName);
}

bool HasValidationLayers(const std::vector<VkLayerProperties> &layerProps)
{
    for (const char *layerName : kVkValidationLayerNames)
    {
        if (!HasValidationLayer(layerProps, layerName))
        {
            return false;
        }
    }

    return true;
}
}  // anonymous namespace

const char *VulkanResultString(VkResult result)
{
    switch (result)
    {
        case VK_SUCCESS:
            return "Command successfully completed";
        case VK_NOT_READY:
            return "A fence or query has not yet completed";
        case VK_TIMEOUT:
            return "A wait operation has not completed in the specified time";
        case VK_EVENT_SET:
            return "An event is signaled";
        case VK_EVENT_RESET:
            return "An event is unsignaled";
        case VK_INCOMPLETE:
            return "A return array was too small for the result";
        case VK_SUBOPTIMAL_KHR:
            return "A swapchain no longer matches the surface properties exactly, but can still be "
                   "used to present to the surface successfully";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "A host memory allocation has failed";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "A device memory allocation has failed";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "Initialization of an object could not be completed for implementation-specific "
                   "reasons";
        case VK_ERROR_DEVICE_LOST:
            return "The logical or physical device has been lost";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "Mapping of a memory object has failed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "A requested layer is not present or could not be loaded";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "A requested extension is not supported";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "A requested feature is not supported";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "The requested version of Vulkan is not supported by the driver or is otherwise "
                   "incompatible for implementation-specific reasons";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "Too many objects of the type have already been created";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "A requested format is not supported on this device";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "A surface is no longer available";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "The requested window is already connected to a VkSurfaceKHR, or to some other "
                   "non-Vulkan API";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "A surface has changed in such a way that it is no longer compatible with the "
                   "swapchain";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "The display used by a swapchain does not use the same presentable image "
                   "layout, or is incompatible in a way that prevents sharing an image";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "The validation layers detected invalid API usage";
        case VK_ERROR_INVALID_SHADER_NV:
            return "Invalid Vulkan shader was generated";
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "A pool memory allocation has failed";
        case VK_ERROR_FRAGMENTED_POOL:
            return "A pool allocation has failed due to fragmentation of the pool's memory";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "An external handle is not a valid handle of the specified type";
        default:
            return "Unknown vulkan error code";
    }
}

bool GetAvailableValidationLayers(const std::vector<VkLayerProperties> &layerProps,
                                  bool mustHaveLayers,
                                  VulkanLayerVector *enabledLayerNames)
{
    // Favor unified Khronos layer, but fallback to standard validation
    if (HasKhronosValidationLayer(layerProps))
    {
        enabledLayerNames->push_back(kVkKhronosValidationLayerName);
    }
    else if (HasStandardValidationLayer(layerProps))
    {
        enabledLayerNames->push_back(kVkStandardValidationLayerName);
    }
    else if (HasValidationLayers(layerProps))
    {
        for (const char *layerName : kVkValidationLayerNames)
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
const char *gLoaderLayersPathEnv   = "VK_LAYER_PATH";
const char *gLoaderICDFilenamesEnv = "VK_ICD_FILENAMES";

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

// Context implementation.
Context::Context(RendererVk *renderer) : mRenderer(renderer), mPerfCounters{} {}

Context::~Context() {}

VkDevice Context::getDevice() const
{
    return mRenderer->getDevice();
}

// MemoryProperties implementation.
MemoryProperties::MemoryProperties() : mMemoryProperties{} {}

void MemoryProperties::init(VkPhysicalDevice physicalDevice)
{
    ASSERT(mMemoryProperties.memoryTypeCount == 0);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mMemoryProperties);
    ASSERT(mMemoryProperties.memoryTypeCount > 0);
}

void MemoryProperties::destroy()
{
    mMemoryProperties = {};
}

bool MemoryProperties::hasLazilyAllocatedMemory() const
{
    for (uint32_t typeIndex = 0; typeIndex < mMemoryProperties.memoryTypeCount; ++typeIndex)
    {
        const VkMemoryType &memoryType = mMemoryProperties.memoryTypes[typeIndex];
        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0)
        {
            return true;
        }
    }
    return false;
}

angle::Result MemoryProperties::findCompatibleMemoryIndex(
    Context *context,
    const VkMemoryRequirements &memoryRequirements,
    VkMemoryPropertyFlags requestedMemoryPropertyFlags,
    bool isExternalMemory,
    VkMemoryPropertyFlags *memoryPropertyFlagsOut,
    uint32_t *typeIndexOut) const
{
    ASSERT(mMemoryProperties.memoryTypeCount > 0 && mMemoryProperties.memoryTypeCount <= 32);

    // Find a compatible memory pool index. If the index doesn't change, we could cache it.
    // Not finding a valid memory pool means an out-of-spec driver, or internal error.
    // TODO(jmadill): Determine if it is possible to cache indexes.
    // TODO(jmadill): More efficient memory allocation.
    if (FindCompatibleMemory(mMemoryProperties, memoryRequirements, requestedMemoryPropertyFlags,
                             memoryPropertyFlagsOut, typeIndexOut))
    {
        return angle::Result::Continue;
    }

    // We did not find a compatible memory type.  If the caller wanted a host visible memory, just
    // return the memory index with fallback, guaranteed, memory flags.
    if (requestedMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        // The Vulkan spec says the following -
        //     There must be at least one memory type with both the
        //     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        //     bits set in its propertyFlags
        constexpr VkMemoryPropertyFlags fallbackMemoryPropertyFlags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        if (FindCompatibleMemory(mMemoryProperties, memoryRequirements, fallbackMemoryPropertyFlags,
                                 memoryPropertyFlagsOut, typeIndexOut))
        {
            return angle::Result::Continue;
        }
    }

    // We did not find a compatible memory type. When importing external memory, there may be
    // additional restrictions on memoryType. Fallback to requesting device local memory.
    if (isExternalMemory)
    {
        // The Vulkan spec says the following -
        //     There must be at least one memory type with the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        //     bit set in its propertyFlags
        if (FindCompatibleMemory(mMemoryProperties, memoryRequirements,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryPropertyFlagsOut,
                                 typeIndexOut))
        {
            return angle::Result::Continue;
        }
    }

    // TODO(jmadill): Add error message to error.
    context->handleError(VK_ERROR_INCOMPATIBLE_DRIVER, __FILE__, ANGLE_FUNCTION, __LINE__);
    return angle::Result::Stop;
}

// StagingBuffer implementation.
StagingBuffer::StagingBuffer() : mSize(0) {}

void StagingBuffer::destroy(RendererVk *renderer)
{
    VkDevice device = renderer->getDevice();
    mBuffer.destroy(device);
    mAllocation.destroy(renderer->getAllocator());
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

    VkMemoryPropertyFlags preferredFlags = 0;
    VkMemoryPropertyFlags requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    RendererVk *renderer       = context->getRenderer();
    const Allocator &allocator = renderer->getAllocator();

    uint32_t memoryTypeIndex = 0;
    ANGLE_VK_TRY(context,
                 allocator.createBuffer(createInfo, requiredFlags, preferredFlags,
                                        renderer->getFeatures().persistentlyMappedBuffers.enabled,
                                        &memoryTypeIndex, &mBuffer, &mAllocation));
    mSize = static_cast<size_t>(size);

    // Wipe memory to an invalid value when the 'allocateNonZeroMemory' feature is enabled. The
    // invalid values ensures our testing doesn't assume zero-initialized memory.
    if (renderer->getFeatures().allocateNonZeroMemory.enabled)
    {
        ANGLE_TRY(InitMappableAllocation(context, allocator, &mAllocation, size, kNonZeroInitValue,
                                         requiredFlags));
    }

    return angle::Result::Continue;
}

void StagingBuffer::release(ContextVk *contextVk)
{
    contextVk->addGarbage(&mBuffer);
    contextVk->addGarbage(&mAllocation);
}

void StagingBuffer::collectGarbage(RendererVk *renderer, Serial serial)
{
    GarbageList garbageList;
    garbageList.emplace_back(GetGarbage(&mBuffer));
    garbageList.emplace_back(GetGarbage(&mAllocation));

    SharedResourceUse sharedUse;
    sharedUse.init();
    sharedUse.updateSerialOneOff(serial);
    renderer->collectGarbage(std::move(sharedUse), std::move(garbageList));
}

angle::Result InitMappableAllocation(Context *context,
                                     const Allocator &allocator,
                                     Allocation *allocation,
                                     VkDeviceSize size,
                                     int value,
                                     VkMemoryPropertyFlags memoryPropertyFlags)
{
    uint8_t *mapPointer;
    ANGLE_VK_TRY(context, allocation->map(allocator, &mapPointer));
    memset(mapPointer, value, static_cast<size_t>(size));

    if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
    {
        allocation->flush(allocator, 0, size);
    }

    allocation->unmap(allocator);

    return angle::Result::Continue;
}

angle::Result InitMappableDeviceMemory(Context *context,
                                       DeviceMemory *deviceMemory,
                                       VkDeviceSize size,
                                       int value,
                                       VkMemoryPropertyFlags memoryPropertyFlags)
{
    VkDevice device = context->getDevice();

    uint8_t *mapPointer;
    ANGLE_VK_TRY(context, deviceMemory->map(device, 0, VK_WHOLE_SIZE, 0, &mapPointer));
    memset(mapPointer, value, static_cast<size_t>(size));

    // if the memory type is not host coherent, we perform an explicit flush
    if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
    {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory              = deviceMemory->getHandle();
        mappedRange.size                = VK_WHOLE_SIZE;
        ANGLE_VK_TRY(context, vkFlushMappedMemoryRanges(device, 1, &mappedRange));
    }

    deviceMemory->unmap(device);

    return angle::Result::Continue;
}

angle::Result AllocateBufferMemory(Context *context,
                                   VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                   VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                   const void *extraAllocationInfo,
                                   Buffer *buffer,
                                   DeviceMemory *deviceMemoryOut,
                                   VkDeviceSize *sizeOut)
{
    return AllocateBufferOrImageMemory(context, requestedMemoryPropertyFlags,
                                       memoryPropertyFlagsOut, extraAllocationInfo, buffer,
                                       deviceMemoryOut, sizeOut);
}

angle::Result AllocateImageMemory(Context *context,
                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                  VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                  const void *extraAllocationInfo,
                                  Image *image,
                                  DeviceMemory *deviceMemoryOut,
                                  VkDeviceSize *sizeOut)
{
    return AllocateBufferOrImageMemory(context, memoryPropertyFlags, memoryPropertyFlagsOut,
                                       extraAllocationInfo, image, deviceMemoryOut, sizeOut);
}

angle::Result AllocateImageMemoryWithRequirements(
    Context *context,
    VkMemoryPropertyFlags memoryPropertyFlags,
    const VkMemoryRequirements &memoryRequirements,
    const void *extraAllocationInfo,
    const VkBindImagePlaneMemoryInfoKHR *extraBindInfo,
    Image *image,
    DeviceMemory *deviceMemoryOut)
{
    VkMemoryPropertyFlags memoryPropertyFlagsOut = 0;
    return AllocateAndBindBufferOrImageMemory(context, memoryPropertyFlags, &memoryPropertyFlagsOut,
                                              memoryRequirements, extraAllocationInfo,
                                              extraBindInfo, image, deviceMemoryOut);
}

angle::Result AllocateBufferMemoryWithRequirements(Context *context,
                                                   VkMemoryPropertyFlags memoryPropertyFlags,
                                                   const VkMemoryRequirements &memoryRequirements,
                                                   const void *extraAllocationInfo,
                                                   Buffer *buffer,
                                                   VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                                   DeviceMemory *deviceMemoryOut)
{
    return AllocateAndBindBufferOrImageMemory(context, memoryPropertyFlags, memoryPropertyFlagsOut,
                                              memoryRequirements, extraAllocationInfo, nullptr,
                                              buffer, deviceMemoryOut);
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

gl::TextureType Get2DTextureType(uint32_t layerCount, GLint samples)
{
    if (layerCount > 1)
    {
        if (samples > 1)
        {
            return gl::TextureType::_2DMultisampleArray;
        }
        else
        {
            return gl::TextureType::_2DArray;
        }
    }
    else
    {
        if (samples > 1)
        {
            return gl::TextureType::_2DMultisample;
        }
        else
        {
            return gl::TextureType::_2D;
        }
    }
}

GarbageObject::GarbageObject() : mHandleType(HandleType::Invalid), mHandle(VK_NULL_HANDLE) {}

GarbageObject::GarbageObject(HandleType handleType, GarbageHandle handle)
    : mHandleType(handleType), mHandle(handle)
{}

GarbageObject::GarbageObject(GarbageObject &&other) : GarbageObject()
{
    *this = std::move(other);
}

GarbageObject &GarbageObject::operator=(GarbageObject &&rhs)
{
    std::swap(mHandle, rhs.mHandle);
    std::swap(mHandleType, rhs.mHandleType);
    return *this;
}

// GarbageObject implementation
// Using c-style casts here to avoid conditional compile for MSVC 32-bit
//  which fails to compile with reinterpret_cast, requiring static_cast.
void GarbageObject::destroy(RendererVk *renderer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "GarbageObject::destroy");
    VkDevice device = renderer->getDevice();
    switch (mHandleType)
    {
        case HandleType::Semaphore:
            vkDestroySemaphore(device, (VkSemaphore)mHandle, nullptr);
            break;
        case HandleType::CommandBuffer:
            // Command buffers are pool allocated.
            UNREACHABLE();
            break;
        case HandleType::Event:
            vkDestroyEvent(device, (VkEvent)mHandle, nullptr);
            break;
        case HandleType::Fence:
            vkDestroyFence(device, (VkFence)mHandle, nullptr);
            break;
        case HandleType::DeviceMemory:
            vkFreeMemory(device, (VkDeviceMemory)mHandle, nullptr);
            break;
        case HandleType::Buffer:
            vkDestroyBuffer(device, (VkBuffer)mHandle, nullptr);
            break;
        case HandleType::BufferView:
            vkDestroyBufferView(device, (VkBufferView)mHandle, nullptr);
            break;
        case HandleType::Image:
            vkDestroyImage(device, (VkImage)mHandle, nullptr);
            break;
        case HandleType::ImageView:
            vkDestroyImageView(device, (VkImageView)mHandle, nullptr);
            break;
        case HandleType::ShaderModule:
            vkDestroyShaderModule(device, (VkShaderModule)mHandle, nullptr);
            break;
        case HandleType::PipelineLayout:
            vkDestroyPipelineLayout(device, (VkPipelineLayout)mHandle, nullptr);
            break;
        case HandleType::RenderPass:
            vkDestroyRenderPass(device, (VkRenderPass)mHandle, nullptr);
            break;
        case HandleType::Pipeline:
            vkDestroyPipeline(device, (VkPipeline)mHandle, nullptr);
            break;
        case HandleType::DescriptorSetLayout:
            vkDestroyDescriptorSetLayout(device, (VkDescriptorSetLayout)mHandle, nullptr);
            break;
        case HandleType::Sampler:
            vkDestroySampler(device, (VkSampler)mHandle, nullptr);
            break;
        case HandleType::DescriptorPool:
            vkDestroyDescriptorPool(device, (VkDescriptorPool)mHandle, nullptr);
            break;
        case HandleType::Framebuffer:
            vkDestroyFramebuffer(device, (VkFramebuffer)mHandle, nullptr);
            break;
        case HandleType::CommandPool:
            vkDestroyCommandPool(device, (VkCommandPool)mHandle, nullptr);
            break;
        case HandleType::QueryPool:
            vkDestroyQueryPool(device, (VkQueryPool)mHandle, nullptr);
            break;
        case HandleType::Allocation:
            vma::FreeMemory(renderer->getAllocator().getHandle(), (VmaAllocation)mHandle);
            break;
        default:
            UNREACHABLE();
            break;
    }

    renderer->onDeallocateHandle(mHandleType);
}

void MakeDebugUtilsLabel(GLenum source, const char *marker, VkDebugUtilsLabelEXT *label)
{
    static constexpr angle::ColorF kLabelColors[6] = {
        angle::ColorF(1.0f, 0.5f, 0.5f, 1.0f),  // DEBUG_SOURCE_API
        angle::ColorF(0.5f, 1.0f, 0.5f, 1.0f),  // DEBUG_SOURCE_WINDOW_SYSTEM
        angle::ColorF(0.5f, 0.5f, 1.0f, 1.0f),  // DEBUG_SOURCE_SHADER_COMPILER
        angle::ColorF(0.7f, 0.7f, 0.7f, 1.0f),  // DEBUG_SOURCE_THIRD_PARTY
        angle::ColorF(0.5f, 0.8f, 0.9f, 1.0f),  // DEBUG_SOURCE_APPLICATION
        angle::ColorF(0.9f, 0.8f, 0.5f, 1.0f),  // DEBUG_SOURCE_OTHER
    };

    int colorIndex = source - GL_DEBUG_SOURCE_API;
    ASSERT(colorIndex >= 0 && static_cast<size_t>(colorIndex) < ArraySize(kLabelColors));

    label->sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label->pNext      = nullptr;
    label->pLabelName = marker;
    kLabelColors[colorIndex].writeData(label->color);
}

angle::Result SetDebugUtilsObjectName(ContextVk *contextVk,
                                      VkObjectType objectType,
                                      uint64_t handle,
                                      const std::string &label)
{
    RendererVk *renderer = contextVk->getRenderer();

    VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
    objectNameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objectNameInfo.objectType   = objectType;
    objectNameInfo.objectHandle = handle;
    objectNameInfo.pObjectName  = label.c_str();

    if (vkSetDebugUtilsObjectNameEXT)
    {
        ANGLE_VK_TRY(contextVk,
                     vkSetDebugUtilsObjectNameEXT(renderer->getDevice(), &objectNameInfo));
    }
    return angle::Result::Continue;
}

// ClearValuesArray implementation.
ClearValuesArray::ClearValuesArray() : mValues{}, mEnabled{} {}

ClearValuesArray::~ClearValuesArray() = default;

ClearValuesArray::ClearValuesArray(const ClearValuesArray &other) = default;

ClearValuesArray &ClearValuesArray::operator=(const ClearValuesArray &rhs) = default;

void ClearValuesArray::store(uint32_t index,
                             VkImageAspectFlags aspectFlags,
                             const VkClearValue &clearValue)
{
    ASSERT(aspectFlags != 0);

    // We do this double if to handle the packed depth-stencil case.
    if ((aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT) != 0)
    {
        // Ensure for packed DS we're writing to the depth index.
        ASSERT(index == kUnpackedDepthIndex ||
               (index == kUnpackedStencilIndex && aspectFlags == VK_IMAGE_ASPECT_STENCIL_BIT));

        storeNoDepthStencil(kUnpackedStencilIndex, clearValue);
    }

    if (aspectFlags != VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        storeNoDepthStencil(index, clearValue);
    }
}

void ClearValuesArray::storeNoDepthStencil(uint32_t index, const VkClearValue &clearValue)
{
    mValues[index] = clearValue;
    mEnabled.set(index);
}

gl::DrawBufferMask ClearValuesArray::getColorMask() const
{
    constexpr uint32_t kColorBuffersMask =
        angle::BitMask<uint32_t>(gl::IMPLEMENTATION_MAX_DRAW_BUFFERS);
    return gl::DrawBufferMask(mEnabled.bits() & kColorBuffersMask);
}

// ResourceSerialFactory implementation.
ResourceSerialFactory::ResourceSerialFactory() : mCurrentUniqueSerial(1) {}

ResourceSerialFactory::~ResourceSerialFactory() {}

uint32_t ResourceSerialFactory::issueSerial()
{
    uint32_t newSerial = ++mCurrentUniqueSerial;
    // make sure serial does not wrap
    ASSERT(newSerial > 0);
    return newSerial;
}

#define ANGLE_DEFINE_GEN_VK_SERIAL(Type)                         \
    Type##Serial ResourceSerialFactory::generate##Type##Serial() \
    {                                                            \
        return Type##Serial(issueSerial());                      \
    }

ANGLE_VK_SERIAL_OP(ANGLE_DEFINE_GEN_VK_SERIAL)

void ClampViewport(VkViewport *viewport)
{
    // 0-sized viewports are invalid in Vulkan.
    ASSERT(viewport);
    if (viewport->width == 0.0f)
    {
        viewport->width = 1.0f;
    }
    if (viewport->height == 0.0f)
    {
        viewport->height = 1.0f;
    }
}

void ApplyPipelineCreationFeedback(Context *context, const VkPipelineCreationFeedback &feedback)
{
    const bool cacheHit =
        (feedback.flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT) != 0;

    angle::VulkanPerfCounters &perfCounters = context->getPerfCounters();

    if (cacheHit)
    {
        ++perfCounters.pipelineCreationCacheHits;
        perfCounters.pipelineCreationTotalCacheHitsDurationNs += feedback.duration;
    }
    else
    {
        ++perfCounters.pipelineCreationCacheMisses;
        perfCounters.pipelineCreationTotalCacheMissesDurationNs += feedback.duration;
    }
}
}  // namespace vk

#if !defined(ANGLE_SHARED_LIBVULKAN)
// VK_EXT_debug_utils
PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT   = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT       = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT           = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT     = nullptr;
PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT       = nullptr;

// VK_EXT_debug_report
PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT   = nullptr;
PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = nullptr;

// VK_KHR_get_physical_device_properties2
PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR             = nullptr;
PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR                 = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR = nullptr;

// VK_KHR_external_semaphore_fd
PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR = nullptr;

// VK_EXT_external_memory_host
PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT = nullptr;

// VK_EXT_host_query_reset
PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT = nullptr;

// VK_EXT_transform_feedback
PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT = nullptr;
PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT             = nullptr;
PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT                 = nullptr;
PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT                       = nullptr;
PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT                           = nullptr;
PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT               = nullptr;

// VK_KHR_get_memory_requirements2
PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR = nullptr;
PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR   = nullptr;

// VK_KHR_bind_memory2
PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR = nullptr;
PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR   = nullptr;

// VK_KHR_external_fence_capabilities
PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR =
    nullptr;

// VK_KHR_external_fence_fd
PFN_vkGetFenceFdKHR vkGetFenceFdKHR       = nullptr;
PFN_vkImportFenceFdKHR vkImportFenceFdKHR = nullptr;

// VK_KHR_external_semaphore_capabilities
PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR
    vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = nullptr;

// VK_KHR_sampler_ycbcr_conversion
PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR   = nullptr;
PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR = nullptr;

// VK_KHR_create_renderpass2
PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR = nullptr;

#    if defined(ANGLE_PLATFORM_FUCHSIA)
// VK_FUCHSIA_imagepipe_surface
PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA = nullptr;
#    endif

#    if defined(ANGLE_PLATFORM_ANDROID)
PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID =
    nullptr;
PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID = nullptr;
#    endif

#    if defined(ANGLE_PLATFORM_GGP)
PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP = nullptr;
#    endif

#    define GET_INSTANCE_FUNC(vkName)                                                          \
        do                                                                                     \
        {                                                                                      \
            vkName = reinterpret_cast<PFN_##vkName>(vkGetInstanceProcAddr(instance, #vkName)); \
            ASSERT(vkName);                                                                    \
        } while (0)

#    define GET_DEVICE_FUNC(vkName)                                                        \
        do                                                                                 \
        {                                                                                  \
            vkName = reinterpret_cast<PFN_##vkName>(vkGetDeviceProcAddr(device, #vkName)); \
            ASSERT(vkName);                                                                \
        } while (0)

// VK_KHR_shared_presentable_image
PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR = nullptr;

// VK_EXT_extended_dynamic_state
PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT             = nullptr;
PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT                           = nullptr;
PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT = nullptr;
PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT               = nullptr;
PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT             = nullptr;
PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT           = nullptr;
PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT                         = nullptr;
PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT         = nullptr;
PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT           = nullptr;
PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT                         = nullptr;
PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT         = nullptr;
PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT         = nullptr;

// VK_EXT_extended_dynamic_state2
PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT                 = nullptr;
PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT                                 = nullptr;
PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT           = nullptr;
PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT   = nullptr;
PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT = nullptr;

// VK_KHR_fragment_shading_rate
PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR = nullptr;
PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR                         = nullptr;

// VK_GOOGLE_display_timing
PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE = nullptr;

void InitDebugUtilsEXTFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkCreateDebugUtilsMessengerEXT);
    GET_INSTANCE_FUNC(vkDestroyDebugUtilsMessengerEXT);
    GET_INSTANCE_FUNC(vkCmdBeginDebugUtilsLabelEXT);
    GET_INSTANCE_FUNC(vkCmdEndDebugUtilsLabelEXT);
    GET_INSTANCE_FUNC(vkCmdInsertDebugUtilsLabelEXT);
    GET_INSTANCE_FUNC(vkSetDebugUtilsObjectNameEXT);
}

void InitDebugReportEXTFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkCreateDebugReportCallbackEXT);
    GET_INSTANCE_FUNC(vkDestroyDebugReportCallbackEXT);
}

void InitGetPhysicalDeviceProperties2KHRFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkGetPhysicalDeviceProperties2KHR);
    GET_INSTANCE_FUNC(vkGetPhysicalDeviceFeatures2KHR);
    GET_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties2KHR);
}

void InitTransformFeedbackEXTFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkCmdBindTransformFeedbackBuffersEXT);
    GET_DEVICE_FUNC(vkCmdBeginTransformFeedbackEXT);
    GET_DEVICE_FUNC(vkCmdEndTransformFeedbackEXT);
    GET_DEVICE_FUNC(vkCmdBeginQueryIndexedEXT);
    GET_DEVICE_FUNC(vkCmdEndQueryIndexedEXT);
    GET_DEVICE_FUNC(vkCmdDrawIndirectByteCountEXT);
}

// VK_KHR_sampler_ycbcr_conversion
void InitSamplerYcbcrKHRFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkCreateSamplerYcbcrConversionKHR);
    GET_DEVICE_FUNC(vkDestroySamplerYcbcrConversionKHR);
}

// VK_KHR_create_renderpass2
void InitRenderPass2KHRFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkCreateRenderPass2KHR);
}

#    if defined(ANGLE_PLATFORM_FUCHSIA)
void InitImagePipeSurfaceFUCHSIAFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkCreateImagePipeSurfaceFUCHSIA);
}
#    endif

#    if defined(ANGLE_PLATFORM_ANDROID)
void InitExternalMemoryHardwareBufferANDROIDFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkGetAndroidHardwareBufferPropertiesANDROID);
    GET_INSTANCE_FUNC(vkGetMemoryAndroidHardwareBufferANDROID);
}
#    endif

#    if defined(ANGLE_PLATFORM_GGP)
void InitGGPStreamDescriptorSurfaceFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkCreateStreamDescriptorSurfaceGGP);
}
#    endif  // defined(ANGLE_PLATFORM_GGP)

void InitExternalSemaphoreFdFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkImportSemaphoreFdKHR);
}

void InitExternalMemoryHostFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkGetMemoryHostPointerPropertiesEXT);
}

void InitHostQueryResetFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkGetMemoryHostPointerPropertiesEXT);
}

// VK_KHR_get_memory_requirements2
void InitGetMemoryRequirements2KHRFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkGetBufferMemoryRequirements2KHR);
    GET_DEVICE_FUNC(vkGetImageMemoryRequirements2KHR);
}

// VK_KHR_bind_memory2
void InitBindMemory2KHRFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkBindBufferMemory2KHR);
    GET_DEVICE_FUNC(vkBindImageMemory2KHR);
}

// VK_KHR_external_fence_capabilities
void InitExternalFenceCapabilitiesFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkGetPhysicalDeviceExternalFencePropertiesKHR);
}

// VK_KHR_external_fence_fd
void InitExternalFenceFdFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkGetFenceFdKHR);
    GET_INSTANCE_FUNC(vkImportFenceFdKHR);
}

// VK_KHR_external_semaphore_capabilities
void InitExternalSemaphoreCapabilitiesFunctions(VkInstance instance)
{
    GET_INSTANCE_FUNC(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR);
}

// VK_KHR_shared_presentable_image
void InitGetSwapchainStatusKHRFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkGetSwapchainStatusKHR);
}

// VK_EXT_extended_dynamic_state
void InitExtendedDynamicStateEXTFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkCmdBindVertexBuffers2EXT);
    GET_DEVICE_FUNC(vkCmdSetCullModeEXT);
    GET_DEVICE_FUNC(vkCmdSetDepthBoundsTestEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetDepthCompareOpEXT);
    GET_DEVICE_FUNC(vkCmdSetDepthTestEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetDepthWriteEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetFrontFaceEXT);
    GET_DEVICE_FUNC(vkCmdSetPrimitiveTopologyEXT);
    GET_DEVICE_FUNC(vkCmdSetScissorWithCountEXT);
    GET_DEVICE_FUNC(vkCmdSetStencilOpEXT);
    GET_DEVICE_FUNC(vkCmdSetStencilTestEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetViewportWithCountEXT);
}

// VK_EXT_extended_dynamic_state2
void InitExtendedDynamicState2EXTFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkCmdSetDepthBiasEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetLogicOpEXT);
    GET_DEVICE_FUNC(vkCmdSetPatchControlPointsEXT);
    GET_DEVICE_FUNC(vkCmdSetPrimitiveRestartEnableEXT);
    GET_DEVICE_FUNC(vkCmdSetRasterizerDiscardEnableEXT);
}

// VK_KHR_fragment_shading_rate
void InitFragmentShadingRateKHRFunctions(VkDevice device)
{
    GET_DEVICE_FUNC(vkGetPhysicalDeviceFragmentShadingRatesKHR);
    GET_DEVICE_FUNC(vkCmdSetFragmentShadingRateKHR);
}

// VK_GOOGLE_display_timing
void InitGetPastPresentationTimingGoogleFunction(VkDevice device)
{
    GET_DEVICE_FUNC(vkGetPastPresentationTimingGOOGLE);
}

#    undef GET_INSTANCE_FUNC
#    undef GET_DEVICE_FUNC

#endif  // !defined(ANGLE_SHARED_LIBVULKAN)

GLenum CalculateGenerateMipmapFilter(ContextVk *contextVk, angle::FormatID formatID)
{
    const bool formatSupportsLinearFiltering = contextVk->getRenderer()->hasImageFormatFeatureBits(
        formatID, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    const bool hintFastest = contextVk->getState().getGenerateMipmapHint() == GL_FASTEST;

    return formatSupportsLinearFiltering && !hintFastest ? GL_LINEAR : GL_NEAREST;
}

// Return the log of samples.  Assumes |sampleCount| is a power of 2.  The result can be used to
// index an array based on sample count.  See for example TextureVk::PerSampleCountArray.
size_t PackSampleCount(GLint sampleCount)
{
    if (sampleCount == 0)
    {
        sampleCount = 1;
    }

    // We currently only support up to 16xMSAA.
    ASSERT(sampleCount <= VK_SAMPLE_COUNT_16_BIT);
    ASSERT(gl::isPow2(sampleCount));
    return gl::ScanForward(static_cast<uint32_t>(sampleCount));
}

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
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case GL_LINEAR:
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
        case gl::PrimitiveMode::LinesAdjacency:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
        case gl::PrimitiveMode::LineStripAdjacency:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
        case gl::PrimitiveMode::TrianglesAdjacency:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
        case gl::PrimitiveMode::TriangleStripAdjacency:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
        case gl::PrimitiveMode::Patches:
            return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        default:
            UNREACHABLE();
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }
}

VkCullModeFlagBits GetCullMode(const gl::RasterizerState &rasterState)
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
            UNREACHABLE();
            return VK_SAMPLE_COUNT_1_BIT;
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

VkCompareOp GetCompareOp(const GLenum compareFunc)
{
    switch (compareFunc)
    {
        case GL_NEVER:
            return VK_COMPARE_OP_NEVER;
        case GL_LESS:
            return VK_COMPARE_OP_LESS;
        case GL_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case GL_LEQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case GL_GREATER:
            return VK_COMPARE_OP_GREATER;
        case GL_NOTEQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case GL_GEQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case GL_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        default:
            UNREACHABLE();
            return VK_COMPARE_OP_ALWAYS;
    }
}

VkStencilOp GetStencilOp(GLenum compareOp)
{
    switch (compareOp)
    {
        case GL_KEEP:
            return VK_STENCIL_OP_KEEP;
        case GL_ZERO:
            return VK_STENCIL_OP_ZERO;
        case GL_REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case GL_INCR:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case GL_DECR:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case GL_INCR_WRAP:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case GL_DECR_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        case GL_INVERT:
            return VK_STENCIL_OP_INVERT;
        default:
            UNREACHABLE();
            return VK_STENCIL_OP_KEEP;
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
        case gl::TextureType::CubeMapArray:
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
        case gl::TextureType::CubeMapArray:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
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

VkShaderStageFlags GetShaderStageFlags(gl::ShaderBitSet activeShaders)
{
    VkShaderStageFlags flags = 0;
    for (const gl::ShaderType shaderType : activeShaders)
    {
        flags |= kShaderStageMap[shaderType];
    }
    return flags;
}

void GetViewport(const gl::Rectangle &viewport,
                 float nearPlane,
                 float farPlane,
                 bool invertViewport,
                 bool clipSpaceOriginUpperLeft,
                 GLint renderAreaHeight,
                 VkViewport *viewportOut)
{
    viewportOut->x        = static_cast<float>(viewport.x);
    viewportOut->y        = static_cast<float>(viewport.y);
    viewportOut->width    = static_cast<float>(viewport.width);
    viewportOut->height   = static_cast<float>(viewport.height);
    viewportOut->minDepth = gl::clamp01(nearPlane);
    viewportOut->maxDepth = gl::clamp01(farPlane);

    // Say an application intends to draw a primitive (shown as 'o' below), it can choose to use
    // different clip space origin. When clip space origin (shown as 'C' below) is switched from
    // lower-left to upper-left, primitives will be rendered with its y-coordinate flipped.

    // Rendered content will differ based on whether it is a default framebuffer or a user defined
    // framebuffer. We modify the viewport's 'y' and 'h' accordingly.

    // clip space origin is lower-left
    // Expected draw in GLES        default framebuffer    user defined framebuffer
    // (0,H)                        (0,0)                  (0,0)
    // +                            +-----------+  (W,0)   +-----------+ (W,0)
    // |                            |                      |  C----+
    // |                            |                      |  |    | (h)
    // |  +----+                    |  +----+              |  | O  |
    // |  | O  |                    |  | O  | (-h)         |  +----+
    // |  |    |                    |  |    |              |
    // |  C----+                    |  C----+              |
    // +-----------+ (W,0)          +                      +
    // (0,0)                        (0,H)                  (0,H)
    //                              y' = H - h             y' = y

    // clip space origin is upper-left
    // Expected draw in GLES        default framebuffer     user defined framebuffer
    // (0,H)                        (0,0)                  (0,0)
    // +                            +-----------+  (W,0)   +-----------+ (W,0)
    // |                            |                      |  +----+
    // |                            |                      |  | O  | (-h)
    // |  C----+                    |  C----+              |  |    |
    // |  |    |                    |  |    | (h)          |  C----+
    // |  | O  |                    |  | O  |              |
    // |  +----+                    |  +----+              |
    // +-----------+  (W,0)         +                      +
    // (0,0)                        (0,H)                  (0,H)
    //                              y' = H - (y + h)       y' = y + H

    if (clipSpaceOriginUpperLeft)
    {
        if (invertViewport)
        {
            viewportOut->y = static_cast<float>(renderAreaHeight - (viewport.height + viewport.y));
        }
        else
        {
            viewportOut->y      = static_cast<float>(viewport.height + viewport.y);
            viewportOut->height = -viewportOut->height;
        }
    }
    else
    {
        if (invertViewport)
        {
            viewportOut->y      = static_cast<float>(renderAreaHeight - viewport.y);
            viewportOut->height = -viewportOut->height;
        }
    }
}

void GetExtentsAndLayerCount(gl::TextureType textureType,
                             const gl::Extents &extents,
                             VkExtent3D *extentsOut,
                             uint32_t *layerCountOut)
{
    extentsOut->width  = extents.width;
    extentsOut->height = extents.height;

    switch (textureType)
    {
        case gl::TextureType::CubeMap:
            extentsOut->depth = 1;
            *layerCountOut    = gl::kCubeFaceCount;
            break;

        case gl::TextureType::_2DArray:
        case gl::TextureType::_2DMultisampleArray:
        case gl::TextureType::CubeMapArray:
            extentsOut->depth = 1;
            *layerCountOut    = extents.depth;
            break;

        default:
            extentsOut->depth = extents.depth;
            *layerCountOut    = 1;
            break;
    }
}

vk::LevelIndex GetLevelIndex(gl::LevelIndex levelGL, gl::LevelIndex baseLevel)
{
    ASSERT(baseLevel <= levelGL);
    return vk::LevelIndex(levelGL.get() - baseLevel.get());
}

}  // namespace gl_vk

namespace vk_gl
{
void AddSampleCounts(VkSampleCountFlags sampleCounts, gl::SupportedSampleSet *setOut)
{
    // The possible bits are VK_SAMPLE_COUNT_n_BIT = n, with n = 1 << b.  At the time of this
    // writing, b is in [0, 6], however, we test all 32 bits in case the enum is extended.
    for (size_t bit : angle::BitSet32<32>(sampleCounts & kSupportedSampleCounts))
    {
        setOut->insert(static_cast<GLuint>(1 << bit));
    }
}

GLuint GetMaxSampleCount(VkSampleCountFlags sampleCounts)
{
    GLuint maxCount = 0;
    for (size_t bit : angle::BitSet32<32>(sampleCounts & kSupportedSampleCounts))
    {
        maxCount = static_cast<GLuint>(1 << bit);
    }
    return maxCount;
}

GLuint GetSampleCount(VkSampleCountFlags supportedCounts, GLuint requestedCount)
{
    for (size_t bit : angle::BitSet32<32>(supportedCounts & kSupportedSampleCounts))
    {
        GLuint sampleCount = static_cast<GLuint>(1 << bit);
        if (sampleCount >= requestedCount)
        {
            return sampleCount;
        }
    }

    UNREACHABLE();
    return 0;
}

gl::LevelIndex GetLevelIndex(vk::LevelIndex levelVk, gl::LevelIndex baseLevel)
{
    return gl::LevelIndex(levelVk.get() + baseLevel.get());
}
}  // namespace vk_gl
}  // namespace rx
