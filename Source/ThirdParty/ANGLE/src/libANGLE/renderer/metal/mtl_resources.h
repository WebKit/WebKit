//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_resources.h:
//    Declares wrapper classes for Metal's MTLTexture and MTLBuffer.
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_RESOURCES_H_
#define LIBANGLE_RENDERER_METAL_MTL_RESOURCES_H_

#import <Metal/Metal.h>

#include <atomic>
#include <memory>

#include "common/FastVector.h"
#include "common/MemoryBuffer.h"
#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"

namespace rx
{

class ContextMtl;

namespace mtl
{

class ContextDevice;
class CommandQueue;
class BlitCommandEncoder;
class Resource;
class Texture;
class Buffer;

using ResourceRef    = std::shared_ptr<Resource>;
using TextureRef     = std::shared_ptr<Texture>;
using TextureWeakRef = std::weak_ptr<Texture>;
using BufferRef      = std::shared_ptr<Buffer>;
using BufferWeakRef  = std::weak_ptr<Buffer>;

class Resource : angle::NonCopyable
{
  public:
    virtual ~Resource() {}

    // Check whether the resource still being used by GPU including the pending (uncommitted)
    // command buffer.
    bool isBeingUsedByGPU(Context *context) const;
    // Checks whether the last command buffer that uses the given resource has been committed or not
    bool hasPendingWorks(Context *context) const;

    void setUsedByCommandBufferWithQueueSerial(uint64_t serial, bool writing);
    void setWrittenToByRenderEncoder(uint64_t serial);

    uint64_t getCommandBufferQueueSerial() const { return mUsageRef->cmdBufferQueueSerial; }

    // Flag indicate whether we should synchronize the content to CPU after GPU changed this
    // resource's content.
    bool isCPUReadMemNeedSync() const { return mUsageRef->cpuReadMemNeedSync; }
    void resetCPUReadMemNeedSync() { mUsageRef->cpuReadMemNeedSync = false; }

    bool isCPUReadMemSyncPending() const { return mUsageRef->cpuReadMemSyncPending; }
    void setCPUReadMemSyncPending(bool value) const { mUsageRef->cpuReadMemSyncPending = value; }
    void resetCPUReadMemSyncPending() { mUsageRef->cpuReadMemSyncPending = false; }

    bool isCPUReadMemDirty() const { return mUsageRef->cpuReadMemDirty; }
    void resetCPUReadMemDirty() { mUsageRef->cpuReadMemDirty = false; }

    bool getLastWritingRenderEncoderSerial() const
    {
        return mUsageRef->lastWritingRenderEncoderSerial;
    }
    void setLastWritingRenderEncoderSerial(uint64_t serial) const
    {
        mUsageRef->lastWritingRenderEncoderSerial = serial;
    }

    virtual size_t estimatedByteSize() const = 0;
    virtual id getID() const                 = 0;

  protected:
    Resource();
    // Share the GPU usage ref with other resource
    Resource(Resource *other);

    void reset();

  private:
    struct UsageRef
    {
        // The id of the last command buffer that is using this resource.
        uint64_t cmdBufferQueueSerial = 0;

        // This flag means the resource was issued to be modified by GPU, if CPU wants to read
        // its content, explicit synchronization call must be invoked.
        bool cpuReadMemNeedSync = false;

        // This flag is set when synchronization for the resource has been
        // encoded on the GPU, and a map operation must wait
        // until it's completed.
        bool cpuReadMemSyncPending = false;

        // This flag is useful for BufferMtl to know whether it should update the shadow copy
        bool cpuReadMemDirty = false;

        // The id of the last render encoder to write to this resource
        uint64_t lastWritingRenderEncoderSerial = 0;
    };

