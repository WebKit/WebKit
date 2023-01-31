//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_resources.mm:
//    Implements wrapper classes for Metal's MTLTexture and MTLBuffer.
//

#include "libANGLE/renderer/metal/mtl_resources.h"

#include <TargetConditionals.h>

#include <algorithm>

#include "common/debug.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_context_device.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"
#include "libANGLE/renderer/metal/mtl_utils.h"

namespace rx
{
namespace mtl
{
namespace
{
inline NSUInteger GetMipSize(NSUInteger baseSize, const MipmapNativeLevel level)
{
    return std::max<NSUInteger>(1, baseSize >> level.get());
}

// Asynchronously synchronize the content of a resource between GPU memory and its CPU cache.
// NOTE: This operation doesn't finish immediately upon function's return.
template <class T>
void InvokeCPUMemSync(ContextMtl *context, mtl::BlitCommandEncoder *blitEncoder, T *resource)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    if (blitEncoder)
    {
        blitEncoder->synchronizeResource(resource);

        resource->resetCPUReadMemNeedSync();
        resource->setCPUReadMemSyncPending(true);
    }
#endif
}

template <class T>
void EnsureCPUMemWillBeSynced(ContextMtl *context, T *resource)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    // Make sure GPU & CPU contents are synchronized.
    // NOTE: Only MacOS has separated storage for resource on CPU and GPU and needs explicit
    // synchronization
    if (resource->get().storageMode == MTLStorageModeManaged && resource->isCPUReadMemNeedSync())
    {
        mtl::BlitCommandEncoder *blitEncoder = context->getBlitCommandEncoder();
        InvokeCPUMemSync(context, blitEncoder, resource);
    }
#endif
    resource->resetCPUReadMemNeedSync();
}

}  // namespace
// Resource implementation
Resource::Resource() : mUsageRef(std::make_shared<UsageRef>()) {}

// Share the GPU usage ref with other resource
Resource::Resource(Resource *other) : mUsageRef(other->mUsageRef)
{
    ASSERT(mUsageRef);
}

void Resource::reset()
{
    mUsageRef->cmdBufferQueueSerial = 0;
    resetCPUReadMemDirty();
    resetCPUReadMemNeedSync();
    resetCPUReadMemSyncPending();
}

bool Resource::isBeingUsedByGPU(Context *context) const
{
    return context->cmdQueue().isResourceBeingUsedByGPU(this);
}

bool Resource::hasPendingWorks(Context *context) const
{
    return context->cmdQueue().resourceHasPendingWorks(this);
}

void Resource::setUsedByCommandBufferWithQueueSerial(uint64_t serial, bool writing)
{
    if (writing)
    {
        mUsageRef->cpuReadMemNeedSync = true;
        mUsageRef->cpuReadMemDirty    = true;
    }

    mUsageRef->cmdBufferQueueSerial = std::max(mUsageRef->cmdBufferQueueSerial, serial);
}

// Texture implemenetation
/** static */
angle::Result Texture::Make2DTexture(ContextMtl *context,
                                     const Format &format,
                                     uint32_t width,
                                     uint32_t height,
                                     uint32_t mips,
                                     bool renderTargetOnly,
                                     bool allowFormatView,
                                     TextureRef *refOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        MTLTextureDescriptor *desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format.metalFormat
                                                               width:width
                                                              height:height
                                                           mipmapped:mips == 0 || mips > 1];
        return MakeTexture(context, format, desc, mips, renderTargetOnly, allowFormatView, refOut);
    }  // ANGLE_MTL_OBJC_SCOPE
}

/** static */
angle::Result Texture::MakeMemoryLess2DTexture(ContextMtl *context,
                                               const Format &format,
                                               uint32_t width,
                                               uint32_t height,
                                               TextureRef *refOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        MTLTextureDescriptor *desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format.metalFormat
                                                               width:width
                                                              height:height
                                                           mipmapped:NO];

        return MakeTexture(context, format, desc, 1, true, false, true, refOut);
    }  // ANGLE_MTL_OBJC_SCOPE
}
/** static */
angle::Result Texture::MakeCubeTexture(ContextMtl *context,
                                       const Format &format,
                                       uint32_t size,
                                       uint32_t mips,
                                       bool renderTargetOnly,
                                       bool allowFormatView,
                                       TextureRef *refOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        MTLTextureDescriptor *desc =
            [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:format.metalFormat
                                                                  size:size
                                                             mipmapped:mips == 0 || mips > 1];

        return MakeTexture(context, format, desc, mips, renderTargetOnly, allowFormatView, refOut);
    }  // ANGLE_MTL_OBJC_SCOPE
}

