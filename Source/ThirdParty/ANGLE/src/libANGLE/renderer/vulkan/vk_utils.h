//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_utils:
//    Helper functions for the Vulkan Renderer.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_

#include <limits>

#include "common/FixedVector.h"
#include "common/Optional.h"
#include "common/PackedEnums.h"
#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/Observer.h"
#include "libANGLE/renderer/vulkan/SecondaryCommandBuffer.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

#define ANGLE_GL_OBJECTS_X(PROC) \
    PROC(Buffer)                 \
    PROC(Context)                \
    PROC(Framebuffer)            \
    PROC(MemoryObject)           \
    PROC(Program)                \
    PROC(Texture)                \
    PROC(VertexArray)

#define ANGLE_PRE_DECLARE_OBJECT(OBJ) class OBJ;

namespace egl
{
class Display;
class Image;
}  // namespace egl

namespace gl
{
struct Box;
struct Extents;
struct RasterizerState;
struct Rectangle;
class State;
struct SwizzleState;
struct VertexAttribute;
class VertexBinding;

ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_OBJECT)
}  // namespace gl

#define ANGLE_PRE_DECLARE_VK_OBJECT(OBJ) class OBJ##Vk;

namespace rx
{
class CommandGraphResource;
class DisplayVk;
class ImageVk;
class RenderTargetVk;
class RendererVk;
class RenderPassCache;
}  // namespace rx

namespace angle
{
egl::Error ToEGL(Result result, rx::DisplayVk *displayVk, EGLint errorCode);
}  // namespace angle

namespace rx
{
ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_VK_OBJECT)

const char *VulkanResultString(VkResult result);

constexpr size_t kMaxVulkanLayers = 20;
using VulkanLayerVector           = angle::FixedVector<const char *, kMaxVulkanLayers>;

// Verify that validation layers are available.
bool GetAvailableValidationLayers(const std::vector<VkLayerProperties> &layerProps,
                                  bool mustHaveLayers,
                                  VulkanLayerVector *enabledLayerNames);

extern const char *g_VkLoaderLayersPathEnv;
extern const char *g_VkICDPathEnv;

enum class TextureDimension
{
    TEX_2D,
    TEX_CUBE,
    TEX_3D,
    TEX_2D_ARRAY,
};

namespace vk
{
struct Format;

// Abstracts error handling. Implemented by both ContextVk for GL and DisplayVk for EGL errors.
class Context : angle::NonCopyable
{
  public:
    Context(RendererVk *renderer);
    virtual ~Context();

    virtual void handleError(VkResult result,
                             const char *file,
                             const char *function,
                             unsigned int line) = 0;
    VkDevice getDevice() const;
    RendererVk *getRenderer() const { return mRenderer; }

  protected:
    RendererVk *const mRenderer;
};

#if ANGLE_USE_CUSTOM_VULKAN_CMD_BUFFERS
using CommandBuffer = priv::SecondaryCommandBuffer;
#else
using CommandBuffer = priv::CommandBuffer;
#endif

using PrimaryCommandBuffer = priv::CommandBuffer;

VkImageAspectFlags GetDepthStencilAspectFlags(const angle::Format &format);
VkImageAspectFlags GetFormatAspectFlags(const angle::Format &format);
VkImageAspectFlags GetDepthStencilAspectFlagsForCopy(bool copyDepth, bool copyStencil);

template <typename T>
struct ImplTypeHelper;

// clang-format off
#define ANGLE_IMPL_TYPE_HELPER_GL(OBJ) \
template<>                             \
struct ImplTypeHelper<gl::OBJ>         \
{                                      \
    using ImplType = OBJ##Vk;          \
};
// clang-format on

ANGLE_GL_OBJECTS_X(ANGLE_IMPL_TYPE_HELPER_GL)

template <>
struct ImplTypeHelper<egl::Display>
{
    using ImplType = DisplayVk;
};

template <>
struct ImplTypeHelper<egl::Image>
{
    using ImplType = ImageVk;
};

template <typename T>
using GetImplType = typename ImplTypeHelper<T>::ImplType;

template <typename T>
GetImplType<T> *GetImpl(const T *glObject)
{
    return GetImplAs<GetImplType<T>>(glObject);
}

class GarbageObject final
{
  public:
    template <typename ObjectT>
    GarbageObject(Serial serial, const ObjectT &object)
        : mSerial(serial),
          mHandleType(HandleTypeHelper<ObjectT>::kHandleType),
          mHandle(reinterpret_cast<VkDevice>(object.getHandle()))
    {}

