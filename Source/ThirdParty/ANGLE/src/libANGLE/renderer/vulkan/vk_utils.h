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
#include <queue>

#include "GLSLANG/ShaderLang.h"
#include "common/FixedVector.h"
#include "common/Optional.h"
#include "common/PackedEnums.h"
#include "common/backtrace_utils.h"
#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/Observer.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/serial_utils.h"
#include "libANGLE/renderer/vulkan/SecondaryCommandBuffer.h"
#include "libANGLE/renderer/vulkan/VulkanSecondaryCommandBuffer.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"
#include "platform/FeaturesVk_autogen.h"
#include "vulkan/vulkan_fuchsia_ext.h"

#define ANGLE_GL_OBJECTS_X(PROC) \
    PROC(Buffer)                 \
    PROC(Context)                \
    PROC(Framebuffer)            \
    PROC(MemoryObject)           \
    PROC(Overlay)                \
    PROC(Program)                \
    PROC(ProgramPipeline)        \
    PROC(Query)                  \
    PROC(Renderbuffer)           \
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
class ShareGroup;
}  // namespace egl

namespace gl
{
class MockOverlay;
class ProgramExecutable;
struct RasterizerState;
struct SwizzleState;
struct VertexAttribute;
class VertexBinding;

ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_OBJECT)
}  // namespace gl

#define ANGLE_PRE_DECLARE_VK_OBJECT(OBJ) class OBJ##Vk;

namespace rx
{
class DisplayVk;
class ImageVk;
class ProgramExecutableVk;
class RenderbufferVk;
class RenderTargetVk;
class RendererVk;
class RenderPassCache;
class ShareGroupVk;
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

// A maximum offset of 4096 covers almost every Vulkan driver on desktop (80%) and mobile (99%). The
// next highest values to meet native drivers are 16 bits or 32 bits.
constexpr uint32_t kAttributeOffsetMaxBits = 15;
constexpr uint32_t kInvalidMemoryTypeIndex = UINT32_MAX;

namespace vk
{
// A packed attachment index interface with vulkan API
class PackedAttachmentIndex final
{
  public:
    explicit constexpr PackedAttachmentIndex(uint32_t index) : mAttachmentIndex(index) {}
    constexpr PackedAttachmentIndex(const PackedAttachmentIndex &other)            = default;
    constexpr PackedAttachmentIndex &operator=(const PackedAttachmentIndex &other) = default;

    constexpr uint32_t get() const { return mAttachmentIndex; }
    PackedAttachmentIndex &operator++()
    {
        ++mAttachmentIndex;
        return *this;
    }
    constexpr bool operator==(const PackedAttachmentIndex &other) const
    {
        return mAttachmentIndex == other.mAttachmentIndex;
    }
    constexpr bool operator!=(const PackedAttachmentIndex &other) const
    {
        return mAttachmentIndex != other.mAttachmentIndex;
    }
    constexpr bool operator<(const PackedAttachmentIndex &other) const
    {
        return mAttachmentIndex < other.mAttachmentIndex;
    }

  private:
    uint32_t mAttachmentIndex;
};
using PackedAttachmentCount                                    = PackedAttachmentIndex;
static constexpr PackedAttachmentIndex kAttachmentIndexInvalid = PackedAttachmentIndex(-1);
static constexpr PackedAttachmentIndex kAttachmentIndexZero    = PackedAttachmentIndex(0);

// Prepend ptr to the pNext chain at chainStart
template <typename VulkanStruct1, typename VulkanStruct2>
void AddToPNextChain(VulkanStruct1 *chainStart, VulkanStruct2 *ptr)
{
    ASSERT(ptr->pNext == nullptr);

    VkBaseOutStructure *localPtr = reinterpret_cast<VkBaseOutStructure *>(chainStart);
    ptr->pNext                   = localPtr->pNext;
    localPtr->pNext              = reinterpret_cast<VkBaseOutStructure *>(ptr);
}

// Append ptr to the end of the chain
template <typename VulkanStruct1, typename VulkanStruct2>
void AppendToPNextChain(VulkanStruct1 *chainStart, VulkanStruct2 *ptr)
{
    if (!ptr)
    {
        return;
    }

    VkBaseOutStructure *endPtr = reinterpret_cast<VkBaseOutStructure *>(chainStart);
    while (endPtr->pNext)
    {
        endPtr = endPtr->pNext;
    }
    endPtr->pNext = reinterpret_cast<VkBaseOutStructure *>(ptr);
}

struct Error
{
    VkResult errorCode;
    const char *file;
    const char *function;
    uint32_t line;
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
    const angle::FeaturesVk &getFeatures() const;