/** static */
angle::Result Texture::Make2DMSTexture(ContextMtl *context,
                                       const Format &format,
                                       uint32_t width,
                                       uint32_t height,
                                       uint32_t samples,
                                       bool renderTargetOnly,
                                       bool allowFormatView,
                                       TextureRef *refOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        MTLTextureDescriptor *desc = [[MTLTextureDescriptor new] ANGLE_MTL_AUTORELEASE];
        desc.textureType           = MTLTextureType2DMultisample;
        desc.pixelFormat           = format.metalFormat;
        desc.width                 = width;
        desc.height                = height;
        desc.mipmapLevelCount      = 1;
        desc.sampleCount           = samples;

        return MakeTexture(context, format, desc, 1, renderTargetOnly, allowFormatView, refOut);
    }  // ANGLE_MTL_OBJC_SCOPE
}

/** static */
angle::Result Texture::Make2DArrayTexture(ContextMtl *context,
                                          const Format &format,
                                          uint32_t width,
                                          uint32_t height,
                                          uint32_t mips,
                                          uint32_t arrayLength,
                                          bool renderTargetOnly,
                                          bool allowFormatView,
                                          TextureRef *refOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        // Use texture2DDescriptorWithPixelFormat to calculate full range mipmap range:
        MTLTextureDescriptor *desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format.metalFormat
                                                               width:width
                                                              height:height
                                                           mipmapped:mips == 0 || mips > 1];

        desc.textureType = MTLTextureType2DArray;
        desc.arrayLength = arrayLength;

        return MakeTexture(context, format, desc, mips, renderTargetOnly, allowFormatView, refOut);
    }  // ANGLE_MTL_OBJC_SCOPE
}

/** static */
angle::Result Texture::Make3DTexture(ContextMtl *context,
                                     const Format &format,
                                     uint32_t width,
                                     uint32_t height,
                                     uint32_t depth,
                                     uint32_t mips,
                                     bool renderTargetOnly,
                                     bool allowFormatView,
                                     TextureRef *refOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        // Use texture2DDescriptorWithPixelFormat to calculate full range mipmap range:
        uint32_t maxDimen = std::max(width, height);
        maxDimen          = std::max(maxDimen, depth);
        MTLTextureDescriptor *desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format.metalFormat
                                                               width:maxDimen
                                                              height:maxDimen
                                                           mipmapped:mips == 0 || mips > 1];

        desc.textureType = MTLTextureType3D;
        desc.width       = width;
        desc.height      = height;
        desc.depth       = depth;

        return MakeTexture(context, format, desc, mips, renderTargetOnly, allowFormatView, refOut);
    }  // ANGLE_MTL_OBJC_SCOPE
}

/** static */
angle::Result Texture::MakeTexture(ContextMtl *context,
                                   const Format &mtlFormat,
                                   MTLTextureDescriptor *desc,
                                   uint32_t mips,
                                   bool renderTargetOnly,
                                   bool allowFormatView,
                                   TextureRef *refOut)
{
    return MakeTexture(context, mtlFormat, desc, mips, renderTargetOnly, allowFormatView, false,
                       refOut);
}

angle::Result Texture::MakeTexture(ContextMtl *context,
                                   const Format &mtlFormat,
                                   MTLTextureDescriptor *desc,
                                   uint32_t mips,
                                   bool renderTargetOnly,
                                   bool allowFormatView,
                                   bool memoryLess,
                                   TextureRef *refOut)
{
    if (desc.pixelFormat == MTLPixelFormatInvalid)
    {
        return angle::Result::Stop;
    }

    ASSERT(refOut);
    Texture *newTexture =
        new Texture(context, desc, mips, renderTargetOnly, allowFormatView, memoryLess);
    ANGLE_MTL_CHECK(context, newTexture->valid(), GL_OUT_OF_MEMORY);
    refOut->reset(newTexture);

    if (!mtlFormat.hasDepthAndStencilBits())
    {
        refOut->get()->setColorWritableMask(GetEmulatedColorWriteMask(mtlFormat));
    }

    size_t estimatedBytes = EstimateTextureSizeInBytes(
        mtlFormat, desc.width, desc.height, desc.depth, desc.sampleCount, desc.mipmapLevelCount);
    if (refOut)
    {
        refOut->get()->setEstimatedByteSize(memoryLess ? 0 : estimatedBytes);
    }

    return angle::Result::Continue;
}

