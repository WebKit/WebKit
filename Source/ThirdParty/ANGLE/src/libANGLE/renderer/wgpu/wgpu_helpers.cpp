//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/wgpu/wgpu_helpers.h"
#include "libANGLE/formatutils.h"

#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"
#include "wgpu_helpers.h"

namespace rx
{
namespace webgpu
{
namespace
{
wgpu::TextureDescriptor TextureDescriptorFromTexture(const wgpu::Texture &texture)
{
    wgpu::TextureDescriptor descriptor = {};
    descriptor.usage                   = texture.GetUsage();
    descriptor.dimension               = texture.GetDimension();
    descriptor.size   = {texture.GetWidth(), texture.GetHeight(), texture.GetDepthOrArrayLayers()};
    descriptor.format = texture.GetFormat();
    descriptor.mipLevelCount   = texture.GetMipLevelCount();
    descriptor.sampleCount     = texture.GetSampleCount();
    descriptor.viewFormatCount = 0;
    return descriptor;
}

size_t GetSafeBufferMapOffset(size_t offset)
{
    static_assert(gl::isPow2(kBufferMapOffsetAlignment));
    return roundDownPow2(offset, kBufferMapOffsetAlignment);
}

size_t GetSafeBufferMapSize(size_t offset, size_t size)
{
    // The offset is rounded down for alignment and the size is rounded up. The safe size must cover
    // both of these offsets.
    size_t offsetChange = offset % kBufferMapOffsetAlignment;
    static_assert(gl::isPow2(kBufferMapSizeAlignment));
    return roundUpPow2(size + offsetChange, kBufferMapSizeAlignment);
}

uint8_t *AdjustMapPointerForOffset(uint8_t *mapPtr, size_t offset)
{
    // Fix up a map pointer that has been adjusted for alignment
    size_t offsetChange = offset % kBufferMapOffsetAlignment;
    return mapPtr + offsetChange;
}

const uint8_t *AdjustMapPointerForOffset(const uint8_t *mapPtr, size_t offset)
{
    return AdjustMapPointerForOffset(const_cast<uint8_t *>(mapPtr), offset);
}

}  // namespace

ImageHelper::ImageHelper() {}

ImageHelper::~ImageHelper() {}

angle::Result ImageHelper::initImage(angle::FormatID intendedFormatID,
                                     angle::FormatID actualFormatID,
                                     wgpu::Device &device,
                                     gl::LevelIndex firstAllocatedLevel,
                                     wgpu::TextureDescriptor textureDescriptor)
{
    mIntendedFormatID    = intendedFormatID;
    mActualFormatID      = actualFormatID;
    mTextureDescriptor   = textureDescriptor;
    mFirstAllocatedLevel = firstAllocatedLevel;
    mTexture             = device.CreateTexture(&mTextureDescriptor);
    mInitialized         = true;

    return angle::Result::Continue;
}

angle::Result ImageHelper::initExternal(angle::FormatID intendedFormatID,
                                        angle::FormatID actualFormatID,
                                        wgpu::Texture externalTexture)
{
    mIntendedFormatID    = intendedFormatID;
    mActualFormatID      = actualFormatID;
    mTextureDescriptor   = TextureDescriptorFromTexture(externalTexture);
    mFirstAllocatedLevel = gl::LevelIndex(0);
    mTexture             = externalTexture;
    mInitialized         = true;

    return angle::Result::Continue;
}

angle::Result ImageHelper::flushStagedUpdates(ContextWgpu *contextWgpu,
                                              ClearValuesArray *deferredClears,
                                              uint32_t deferredClearIndex)
{
    if (mSubresourceQueue.empty())
    {
        return angle::Result::Continue;
    }
    wgpu::Device device          = contextWgpu->getDevice();
    wgpu::Queue queue            = contextWgpu->getQueue();
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ImageCopyTexture dst;
    dst.texture = mTexture;
    std::vector<wgpu::RenderPassColorAttachment> colorAttachments;
    for (const SubresourceUpdate &srcUpdate : mSubresourceQueue)
    {
        if (!isTextureLevelInAllocatedImage(srcUpdate.targetLevel))
        {
            continue;
        }
        switch (srcUpdate.updateSource)
        {
            case UpdateSource::Texture:
                dst.mipLevel = toWgpuLevel(srcUpdate.targetLevel).get();
                encoder.CopyBufferToTexture(&srcUpdate.textureData, &dst, &mTextureDescriptor.size);
                break;
            case UpdateSource::Clear:
                if (deferredClears)
                {
                    deferredClears->store(deferredClearIndex, srcUpdate.clearData);
                }
                else
                {

                    wgpu::TextureView textureView;
                    ANGLE_TRY(createTextureView(srcUpdate.targetLevel, 0, textureView));

                    colorAttachments.push_back(
                        CreateNewClearColorAttachment(srcUpdate.clearData.clearColor,
                                                      srcUpdate.clearData.depthSlice, textureView));
                }
                break;
        }
    }

    if (!colorAttachments.empty())
    {
        FramebufferWgpu *frameBuffer =
            GetImplAs<FramebufferWgpu>(contextWgpu->getState().getDrawFramebuffer());
        frameBuffer->addNewColorAttachments(colorAttachments);
    }
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);
    encoder = nullptr;
    mSubresourceQueue.clear();