    const angle::VulkanPerfCounters &getPerfCounters() const { return mPerfCounters; }
    angle::VulkanPerfCounters &getPerfCounters() { return mPerfCounters; }

  protected:
    RendererVk *const mRenderer;
    angle::VulkanPerfCounters mPerfCounters;
};

class RenderPassDesc;

#if ANGLE_USE_CUSTOM_VULKAN_OUTSIDE_RENDER_PASS_CMD_BUFFERS
using OutsideRenderPassCommandBuffer = priv::SecondaryCommandBuffer;
#else
using OutsideRenderPassCommandBuffer         = VulkanSecondaryCommandBuffer;
#endif
#if ANGLE_USE_CUSTOM_VULKAN_RENDER_PASS_CMD_BUFFERS
using RenderPassCommandBuffer = priv::SecondaryCommandBuffer;
#else
using RenderPassCommandBuffer                = VulkanSecondaryCommandBuffer;
#endif

struct SecondaryCommandBufferList
{
    std::vector<OutsideRenderPassCommandBuffer> outsideRenderPassCommandBuffers;
    std::vector<RenderPassCommandBuffer> renderPassCommandBuffers;
};

struct SecondaryCommandPools
{
    CommandPool outsideRenderPassPool;
    CommandPool renderPassPool;
};

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
struct ImplTypeHelper<gl::MockOverlay>
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

template <>
struct ImplTypeHelper<egl::ShareGroup>
{
    using ImplType = ShareGroupVk;
};

template <typename T>
using GetImplType = typename ImplTypeHelper<T>::ImplType;

template <typename T>
GetImplType<T> *GetImpl(const T *glObject)
{
    return GetImplAs<GetImplType<T>>(glObject);
}

template <typename T>
GetImplType<T> *SafeGetImpl(const T *glObject)
{
    return SafeGetImplAs<GetImplType<T>>(glObject);
}

template <>
inline OverlayVk *GetImpl(const gl::MockOverlay *glObject)
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
    void destroy(RendererVk *renderer);

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

// Houses multiple lists of garbage objects. Each sub-list has a different lifetime. They should be
// sorted such that later-living garbage is ordered later in the list.
using GarbageQueue = std::queue<GarbageAndSerial>;

class MemoryProperties final : angle::NonCopyable
{
  public:
    MemoryProperties();

    void init(VkPhysicalDevice physicalDevice);
    bool hasLazilyAllocatedMemory() const;
    angle::Result findCompatibleMemoryIndex(Context *context,
                                            const VkMemoryRequirements &memoryRequirements,
                                            VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                            bool isExternalMemory,
                                            VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                            uint32_t *indexOut) const;
    void destroy();

    VkDeviceSize getHeapSizeForMemoryType(uint32_t memoryType) const
    {
        uint32_t heapIndex = mMemoryProperties.memoryTypes[memoryType].heapIndex;
        return mMemoryProperties.memoryHeaps[heapIndex].size;
    }

    const VkMemoryType &getMemoryType(uint32_t i) const { return mMemoryProperties.memoryTypes[i]; }

    uint32_t getMemoryTypeCount() const { return mMemoryProperties.memoryTypeCount; }

  private:
    VkPhysicalDeviceMemoryProperties mMemoryProperties;
};

// Similar to StagingImage, for Buffers.
class StagingBuffer final : angle::NonCopyable
{
  public:
    StagingBuffer();
    void release(ContextVk *contextVk);
    void collectGarbage(RendererVk *renderer, Serial serial);
    void destroy(RendererVk *renderer);

