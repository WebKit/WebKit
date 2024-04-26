//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureWgpu.cpp:
//    Implements the class methods for TextureWgpu.
//

#include "libANGLE/renderer/wgpu/TextureWgpu.h"

#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/RenderTargetWgpu.h"

namespace rx
{

namespace
{

void GetRenderTargetLayerCountAndIndex(webgpu::ImageHelper *image,
                                       const gl::ImageIndex &index,
                                       GLuint *layerIndex,
                                       GLuint *layerCount,
                                       GLuint *imageLayerCount)
{
    *layerIndex = index.hasLayer() ? index.getLayerIndex() : 0;
    *layerCount = index.getLayerCount();

    switch (index.getType())
    {
        case gl::TextureType::_2D:
        case gl::TextureType::_2DMultisample:
        case gl::TextureType::External:
            ASSERT(*layerIndex == 0 &&
                   (*layerCount == 1 ||
                    *layerCount == static_cast<GLuint>(gl::ImageIndex::kEntireLevel)));
            *imageLayerCount = 1;
            break;

        case gl::TextureType::CubeMap:
            ASSERT(!index.hasLayer() ||
                   *layerIndex == static_cast<GLuint>(index.cubeMapFaceIndex()));
            *imageLayerCount = gl::kCubeFaceCount;
            break;

        case gl::TextureType::_3D:
        {
            gl::LevelIndex levelGL(index.getLevelIndex());
            *imageLayerCount = image->getTextureDescriptor().size.depthOrArrayLayers;
            break;
        }

        case gl::TextureType::_2DArray:
        case gl::TextureType::_2DMultisampleArray:
        case gl::TextureType::CubeMapArray:
            // NOTE: Not yet supported, should set *imageLayerCount.
            UNIMPLEMENTED();
            break;

        default:
            UNREACHABLE();
    }

    if (*layerCount == static_cast<GLuint>(gl::ImageIndex::kEntireLevel))
    {
        ASSERT(*layerIndex == 0);
        *layerCount = *imageLayerCount;
    }
}

}  // namespace

TextureWgpu::TextureWgpu(const gl::TextureState &state) : TextureImpl(state) {}

TextureWgpu::~TextureWgpu() {}

angle::Result TextureWgpu::setImage(const gl::Context *context,
                                    const gl::ImageIndex &index,
                                    GLenum internalFormat,
                                    const gl::Extents &size,
                                    GLenum format,
                                    GLenum type,
                                    const gl::PixelUnpackState &unpack,
                                    gl::Buffer *unpackBuffer,
                                    const uint8_t *pixels)
{
    // TODO(liza): Upload texture data.
    UNIMPLEMENTED();
    return setImageImpl(context, index, size);
}

angle::Result TextureWgpu::setSubImage(const gl::Context *context,
                                       const gl::ImageIndex &index,
                                       const gl::Box &area,
                                       GLenum format,
                                       GLenum type,
                                       const gl::PixelUnpackState &unpack,
                                       gl::Buffer *unpackBuffer,
                                       const uint8_t *pixels)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::setCompressedImage(const gl::Context *context,
                                              const gl::ImageIndex &index,
                                              GLenum internalFormat,
                                              const gl::Extents &size,
                                              const gl::PixelUnpackState &unpack,
                                              size_t imageSize,
                                              const uint8_t *pixels)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::setCompressedSubImage(const gl::Context *context,
                                                 const gl::ImageIndex &index,
                                                 const gl::Box &area,
                                                 GLenum format,
                                                 const gl::PixelUnpackState &unpack,
                                                 size_t imageSize,
                                                 const uint8_t *pixels)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::copyImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     const gl::Rectangle &sourceArea,
                                     GLenum internalFormat,
                                     gl::Framebuffer *source)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::copySubImage(const gl::Context *context,
                                        const gl::ImageIndex &index,
                                        const gl::Offset &destOffset,
                                        const gl::Rectangle &sourceArea,
                                        gl::Framebuffer *source)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::copyTexture(const gl::Context *context,
                                       const gl::ImageIndex &index,
                                       GLenum internalFormat,
                                       GLenum type,
                                       GLint sourceLevel,
                                       bool unpackFlipY,
                                       bool unpackPremultiplyAlpha,
                                       bool unpackUnmultiplyAlpha,
                                       const gl::Texture *source)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::copySubTexture(const gl::Context *context,
                                          const gl::ImageIndex &index,
                                          const gl::Offset &destOffset,
                                          GLint sourceLevel,
                                          const gl::Box &sourceBox,
                                          bool unpackFlipY,
                                          bool unpackPremultiplyAlpha,
                                          bool unpackUnmultiplyAlpha,
                                          const gl::Texture *source)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::copyRenderbufferSubData(const gl::Context *context,
                                                   const gl::Renderbuffer *srcBuffer,
                                                   GLint srcLevel,
                                                   GLint srcX,
                                                   GLint srcY,
                                                   GLint srcZ,
                                                   GLint dstLevel,
                                                   GLint dstX,
                                                   GLint dstY,
                                                   GLint dstZ,
                                                   GLsizei srcWidth,
                                                   GLsizei srcHeight,
                                                   GLsizei srcDepth)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::copyTextureSubData(const gl::Context *context,
                                              const gl::Texture *srcTexture,
                                              GLint srcLevel,
                                              GLint srcX,
                                              GLint srcY,
                                              GLint srcZ,
                                              GLint dstLevel,
                                              GLint dstX,
                                              GLint dstY,
                                              GLint dstZ,
                                              GLsizei srcWidth,
                                              GLsizei srcHeight,
                                              GLsizei srcDepth)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::copyCompressedTexture(const gl::Context *context,
                                                 const gl::Texture *source)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::setStorage(const gl::Context *context,
                                      gl::TextureType type,
                                      size_t levels,
                                      GLenum internalFormat,
                                      const gl::Extents &size)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::setStorageExternalMemory(const gl::Context *context,
                                                    gl::TextureType type,
                                                    size_t levels,
                                                    GLenum internalFormat,
                                                    const gl::Extents &size,
                                                    gl::MemoryObject *memoryObject,
                                                    GLuint64 offset,
                                                    GLbitfield createFlags,
                                                    GLbitfield usageFlags,
                                                    const void *imageCreateInfoPNext)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::setEGLImageTarget(const gl::Context *context,
                                             gl::TextureType type,
                                             egl::Image *image)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::setImageExternal(const gl::Context *context,
                                            gl::TextureType type,
                                            egl::Stream *stream,
                                            const egl::Stream::GLTextureDescription &desc)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::generateMipmap(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::releaseTexImage(const gl::Context *context)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::syncState(const gl::Context *context,
                                     const gl::Texture::DirtyBits &dirtyBits,
                                     gl::Command source)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::setStorageMultisample(const gl::Context *context,
                                                 gl::TextureType type,
                                                 GLsizei samples,
                                                 GLint internalformat,
                                                 const gl::Extents &size,
                                                 bool fixedSampleLocations)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::initializeContents(const gl::Context *context,
                                              GLenum binding,
                                              const gl::ImageIndex &imageIndex)
{
    return angle::Result::Continue;
}

angle::Result TextureWgpu::getAttachmentRenderTarget(const gl::Context *context,
                                                     GLenum binding,
                                                     const gl::ImageIndex &imageIndex,
                                                     GLsizei samples,
                                                     FramebufferAttachmentRenderTarget **rtOut)
{
    GLuint layerIndex = 0, layerCount = 0, imageLayerCount = 0;
    GetRenderTargetLayerCountAndIndex(mImage, imageIndex, &layerIndex, &layerCount,
                                      &imageLayerCount);

    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);

    // NOTE: Multisampling not yet supported
    ASSERT(samples <= 1);
    const gl::RenderToTextureImageIndex renderToTextureIndex =
        gl::RenderToTextureImageIndex::Default;

    if (layerCount == 1)
    {
        initSingleLayerRenderTargets(contextWgpu, imageLayerCount,
                                     gl::LevelIndex(imageIndex.getLevelIndex()),
                                     renderToTextureIndex);

        std::vector<std::vector<RenderTargetWgpu>> &levelRenderTargets =
            mSingleLayerRenderTargets[renderToTextureIndex];
        ASSERT(imageIndex.getLevelIndex() < static_cast<int32_t>(levelRenderTargets.size()));

        std::vector<RenderTargetWgpu> &layerRenderTargets =
            levelRenderTargets[imageIndex.getLevelIndex()];
        ASSERT(imageIndex.getLayerIndex() < static_cast<int32_t>(layerRenderTargets.size()));

        *rtOut = &layerRenderTargets[layerIndex];
    }
    else
    {
        // Not yet supported.
        UNIMPLEMENTED();
    }

    return angle::Result::Continue;
}