    return angle::Result::Continue;
}

wgpu::TextureDescriptor ImageHelper::createTextureDescriptor(wgpu::TextureUsage usage,
                                                             wgpu::TextureDimension dimension,
                                                             wgpu::Extent3D size,
                                                             wgpu::TextureFormat format,
                                                             std::uint32_t mipLevelCount,
                                                             std::uint32_t sampleCount)
{
    wgpu::TextureDescriptor textureDescriptor = {};
    textureDescriptor.usage                   = usage;
    textureDescriptor.dimension               = dimension;
    textureDescriptor.size                    = size;
    textureDescriptor.format                  = format;
    textureDescriptor.mipLevelCount           = mipLevelCount;
    textureDescriptor.sampleCount             = sampleCount;
    textureDescriptor.viewFormatCount         = 0;
    return textureDescriptor;
}

angle::Result ImageHelper::stageTextureUpload(ContextWgpu *contextWgpu,
                                              const webgpu::Format &webgpuFormat,
                                              GLenum type,
                                              const gl::Extents &glExtents,
                                              GLuint inputRowPitch,
                                              GLuint inputDepthPitch,
                                              uint32_t outputRowPitch,
                                              uint32_t outputDepthPitch,
                                              uint32_t allocationSize,
                                              const gl::ImageIndex &index,
                                              const uint8_t *pixels)
{
    if (pixels == nullptr)
    {
        return angle::Result::Continue;
    }
    wgpu::Device device = contextWgpu->getDevice();
    wgpu::Queue queue   = contextWgpu->getQueue();
    gl::LevelIndex levelGL(index.getLevelIndex());
    BufferHelper bufferHelper;
    wgpu::BufferUsage usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    ANGLE_TRY(bufferHelper.initBuffer(device, allocationSize, usage, MapAtCreation::Yes));
    LoadImageFunctionInfo loadFunctionInfo = webgpuFormat.getTextureLoadFunction(type);
    uint8_t *data                          = bufferHelper.getMapWritePointer(0, allocationSize);
    loadFunctionInfo.loadFunction(contextWgpu->getImageLoadContext(), glExtents.width,
                                  glExtents.height, glExtents.depth, pixels, inputRowPitch,
                                  inputDepthPitch, data, outputRowPitch, outputDepthPitch);
    ANGLE_TRY(bufferHelper.unmap());

    wgpu::TextureDataLayout textureDataLayout = {};
    textureDataLayout.bytesPerRow             = outputRowPitch;
    textureDataLayout.rowsPerImage            = outputDepthPitch;
    wgpu::ImageCopyBuffer imageCopyBuffer;
    imageCopyBuffer.layout = textureDataLayout;
    imageCopyBuffer.buffer = bufferHelper.getBuffer();
    SubresourceUpdate subresourceUpdate(UpdateSource::Texture, levelGL, imageCopyBuffer);
    mSubresourceQueue.emplace_back(std::move(subresourceUpdate));
    return angle::Result::Continue;
}

void ImageHelper::stageClear(gl::LevelIndex targetLevel, ClearValues clearValues)
{
    SubresourceUpdate subresourceUpdate(UpdateSource::Clear, targetLevel, clearValues);
    mSubresourceQueue.emplace_back(std::move(subresourceUpdate));
}

void ImageHelper::removeStagedUpdates(gl::LevelIndex levelToRemove)
{
    std::erase_if(mSubresourceQueue, [levelToRemove](const SubresourceUpdate &subUpdate) {
        return subUpdate.updateSource == UpdateSource::Texture &&
               subUpdate.targetLevel == levelToRemove;
    });
}

void ImageHelper::resetImage()
{
    mTexture.Destroy();
    mTextureDescriptor   = {};
    mInitialized         = false;
    mFirstAllocatedLevel = gl::LevelIndex(0);
}
// static
angle::Result ImageHelper::getReadPixelsParams(rx::ContextWgpu *contextWgpu,
                                               const gl::PixelPackState &packState,
                                               gl::Buffer *packBuffer,
                                               GLenum format,
                                               GLenum type,
                                               const gl::Rectangle &area,
                                               const gl::Rectangle &clippedArea,
                                               rx::PackPixelsParams *paramsOut,
                                               GLuint *skipBytesOut)
{
    const gl::InternalFormat &sizedFormatInfo = gl::GetInternalFormatInfo(format, type);

    GLuint outputPitch = 0;
    ANGLE_CHECK_GL_MATH(contextWgpu,
                        sizedFormatInfo.computeRowPitch(type, area.width, packState.alignment,
                                                        packState.rowLength, &outputPitch));
    ANGLE_CHECK_GL_MATH(contextWgpu, sizedFormatInfo.computeSkipBytes(
                                         type, outputPitch, 0, packState, false, skipBytesOut));

    ANGLE_TRY(GetPackPixelsParams(sizedFormatInfo, outputPitch, packState, packBuffer, area,
                                  clippedArea, paramsOut, skipBytesOut));
    return angle::Result::Continue;
}

angle::Result ImageHelper::readPixels(rx::ContextWgpu *contextWgpu,
                                      const gl::Rectangle &area,
                                      const rx::PackPixelsParams &packPixelsParams,
                                      void *pixels)
{
    wgpu::Device device          = contextWgpu->getDisplay()->getDevice();
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::Queue queue            = contextWgpu->getDisplay()->getQueue();

    const angle::Format &actualFormat = angle::Format::Get(mActualFormatID);
    uint32_t textureBytesPerRow =
        roundUp(actualFormat.pixelBytes * area.width, kCopyBufferAlignment);
    wgpu::TextureDataLayout textureDataLayout;
    textureDataLayout.bytesPerRow  = textureBytesPerRow;
    textureDataLayout.rowsPerImage = area.height;

    size_t allocationSize = textureBytesPerRow * area.height;

    BufferHelper bufferHelper;
    ANGLE_TRY(bufferHelper.initBuffer(device, allocationSize,
                                      wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                                      MapAtCreation::No));
    wgpu::ImageCopyBuffer copyBuffer;
    copyBuffer.buffer = bufferHelper.getBuffer();
    copyBuffer.layout = textureDataLayout;

    wgpu::ImageCopyTexture copyTexture;
    wgpu::Origin3D textureOrigin;
    textureOrigin.x      = area.x;
    textureOrigin.y      = area.y;
    copyTexture.origin   = textureOrigin;
    copyTexture.texture  = mTexture;
    copyTexture.mipLevel = toWgpuLevel(mFirstAllocatedLevel).get();

    wgpu::Extent3D copySize;
    copySize.width  = area.width;
    copySize.height = area.height;
    encoder.CopyTextureToBuffer(&copyTexture, &copyBuffer, &copySize);

    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);
    encoder = nullptr;