    angle::Result init(Context *context, VkDeviceSize size, StagingUsage usage);

    Buffer &getBuffer() { return mBuffer; }
    const Buffer &getBuffer() const { return mBuffer; }
    size_t getSize() const { return mSize; }

  private:
    Buffer mBuffer;
    Allocation mAllocation;
    size_t mSize;
};

angle::Result InitMappableAllocation(Context *context,
                                     const Allocator &allocator,
                                     Allocation *allocation,
                                     VkDeviceSize size,
                                     int value,
                                     VkMemoryPropertyFlags memoryPropertyFlags);

angle::Result InitMappableDeviceMemory(Context *context,
                                       DeviceMemory *deviceMemory,
                                       VkDeviceSize size,
                                       int value,
                                       VkMemoryPropertyFlags memoryPropertyFlags);

angle::Result AllocateBufferMemory(Context *context,
                                   VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                   VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                   const void *extraAllocationInfo,
                                   Buffer *buffer,
                                   DeviceMemory *deviceMemoryOut,
                                   VkDeviceSize *sizeOut);

angle::Result AllocateImageMemory(Context *context,
                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                  VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                  const void *extraAllocationInfo,
                                  Image *image,
                                  DeviceMemory *deviceMemoryOut,
                                  VkDeviceSize *sizeOut);

angle::Result AllocateImageMemoryWithRequirements(
    Context *context,
    VkMemoryPropertyFlags memoryPropertyFlags,
    const VkMemoryRequirements &memoryRequirements,
    const void *extraAllocationInfo,
    const VkBindImagePlaneMemoryInfoKHR *extraBindInfo,
    Image *image,
    DeviceMemory *deviceMemoryOut);

angle::Result AllocateBufferMemoryWithRequirements(Context *context,
                                                   VkMemoryPropertyFlags memoryPropertyFlags,
                                                   const VkMemoryRequirements &memoryRequirements,
                                                   const void *extraAllocationInfo,
                                                   Buffer *buffer,
                                                   VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                                   DeviceMemory *deviceMemoryOut);

angle::Result InitShaderModule(Context *context,
                               ShaderModule *shaderModule,
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
class [[nodiscard]] DeviceScoped final : angle::NonCopyable
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

template <typename T>
class [[nodiscard]] AllocatorScoped final : angle::NonCopyable
{
  public:
    AllocatorScoped(const Allocator &allocator) : mAllocator(allocator) {}
    ~AllocatorScoped() { mVar.destroy(mAllocator); }

    const T &get() const { return mVar; }
    T &get() { return mVar; }

    T &&release() { return std::move(mVar); }

  private:
    const Allocator &mAllocator;
    T mVar;
};

// Similar to DeviceScoped, but releases objects instead of destroying them. Requires that T have a
// release method that takes a ContextVk * and returns void.
template <typename T>
class [[nodiscard]] ContextScoped final : angle::NonCopyable
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
class [[nodiscard]] RendererScoped final : angle::NonCopyable
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

    // A debug function to validate that the reference count is as expected used for assertions.
    bool isRefCountAsExpected(uint32_t expectedRefCount) { return mRefCount == expectedRefCount; }

  private:
    uint32_t mRefCount;
    T mObject;
};

template <typename T>
class BindingPointer final : angle::NonCopyable
{
  public:
    BindingPointer() = default;
    ~BindingPointer() { reset(); }

    BindingPointer(BindingPointer &&other)
    {
        set(other.mRefCounted);
        other.reset();
    }

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

    RefCounted<T> *getRefCounted() { return mRefCounted; }

  private:
    RefCounted<T> *mRefCounted = nullptr;
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

    void setUnreferenced(RefCounted<T> *refCounted)
    {
        ASSERT(!mRefCounted);
        ASSERT(refCounted);

        mRefCounted = refCounted;
        mRefCounted->addRef();
    }

    void assign(VkDevice device, T &&newObject)
    {
        set(device, new RefCounted<T>(std::move(newObject)));
    }

    void copy(VkDevice device, const Shared<T> &other) { set(device, other.mRefCounted); }

