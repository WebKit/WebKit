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

#include <atomic>
#include <limits>

#include "common/FixedVector.h"
#include "common/Optional.h"
#include "common/PackedEnums.h"
#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/Observer.h"
#include "libANGLE/renderer/serial_utils.h"
#include "libANGLE/renderer/vulkan/SecondaryCommandBuffer.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

#define ANGLE_GL_OBJECTS_X(PROC) \
    PROC(Buffer)                 \
    PROC(Context)                \
    PROC(Framebuffer)            \
    PROC(MemoryObject)           \
    PROC(Query)                  \
    PROC(Overlay)                \
    PROC(Program)                \
    PROC(Sampler)                \
    PROC(Semaphore)              \
    PROC(Texture)                \
    PROC(TransformFeedback)      \
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
class DummyOverlay;
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

// Append ptr to end of pNext chain beginning at chainStart
template <typename VulkanStruct1, typename VulkanStruct2>
void AppendToPNextChain(VulkanStruct1 *chainStart, VulkanStruct2 *ptr)
{
    VkBaseOutStructure *localPtr = reinterpret_cast<VkBaseOutStructure *>(chainStart);
    while (localPtr->pNext)
    {
        localPtr = localPtr->pNext;
    }
    localPtr->pNext = reinterpret_cast<VkBaseOutStructure *>(ptr);
}

extern const char *gLoaderLayersPathEnv;
extern const char *gLoaderICDFilenamesEnv;

enum class ICD
{
    Default,
    Mock,
    SwiftShader,
};

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
struct ImplTypeHelper<gl::DummyOverlay>
{
    using ImplType = OverlayVk;
};

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

template <>
inline OverlayVk *GetImpl(const gl::DummyOverlay *glObject)
{
    return nullptr;
}

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

// Reference to a deleted object. The object is due to be destroyed at some point in the future.
// |mHandleType| determines the type of the object and which destroy function should be called.
class GarbageObject
{
  public:
    GarbageObject();
    GarbageObject(GarbageObject &&other);
    GarbageObject &operator=(GarbageObject &&rhs);

    bool valid() const { return mHandle != VK_NULL_HANDLE; }
    void destroy(VkDevice device);

    template <typename DerivedT, typename HandleT>
    static GarbageObject Get(WrappedObject<DerivedT, HandleT> *object)
    {
        // Using c-style cast here to avoid conditional compile for MSVC 32-bit
        //  which fails to compile with reinterpret_cast, requiring static_cast.
        return GarbageObject(HandleTypeHelper<DerivedT>::kHandleType,
                             (GarbageHandle)(object->release()));
    }

  private:
    VK_DEFINE_NON_DISPATCHABLE_HANDLE(GarbageHandle)
    GarbageObject(HandleType handleType, GarbageHandle handle);

    HandleType mHandleType;
    GarbageHandle mHandle;
};

template <typename T>
GarbageObject GetGarbage(T *obj)
{
    return GarbageObject::Get(obj);
}

// A list of garbage objects. Has no object lifetime information.
using GarbageList = std::vector<GarbageObject>;

// A list of garbage objects and the associated serial after which the objects can be destroyed.
using GarbageAndSerial = ObjectAndSerial<GarbageList>;

// Houses multiple lists of garbage objects. Each sub-list has a different lifetime. They should
// be sorted such that later-living garbage is ordered later in the list.
using GarbageQueue = std::vector<GarbageAndSerial>;

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
    void release(ContextVk *contextVk);
    void destroy(VkDevice device);

    angle::Result init(ContextVk *context, VkDeviceSize size, StagingUsage usage);

    Buffer &getBuffer() { return mBuffer; }
    const Buffer &getBuffer() const { return mBuffer; }
    DeviceMemory &getDeviceMemory() { return mDeviceMemory; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }
    size_t getSize() const { return mSize; }

  private:
    Buffer mBuffer;
    DeviceMemory mDeviceMemory;
    size_t mSize;
};

angle::Result AllocateBufferMemory(Context *context,
                                   VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                   VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                   const void *extraAllocationInfo,
                                   Buffer *buffer,
                                   DeviceMemory *deviceMemoryOut);

angle::Result AllocateImageMemory(Context *context,
                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                  const void *extraAllocationInfo,
                                  Image *image,
                                  DeviceMemory *deviceMemoryOut);