angle::Result TextureWgpu::setImageImpl(const gl::Context *context,
                                        const gl::ImageIndex &index,
                                        const gl::Extents &size)
{
    return redefineLevel(context, index, size);
}

angle::Result TextureWgpu::redefineLevel(const gl::Context *context,
                                         const gl::ImageIndex &index,
                                         const gl::Extents &size)
{
    bool levelWithinRange = false;
    gl::LevelIndex levelIndexGL(index.getLevelIndex());
    if (mImage && levelIndexGL >= mImage->getFirstAllocatedLevel() &&
        levelIndexGL <
            (mImage->getFirstAllocatedLevel() + mImage->getTextureDescriptor().mipLevelCount))
    {
        levelWithinRange      = true;
        bool dimensionChanged = mImage->getTextureDescriptor().dimension !=
                                gl_wgpu::getWgpuTextureDimension(index.getType());
        if (dimensionChanged || size != wgpu_gl::getExtents(mImage->getTextureDescriptor().size))
        {
            mImage = nullptr;
        }
    }

    if (size.empty())
    {
        return angle::Result::Continue;
    }

    wgpu::Device device = webgpu::GetDevice(context);

    if (mImage == nullptr && !levelWithinRange)
    {
        mImage                          = new webgpu::ImageHelper();
        webgpu::TextureInfo textureInfo = mImage->getWgpuTextureInfo(index);
        return mImage->initImage(device, textureInfo.usage, textureInfo.dimension,
                                 gl_wgpu::getExtent3D(size), wgpu::TextureFormat::RGBA8Sint,
                                 textureInfo.mipLevelCount, 1, 0);
    }
    return angle::Result::Continue;
}