    void copyUnreferenced(const Shared<T> &other) { setUnreferenced(other.mRefCounted); }

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

    template <typename OnRelease>
    void resetAndRelease(OnRelease *onRelease)
    {
        if (mRefCounted)
        {
            mRefCounted->releaseRef();
            if (!mRefCounted->isReferenced())
            {
                ASSERT(mRefCounted->get().valid());
                (*onRelease)(std::move(mRefCounted->get()));
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
        mObjectFreeList.clear();
    }

    bool empty() const { return mObjectFreeList.empty(); }

  private:
    std::vector<T> mObjectFreeList;
};

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
struct SpecializationConstants final
{
    VkBool32 surfaceRotation;
    uint32_t dither;
};
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

template <typename T>
using SpecializationConstantMap = angle::PackedEnumMap<sh::vk::SpecializationConstantId, T>;

using ShaderModulePointer = BindingPointer<ShaderModule>;
using ShaderModuleMap     = gl::ShaderMap<ShaderModulePointer>;

void MakeDebugUtilsLabel(GLenum source, const char *marker, VkDebugUtilsLabelEXT *label);

constexpr size_t kUnpackedDepthIndex   = gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
constexpr size_t kUnpackedStencilIndex = gl::IMPLEMENTATION_MAX_DRAW_BUFFERS + 1;

class ClearValuesArray final
{
  public:
    ClearValuesArray();
    ~ClearValuesArray();

    ClearValuesArray(const ClearValuesArray &other);
    ClearValuesArray &operator=(const ClearValuesArray &rhs);

    void store(uint32_t index, VkImageAspectFlags aspectFlags, const VkClearValue &clearValue);
    void storeNoDepthStencil(uint32_t index, const VkClearValue &clearValue);

    void reset(size_t index)
    {
        mValues[index] = {};
        mEnabled.reset(index);
    }

    bool test(size_t index) const { return mEnabled.test(index); }
    bool testDepth() const { return mEnabled.test(kUnpackedDepthIndex); }
    bool testStencil() const { return mEnabled.test(kUnpackedStencilIndex); }
    gl::DrawBufferMask getColorMask() const;

    const VkClearValue &operator[](size_t index) const { return mValues[index]; }

    float getDepthValue() const { return mValues[kUnpackedDepthIndex].depthStencil.depth; }
    uint32_t getStencilValue() const { return mValues[kUnpackedStencilIndex].depthStencil.stencil; }

    const VkClearValue *data() const { return mValues.data(); }
    bool empty() const { return mEnabled.none(); }
    bool any() const { return mEnabled.any(); }

  private:
    gl::AttachmentArray<VkClearValue> mValues;
    gl::AttachmentsMask mEnabled;
};

// Defines Serials for Vulkan objects.
#define ANGLE_VK_SERIAL_OP(X) \
    X(Buffer)                 \
    X(Image)                  \
    X(ImageOrBufferView)      \
    X(Sampler)

#define ANGLE_DEFINE_VK_SERIAL_TYPE(Type)                                  \
    class Type##Serial                                                     \
    {                                                                      \
      public:                                                              \
        constexpr Type##Serial() : mSerial(kInvalid)                       \
        {}                                                                 \
        constexpr explicit Type##Serial(uint32_t serial) : mSerial(serial) \
        {}                                                                 \
                                                                           \
        constexpr bool operator==(const Type##Serial &other) const         \
        {                                                                  \
            ASSERT(mSerial != kInvalid || other.mSerial != kInvalid);      \
            return mSerial == other.mSerial;                               \
        }                                                                  \
        constexpr bool operator!=(const Type##Serial &other) const         \
        {                                                                  \
            ASSERT(mSerial != kInvalid || other.mSerial != kInvalid);      \
            return mSerial != other.mSerial;                               \
        }                                                                  \
        constexpr uint32_t getValue() const                                \
        {                                                                  \
            return mSerial;                                                \
        }                                                                  \
        constexpr bool valid() const                                       \
        {                                                                  \
            return mSerial != kInvalid;                                    \
        }                                                                  \
                                                                           \
      private:                                                             \
        uint32_t mSerial;                                                  \
        static constexpr uint32_t kInvalid = 0;                            \
    };                                                                     \
    static constexpr Type##Serial kInvalid##Type##Serial = Type##Serial();

ANGLE_VK_SERIAL_OP(ANGLE_DEFINE_VK_SERIAL_TYPE)

#define ANGLE_DECLARE_GEN_VK_SERIAL(Type) Type##Serial generate##Type##Serial();

class ResourceSerialFactory final : angle::NonCopyable
{
  public:
    ResourceSerialFactory();
    ~ResourceSerialFactory();

    ANGLE_VK_SERIAL_OP(ANGLE_DECLARE_GEN_VK_SERIAL)

  private:
    uint32_t issueSerial();

    // Kept atomic so it can be accessed from multiple Context threads at once.
    std::atomic<uint32_t> mCurrentUniqueSerial;
};

#if defined(ANGLE_ENABLE_PERF_COUNTER_OUTPUT)
constexpr bool kOutputCumulativePerfCounters = ANGLE_ENABLE_PERF_COUNTER_OUTPUT;
#else
constexpr bool kOutputCumulativePerfCounters = false;
#endif

// Performance and resource counters.
struct RenderPassPerfCounters
{
    // load/storeOps. Includes ops for resolve attachment. Maximum value = 2.
    uint8_t colorLoadOpClears;
    uint8_t colorLoadOpLoads;
    uint8_t colorLoadOpNones;
    uint8_t colorStoreOpStores;
    uint8_t colorStoreOpNones;
    uint8_t depthLoadOpClears;
    uint8_t depthLoadOpLoads;
    uint8_t depthLoadOpNones;
    uint8_t depthStoreOpStores;
    uint8_t depthStoreOpNones;
    uint8_t stencilLoadOpClears;
    uint8_t stencilLoadOpLoads;
    uint8_t stencilLoadOpNones;
    uint8_t stencilStoreOpStores;
    uint8_t stencilStoreOpNones;
    // Number of unresolve and resolve operations.  Maximum value for color =
    // gl::IMPLEMENTATION_MAX_DRAW_BUFFERS and for depth/stencil = 1 each.
    uint8_t colorAttachmentUnresolves;
    uint8_t colorAttachmentResolves;
    uint8_t depthAttachmentUnresolves;
    uint8_t depthAttachmentResolves;
    uint8_t stencilAttachmentUnresolves;
    uint8_t stencilAttachmentResolves;
    // Whether the depth/stencil attachment is using a read-only layout.
    uint8_t readOnlyDepthStencil;
};

// A Vulkan image level index.
using LevelIndex = gl::LevelIndexWrapper<uint32_t>;

// Ensure viewport is within Vulkan requirements
void ClampViewport(VkViewport *viewport);

constexpr bool IsDynamicDescriptor(VkDescriptorType descriptorType)
{
    switch (descriptorType)
    {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            return true;
        default:
            return false;
    }
}

void ApplyPipelineCreationFeedback(Context *context, const VkPipelineCreationFeedback &feedback);

angle::Result SetDebugUtilsObjectName(ContextVk *contextVk,
                                      VkObjectType objectType,
                                      uint64_t handle,
                                      const std::string &label);

}  // namespace vk

#if !defined(ANGLE_SHARED_LIBVULKAN)
// Lazily load entry points for each extension as necessary.
void InitDebugUtilsEXTFunctions(VkInstance instance);
void InitDebugReportEXTFunctions(VkInstance instance);
void InitGetPhysicalDeviceProperties2KHRFunctions(VkInstance instance);
void InitTransformFeedbackEXTFunctions(VkDevice device);
void InitSamplerYcbcrKHRFunctions(VkDevice device);
void InitRenderPass2KHRFunctions(VkDevice device);

#    if defined(ANGLE_PLATFORM_FUCHSIA)
// VK_FUCHSIA_imagepipe_surface
void InitImagePipeSurfaceFUCHSIAFunctions(VkInstance instance);
#    endif

#    if defined(ANGLE_PLATFORM_ANDROID)
// VK_ANDROID_external_memory_android_hardware_buffer
void InitExternalMemoryHardwareBufferANDROIDFunctions(VkInstance instance);
#    endif

#    if defined(ANGLE_PLATFORM_GGP)
// VK_GGP_stream_descriptor_surface
void InitGGPStreamDescriptorSurfaceFunctions(VkInstance instance);
#    endif  // defined(ANGLE_PLATFORM_GGP)

// VK_KHR_external_semaphore_fd
void InitExternalSemaphoreFdFunctions(VkInstance instance);

// VK_EXT_external_memory_host
void InitExternalMemoryHostFunctions(VkInstance instance);

// VK_EXT_external_memory_host
void InitHostQueryResetFunctions(VkInstance instance);

// VK_KHR_external_fence_capabilities
void InitExternalFenceCapabilitiesFunctions(VkInstance instance);

// VK_KHR_get_memory_requirements2
void InitGetMemoryRequirements2KHRFunctions(VkDevice device);

// VK_KHR_bind_memory2
void InitBindMemory2KHRFunctions(VkDevice device);

// VK_KHR_external_fence_fd
void InitExternalFenceFdFunctions(VkInstance instance);

// VK_KHR_external_semaphore_capabilities
void InitExternalSemaphoreCapabilitiesFunctions(VkInstance instance);

// VK_KHR_shared_presentable_image
void InitGetSwapchainStatusKHRFunctions(VkDevice device);

// VK_EXT_extended_dynamic_state
void InitExtendedDynamicStateEXTFunctions(VkDevice device);

// VK_EXT_extended_dynamic_state2
void InitExtendedDynamicState2EXTFunctions(VkDevice device);

// VK_KHR_fragment_shading_rate
void InitFragmentShadingRateKHRInstanceFunction(VkInstance instance);
void InitFragmentShadingRateKHRDeviceFunction(VkDevice device);

// VK_GOOGLE_display_timing
void InitGetPastPresentationTimingGoogleFunction(VkDevice device);

#endif  // !defined(ANGLE_SHARED_LIBVULKAN)

GLenum CalculateGenerateMipmapFilter(ContextVk *contextVk, angle::FormatID formatID);
size_t PackSampleCount(GLint sampleCount);

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
VkStencilOp GetStencilOp(const GLenum compareOp);
VkLogicOp GetLogicOp(const GLenum logicOp);

constexpr gl::ShaderMap<VkShaderStageFlagBits> kShaderStageMap = {
    {gl::ShaderType::Vertex, VK_SHADER_STAGE_VERTEX_BIT},
    {gl::ShaderType::TessControl, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT},
    {gl::ShaderType::TessEvaluation, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT},
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
                 bool upperLeftOrigin,
                 GLint renderAreaHeight,
                 VkViewport *viewportOut);

void GetExtentsAndLayerCount(gl::TextureType textureType,
                             const gl::Extents &extents,
                             VkExtent3D *extentsOut,
                             uint32_t *layerCountOut);

vk::LevelIndex GetLevelIndex(gl::LevelIndex levelGL, gl::LevelIndex baseLevel);

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

gl::LevelIndex GetLevelIndex(vk::LevelIndex levelVk, gl::LevelIndex baseLevel);
}  // namespace vk_gl

enum class RenderPassClosureReason
{
    // Don't specify the reason (it should already be specified elsewhere)
    AlreadySpecifiedElsewhere,

    // Implicit closures due to flush/wait/etc.
    ContextDestruction,
    ContextChange,
    GLFlush,
    GLFinish,
    EGLSwapBuffers,
    EGLWaitClient,

    // Closure due to switching rendering to another framebuffer.
    FramebufferBindingChange,
    FramebufferChange,
    NewRenderPass,

    // Incompatible use of resource in the same render pass
    BufferUseThenXfbWrite,
    XfbWriteThenVertexIndexBuffer,
    XfbWriteThenIndirectDrawBuffer,
    XfbResumeAfterDrawBasedClear,
    DepthStencilUseInFeedbackLoop,
    DepthStencilWriteAfterFeedbackLoop,
    PipelineBindWhileXfbActive,

    // Use of resource after render pass
    BufferWriteThenMap,
    BufferWriteThenOutOfRPRead,
    BufferUseThenOutOfRPWrite,
    ImageUseThenOutOfRPRead,
    ImageUseThenOutOfRPWrite,
    XfbWriteThenComputeRead,
    XfbWriteThenIndirectDispatchBuffer,
    ImageAttachmentThenComputeRead,
    GraphicsTextureImageAccessThenComputeAccess,
    GetQueryResult,
    BeginNonRenderPassQuery,
    EndNonRenderPassQuery,
    TimestampQuery,
    EndRenderPassQuery,
    GLReadPixels,

    // Synchronization
    BufferUseThenReleaseToExternal,
    ImageUseThenReleaseToExternal,
    BufferInUseWhenSynchronizedMap,
    ImageOrphan,
    GLMemoryBarrierThenStorageResource,
    StorageResourceUseThenGLMemoryBarrier,
    ExternalSemaphoreSignal,
    SyncObjectInit,
    SyncObjectWithFdInit,
    SyncObjectClientWait,
    SyncObjectServerWait,
    SyncObjectGetStatus,

    // Closures that ANGLE could have avoided, but doesn't for simplicity or optimization of more
    // common cases.
    XfbPause,
    FramebufferFetchEmulation,
    ColorBufferInvalidate,
    GenerateMipmapOnCPU,
    CopyTextureOnCPU,
    TextureReformatToRenderable,
    DeviceLocalBufferMap,

    // UtilsVk
    PrepareForBlit,
    PrepareForImageCopy,
    TemporaryForImageClear,
    TemporaryForImageCopy,
    TemporaryForOverlayDraw,

    InvalidEnum,
    EnumCount = InvalidEnum,
};

}  // namespace rx

#define ANGLE_VK_TRY(context, command)                                                   \
    do                                                                                   \
    {                                                                                    \
        auto ANGLE_LOCAL_VAR = command;                                                  \
        if (ANGLE_UNLIKELY(ANGLE_LOCAL_VAR != VK_SUCCESS))                               \
        {                                                                                \
            (context)->handleError(ANGLE_LOCAL_VAR, __FILE__, ANGLE_FUNCTION, __LINE__); \
            return angle::Result::Stop;                                                  \
        }                                                                                \
    } while (0)

#define ANGLE_VK_CHECK(context, test, error) ANGLE_VK_TRY(context, test ? VK_SUCCESS : error)

#define ANGLE_VK_CHECK_MATH(context, result) \
    ANGLE_VK_CHECK(context, result, VK_ERROR_VALIDATION_FAILED_EXT)

#define ANGLE_VK_CHECK_ALLOC(context, result) \
    ANGLE_VK_CHECK(context, result, VK_ERROR_OUT_OF_HOST_MEMORY)

#define ANGLE_VK_UNREACHABLE(context) \
    UNREACHABLE();                    \
    ANGLE_VK_CHECK(context, false, VK_ERROR_FEATURE_NOT_PRESENT)

// NVIDIA uses special formatting for the driver version:
// Major: 10
// Minor: 8
// Sub-minor: 8
// patch: 6
#define ANGLE_VK_VERSION_MAJOR_NVIDIA(version) (((uint32_t)(version) >> 22) & 0x3ff)
#define ANGLE_VK_VERSION_MINOR_NVIDIA(version) (((uint32_t)(version) >> 14) & 0xff)
#define ANGLE_VK_VERSION_SUB_MINOR_NVIDIA(version) (((uint32_t)(version) >> 6) & 0xff)
#define ANGLE_VK_VERSION_PATCH_NVIDIA(version) ((uint32_t)(version)&0x3f)

// Similarly for Intel on Windows:
// Major: 18
// Minor: 14
#define ANGLE_VK_VERSION_MAJOR_WIN_INTEL(version) (((uint32_t)(version) >> 14) & 0x3ffff)
#define ANGLE_VK_VERSION_MINOR_WIN_INTEL(version) ((uint32_t)(version)&0x3fff)

#endif  // LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_