    ANGLE_TRY(bufferHelper.mapImmediate(contextWgpu, wgpu::MapMode::Read, 0, allocationSize));
    const uint8_t *readPixelBuffer = bufferHelper.getMapReadPointer(0, allocationSize);
    PackPixels(packPixelsParams, actualFormat, textureBytesPerRow, readPixelBuffer,
               static_cast<uint8_t *>(pixels));
    return angle::Result::Continue;
}

angle::Result ImageHelper::createTextureView(gl::LevelIndex targetLevel,
                                             uint32_t layerIndex,
                                             wgpu::TextureView &textureViewOut)
{
    if (!isTextureLevelInAllocatedImage(targetLevel))
    {
        return angle::Result::Stop;
    }
    wgpu::TextureViewDescriptor textureViewDesc;
    textureViewDesc.aspect          = wgpu::TextureAspect::All;
    textureViewDesc.baseArrayLayer  = layerIndex;
    textureViewDesc.arrayLayerCount = 1;
    textureViewDesc.baseMipLevel    = toWgpuLevel(targetLevel).get();
    textureViewDesc.mipLevelCount   = 1;
    switch (mTextureDescriptor.dimension)
    {
        case wgpu::TextureDimension::Undefined:
            textureViewDesc.dimension = wgpu::TextureViewDimension::Undefined;
            break;
        case wgpu::TextureDimension::e1D:
            textureViewDesc.dimension = wgpu::TextureViewDimension::e1D;
            break;
        case wgpu::TextureDimension::e2D:
            textureViewDesc.dimension = wgpu::TextureViewDimension::e2D;
            break;
        case wgpu::TextureDimension::e3D:
            textureViewDesc.dimension = wgpu::TextureViewDimension::e3D;
            break;
    }
    textureViewDesc.format = mTextureDescriptor.format;
    textureViewOut         = mTexture.CreateView(&textureViewDesc);
    return angle::Result::Continue;
}

