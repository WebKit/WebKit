//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_common.h:
//      Declares common constants, template classes, and mtl::Context - the MTLDevice container &
//      error handler base class.
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_COMMON_H_
#define LIBANGLE_RENDERER_METAL_MTL_COMMON_H_

#import <Metal/Metal.h>

#include <TargetConditionals.h>

#include <string>

#include "common/Optional.h"
#include "common/PackedEnums.h"
#include "common/angleutils.h"
#include "common/apple_platform_utils.h"
#include "libANGLE/Constants.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/Version.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/metal/mtl_constants.h"

#if TARGET_OS_IPHONE
#    if !defined(__IPHONE_11_0)
#        define __IPHONE_11_0 110000
#    endif
#    if !defined(ANGLE_IOS_DEPLOY_TARGET)
#        define ANGLE_IOS_DEPLOY_TARGET __IPHONE_11_0
#    endif
#    if !defined(__IPHONE_OS_VERSION_MAX_ALLOWED)
#        define __IPHONE_OS_VERSION_MAX_ALLOWED __IPHONE_11_0
#    endif
#    if !defined(__TV_OS_VERSION_MAX_ALLOWED)
#        define __TV_OS_VERSION_MAX_ALLOWED __IPHONE_11_0
#    endif
#endif

#if !defined(TARGET_OS_MACCATALYST)
#    define TARGET_OS_MACCATALYST 0
#endif

#if defined(__ARM_ARCH)
#    define ANGLE_MTL_ARM (__ARM_ARCH != 0)
#else
#    define ANGLE_MTL_ARM 0
#endif

#define ANGLE_MTL_OBJC_SCOPE @autoreleasepool

#if !__has_feature(objc_arc)
#    define ANGLE_MTL_AUTORELEASE autorelease
#    define ANGLE_MTL_RETAIN retain
#    define ANGLE_MTL_RELEASE release
#else
#    define ANGLE_MTL_AUTORELEASE self
#    define ANGLE_MTL_RETAIN self
#    define ANGLE_MTL_RELEASE self
#endif

#define ANGLE_MTL_UNUSED __attribute__((unused))

#if defined(ANGLE_MTL_ENABLE_TRACE)
#    define ANGLE_MTL_LOG(...) NSLog(@__VA_ARGS__)
#else
#    define ANGLE_MTL_LOG(...) (void)0
#endif

namespace egl
{
class Display;
class Image;
class Surface;
}  // namespace egl

#define ANGLE_GL_OBJECTS_X(PROC) \
    PROC(Buffer)                 \
    PROC(Context)                \
    PROC(Framebuffer)            \
    PROC(MemoryObject)           \
    PROC(Query)                  \
    PROC(Program)                \
    PROC(Sampler)                \
    PROC(Semaphore)              \
    PROC(Texture)                \
    PROC(TransformFeedback)      \
    PROC(VertexArray)

#define ANGLE_PRE_DECLARE_OBJECT(OBJ) class OBJ;

namespace gl
{
struct Rectangle;
ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_OBJECT)
}  // namespace gl

#define ANGLE_PRE_DECLARE_MTL_OBJECT(OBJ) class OBJ##Mtl;

namespace rx
{
class DisplayMtl;
class ContextMtl;
class FramebufferMtl;
class BufferMtl;
class VertexArrayMtl;
class TextureMtl;
class ProgramMtl;
class SamplerMtl;
class TransformFeedbackMtl;

ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_MTL_OBJECT)

namespace mtl
{
// This special constant is used to indicate that a particular vertex descriptor's buffer layout
// index is unused.
constexpr MTLVertexStepFunction kVertexStepFunctionInvalid =
    static_cast<MTLVertexStepFunction>(0xff);

// Work-around the enum is not available on macOS
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
constexpr MTLBlitOption kBlitOptionRowLinearPVRTC = MTLBlitOptionNone;
#else
constexpr MTLBlitOption kBlitOptionRowLinearPVRTC          = MTLBlitOptionRowLinearPVRTC;
#endif

#if defined(__IPHONE_13_0) || defined(__MAC_10_15)
using TextureSwizzleChannels                   = MTLTextureSwizzleChannels;
using RenderStages                             = MTLRenderStages;
constexpr MTLRenderStages kRenderStageVertex   = MTLRenderStageVertex;
constexpr MTLRenderStages kRenderStageFragment = MTLRenderStageFragment;
#else
using TextureSwizzleChannels                               = int;
using RenderStages                                         = int;
constexpr RenderStages kRenderStageVertex                  = 1;
constexpr RenderStages kRenderStageFragment                = 2;
#endif

enum class PixelType
{
    Int,
    UInt,
    Float,
    EnumCount,
};

template <typename T>
struct ImplTypeHelper;

// clang-format off
#define ANGLE_IMPL_TYPE_HELPER_GL(OBJ) \
template<>                             \
struct ImplTypeHelper<gl::OBJ>         \
{                                      \
    using ImplType = OBJ##Mtl;         \
};
// clang-format on

ANGLE_GL_OBJECTS_X(ANGLE_IMPL_TYPE_HELPER_GL)

template <>
struct ImplTypeHelper<egl::Display>
{
    using ImplType = DisplayMtl;
};
template <typename T>
using GetImplType = typename ImplTypeHelper<T>::ImplType;

template <typename T>
GetImplType<T> *GetImpl(const T *glObject)
{
    return GetImplAs<GetImplType<T>>(glObject);
}

// This class wraps Objective-C pointer inside, it will manage the lifetime of
// the Objective-C pointer. Changing pointer is not supported outside subclass.
template <typename T>
class WrappedObject
{
  public:
    WrappedObject() = default;
    ~WrappedObject() { release(); }