    // One resource object might just be a view of another resource. For example, a texture 2d
    // object might be a view of one face of a cube texture object. Another example is one texture
    // object of size 2x2 might be a mipmap view of a texture object size 4x4. Thus, if one object
    // is being used by a command buffer, it means the other object is being used also. In this
    // case, the two objects must share the same UsageRef property.
    std::shared_ptr<UsageRef> mUsageRef;
};

class Texture final : public Resource,
                      public WrappedObject<id<MTLTexture>>,
                      public std::enable_shared_from_this<Texture>
{
  public:
    static angle::Result Make2DTexture(ContextMtl *context,
                                       const Format &format,
                                       uint32_t width,
                                       uint32_t height,
                                       uint32_t mips /** use zero to create full mipmaps chain */,
                                       bool renderTargetOnly,
                                       bool allowFormatView,
                                       TextureRef *refOut);

    // On macOS, memory will still be allocated for this texture.
    static angle::Result MakeMemoryLess2DTexture(ContextMtl *context,
                                                 const Format &format,
                                                 uint32_t width,
                                                 uint32_t height,
                                                 TextureRef *refOut);

    static angle::Result MakeCubeTexture(ContextMtl *context,
                                         const Format &format,
                                         uint32_t size,
                                         uint32_t mips /** use zero to create full mipmaps chain */,
                                         bool renderTargetOnly,
                                         bool allowFormatView,
                                         TextureRef *refOut);

    static angle::Result Make2DMSTexture(ContextMtl *context,
                                         const Format &format,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t samples,
                                         bool renderTargetOnly,
                                         bool allowFormatView,
                                         TextureRef *refOut);

    static angle::Result Make2DArrayTexture(ContextMtl *context,
                                            const Format &format,
                                            uint32_t width,
                                            uint32_t height,
                                            uint32_t mips,
                                            uint32_t arrayLength,
                                            bool renderTargetOnly,
                                            bool allowFormatView,
                                            TextureRef *refOut);

    static angle::Result Make3DTexture(ContextMtl *context,
                                       const Format &format,
                                       uint32_t width,
                                       uint32_t height,
                                       uint32_t depth,
                                       uint32_t mips,
                                       bool renderTargetOnly,
                                       bool allowFormatView,
                                       TextureRef *refOut);
    static TextureRef MakeFromMetal(id<MTLTexture> metalTexture);

    // Allow CPU to read & write data directly to this texture?
    bool isCPUAccessible() const;
    // Allow shaders to read/sample this texture?
    // Texture created with renderTargetOnly flag won't be readable
    bool isShaderReadable() const;
    // Allow shaders to write this texture?
    bool isShaderWritable() const;

    bool supportFormatView() const;

    void replace2DRegion(ContextMtl *context,
                         const MTLRegion &region,
                         const MipmapNativeLevel &mipmapLevel,
                         uint32_t slice,
                         const uint8_t *data,
                         size_t bytesPerRow);

    void replaceRegion(ContextMtl *context,
                       const MTLRegion &region,
                       const MipmapNativeLevel &mipmapLevel,
                       uint32_t slice,
                       const uint8_t *data,
                       size_t bytesPerRow,
                       size_t bytesPer2DImage);

    void getBytes(ContextMtl *context,
                  size_t bytesPerRow,
                  size_t bytesPer2DInage,
                  const MTLRegion &region,
                  const MipmapNativeLevel &mipmapLevel,
                  uint32_t slice,
                  uint8_t *dataOut);

    // Create 2d view of a cube face which full range of mip levels.
    TextureRef createCubeFaceView(uint32_t face);
    // Create a view of one slice at a level.
    TextureRef createSliceMipView(uint32_t slice, const MipmapNativeLevel &level);
    // Create a view of a level.
    TextureRef createMipView(const MipmapNativeLevel &level);
    // Create a view with different format
    TextureRef createViewWithDifferentFormat(MTLPixelFormat format);
    // Create a view for a shader image binding.
    TextureRef createShaderImageView(const MipmapNativeLevel &level,
                                     int layer,
                                     MTLPixelFormat format);
    // Same as above but the target format must be compatible, for example sRGB to linear. In this
    // case texture doesn't need format view usage flag.
    TextureRef createViewWithCompatibleFormat(MTLPixelFormat format);
    // Create a swizzled view
    TextureRef createSwizzleView(const TextureSwizzleChannels &swizzle);

    MTLTextureType textureType() const;
    MTLPixelFormat pixelFormat() const;

    uint32_t mipmapLevels() const;
    uint32_t arrayLength() const;
    uint32_t cubeFacesOrArrayLength() const;

    uint32_t width(const MipmapNativeLevel &level) const;
    uint32_t height(const MipmapNativeLevel &level) const;
    uint32_t depth(const MipmapNativeLevel &level) const;

    gl::Extents size(const MipmapNativeLevel &level) const;
    gl::Extents size(const ImageNativeIndex &index) const;

    uint32_t widthAt0() const { return width(kZeroNativeMipLevel); }
    uint32_t heightAt0() const { return height(kZeroNativeMipLevel); }
    uint32_t depthAt0() const { return depth(kZeroNativeMipLevel); }
    gl::Extents sizeAt0() const { return size(kZeroNativeMipLevel); }

    uint32_t samples() const;

    bool hasIOSurface() const;
    bool sameTypeAndDimemsionsAs(const TextureRef &other) const;

    angle::Result resize(ContextMtl *context, uint32_t width, uint32_t height);

    // For render target
    MTLColorWriteMask getColorWritableMask() const { return *mColorWritableMask; }
    void setColorWritableMask(MTLColorWriteMask mask) { *mColorWritableMask = mask; }

    // Get reading copy. Used for reading non-readable texture or reading stencil value from
    // packed depth & stencil texture.
    // NOTE: this only copies 1 depth slice of the 3D texture.
    // The texels will be copied to region(0, 0, 0, areaToCopy.size) of the returned texture.
    // The returned pointer will be retained by the original texture object.
    // Calling getReadableCopy() will overwrite previously returned texture.
    TextureRef getReadableCopy(ContextMtl *context,
                               mtl::BlitCommandEncoder *encoder,
                               const uint32_t levelToCopy,
                               const uint32_t sliceToCopy,
                               const MTLRegion &areaToCopy);

    void releaseReadableCopy();

    // Get stencil view
    TextureRef getStencilView();
    // Get linear color
    TextureRef getLinearColorView();

    // Change the wrapped metal object. Special case for swapchain image
    void set(id<MTLTexture> metalTexture);

    // Explicitly sync content between CPU and GPU
    void syncContent(ContextMtl *context, mtl::BlitCommandEncoder *encoder);
    void setEstimatedByteSize(size_t bytes) { mEstimatedByteSize = bytes; }
    size_t estimatedByteSize() const override { return mEstimatedByteSize; }
    id getID() const override { return get(); }

  private:
    using ParentClass = WrappedObject<id<MTLTexture>>;

    static angle::Result MakeTexture(ContextMtl *context,
                                     const Format &mtlFormat,
                                     MTLTextureDescriptor *desc,
                                     uint32_t mips,
                                     bool renderTargetOnly,
                                     bool allowFormatView,
                                     TextureRef *refOut);

    static angle::Result MakeTexture(ContextMtl *context,
                                     const Format &mtlFormat,
                                     MTLTextureDescriptor *desc,
                                     uint32_t mips,
                                     bool renderTargetOnly,
                                     bool allowFormatView,
                                     bool memoryLess,
                                     TextureRef *refOut);

    static angle::Result MakeTexture(ContextMtl *context,
                                     const Format &mtlFormat,
                                     MTLTextureDescriptor *desc,
                                     IOSurfaceRef surfaceRef,
                                     NSUInteger slice,
                                     bool renderTargetOnly,
                                     TextureRef *refOut);

    Texture(id<MTLTexture> metalTexture);
    Texture(ContextMtl *context,
            MTLTextureDescriptor *desc,
            uint32_t mips,
            bool renderTargetOnly,
            bool allowFormatView);
    Texture(ContextMtl *context,
            MTLTextureDescriptor *desc,
            uint32_t mips,
            bool renderTargetOnly,
            bool allowFormatView,
            bool memoryLess);

    Texture(ContextMtl *context,
            MTLTextureDescriptor *desc,
            IOSurfaceRef iosurface,
            NSUInteger plane,
            bool renderTargetOnly);

    // Create a texture view
    Texture(Texture *original, MTLPixelFormat format);
    Texture(Texture *original, MTLTextureType type, NSRange mipmapLevelRange, NSRange slices);
    Texture(Texture *original, const TextureSwizzleChannels &swizzle);

    // Creates a view for a shader image binding.
    Texture(Texture *original,
            MTLTextureType type,
            const MipmapNativeLevel &level,
            int layer,
            MTLPixelFormat pixelFormat);

    void syncContentIfNeeded(ContextMtl *context);

    AutoObjCObj<MTLTextureDescriptor> mCreationDesc;

    // This property is shared between this object and its views:
    std::shared_ptr<MTLColorWriteMask> mColorWritableMask;

    // Linear view of sRGB texture
    TextureRef mLinearColorView;

    TextureRef mStencilView;
    // Readable copy of texture
    TextureRef mReadCopy;

    size_t mEstimatedByteSize = 0;
};

class Buffer final : public Resource, public WrappedObject<id<MTLBuffer>>
{
  public:
    static angle::Result MakeBuffer(ContextMtl *context,
                                    size_t size,
                                    const uint8_t *data,
                                    BufferRef *bufferOut);