void TextureWgpu::initSingleLayerRenderTargets(ContextWgpu *contextWgpu,
                                               GLuint layerCount,
                                               gl::LevelIndex levelIndex,
                                               gl::RenderToTextureImageIndex renderToTextureIndex)
{
    std::vector<std::vector<RenderTargetWgpu>> &allLevelsRenderTargets =
        mSingleLayerRenderTargets[renderToTextureIndex];

    if (allLevelsRenderTargets.size() <= static_cast<uint32_t>(levelIndex.get()))
    {
        allLevelsRenderTargets.resize(levelIndex.get() + 1);
    }

    std::vector<RenderTargetWgpu> &renderTargets = allLevelsRenderTargets[levelIndex.get()];

    // Lazy init. Check if already initialized.
    if (!renderTargets.empty())
    {
        return;
    }

    // There are |layerCount| render targets, one for each layer
    renderTargets.resize(layerCount);

    for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        wgpu::TextureViewDescriptor textureViewDesc;
        textureViewDesc.aspect          = wgpu::TextureAspect::All;
        textureViewDesc.baseArrayLayer  = layerIndex;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.baseMipLevel    = mImage->toWgpuLevel(levelIndex).get();
        textureViewDesc.mipLevelCount   = 1;
        switch (mImage->getTextureDescriptor().dimension)
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
        textureViewDesc.format        = mImage->getTextureDescriptor().format;
        wgpu::TextureView textureView = mImage->getTexture().CreateView(&textureViewDesc);

        renderTargets[layerIndex].set(textureView, mImage->toWgpuLevel(levelIndex), layerIndex,
                                      mImage->toWgpuTextureFormat());
    }
}

}  // namespace rx