    GarbageObject();
    GarbageObject(const GarbageObject &other);
    GarbageObject &operator=(const GarbageObject &other);

    bool destroyIfComplete(VkDevice device, Serial completedSerial);
    void destroy(VkDevice device);

  private:
    // TODO(jmadill): Since many objects will have the same serial, it might be more efficient to
    // store the serial outside of the garbage object itself. We could index ranges of garbage
    // objects in the Renderer, using a circular buffer.
    Serial mSerial;
    HandleType mHandleType;
    VkDevice mHandle;
};

class MemoryProperties final : angle::NonCopyable
{
  public:
    MemoryProperties();

    void init(VkPhysicalDevice physicalDevice);
    angle::Result findCompatibleMemoryIndex(Context *context,
                                            const VkMemoryRequirements &memoryRequirements,
                                            VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                            VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                            uint32_t *indexOut) const;
    void destroy();

  private:
    VkPhysicalDeviceMemoryProperties mMemoryProperties;
};

// Similar to StagingImage, for Buffers.
class StagingBuffer final : angle::NonCopyable
{
  public:
    StagingBuffer();
    void destroy(VkDevice device);

    angle::Result init(Context *context, VkDeviceSize size, StagingUsage usage);

    Buffer &getBuffer() { return mBuffer; }
    const Buffer &getBuffer() const { return mBuffer; }
    DeviceMemory &getDeviceMemory() { return mDeviceMemory; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }
    size_t getSize() const { return mSize; }

    void dumpResources(Serial serial, std::vector<GarbageObject> *garbageQueue);

  private:
    Buffer mBuffer;
    DeviceMemory mDeviceMemory;
    size_t mSize;
};

template <typename ObjT>
class ObjectAndSerial final : angle::NonCopyable
{
  public:
    ObjectAndSerial() {}

    ObjectAndSerial(ObjT &&object, Serial serial) : mObject(std::move(object)), mSerial(serial) {}

    ObjectAndSerial(ObjectAndSerial &&other)
        : mObject(std::move(other.mObject)), mSerial(std::move(other.mSerial))
    {}
    ObjectAndSerial &operator=(ObjectAndSerial &&other)
    {
        mObject = std::move(other.mObject);
        mSerial = std::move(other.mSerial);
        return *this;
    }

    Serial getSerial() const { return mSerial; }
    void updateSerial(Serial newSerial) { mSerial = newSerial; }

    const ObjT &get() const { return mObject; }
    ObjT &get() { return mObject; }

    bool valid() const { return mObject.valid(); }

    void destroy(VkDevice device)
    {
        mObject.destroy(device);
        mSerial = Serial();
    }

  private:
    ObjT mObject;
    Serial mSerial;
};

angle::Result AllocateBufferMemory(vk::Context *context,
                                   VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                   VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                   const void *extraAllocationInfo,
                                   Buffer *buffer,
                                   DeviceMemory *deviceMemoryOut);

angle::Result AllocateImageMemory(vk::Context *context,
                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                  const void *extraAllocationInfo,
                                  Image *image,
                                  DeviceMemory *deviceMemoryOut);
angle::Result AllocateImageMemoryWithRequirements(vk::Context *context,
                                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                                  const VkMemoryRequirements &memoryRequirements,
                                                  const void *extraAllocationInfo,
                                                  Image *image,
                                                  DeviceMemory *deviceMemoryOut);

using ShaderAndSerial = ObjectAndSerial<ShaderModule>;

angle::Result InitShaderAndSerial(Context *context,
                                  ShaderAndSerial *shaderAndSerial,
                                  const uint32_t *shaderCode,
                                  size_t shaderCodeSize);

enum class RecordingMode
{
    Start,
    Append,
};

// Helper class to handle RAII patterns for initialization. Requires that T have a destroy method
// that takes a VkDevice and returns void.
template <typename T>
class Scoped final : angle::NonCopyable
{
  public:
    Scoped(VkDevice device) : mDevice(device) {}
    ~Scoped() { mVar.destroy(mDevice); }