    static angle::Result MakeBufferWithSharedMemOpt(ContextMtl *context,
                                                    bool forceUseSharedMem,
                                                    size_t size,
                                                    const uint8_t *data,
                                                    BufferRef *bufferOut);

    static angle::Result MakeBufferWithResOpt(ContextMtl *context,
                                              MTLResourceOptions resourceOptions,
                                              size_t size,
                                              const uint8_t *data,
                                              BufferRef *bufferOut);

    angle::Result reset(ContextMtl *context, size_t size, const uint8_t *data);
    angle::Result resetWithSharedMemOpt(ContextMtl *context,
                                        bool forceUseSharedMem,
                                        size_t size,
                                        const uint8_t *data);
    angle::Result resetWithResOpt(ContextMtl *context,
                                  MTLResourceOptions resourceOptions,
                                  size_t size,
                                  const uint8_t *data);

    const uint8_t *mapReadOnly(ContextMtl *context);
    uint8_t *map(ContextMtl *context);
    uint8_t *mapWithOpt(ContextMtl *context, bool readonly, bool noSync);

    void unmap(ContextMtl *context);
    // Same as unmap but do not do implicit flush()
    void unmapNoFlush(ContextMtl *context);
    void unmapAndFlushSubset(ContextMtl *context, size_t offsetWritten, size_t sizeWritten);
    void flush(ContextMtl *context, size_t offsetWritten, size_t sizeWritten);