gl::LevelIndex ImageHelper::getLastAllocatedLevel()
{
    return mFirstAllocatedLevel + mTextureDescriptor.mipLevelCount - 1;
}

LevelIndex ImageHelper::toWgpuLevel(gl::LevelIndex levelIndexGl) const
{
    return gl_wgpu::getLevelIndex(levelIndexGl, mFirstAllocatedLevel);
}

gl::LevelIndex ImageHelper::toGlLevel(LevelIndex levelIndexWgpu) const
{
    return wgpu_gl::getLevelIndex(levelIndexWgpu, mFirstAllocatedLevel);
}

bool ImageHelper::isTextureLevelInAllocatedImage(gl::LevelIndex textureLevel)
{
    if (!mInitialized || textureLevel < mFirstAllocatedLevel)
    {
        return false;
    }
    LevelIndex wgpuTextureLevel = toWgpuLevel(textureLevel);
    return wgpuTextureLevel < LevelIndex(mTextureDescriptor.mipLevelCount);
}

BufferHelper::BufferHelper() {}

BufferHelper::~BufferHelper() {}

void BufferHelper::reset()
{
    mBuffer = nullptr;
    mMappedState.reset();
}

angle::Result BufferHelper::initBuffer(wgpu::Device device,
                                       size_t size,
                                       wgpu::BufferUsage usage,
                                       MapAtCreation mappedAtCreation)
{
    wgpu::BufferDescriptor descriptor;
    descriptor.size             = roundUp(size, kBufferSizeAlignment);
    descriptor.usage            = usage;
    descriptor.mappedAtCreation = mappedAtCreation == MapAtCreation::Yes;

    mBuffer = device.CreateBuffer(&descriptor);

    if (mappedAtCreation == MapAtCreation::Yes)
    {
        mMappedState = {wgpu::MapMode::Read | wgpu::MapMode::Write, 0, size};
    }
    else
    {
        mMappedState.reset();
    }

    mRequestedSize = size;

    return angle::Result::Continue;
}