    const T &get() const { return mVar; }
    T &get() { return mVar; }

    T &&release() { return std::move(mVar); }

  private:
    VkDevice mDevice;
    T mVar;
};

// This is a very simple RefCount class that has no autoreleasing. Used in the descriptor set and
// pipeline layout caches.
template <typename T>
class RefCounted : angle::NonCopyable
{
  public:
    RefCounted() : mRefCount(0) {}
    explicit RefCounted(T &&newObject) : mRefCount(0), mObject(std::move(newObject)) {}
    ~RefCounted() { ASSERT(mRefCount == 0 && !mObject.valid()); }

    RefCounted(RefCounted &&copy) : mRefCount(copy.mRefCount), mObject(std::move(copy.mObject))
    {
        ASSERT(this != &copy);
        copy.mRefCount = 0;
    }

    RefCounted &operator=(RefCounted &&rhs)
    {
        std::swap(mRefCount, rhs.mRefCount);
        mObject = std::move(rhs.mObject);
        return *this;
    }

    void addRef()
    {
        ASSERT(mRefCount != std::numeric_limits<uint32_t>::max());
        mRefCount++;
    }

    void releaseRef()
    {
        ASSERT(isReferenced());
        mRefCount--;
    }

    bool isReferenced() const { return mRefCount != 0; }

    T &get() { return mObject; }
    const T &get() const { return mObject; }

  private:
    uint32_t mRefCount;
    T mObject;
};

template <typename T>
class BindingPointer final : angle::NonCopyable
{
  public:
    BindingPointer() : mRefCounted(nullptr) {}

    ~BindingPointer() { reset(); }

    void set(RefCounted<T> *refCounted)
    {
        if (mRefCounted)
        {
            mRefCounted->releaseRef();
        }

        mRefCounted = refCounted;

        if (mRefCounted)
        {
            mRefCounted->addRef();
        }
    }

    void reset() { set(nullptr); }

    T &get() { return mRefCounted->get(); }
    const T &get() const { return mRefCounted->get(); }

    bool valid() const { return mRefCounted != nullptr; }

  private:
    RefCounted<T> *mRefCounted;
};

// Helper class to share ref-counted Vulkan objects.  Requires that T have a destroy method
// that takes a VkDevice and returns void.
template <typename T>
class Shared final : angle::NonCopyable
{
  public:
    Shared() : mRefCounted(nullptr) {}
    ~Shared() { ASSERT(mRefCounted == nullptr); }

    Shared(Shared &&other) { *this = std::move(other); }
    Shared &operator=(Shared &&other)
    {
        ASSERT(this != &other);
        mRefCounted       = other.mRefCounted;
        other.mRefCounted = nullptr;
        return *this;
    }

    void set(VkDevice device, RefCounted<T> *refCounted)
    {
        if (mRefCounted)
        {
            mRefCounted->releaseRef();
            if (!mRefCounted->isReferenced())
            {
                mRefCounted->get().destroy(device);
                SafeDelete(mRefCounted);
            }
        }

        mRefCounted = refCounted;

        if (mRefCounted)
        {
            mRefCounted->addRef();
        }
    }

    void assign(VkDevice device, T &&newObject)
    {
        set(device, new RefCounted<T>(std::move(newObject)));
    }

    void copy(VkDevice device, const Shared<T> &other) { set(device, other.mRefCounted); }

    void reset(VkDevice device) { set(device, nullptr); }

    bool isReferenced() const
    {
        // If reference is zero, the object should have been deleted.  I.e. if the object is not
        // nullptr, it should have a reference.
        ASSERT(!mRefCounted || mRefCounted->isReferenced());
        return mRefCounted != nullptr;
    }

    T &get()
    {
        ASSERT(mRefCounted && mRefCounted->isReferenced());
        return mRefCounted->get();
    }
    const T &get() const
    {
        ASSERT(mRefCounted && mRefCounted->isReferenced());
        return mRefCounted->get();
    }