angle::Result AllocateImageMemoryWithRequirements(Context *context,
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

gl::TextureType Get2DTextureType(uint32_t layerCount, GLint samples);

enum class RecordingMode
{
    Start,
    Append,
};

// Helper class to handle RAII patterns for initialization. Requires that T have a destroy method
// that takes a VkDevice and returns void.
template <typename T>
class DeviceScoped final : angle::NonCopyable
{
  public:
    DeviceScoped(VkDevice device) : mDevice(device) {}
    ~DeviceScoped() { mVar.destroy(mDevice); }

    const T &get() const { return mVar; }
    T &get() { return mVar; }

    T &&release() { return std::move(mVar); }

  private:
    VkDevice mDevice;
    T mVar;
};

// Similar to DeviceScoped, but releases objects instead of destroying them. Requires that T have a
// release method that takes a ContextVk * and returns void.
template <typename T>
class ContextScoped final : angle::NonCopyable
{
  public:
    ContextScoped(ContextVk *contextVk) : mContextVk(contextVk) {}
    ~ContextScoped() { mVar.release(mContextVk); }

    const T &get() const { return mVar; }
    T &get() { return mVar; }

    T &&release() { return std::move(mVar); }

  private:
    ContextVk *mContextVk;
    T mVar;
};

template <typename T>
class RendererScoped final : angle::NonCopyable
{
  public:
    RendererScoped(RendererVk *renderer) : mRenderer(renderer) {}
    ~RendererScoped() { mVar.release(mRenderer); }

    const T &get() const { return mVar; }
    T &get() { return mVar; }

    T &&release() { return std::move(mVar); }

  private:
    RendererVk *mRenderer;
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

    template <typename RecyclerT>
    void resetAndRecycle(RecyclerT *recycler)
    {
        if (mRefCounted)
        {
            mRefCounted->releaseRef();
            if (!mRefCounted->isReferenced())
            {
                ASSERT(mRefCounted->get().valid());
                recycler->recycle(std::move(mRefCounted->get()));
                SafeDelete(mRefCounted);
            }

            mRefCounted = nullptr;
        }
    }

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

template <typename T>
class Recycler final : angle::NonCopyable
{
  public:
    Recycler() = default;

    void recycle(T &&garbageObject) { mObjectFreeList.emplace_back(std::move(garbageObject)); }

    void fetch(T *outObject)
    {
        ASSERT(!empty());
        *outObject = std::move(mObjectFreeList.back());
        mObjectFreeList.pop_back();
    }

    void destroy(VkDevice device)
    {
        for (T &object : mObjectFreeList)
        {
            object.destroy(device);
        }
    }

    bool empty() const { return mObjectFreeList.empty(); }

  private:
    std::vector<T> mObjectFreeList;
};

}  // namespace vk

namespace gl_vk
{
VkRect2D GetRect(const gl::Rectangle &source);
VkFilter GetFilter(const GLenum filter);
VkSamplerMipmapMode GetSamplerMipmapMode(const GLenum filter);
VkSamplerAddressMode GetSamplerAddressMode(const GLenum wrap);
VkPrimitiveTopology GetPrimitiveTopology(gl::PrimitiveMode mode);
VkCullModeFlagBits GetCullMode(const gl::RasterizerState &rasterState);
VkFrontFace GetFrontFace(GLenum frontFace, bool invertCullFace);
VkSampleCountFlagBits GetSamples(GLint sampleCount);
VkComponentSwizzle GetSwizzle(const GLenum swizzle);
VkCompareOp GetCompareOp(const GLenum compareFunc);

constexpr angle::PackedEnumMap<gl::DrawElementsType, VkIndexType> kIndexTypeMap = {
    {gl::DrawElementsType::UnsignedByte, VK_INDEX_TYPE_UINT16},
    {gl::DrawElementsType::UnsignedShort, VK_INDEX_TYPE_UINT16},
    {gl::DrawElementsType::UnsignedInt, VK_INDEX_TYPE_UINT32},
};

constexpr gl::ShaderMap<VkShaderStageFlagBits> kShaderStageMap = {
    {gl::ShaderType::Vertex, VK_SHADER_STAGE_VERTEX_BIT},
    {gl::ShaderType::Fragment, VK_SHADER_STAGE_FRAGMENT_BIT},
    {gl::ShaderType::Geometry, VK_SHADER_STAGE_GEOMETRY_BIT},
    {gl::ShaderType::Compute, VK_SHADER_STAGE_COMPUTE_BIT},
};

void GetOffset(const gl::Offset &glOffset, VkOffset3D *vkOffset);
void GetExtent(const gl::Extents &glExtent, VkExtent3D *vkExtent);
VkImageType GetImageType(gl::TextureType textureType);
VkImageViewType GetImageViewType(gl::TextureType textureType);
VkColorComponentFlags GetColorComponentFlags(bool red, bool green, bool blue, bool alpha);
VkShaderStageFlags GetShaderStageFlags(gl::ShaderBitSet activeShaders);

void GetViewport(const gl::Rectangle &viewport,
                 float nearPlane,
                 float farPlane,
                 bool invertViewport,
                 GLint renderAreaHeight,
                 VkViewport *viewportOut);

void GetExtentsAndLayerCount(gl::TextureType textureType,
                             const gl::Extents &extents,
                             VkExtent3D *extentsOut,
                             uint32_t *layerCountOut);
}  // namespace gl_vk

namespace vk_gl
{
// The Vulkan back-end will not support a sample count of 1, because of a Vulkan specification
// restriction:
//
//   If the image was created with VkImageCreateInfo::samples equal to VK_SAMPLE_COUNT_1_BIT, the
//   instruction must: have MS = 0.
//
// This restriction was tracked in http://anglebug.com/4196 and Khronos-private Vulkan
// specification issue https://gitlab.khronos.org/vulkan/vulkan/issues/1925.
//
// In addition, the Vulkan back-end will not support sample counts of 32 or 64, since there are no
// standard sample locations for those sample counts.
constexpr unsigned int kSupportedSampleCounts = (VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT |
                                                 VK_SAMPLE_COUNT_8_BIT | VK_SAMPLE_COUNT_16_BIT);

// Find set bits in sampleCounts and add the corresponding sample count to the set.
void AddSampleCounts(VkSampleCountFlags sampleCounts, gl::SupportedSampleSet *outSet);
// Return the maximum sample count with a bit set in |sampleCounts|.
GLuint GetMaxSampleCount(VkSampleCountFlags sampleCounts);
// Return a supported sample count that's at least as large as the requested one.
GLuint GetSampleCount(VkSampleCountFlags supportedCounts, GLuint requestedCount);
}  // namespace vk_gl

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