    size_t size() const;
    bool useSharedMem() const;

    // Explicitly sync content between CPU and GPU
    void syncContent(ContextMtl *context, mtl::BlitCommandEncoder *encoder);

    size_t estimatedByteSize() const override { return size(); }
    id getID() const override { return get(); }

  private:
    Buffer(ContextMtl *context, bool forceUseSharedMem, size_t size, const uint8_t *data);
    Buffer(ContextMtl *context,
           MTLResourceOptions resourceOptions,
           size_t size,
           const uint8_t *data);

    bool mMapReadOnly = true;
};

class NativeTexLevelArray
{
  public:
    TextureRef &at(const MipmapNativeLevel &level) { return mTexLevels.at(level.get()); }
    const TextureRef &at(const MipmapNativeLevel &level) const
    {
        return mTexLevels.at(level.get());
    }

    TextureRef &operator[](const MipmapNativeLevel &level) { return at(level); }
    const TextureRef &operator[](const MipmapNativeLevel &level) const { return at(level); }

    gl::TexLevelArray<TextureRef>::iterator begin() { return mTexLevels.begin(); }
    gl::TexLevelArray<TextureRef>::const_iterator begin() const { return mTexLevels.begin(); }
    gl::TexLevelArray<TextureRef>::iterator end() { return mTexLevels.end(); }
    gl::TexLevelArray<TextureRef>::const_iterator end() const { return mTexLevels.end(); }

  private:
    gl::TexLevelArray<TextureRef> mTexLevels;
};

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_MTL_RESOURCES_H_ */