angle::Result Texture::MakeTexture(ContextMtl *context,
                                   const Format &mtlFormat,
                                   MTLTextureDescriptor *desc,
                                   IOSurfaceRef surfaceRef,
                                   NSUInteger slice,
                                   bool renderTargetOnly,
                                   TextureRef *refOut)
{

    ASSERT(refOut);
    Texture *newTexture = new Texture(context, desc, surfaceRef, slice, renderTargetOnly);
    ANGLE_MTL_CHECK(context, newTexture->valid(), GL_OUT_OF_MEMORY);
    refOut->reset(newTexture);
    if (!mtlFormat.hasDepthAndStencilBits())
    {
        refOut->get()->setColorWritableMask(GetEmulatedColorWriteMask(mtlFormat));
    }

    size_t estimatedBytes = EstimateTextureSizeInBytes(
        mtlFormat, desc.width, desc.height, desc.depth, desc.sampleCount, desc.mipmapLevelCount);
    refOut->get()->setEstimatedByteSize(estimatedBytes);

    return angle::Result::Continue;
}

bool needMultisampleColorFormatShaderReadWorkaround(ContextMtl *context, MTLTextureDescriptor *desc)
{
    return desc.sampleCount > 1 &&
           context->getDisplay()
               ->getFeatures()
               .multisampleColorFormatShaderReadWorkaround.enabled &&
           context->getNativeFormatCaps(desc.pixelFormat).colorRenderable;
}

/** static */
TextureRef Texture::MakeFromMetal(id<MTLTexture> metalTexture)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        return TextureRef(new Texture(metalTexture));
    }
}

Texture::Texture(id<MTLTexture> metalTexture)
    : mColorWritableMask(std::make_shared<MTLColorWriteMask>(MTLColorWriteMaskAll))
{
    set(metalTexture);
}

Texture::Texture(ContextMtl *context,
                 MTLTextureDescriptor *desc,
                 uint32_t mips,
                 bool renderTargetOnly,
                 bool allowFormatView)
    : Texture(context, desc, mips, renderTargetOnly, allowFormatView, false)
{}

Texture::Texture(ContextMtl *context,
                 MTLTextureDescriptor *desc,
                 uint32_t mips,
                 bool renderTargetOnly,
                 bool allowFormatView,
                 bool memoryLess)
    : mColorWritableMask(std::make_shared<MTLColorWriteMask>(MTLColorWriteMaskAll))
{
    ANGLE_MTL_OBJC_SCOPE
    {
        const mtl::ContextDevice &metalDevice = context->getMetalDevice();

        if (mips > 1 && mips < desc.mipmapLevelCount)
        {
            desc.mipmapLevelCount = mips;
        }

        // Every texture will support being rendered for now
        desc.usage = 0;

        if (context->getNativeFormatCaps(desc.pixelFormat).isRenderable())
        {
            desc.usage |= MTLTextureUsageRenderTarget;
        }

        if (memoryLess)
        {
#if (TARGET_OS_IOS || TARGET_OS_TV) && !TARGET_OS_MACCATALYST
            desc.resourceOptions = MTLResourceStorageModeMemoryless;
#else
            desc.resourceOptions = MTLResourceStorageModePrivate;
#endif
        }
        else if (context->getNativeFormatCaps(desc.pixelFormat).depthRenderable ||
                 desc.textureType == MTLTextureType2DMultisample)
        {
            // Metal doesn't support host access to depth stencil texture's data
            desc.resourceOptions = MTLResourceStorageModePrivate;
        }

        if (!renderTargetOnly || needMultisampleColorFormatShaderReadWorkaround(context, desc))
        {
            desc.usage = desc.usage | MTLTextureUsageShaderRead;
            if (context->getNativeFormatCaps(desc.pixelFormat).writable)
            {
                desc.usage = desc.usage | MTLTextureUsageShaderWrite;
            }
        }
        if (desc.pixelFormat == MTLPixelFormatDepth32Float_Stencil8)
        {
            ASSERT(allowFormatView);
        }

        if (allowFormatView)
        {
            desc.usage = desc.usage | MTLTextureUsagePixelFormatView;
        }

        set(metalDevice.newTextureWithDescriptor(desc));

        mCreationDesc.retainAssign(desc);
    }
}