  private:
    RefCounted<T> *mRefCounted;
};
}  // namespace vk

// List of function pointers for used extensions.
// VK_EXT_debug_utils
extern PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
extern PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
extern PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;

// VK_EXT_debug_report
extern PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
extern PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;

// VK_KHR_get_physical_device_properties2
extern PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;

// Lazily load entry points for each extension as necessary.
void InitDebugUtilsEXTFunctions(VkInstance instance);
void InitDebugReportEXTFunctions(VkInstance instance);
void InitGetPhysicalDeviceProperties2KHRFunctions(VkInstance instance);

#if defined(ANGLE_PLATFORM_FUCHSIA)
// VK_FUCHSIA_imagepipe_surface
extern PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
void InitImagePipeSurfaceFUCHSIAFunctions(VkInstance instance);
#endif

#if defined(ANGLE_PLATFORM_ANDROID)
// VK_ANDROID_external_memory_android_hardware_buffer
extern PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
extern PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
void InitExternalMemoryHardwareBufferANDROIDFunctions(VkInstance instance);
#endif

namespace gl_vk
{
VkRect2D GetRect(const gl::Rectangle &source);
VkFilter GetFilter(const GLenum filter);
VkSamplerMipmapMode GetSamplerMipmapMode(const GLenum filter);
VkSamplerAddressMode GetSamplerAddressMode(const GLenum wrap);
VkPrimitiveTopology GetPrimitiveTopology(gl::PrimitiveMode mode);
VkCullModeFlags GetCullMode(const gl::RasterizerState &rasterState);
VkFrontFace GetFrontFace(GLenum frontFace, bool invertCullFace);
VkSampleCountFlagBits GetSamples(GLint sampleCount);
VkComponentSwizzle GetSwizzle(const GLenum swizzle);

constexpr angle::PackedEnumMap<gl::DrawElementsType, VkIndexType> kIndexTypeMap = {
    {gl::DrawElementsType::UnsignedByte, VK_INDEX_TYPE_UINT16},
    {gl::DrawElementsType::UnsignedShort, VK_INDEX_TYPE_UINT16},
    {gl::DrawElementsType::UnsignedInt, VK_INDEX_TYPE_UINT32},
};

void GetOffset(const gl::Offset &glOffset, VkOffset3D *vkOffset);
void GetExtent(const gl::Extents &glExtent, VkExtent3D *vkExtent);
VkImageType GetImageType(gl::TextureType textureType);
VkImageViewType GetImageViewType(gl::TextureType textureType);
VkColorComponentFlags GetColorComponentFlags(bool red, bool green, bool blue, bool alpha);

void GetViewport(const gl::Rectangle &viewport,
                 float nearPlane,
                 float farPlane,
                 bool invertViewport,
                 GLint renderAreaHeight,
                 VkViewport *viewportOut);
}  // namespace gl_vk

}  // namespace rx

#define ANGLE_VK_TRY(context, command)                                                 \
    do                                                                                 \
    {                                                                                  \
        auto ANGLE_LOCAL_VAR = command;                                                \
        if (ANGLE_UNLIKELY(ANGLE_LOCAL_VAR != VK_SUCCESS))                             \
        {                                                                              \
            context->handleError(ANGLE_LOCAL_VAR, __FILE__, ANGLE_FUNCTION, __LINE__); \
            return angle::Result::Stop;                                                \
        }                                                                              \
    } while (0)

#define ANGLE_VK_CHECK(context, test, error) ANGLE_VK_TRY(context, test ? VK_SUCCESS : error)

#define ANGLE_VK_CHECK_MATH(context, result) \
    ANGLE_VK_CHECK(context, result, VK_ERROR_VALIDATION_FAILED_EXT)

#define ANGLE_VK_CHECK_ALLOC(context, result) \
    ANGLE_VK_CHECK(context, result, VK_ERROR_OUT_OF_HOST_MEMORY)

#define ANGLE_VK_UNREACHABLE(context) \
    UNREACHABLE();                    \
    ANGLE_VK_CHECK(context, false, VK_ERROR_FEATURE_NOT_PRESENT)

#endif  // LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_