    bool valid() const { return (mMetalObject != nil); }

    T get() const { return mMetalObject; }
    inline void reset() { release(); }

    operator T() const { return get(); }

  protected:
    inline void set(T obj) { retainAssign(obj); }

    void retainAssign(T obj)
    {
        T retained = obj;
#if !__has_feature(objc_arc)
        [retained retain];
#endif
        release();
        mMetalObject = obj;
    }

  private:
    void release()
    {
#if !__has_feature(objc_arc)
        [mMetalObject release];
#endif
        mMetalObject = nil;
    }

    T mMetalObject = nil;
};

// This class is similar to WrappedObject, however, it allows changing the
// internal pointer with public methods.
template <typename T>
class AutoObjCPtr : public WrappedObject<T>
{
  public:
    using ParentType = WrappedObject<T>;

    AutoObjCPtr() {}

    AutoObjCPtr(const std::nullptr_t &theNull) {}

    AutoObjCPtr(const AutoObjCPtr &src) { this->retainAssign(src.get()); }

    AutoObjCPtr(AutoObjCPtr &&src) { this->transfer(std::forward<AutoObjCPtr>(src)); }

    // Take ownership of the pointer
    AutoObjCPtr(T &&src)
    {
        this->retainAssign(src);
        src = nil;
    }

    AutoObjCPtr &operator=(const AutoObjCPtr &src)
    {
        this->retainAssign(src.get());
        return *this;
    }

    AutoObjCPtr &operator=(AutoObjCPtr &&src)
    {
        this->transfer(std::forward<AutoObjCPtr>(src));
        return *this;
    }

    // Take ownership of the pointer
    AutoObjCPtr &operator=(T &&src)
    {
        this->retainAssign(src);
        src = nil;
        return *this;
    }

    AutoObjCPtr &operator=(const std::nullptr_t &theNull)
    {
        this->set(nil);
        return *this;
    }

    bool operator==(const AutoObjCPtr &rhs) const { return (*this) == rhs.get(); }

    bool operator==(T rhs) const { return this->get() == rhs; }

    bool operator==(const std::nullptr_t &theNull) const { return this->get(); }

    inline operator bool() { return this->get(); }

    bool operator!=(const AutoObjCPtr &rhs) const { return (*this) != rhs.get(); }

    bool operator!=(T rhs) const { return this->get() != rhs; }

    using ParentType::retainAssign;