Texture::Texture(ContextMtl *context,
                 MTLTextureDescriptor *desc,
                 IOSurfaceRef iosurface,
                 NSUInteger plane,
                 bool renderTargetOnly)
    : mColorWritableMask(std::make_shared<MTLColorWriteMask>(MTLColorWriteMaskAll))
{
    ANGLE_MTL_OBJC_SCOPE
    {
        const mtl::ContextDevice &metalDevice = context->getMetalDevice();

        // Every texture will support being rendered for now
        desc.usage = MTLTextureUsagePixelFormatView;

        if (context->getNativeFormatCaps(desc.pixelFormat).isRenderable())
        {
            desc.usage |= MTLTextureUsageRenderTarget;
        }

#if (TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH) && !TARGET_OS_MACCATALYST
        desc.resourceOptions = MTLResourceStorageModeShared;
#else
        desc.resourceOptions = MTLResourceStorageModeManaged;
#endif

        if (!renderTargetOnly)
        {
            desc.usage = desc.usage | MTLTextureUsageShaderRead;
            if (context->getNativeFormatCaps(desc.pixelFormat).writable)
            {
                desc.usage = desc.usage | MTLTextureUsageShaderWrite;
            }
        }
        set(metalDevice.newTextureWithDescriptor(desc, iosurface, plane));
    }
}

Texture::Texture(Texture *original, MTLPixelFormat format)
    : Resource(original),
      mColorWritableMask(original->mColorWritableMask)  // Share color write mask property
{
    ANGLE_MTL_OBJC_SCOPE
    {
        auto view = [original->get() newTextureViewWithPixelFormat:format];

        set([view ANGLE_MTL_AUTORELEASE]);
        // Texture views consume no additional memory
        mEstimatedByteSize = 0;
    }
}

Texture::Texture(Texture *original, MTLTextureType type, NSRange mipmapLevelRange, NSRange slices)
    : Resource(original),
      mColorWritableMask(original->mColorWritableMask)  // Share color write mask property
{
    ANGLE_MTL_OBJC_SCOPE
    {
        auto view = [original->get() newTextureViewWithPixelFormat:original->pixelFormat()
                                                       textureType:type
                                                            levels:mipmapLevelRange
                                                            slices:slices];

        set([view ANGLE_MTL_AUTORELEASE]);
        // Texture views consume no additional memory
        mEstimatedByteSize = 0;
    }
}

Texture::Texture(Texture *original, const TextureSwizzleChannels &swizzle)
    : Resource(original),
      mColorWritableMask(original->mColorWritableMask)  // Share color write mask property
{
#if ANGLE_MTL_SWIZZLE_AVAILABLE
    ANGLE_MTL_OBJC_SCOPE
    {
        auto view = [original->get()
            newTextureViewWithPixelFormat:original->pixelFormat()
                              textureType:original->textureType()
                                   levels:NSMakeRange(0, original->mipmapLevels())
                                   slices:NSMakeRange(0, original->cubeFacesOrArrayLength())
                                  swizzle:swizzle];

        set([view ANGLE_MTL_AUTORELEASE]);
        // Texture views consume no additional memory
        mEstimatedByteSize = 0;
    }
#else
    UNREACHABLE();
#endif
}

Texture::Texture(Texture *original,
                 MTLTextureType type,
                 const MipmapNativeLevel &level,
                 int layer,
                 MTLPixelFormat pixelFormat)
    : Resource(original),
      mColorWritableMask(std::make_shared<MTLColorWriteMask>(MTLColorWriteMaskAll))
{
    ANGLE_MTL_OBJC_SCOPE
    {
        ASSERT(original->pixelFormat() == pixelFormat || original->supportFormatView());
        auto view = [original->get() newTextureViewWithPixelFormat:pixelFormat
                                                       textureType:type
                                                            levels:NSMakeRange(level.get(), 1)
                                                            slices:NSMakeRange(layer, 1)];

        set([view ANGLE_MTL_AUTORELEASE]);
        // Texture views consume no additional memory
        mEstimatedByteSize = 0;
    }
}

