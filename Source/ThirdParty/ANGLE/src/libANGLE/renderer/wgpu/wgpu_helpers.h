//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef LIBANGLE_RENDERER_WGPU_WGPU_HELPERS_H_
#define LIBANGLE_RENDERER_WGPU_WGPU_HELPERS_H_

#include <stdint.h>
#include <webgpu/webgpu.h>
#include <algorithm>

#include "libANGLE/Error.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{

class ContextWgpu;

namespace webgpu
{

// WebGPU requires copy buffers bytesPerRow to be aligned to 256.
// https://www.w3.org/TR/webgpu/#abstract-opdef-validating-gputexelcopybufferinfo
static const GLuint kCopyBufferAlignment = 256;

enum class UpdateSource
{
    Clear,
    Texture,
};

struct ClearUpdate
{
    ClearValues clearValues;
    bool hasDepth;
    bool hasStencil;
};

struct SubresourceUpdate
{
    SubresourceUpdate() {}
    ~SubresourceUpdate() {}

    SubresourceUpdate(UpdateSource targetUpdateSource,
                      gl::LevelIndex newTargetLevel,
                      BufferHandle targetBuffer,
                      const WGPUTexelCopyBufferLayout &targetBufferLayout)
    {
        updateSource      = targetUpdateSource;
        textureData       = targetBuffer;
        textureDataLayout = targetBufferLayout;
        targetLevel       = newTargetLevel;
    }

    SubresourceUpdate(UpdateSource targetUpdateSource,
                      gl::LevelIndex newTargetLevel,
                      uint32_t newLayerIndex,
                      uint32_t newLayerCount,
                      BufferHandle targetBuffer,
                      const WGPUTexelCopyBufferLayout &targetBufferLayout)
    {
        updateSource      = targetUpdateSource;
        textureData       = targetBuffer;
        textureDataLayout = targetBufferLayout;
        targetLevel       = newTargetLevel;
        layerIndex        = newLayerIndex;
        layerCount        = newLayerCount;
    }

    SubresourceUpdate(UpdateSource targetUpdateSource,
                      gl::LevelIndex newTargetLevel,
                      ClearValues clearValues,
                      bool hasDepth,
                      bool hasStencil)
    {
        updateSource          = targetUpdateSource;
        targetLevel           = newTargetLevel;
        clearData.clearValues = clearValues;
        clearData.hasDepth    = hasDepth;
        clearData.hasStencil  = hasStencil;
    }

    UpdateSource updateSource;
    ClearUpdate clearData;
    BufferHandle textureData;
    WGPUTexelCopyBufferLayout textureDataLayout;

    gl::LevelIndex targetLevel;
    uint32_t layerIndex = 0;
    uint32_t layerCount = 1;
};

WGPUTextureDimension ToWgpuTextureDimension(gl::TextureType glTextureType);

class ImageHelper : public angle::Subject
{
  public:
    ImageHelper();
    ~ImageHelper() override;

    angle::Result initImage(const DawnProcTable *wgpu,
                            angle::FormatID intendedFormatID,
                            angle::FormatID actualFormatID,
                            DeviceHandle device,
                            gl::LevelIndex firstAllocatedLevel,
                            WGPUTextureDescriptor textureDescriptor);
    angle::Result initExternal(const DawnProcTable *wgpu,
                               angle::FormatID intendedFormatID,
                               angle::FormatID actualFormatID,
                               webgpu::TextureHandle externalTexture);

    angle::Result flushStagedUpdates(ContextWgpu *contextWgpu);
    angle::Result flushSingleLevelUpdates(ContextWgpu *contextWgpu,
                                          gl::LevelIndex levelGL,
                                          ClearValuesArray *deferredClears,
                                          uint32_t deferredClearIndex);

    WGPUTextureDescriptor createTextureDescriptor(WGPUTextureUsage usage,
                                                  WGPUTextureDimension dimension,
                                                  WGPUExtent3D size,
                                                  WGPUTextureFormat format,
                                                  std::uint32_t mipLevelCount,
                                                  std::uint32_t sampleCount);

    angle::Result stageTextureUpload(ContextWgpu *contextWgpu,
                                     const webgpu::Format &webgpuFormat,
                                     GLenum type,
                                     const gl::Extents &glExtents,
                                     GLuint inputRowPitch,
                                     GLuint inputDepthPitch,
                                     uint32_t outputRowPitch,
                                     uint32_t outputDepthPitch,
                                     uint32_t allocationSize,
                                     const gl::ImageIndex &index,
                                     const uint8_t *pixels);

    void stageClear(gl::LevelIndex targetLevel,
                    ClearValues clearValues,
                    bool hasDepth,
                    bool hasStencil);

    void removeStagedUpdates(gl::LevelIndex levelToRemove);
    void removeSingleSubresourceStagedUpdates(gl::LevelIndex levelToRemove,
                                              uint32_t layerIndex,
                                              uint32_t layerCount);

    void resetImage();

    angle::Result CopyImage(ContextWgpu *contextWgpu,
                            ImageHelper *srcImage,
                            const gl::ImageIndex &dstIndex,
                            const gl::Offset &dstOffset,
                            gl::LevelIndex sourceLevelGL,
                            uint32_t sourceLayer,
                            const gl::Box &sourceBox);