angle::Result BufferHelper::mapImmediate(ContextWgpu *context,
                                         wgpu::MapMode mode,
                                         size_t offset,
                                         size_t size)
{
    ASSERT(!mMappedState.has_value());

    WGPUBufferMapAsyncStatus mapResult = WGPUBufferMapAsyncStatus_Unknown;

    wgpu::BufferMapCallbackInfo callbackInfo;
    callbackInfo.mode     = wgpu::CallbackMode::WaitAnyOnly;
    callbackInfo.callback = [](WGPUBufferMapAsyncStatus status, void *userdata) {
        *static_cast<WGPUBufferMapAsyncStatus *>(userdata) = status;
    };
    callbackInfo.userdata = &mapResult;

    wgpu::FutureWaitInfo waitInfo;
    waitInfo.future = mBuffer.MapAsync(mode, GetSafeBufferMapOffset(offset),
                                       GetSafeBufferMapSize(offset, size), callbackInfo);

    wgpu::Instance instance = context->getDisplay()->getInstance();
    ANGLE_WGPU_TRY(context, instance.WaitAny(1, &waitInfo, -1));
    ANGLE_WGPU_TRY(context, mapResult);

    ASSERT(waitInfo.completed);

    mMappedState = {mode, offset, size};

    return angle::Result::Continue;
}

angle::Result BufferHelper::unmap()
{
    ASSERT(mMappedState.has_value());
    mBuffer.Unmap();
    mMappedState.reset();
    return angle::Result::Continue;
}

uint8_t *BufferHelper::getMapWritePointer(size_t offset, size_t size) const
{
    ASSERT(mBuffer.GetMapState() == wgpu::BufferMapState::Mapped);
    ASSERT(mMappedState.has_value());
    ASSERT(mMappedState->offset <= offset);
    ASSERT(mMappedState->offset + mMappedState->size >= offset + size);

    void *mapPtr =
        mBuffer.GetMappedRange(GetSafeBufferMapOffset(offset), GetSafeBufferMapSize(offset, size));
    ASSERT(mapPtr);

    return AdjustMapPointerForOffset(static_cast<uint8_t *>(mapPtr), offset);
}

const uint8_t *BufferHelper::getMapReadPointer(size_t offset, size_t size) const
{
    ASSERT(mBuffer.GetMapState() == wgpu::BufferMapState::Mapped);
    ASSERT(mMappedState.has_value());
    ASSERT(mMappedState->offset <= offset);
    ASSERT(mMappedState->offset + mMappedState->size >= offset + size);

    // GetConstMappedRange is used for reads whereas GetMappedRange is only used for writes.
    const void *mapPtr = mBuffer.GetConstMappedRange(GetSafeBufferMapOffset(offset),
                                                     GetSafeBufferMapSize(offset, size));
    ASSERT(mapPtr);

    return AdjustMapPointerForOffset(static_cast<const uint8_t *>(mapPtr), offset);
}

const std::optional<BufferMapState> &BufferHelper::getMappedState() const
{
    return mMappedState;
}

bool BufferHelper::canMapForRead() const
{
    return (mMappedState.has_value() && (mMappedState->mode & wgpu::MapMode::Read)) ||
           (mBuffer && (mBuffer.GetUsage() & wgpu::BufferUsage::MapRead));
}

bool BufferHelper::canMapForWrite() const
{
    return (mMappedState.has_value() && (mMappedState->mode & wgpu::MapMode::Write)) ||
           (mBuffer && (mBuffer.GetUsage() & wgpu::BufferUsage::MapWrite));
}

wgpu::Buffer &BufferHelper::getBuffer()
{
    return mBuffer;
}

uint64_t BufferHelper::requestedSize() const
{
    return mRequestedSize;
}

uint64_t BufferHelper::actualSize() const
{
    return mBuffer ? mBuffer.GetSize() : 0;
}

}  // namespace webgpu
}  // namespace rx