void Texture::syncContent(ContextMtl *context, mtl::BlitCommandEncoder *blitEncoder)
{
    InvokeCPUMemSync(context, blitEncoder, this);
}

void Texture::syncContentIfNeeded(ContextMtl *context)
{
    EnsureCPUMemWillBeSynced(context, this);
}

bool Texture::isCPUAccessible() const
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    if (get().storageMode == MTLStorageModeManaged)
    {
        return true;
    }
#endif
    return get().storageMode == MTLStorageModeShared;
}

bool Texture::isShaderReadable() const
{
    return get().usage & MTLTextureUsageShaderRead;
}

bool Texture::isShaderWritable() const
{
    return get().usage & MTLTextureUsageShaderWrite;
}

bool Texture::supportFormatView() const
{
    return get().usage & MTLTextureUsagePixelFormatView;
}

void Texture::replace2DRegion(ContextMtl *context,
                              const MTLRegion &region,
                              const MipmapNativeLevel &mipmapLevel,
                              uint32_t slice,
                              const uint8_t *data,
                              size_t bytesPerRow)
{
    ASSERT(region.size.depth == 1);
    replaceRegion(context, region, mipmapLevel, slice, data, bytesPerRow, 0);
}

void Texture::replaceRegion(ContextMtl *context,
                            const MTLRegion &region,
                            const MipmapNativeLevel &mipmapLevel,
                            uint32_t slice,
                            const uint8_t *data,
                            size_t bytesPerRow,
                            size_t bytesPer2DImage)
{
    if (mipmapLevel.get() >= this->mipmapLevels())
    {
        return;
    }

    ASSERT(isCPUAccessible());

    CommandQueue &cmdQueue = context->cmdQueue();

    syncContentIfNeeded(context);

    // NOTE(hqle): what if multiple contexts on multiple threads are using this texture?
    if (this->isBeingUsedByGPU(context))
    {
        context->flushCommandBuffer(mtl::NoWait);
    }

    cmdQueue.ensureResourceReadyForCPU(this);

    if (textureType() != MTLTextureType3D)
    {
        bytesPer2DImage = 0;
    }

    [get() replaceRegion:region
             mipmapLevel:mipmapLevel.get()
                   slice:slice
               withBytes:data
             bytesPerRow:bytesPerRow
           bytesPerImage:bytesPer2DImage];
}

void Texture::getBytes(ContextMtl *context,
                       size_t bytesPerRow,
                       size_t bytesPer2DInage,
                       const MTLRegion &region,
                       const MipmapNativeLevel &mipmapLevel,
                       uint32_t slice,
                       uint8_t *dataOut)
{
    ASSERT(isCPUAccessible());

    CommandQueue &cmdQueue = context->cmdQueue();

    syncContentIfNeeded(context);

    // NOTE(hqle): what if multiple contexts on multiple threads are using this texture?
    if (this->isBeingUsedByGPU(context))
    {
        context->flushCommandBuffer(mtl::NoWait);
    }

    cmdQueue.ensureResourceReadyForCPU(this);

    [get() getBytes:dataOut
          bytesPerRow:bytesPerRow
        bytesPerImage:bytesPer2DInage
           fromRegion:region
          mipmapLevel:mipmapLevel.get()
                slice:slice];
}

TextureRef Texture::createCubeFaceView(uint32_t face)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        switch (textureType())
        {
            case MTLTextureTypeCube:
                return TextureRef(new Texture(
                    this, MTLTextureType2D, NSMakeRange(0, mipmapLevels()), NSMakeRange(face, 1)));
            default:
                UNREACHABLE();
                return nullptr;
        }
    }
}

TextureRef Texture::createSliceMipView(uint32_t slice, const MipmapNativeLevel &level)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        switch (textureType())
        {
            case MTLTextureTypeCube:
            case MTLTextureType2D:
            case MTLTextureType2DArray:
                return TextureRef(new Texture(this, MTLTextureType2D, NSMakeRange(level.get(), 1),
                                              NSMakeRange(slice, 1)));
            default:
                UNREACHABLE();
                return nullptr;
        }
    }
}