    static angle::Result getReadPixelsParams(rx::ContextWgpu *contextWgpu,
                                             const gl::PixelPackState &packState,
                                             gl::Buffer *packBuffer,
                                             GLenum format,
                                             GLenum type,
                                             const gl::Rectangle &area,
                                             const gl::Rectangle &clippedArea,
                                             rx::PackPixelsParams *paramsOut,
                                             GLuint *skipBytesOut);

    angle::Result readPixels(rx::ContextWgpu *contextWgpu,
                             const gl::Rectangle &area,
                             const rx::PackPixelsParams &packPixelsParams,
                             webgpu::LevelIndex level,
                             uint32_t layer,
                             void *pixels);

    angle::Result createTextureViewSingleLevel(gl::LevelIndex targetLevel,
                                               uint32_t layerIndex,
                                               TextureViewHandle &textureViewOut);
    angle::Result createFullTextureView(TextureViewHandle &textureViewOut,
                                        WGPUTextureViewDimension desiredViewDimension);
    angle::Result createTextureView(gl::LevelIndex targetLevel,
                                    uint32_t levelCount,
                                    uint32_t layerIndex,
                                    uint32_t arrayLayerCount,
                                    TextureViewHandle &textureViewOut,
                                    Optional<WGPUTextureViewDimension> desiredViewDimension);
    LevelIndex toWgpuLevel(gl::LevelIndex levelIndexGl) const;
    gl::LevelIndex toGlLevel(LevelIndex levelIndexWgpu) const;
    bool isTextureLevelInAllocatedImage(gl::LevelIndex textureLevel) const;
    TextureHandle &getTexture() { return mTexture; }
    WGPUTextureFormat toWgpuTextureFormat() const { return mTextureDescriptor.format; }
    angle::FormatID getIntendedFormatID() const { return mIntendedFormatID; }
    angle::FormatID getActualFormatID() const { return mActualFormatID; }
    const WGPUTextureDescriptor &getTextureDescriptor() const { return mTextureDescriptor; }
    gl::LevelIndex getFirstAllocatedLevel() const { return mFirstAllocatedLevel; }
    gl::LevelIndex getLastAllocatedLevel() const;
    uint32_t getLevelCount() const { return mTextureDescriptor.mipLevelCount; }
    WGPUExtent3D getSize() const { return mTextureDescriptor.size; }
    WGPUExtent3D getLevelSize(LevelIndex wgpuLevel) const;
    uint32_t getSamples() const { return mTextureDescriptor.sampleCount; }
    WGPUTextureUsage getUsage() const { return mTextureDescriptor.usage; }
    bool isInitialized() const { return mInitialized; }

  private:
    void appendSubresourceUpdate(gl::LevelIndex level, SubresourceUpdate &&update);
    std::vector<SubresourceUpdate> *getLevelUpdates(gl::LevelIndex level);

    const DawnProcTable *mProcTable = nullptr;
    TextureHandle mTexture;
    WGPUTextureDescriptor mTextureDescriptor   = {};
    bool mInitialized                          = false;

    gl::LevelIndex mFirstAllocatedLevel = gl::LevelIndex(0);
    angle::FormatID mIntendedFormatID;
    angle::FormatID mActualFormatID;

    std::vector<std::vector<SubresourceUpdate>> mSubresourceQueue;
};
struct BufferMapState
{
    WGPUMapMode mode;
    size_t offset;
    size_t size;
};

enum class MapAtCreation
{
    No,
    Yes,
};

struct BufferReadback;

class BufferHelper : public angle::NonCopyable
{
  public:
    BufferHelper();
    ~BufferHelper();

    bool valid() const { return mBuffer.operator bool(); }
    void reset();

    angle::Result initBuffer(const DawnProcTable *wgpu,
                             webgpu::DeviceHandle device,
                             size_t size,
                             WGPUBufferUsage usage,
                             MapAtCreation mappedAtCreation);

    angle::Result mapImmediate(ContextWgpu *context, WGPUMapMode mode, size_t offset, size_t size);
    angle::Result unmap();

    uint8_t *getMapWritePointer(size_t offset, size_t size) const;
    const uint8_t *getMapReadPointer(size_t offset, size_t size) const;

    const std::optional<BufferMapState> &getMappedState() const;

    bool canMapForRead() const;
    bool canMapForWrite() const;

    bool isMappedForRead() const;
    bool isMappedForWrite() const;

    webgpu::BufferHandle getBuffer() const;
    uint64_t requestedSize() const;
    uint64_t actualSize() const;

    // Helper to copy data to a staging buffer and map it. Staging data is cleaned up by the
    // BufferReadback RAII object.
    angle::Result readDataImmediate(ContextWgpu *context,
                                    size_t offset,
                                    size_t size,
                                    webgpu::RenderPassClosureReason reason,
                                    BufferReadback *result);

  private:
    const DawnProcTable *mProcTable = nullptr;
    webgpu::BufferHandle mBuffer;
    size_t mRequestedSize = 0;

    std::optional<BufferMapState> mMappedState;
};

struct BufferReadback
{
    BufferHelper buffer;
    const uint8_t *data = nullptr;
};

}  // namespace webgpu
}  // namespace rx
#endif  // LIBANGLE_RENDERER_WGPU_WGPU_HELPERS_H_