  private:
    void transfer(AutoObjCPtr &&src)
    {
        this->retainAssign(std::move(src.get()));
        src.reset();
    }
};

template <typename T>
using AutoObjCObj = AutoObjCPtr<T *>;

// NOTE: SharedEvent is only declared on iOS 12.0+ or mac 10.14+
#if defined(__IPHONE_12_0) || defined(__MAC_10_14)
using SharedEventRef = AutoObjCPtr<id<MTLSharedEvent>>;
#else
using SharedEventRef                                       = AutoObjCObj<NSObject>;
#endif

// The native image index used by Metal back-end,  the image index uses native mipmap level instead
// of "virtual" level modified by OpenGL's base level.
using MipmapNativeLevel = gl::LevelIndexWrapper<uint32_t>;
constexpr MipmapNativeLevel kZeroNativeMipLevel(0);
class ImageNativeIndexIterator;
class ImageNativeIndex final
{
  public:
    ImageNativeIndex() = delete;
    ImageNativeIndex(const gl::ImageIndex &src, GLint baseLevel)
    {
        mNativeIndex = gl::ImageIndex::MakeFromType(src.getType(), src.getLevelIndex() - baseLevel,
                                                    src.getLayerIndex(), src.getLayerCount());
    }
    static ImageNativeIndex FromBaseZeroGLIndex(const gl::ImageIndex &src)
    {
        return ImageNativeIndex(src, 0);
    }
    MipmapNativeLevel getNativeLevel() const
    {
        return MipmapNativeLevel(mNativeIndex.getLevelIndex());
    }
    gl::TextureType getType() const { return mNativeIndex.getType(); }
    GLint getLayerIndex() const { return mNativeIndex.getLayerIndex(); }
    GLint getLayerCount() const { return mNativeIndex.getLayerCount(); }
    GLint cubeMapFaceIndex() const { return mNativeIndex.cubeMapFaceIndex(); }
    bool isLayered() const { return mNativeIndex.isLayered(); }
    bool hasLayer() const { return mNativeIndex.hasLayer(); }
    bool has3DLayer() const { return mNativeIndex.has3DLayer(); }
    bool usesTex3D() const { return mNativeIndex.usesTex3D(); }
    bool valid() const { return mNativeIndex.valid(); }
    ImageNativeIndexIterator getLayerIterator(GLint layerCount) const;
  private:
    gl::ImageIndex mNativeIndex;
};
class ImageNativeIndexIterator final
{
  public:
    ImageNativeIndex next() { return ImageNativeIndex(mNativeIndexIte.next(), 0); }
    ImageNativeIndex current() const { return ImageNativeIndex(mNativeIndexIte.current(), 0); }
    bool hasNext() const { return mNativeIndexIte.hasNext(); }
  private:
    // This class is only constructable from ImageNativeIndex
    friend class ImageNativeIndex;
    explicit ImageNativeIndexIterator(const gl::ImageIndexIterator &baseZeroSrc)
        : mNativeIndexIte(baseZeroSrc)
    {}
    gl::ImageIndexIterator mNativeIndexIte;
};
using ClearColorValueBytes = std::array<uint8_t, 4 * sizeof(float)>;
class ClearColorValue
{
  public:
    constexpr ClearColorValue()
        : mType(PixelType::Float), mRedF(0), mGreenF(0), mBlueF(0), mAlphaF(0)
    {}
    constexpr ClearColorValue(float r, float g, float b, float a)
        : mType(PixelType::Float), mRedF(r), mGreenF(g), mBlueF(b), mAlphaF(a)
    {}
    constexpr ClearColorValue(int32_t r, int32_t g, int32_t b, int32_t a)
        : mType(PixelType::Int), mRedI(r), mGreenI(g), mBlueI(b), mAlphaI(a)
    {}
    constexpr ClearColorValue(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
        : mType(PixelType::UInt), mRedU(r), mGreenU(g), mBlueU(b), mAlphaU(a)
    {}
    constexpr ClearColorValue(const ClearColorValue &src)
        : mType(src.mType), mValueBytes(src.mValueBytes)
    {}
    MTLClearColor toMTLClearColor() const;
    PixelType getType() const { return mType; }
    const ClearColorValueBytes &getValueBytes() const { return mValueBytes; }
    ClearColorValue &operator=(const ClearColorValue &src);
    void setAsFloat(float r, float g, float b, float a);
    void setAsInt(int32_t r, int32_t g, int32_t b, int32_t a);
    void setAsUInt(uint32_t r, uint32_t g, uint32_t b, uint32_t a);
  private:
    PixelType mType;
    union
    {
        struct
        {
            float mRedF, mGreenF, mBlueF, mAlphaF;
        };
        struct
        {
            int32_t mRedI, mGreenI, mBlueI, mAlphaI;
        };
        struct
        {
            uint32_t mRedU, mGreenU, mBlueU, mAlphaU;
        };
        ClearColorValueBytes mValueBytes;
    };
};

class CommandQueue;
class ErrorHandler
{
  public:
    virtual ~ErrorHandler() {}

    virtual void handleError(GLenum error,
                             const char *file,
                             const char *function,
                             unsigned int line) = 0;

    virtual void handleError(NSError *error,
                             const char *file,
                             const char *function,
                             unsigned int line) = 0;
};

class Context : public ErrorHandler
{
  public:
    Context(DisplayMtl *displayMtl);
    id<MTLDevice> getMetalDevice() const;
    mtl::CommandQueue &cmdQueue();

    DisplayMtl *getDisplay() const { return mDisplay; }

  protected:
    DisplayMtl *mDisplay;
};

#define ANGLE_MTL_CHECK(context, test, error)                                \
    do                                                                       \
    {                                                                        \
        if (ANGLE_UNLIKELY(!(test)))                                         \
        {                                                                    \
            context->handleError(error, __FILE__, ANGLE_FUNCTION, __LINE__); \
            return angle::Result::Stop;                                      \
        }                                                                    \
    } while (0)

#define ANGLE_MTL_TRY(context, test) ANGLE_MTL_CHECK(context, test, GL_INVALID_OPERATION)

#define ANGLE_MTL_UNREACHABLE(context) \
    UNREACHABLE();                     \
    ANGLE_MTL_TRY(context, false)

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_MTL_COMMON_H_ */