TextureRef Texture::createMipView(const MipmapNativeLevel &level)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        NSUInteger slices = cubeFacesOrArrayLength();
        return TextureRef(
            new Texture(this, textureType(), NSMakeRange(level.get(), 1), NSMakeRange(0, slices)));
    }
}

TextureRef Texture::createViewWithDifferentFormat(MTLPixelFormat format)
{
    ASSERT(supportFormatView());
    return TextureRef(new Texture(this, format));
}

TextureRef Texture::createShaderImageView(const MipmapNativeLevel &level,
                                          int layer,
                                          MTLPixelFormat format)
{
    ASSERT(isShaderReadable());
    ASSERT(isShaderWritable());
    ASSERT(format == pixelFormat() || supportFormatView());
    return TextureRef(new Texture(this, textureType(), level, layer, format));
}

TextureRef Texture::createViewWithCompatibleFormat(MTLPixelFormat format)
{
    return TextureRef(new Texture(this, format));
}

TextureRef Texture::createSwizzleView(const TextureSwizzleChannels &swizzle)
{
#if ANGLE_MTL_SWIZZLE_AVAILABLE
    return TextureRef(new Texture(this, swizzle));
#else
    WARN() << "Texture swizzle is not supported on pre iOS 13.0 and macOS 15.0";
    UNIMPLEMENTED();
    return shared_from_this();
#endif
}

MTLPixelFormat Texture::pixelFormat() const
{
    return get().pixelFormat;
}

MTLTextureType Texture::textureType() const
{
    return get().textureType;
}

uint32_t Texture::mipmapLevels() const
{
    return static_cast<uint32_t>(get().mipmapLevelCount);
}

uint32_t Texture::arrayLength() const
{
    return static_cast<uint32_t>(get().arrayLength);
}

uint32_t Texture::cubeFacesOrArrayLength() const
{
    if (textureType() == MTLTextureTypeCube)
    {
        return 6;
    }
    return arrayLength();
}

uint32_t Texture::width(const MipmapNativeLevel &level) const
{
    return static_cast<uint32_t>(GetMipSize(get().width, level));
}

uint32_t Texture::height(const MipmapNativeLevel &level) const
{
    return static_cast<uint32_t>(GetMipSize(get().height, level));
}

uint32_t Texture::depth(const MipmapNativeLevel &level) const
{
    return static_cast<uint32_t>(GetMipSize(get().depth, level));
}

gl::Extents Texture::size(const MipmapNativeLevel &level) const
{
    gl::Extents re;

    re.width  = width(level);
    re.height = height(level);
    re.depth  = depth(level);

    return re;
}

gl::Extents Texture::size(const ImageNativeIndex &index) const
{
    gl::Extents extents = size(index.getNativeLevel());

    if (index.hasLayer())
    {
        extents.depth = 1;
    }

    return extents;
}

uint32_t Texture::samples() const
{
    return static_cast<uint32_t>(get().sampleCount);
}

bool Texture::hasIOSurface() const
{
    return (get().iosurface) != nullptr;
}

bool Texture::sameTypeAndDimemsionsAs(const TextureRef &other) const
{
    return textureType() == other->textureType() && pixelFormat() == other->pixelFormat() &&
           mipmapLevels() == other->mipmapLevels() &&
           cubeFacesOrArrayLength() == other->cubeFacesOrArrayLength() &&
           widthAt0() == other->widthAt0() && heightAt0() == other->heightAt0() &&
           depthAt0() == other->depthAt0();
}

angle::Result Texture::resize(ContextMtl *context, uint32_t width, uint32_t height)
{
    // Resizing texture view is not supported.
    ASSERT(mCreationDesc);

    ANGLE_MTL_OBJC_SCOPE
    {
        MTLTextureDescriptor *newDesc = [[mCreationDesc.get() copy] ANGLE_MTL_AUTORELEASE];
        newDesc.width                 = width;
        newDesc.height                = height;
        auto newTexture               = context->getMetalDevice().newTextureWithDescriptor(newDesc);
        ANGLE_CHECK_GL_ALLOC(context, newTexture);
        mCreationDesc.retainAssign(newDesc);
        set(newTexture);
        // Reset reference counter
        Resource::reset();
    }

    return angle::Result::Continue;
}

TextureRef Texture::getLinearColorView()
{
    if (mLinearColorView)
    {
        return mLinearColorView;
    }

    switch (pixelFormat())
    {
        case MTLPixelFormatRGBA8Unorm_sRGB:
            mLinearColorView = createViewWithCompatibleFormat(MTLPixelFormatRGBA8Unorm);
            break;
        case MTLPixelFormatBGRA8Unorm_sRGB:
            mLinearColorView = createViewWithCompatibleFormat(MTLPixelFormatBGRA8Unorm);
            break;
        default:
            // NOTE(hqle): Not all sRGB formats are supported yet.
            UNREACHABLE();
    }

    return mLinearColorView;
}

TextureRef Texture::getReadableCopy(ContextMtl *context,
                                    mtl::BlitCommandEncoder *encoder,
                                    const uint32_t levelToCopy,
                                    const uint32_t sliceToCopy,
                                    const MTLRegion &areaToCopy)
{
    gl::Extents firstLevelSize = size(kZeroNativeMipLevel);
    if (!mReadCopy || mReadCopy->get().width < static_cast<size_t>(firstLevelSize.width) ||
        mReadCopy->get().height < static_cast<size_t>(firstLevelSize.height))
    {
        // Create a texture that big enough to store the first level data and any smaller level
        ANGLE_MTL_OBJC_SCOPE
        {
            auto desc            = [[MTLTextureDescriptor new] ANGLE_MTL_AUTORELEASE];
            desc.textureType     = get().textureType;
            desc.pixelFormat     = get().pixelFormat;
            desc.width           = firstLevelSize.width;
            desc.height          = firstLevelSize.height;
            desc.depth           = 1;
            desc.arrayLength     = 1;
            desc.resourceOptions = MTLResourceStorageModePrivate;
            desc.sampleCount     = get().sampleCount;
            desc.usage           = MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;
            mReadCopy.reset(new Texture(context->getMetalDevice().newTextureWithDescriptor(desc)));
        }  // ANGLE_MTL_OBJC_SCOPE
    }

    ASSERT(encoder);

    encoder->copyTexture(shared_from_this(), sliceToCopy, mtl::MipmapNativeLevel(levelToCopy),
                         mReadCopy, 0, mtl::kZeroNativeMipLevel, 1, 1);

    return mReadCopy;
}

void Texture::releaseReadableCopy()
{
    mReadCopy = nullptr;
}

TextureRef Texture::getStencilView()
{
    if (mStencilView)
    {
        return mStencilView;
    }

    switch (pixelFormat())
    {
        case MTLPixelFormatStencil8:
        case MTLPixelFormatX32_Stencil8:
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        case MTLPixelFormatX24_Stencil8:
#endif
            return mStencilView = shared_from_this();
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
        case MTLPixelFormatDepth24Unorm_Stencil8:
            mStencilView = createViewWithDifferentFormat(MTLPixelFormatX24_Stencil8);
            break;
#endif
        case MTLPixelFormatDepth32Float_Stencil8:
            mStencilView = createViewWithDifferentFormat(MTLPixelFormatX32_Stencil8);
            break;
        default:
            UNREACHABLE();
    }

    return mStencilView;
}

void Texture::set(id<MTLTexture> metalTexture)
{
    ParentClass::set(metalTexture);
    // Reset stencil view
    mStencilView     = nullptr;
    mLinearColorView = nullptr;
}

// Buffer implementation
angle::Result Buffer::MakeBuffer(ContextMtl *context,
                                 size_t size,
                                 const uint8_t *data,
                                 BufferRef *bufferOut)
{

    return MakeBufferWithSharedMemOpt(context, false, size, data, bufferOut);
}

angle::Result Buffer::MakeBufferWithSharedMemOpt(ContextMtl *context,
                                                 bool forceUseSharedMem,
                                                 size_t size,
                                                 const uint8_t *data,
                                                 BufferRef *bufferOut)
{
    bufferOut->reset(new Buffer(context, forceUseSharedMem, size, data));

    if (!(*bufferOut) || !(*bufferOut)->get())
    {
        ANGLE_MTL_CHECK(context, false, GL_OUT_OF_MEMORY);
    }

    return angle::Result::Continue;
}

angle::Result Buffer::MakeBufferWithResOpt(ContextMtl *context,
                                           MTLResourceOptions options,
                                           size_t size,
                                           const uint8_t *data,
                                           BufferRef *bufferOut)
{
    bufferOut->reset(new Buffer(context, options, size, data));

    if (!(*bufferOut) || !(*bufferOut)->get())
    {
        ANGLE_MTL_CHECK(context, false, GL_OUT_OF_MEMORY);
    }

    return angle::Result::Continue;
}

Buffer::Buffer(ContextMtl *context, bool forceUseSharedMem, size_t size, const uint8_t *data)
{
    (void)resetWithSharedMemOpt(context, forceUseSharedMem, size, data);
}

Buffer::Buffer(ContextMtl *context, MTLResourceOptions options, size_t size, const uint8_t *data)
{
    (void)resetWithResOpt(context, options, size, data);
}

angle::Result Buffer::reset(ContextMtl *context, size_t size, const uint8_t *data)
{
    return resetWithSharedMemOpt(context, false, size, data);
}

angle::Result Buffer::resetWithSharedMemOpt(ContextMtl *context,
                                            bool forceUseSharedMem,
                                            size_t size,
                                            const uint8_t *data)
{
    MTLResourceOptions options;

    options = 0;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    if (!forceUseSharedMem || context->getDisplay()->getFeatures().forceBufferGPUStorage.enabled)
    {
        options |= MTLResourceStorageModeManaged;
    }
    else
#endif
    {
        options |= MTLResourceStorageModeShared;
    }

    return resetWithResOpt(context, options, size, data);
}

angle::Result Buffer::resetWithResOpt(ContextMtl *context,
                                      MTLResourceOptions options,
                                      size_t size,
                                      const uint8_t *data)
{
    set([&] {
        const mtl::ContextDevice &metalDevice = context->getMetalDevice();
        if (data)
        {
            return metalDevice.newBufferWithBytes(data, size, options);
        }
        return metalDevice.newBufferWithLength(size, options);
    }());
    // Reset command buffer's reference serial
    Resource::reset();

    return angle::Result::Continue;
}

void Buffer::syncContent(ContextMtl *context, mtl::BlitCommandEncoder *blitEncoder)
{
    InvokeCPUMemSync(context, blitEncoder, this);
}

const uint8_t *Buffer::mapReadOnly(ContextMtl *context)
{
    return mapWithOpt(context, true, false);
}

uint8_t *Buffer::map(ContextMtl *context)
{
    return mapWithOpt(context, false, false);
}

uint8_t *Buffer::mapWithOpt(ContextMtl *context, bool readonly, bool noSync)
{
    mMapReadOnly = readonly;

    if (!noSync && (isCPUReadMemSyncPending() || isCPUReadMemNeedSync() || !readonly))
    {
        CommandQueue &cmdQueue = context->cmdQueue();

        EnsureCPUMemWillBeSynced(context, this);

        if (this->isBeingUsedByGPU(context))
        {
            context->flushCommandBuffer(mtl::NoWait);
        }

        cmdQueue.ensureResourceReadyForCPU(this);
        resetCPUReadMemSyncPending();
    }

    return reinterpret_cast<uint8_t *>([get() contents]);
}

void Buffer::unmap(ContextMtl *context)
{
    flush(context, 0, size());

    // Reset read only flag
    mMapReadOnly = true;
}

void Buffer::unmapNoFlush(ContextMtl *context)
{
    mMapReadOnly = true;
}

void Buffer::unmapAndFlushSubset(ContextMtl *context, size_t offsetWritten, size_t sizeWritten)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    flush(context, offsetWritten, sizeWritten);
#endif
    mMapReadOnly = true;
}

void Buffer::flush(ContextMtl *context, size_t offsetWritten, size_t sizeWritten)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    if (!mMapReadOnly)
    {
        if (get().storageMode == MTLStorageModeManaged)
        {
            size_t bufferSize  = size();
            size_t startOffset = std::min(offsetWritten, bufferSize);
            size_t endOffset   = std::min(offsetWritten + sizeWritten, bufferSize);
            size_t clampedSize = endOffset - startOffset;
            if (clampedSize > 0)
            {
                [get() didModifyRange:NSMakeRange(startOffset, clampedSize)];
            }
        }
    }
#endif
}

size_t Buffer::size() const
{
    return get().length;
}

bool Buffer::useSharedMem() const
{
    return get().storageMode == MTLStorageModeShared;
}
}  // namespace mtl
}  // namespace rx
