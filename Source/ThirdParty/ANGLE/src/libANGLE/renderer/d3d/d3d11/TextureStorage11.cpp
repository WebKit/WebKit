//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage11.cpp: Implements the abstract rx::TextureStorage11 class and its concrete derived
// classes TextureStorage11_2D and TextureStorage11_Cube, which act as the interface to the D3D11
// texture.

#include "libANGLE/renderer/d3d/d3d11/TextureStorage11.h"

#include <tuple>

#include "common/MemoryBuffer.h"
#include "common/utilities.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/renderer/d3d/d3d11/Blit11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/d3d11/Image11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/StreamProducerNV12.h"
#include "libANGLE/renderer/d3d/d3d11/SwapChain11.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"
#include "libANGLE/renderer/d3d/EGLImageD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"

namespace rx
{

namespace
{

void InvalidateRenderTarget(const gl::Context *context, RenderTarget11 *renderTarget)
{
    if (renderTarget)
    {
        renderTarget->signalDirty(context);
    }
}

RenderTarget11 *GetRenderTarget(std::unique_ptr<RenderTarget11> *pointer)
{
    return pointer->get();
}

template <typename KeyT>
RenderTarget11 *GetRenderTarget(std::pair<KeyT, std::unique_ptr<RenderTarget11>> *pair)
{
    return pair->second.get();
}

template <typename T>
void InvalidateRenderTargetContainer(const gl::Context *context, T *renderTargetContainer)
{
    for (auto &rt : *renderTargetContainer)
    {
        InvalidateRenderTarget(context, GetRenderTarget(&rt));
    }
}

}  // anonymous namespace

TextureStorage11::SRVKey::SRVKey(int baseLevel, int mipLevels, bool swizzle, bool dropStencil)
    : baseLevel(baseLevel), mipLevels(mipLevels), swizzle(swizzle), dropStencil(dropStencil)
{
}

bool TextureStorage11::SRVKey::operator<(const SRVKey &rhs) const
{
    return std::tie(baseLevel, mipLevels, swizzle, dropStencil) <
           std::tie(rhs.baseLevel, rhs.mipLevels, rhs.swizzle, rhs.dropStencil);
}

TextureStorage11::TextureStorage11(Renderer11 *renderer,
                                   UINT bindFlags,
                                   UINT miscFlags,
                                   GLenum internalFormat)
    : mRenderer(renderer),
      mTopLevel(0),
      mMipLevels(0),
      mFormatInfo(d3d11::Format::Get(internalFormat, mRenderer->getRenderer11DeviceCaps())),
      mTextureWidth(0),
      mTextureHeight(0),
      mTextureDepth(0),
      mDropStencilTexture(),
      mBindFlags(bindFlags),
      mMiscFlags(miscFlags)
{
}

TextureStorage11::~TextureStorage11()
{
    mSrvCache.clear();
}

DWORD TextureStorage11::GetTextureBindFlags(GLenum internalFormat,
                                            const Renderer11DeviceCaps &renderer11DeviceCaps,
                                            bool renderTarget)
{
    UINT bindFlags = 0;

    const d3d11::Format &formatInfo = d3d11::Format::Get(internalFormat, renderer11DeviceCaps);
    if (formatInfo.srvFormat != DXGI_FORMAT_UNKNOWN)
    {
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (formatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN)
    {
        bindFlags |= D3D11_BIND_DEPTH_STENCIL;
    }
    if (formatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN && renderTarget)
    {
        bindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    return bindFlags;
}

DWORD TextureStorage11::GetTextureMiscFlags(GLenum internalFormat,
                                            const Renderer11DeviceCaps &renderer11DeviceCaps,
                                            bool renderTarget,
                                            int levels)
{
    UINT miscFlags = 0;

    const d3d11::Format &formatInfo = d3d11::Format::Get(internalFormat, renderer11DeviceCaps);
    if (renderTarget && levels > 1)
    {
        if (d3d11::SupportsMipGen(formatInfo.texFormat, renderer11DeviceCaps.featureLevel))
        {
            miscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        }
    }

    return miscFlags;
}

UINT TextureStorage11::getBindFlags() const
{
    return mBindFlags;
}

UINT TextureStorage11::getMiscFlags() const
{
    return mMiscFlags;
}

int TextureStorage11::getTopLevel() const
{
    // Applying top level is meant to be encapsulated inside TextureStorage11.
    UNREACHABLE();
    return mTopLevel;
}

bool TextureStorage11::isRenderTarget() const
{
    return (mBindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL)) != 0;
}

bool TextureStorage11::isManaged() const
{
    return false;
}

bool TextureStorage11::supportsNativeMipmapFunction() const
{
    return (mMiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS) != 0;
}

int TextureStorage11::getLevelCount() const
{
    return mMipLevels - mTopLevel;
}

int TextureStorage11::getLevelWidth(int mipLevel) const
{
    return std::max(static_cast<int>(mTextureWidth) >> mipLevel, 1);
}

int TextureStorage11::getLevelHeight(int mipLevel) const
{
    return std::max(static_cast<int>(mTextureHeight) >> mipLevel, 1);
}

int TextureStorage11::getLevelDepth(int mipLevel) const
{
    return std::max(static_cast<int>(mTextureDepth) >> mipLevel, 1);
}

gl::Error TextureStorage11::getMippedResource(const gl::Context *context,
                                              const TextureHelper11 **outResource)
{
    return getResource(context, outResource);
}

UINT TextureStorage11::getSubresourceIndex(const gl::ImageIndex &index) const
{
    UINT mipSlice    = static_cast<UINT>(index.mipIndex + mTopLevel);
    UINT arraySlice  = static_cast<UINT>(index.hasLayer() ? index.layerIndex : 0);
    UINT subresource = D3D11CalcSubresource(mipSlice, arraySlice, mMipLevels);
    ASSERT(subresource != std::numeric_limits<UINT>::max());
    return subresource;
}

gl::Error TextureStorage11::getSRV(const gl::Context *context,
                                   const gl::TextureState &textureState,
                                   const d3d11::SharedSRV **outSRV)
{
    // Make sure to add the level offset for our tiny compressed texture workaround
    const GLuint effectiveBaseLevel = textureState.getEffectiveBaseLevel();
    bool swizzleRequired            = textureState.swizzleRequired();
    bool mipmapping                 = gl::IsMipmapFiltered(textureState.getSamplerState());
    unsigned int mipLevels =
        mipmapping ? (textureState.getEffectiveMaxLevel() - effectiveBaseLevel + 1) : 1;

    // Make sure there's 'mipLevels' mipmap levels below the base level (offset by the top level,
    // which corresponds to GL level 0)
    mipLevels = std::min(mipLevels, mMipLevels - mTopLevel - effectiveBaseLevel);

    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        ASSERT(!swizzleRequired);
        ASSERT(mipLevels == 1 || mipLevels == mMipLevels);
    }

    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        // We must ensure that the level zero texture is in sync with mipped texture.
        ANGLE_TRY(useLevelZeroWorkaroundTexture(context, mipLevels == 1));
    }

    if (swizzleRequired)
    {
        verifySwizzleExists(textureState.getSwizzleState());
    }

    // We drop the stencil when sampling from the SRV if three conditions hold:
    // 1. the drop stencil workaround is enabled.
    bool workaround = mRenderer->getWorkarounds().emulateTinyStencilTextures;
    // 2. this is a stencil texture.
    bool hasStencil = (mFormatInfo.format().stencilBits > 0);
    // 3. the texture has a 1x1 or 2x2 mip.
    int effectiveTopLevel = effectiveBaseLevel + mipLevels - 1;
    bool hasSmallMips =
        (getLevelWidth(effectiveTopLevel) <= 2 || getLevelHeight(effectiveTopLevel) <= 2);

    bool useDropStencil = (workaround && hasStencil && hasSmallMips);
    SRVKey key(effectiveBaseLevel, mipLevels, swizzleRequired, useDropStencil);
    if (useDropStencil)
    {
        // Ensure drop texture gets created.
        DropStencil result = DropStencil::CREATED;
        ANGLE_TRY_RESULT(ensureDropStencilTexture(context), result);

        // Clear the SRV cache if necessary.
        // TODO(jmadill): Re-use find query result.
        auto srvEntry = mSrvCache.find(key);
        if (result == DropStencil::CREATED && srvEntry != mSrvCache.end())
        {
            mSrvCache.erase(key);
        }
    }

    ANGLE_TRY(getCachedOrCreateSRV(context, key, outSRV));

    return gl::NoError();
}

gl::Error TextureStorage11::getCachedOrCreateSRV(const gl::Context *context,
                                                 const SRVKey &key,
                                                 const d3d11::SharedSRV **outSRV)
{
    auto iter = mSrvCache.find(key);
    if (iter != mSrvCache.end())
    {
        *outSRV = &iter->second;
        return gl::NoError();
    }

    const TextureHelper11 *texture = nullptr;
    DXGI_FORMAT format      = DXGI_FORMAT_UNKNOWN;

    if (key.swizzle)
    {
        const auto &swizzleFormat =
            mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps());
        ASSERT(!key.dropStencil || swizzleFormat.format().stencilBits == 0);
        ANGLE_TRY(getSwizzleTexture(&texture));
        format = swizzleFormat.srvFormat;
    }
    else if (key.dropStencil)
    {
        ASSERT(mDropStencilTexture.valid());
        texture = &mDropStencilTexture;
        format  = DXGI_FORMAT_R32_FLOAT;
    }
    else
    {
        ANGLE_TRY(getResource(context, &texture));
        format = mFormatInfo.srvFormat;
    }

    d3d11::SharedSRV srv;

    ANGLE_TRY(createSRV(context, key.baseLevel, key.mipLevels, format, *texture, &srv));

    const auto &insertIt = mSrvCache.insert(std::make_pair(key, std::move(srv)));
    *outSRV              = &insertIt.first->second;

    return gl::NoError();
}

gl::Error TextureStorage11::getSRVLevel(const gl::Context *context,
                                        int mipLevel,
                                        bool blitSRV,
                                        const d3d11::SharedSRV **outSRV)
{
    ASSERT(mipLevel >= 0 && mipLevel < getLevelCount());

    auto &levelSRVs      = (blitSRV) ? mLevelBlitSRVs : mLevelSRVs;
    auto &otherLevelSRVs = (blitSRV) ? mLevelSRVs : mLevelBlitSRVs;

    if (!levelSRVs[mipLevel].valid())
    {
        // Only create a different SRV for blit if blit format is different from regular srv format
        if (otherLevelSRVs[mipLevel].valid() && mFormatInfo.srvFormat == mFormatInfo.blitSRVFormat)
        {
            levelSRVs[mipLevel] = otherLevelSRVs[mipLevel].makeCopy();
        }
        else
        {
            const TextureHelper11 *resource = nullptr;
            ANGLE_TRY(getResource(context, &resource));

            DXGI_FORMAT resourceFormat =
                blitSRV ? mFormatInfo.blitSRVFormat : mFormatInfo.srvFormat;
            ANGLE_TRY(
                createSRV(context, mipLevel, 1, resourceFormat, *resource, &levelSRVs[mipLevel]));
        }
    }

    *outSRV = &levelSRVs[mipLevel];

    return gl::NoError();
}

gl::Error TextureStorage11::getSRVLevels(const gl::Context *context,
                                         GLint baseLevel,
                                         GLint maxLevel,
                                         const d3d11::SharedSRV **outSRV)
{
    unsigned int mipLevels = maxLevel - baseLevel + 1;

    // Make sure there's 'mipLevels' mipmap levels below the base level (offset by the top level,
    // which corresponds to GL level 0)
    mipLevels = std::min(mipLevels, mMipLevels - mTopLevel - baseLevel);

    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        ASSERT(mipLevels == 1 || mipLevels == mMipLevels);
    }

    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        // We must ensure that the level zero texture is in sync with mipped texture.
        ANGLE_TRY(useLevelZeroWorkaroundTexture(context, mipLevels == 1));
    }

    // TODO(jmadill): Assert we don't need to drop stencil.

    SRVKey key(baseLevel, mipLevels, false, false);
    ANGLE_TRY(getCachedOrCreateSRV(context, key, outSRV));

    return gl::NoError();
}

const d3d11::Format &TextureStorage11::getFormatSet() const
{
    return mFormatInfo;
}

gl::Error TextureStorage11::generateSwizzles(const gl::Context *context,
                                             const gl::SwizzleState &swizzleTarget)
{
    for (int level = 0; level < getLevelCount(); level++)
    {
        // Check if the swizzle for this level is out of date
        if (mSwizzleCache[level] != swizzleTarget)
        {
            // Need to re-render the swizzle for this level
            const d3d11::SharedSRV *sourceSRV = nullptr;
            ANGLE_TRY(getSRVLevel(context, level, true, &sourceSRV));

            const d3d11::RenderTargetView *destRTV;
            ANGLE_TRY(getSwizzleRenderTarget(level, &destRTV));

            gl::Extents size(getLevelWidth(level), getLevelHeight(level), getLevelDepth(level));

            Blit11 *blitter = mRenderer->getBlitter();

            ANGLE_TRY(blitter->swizzleTexture(context, *sourceSRV, *destRTV, size, swizzleTarget));

            mSwizzleCache[level] = swizzleTarget;
        }
    }

    return gl::NoError();
}

void TextureStorage11::markLevelDirty(int mipLevel)
{
    if (mipLevel >= 0 && static_cast<size_t>(mipLevel) < mSwizzleCache.size())
    {
        // The default constructor of SwizzleState has GL_INVALID_INDEX for all channels which is
        // not a valid swizzle combination
        if (mSwizzleCache[mipLevel] != gl::SwizzleState())
        {
            // TODO(jmadill): Invalidate specific swizzle.
            mRenderer->getStateManager()->invalidateSwizzles();
            mSwizzleCache[mipLevel] = gl::SwizzleState();
        }
    }

    if (mDropStencilTexture.valid())
    {
        mDropStencilTexture.reset();
    }
}

void TextureStorage11::markDirty()
{
    for (size_t mipLevel = 0; mipLevel < mSwizzleCache.size(); ++mipLevel)
    {
        markLevelDirty(static_cast<int>(mipLevel));
    }
}

gl::Error TextureStorage11::updateSubresourceLevel(const gl::Context *context,
                                                   const TextureHelper11 &srcTexture,
                                                   unsigned int sourceSubresource,
                                                   const gl::ImageIndex &index,
                                                   const gl::Box &copyArea)
{
    ASSERT(srcTexture.valid());

    const GLint level = index.mipIndex;

    markLevelDirty(level);

    gl::Extents texSize(getLevelWidth(level), getLevelHeight(level), getLevelDepth(level));

    bool fullCopy = copyArea.x == 0 && copyArea.y == 0 && copyArea.z == 0 &&
                    copyArea.width == texSize.width && copyArea.height == texSize.height &&
                    copyArea.depth == texSize.depth;

    const TextureHelper11 *dstTexture = nullptr;

    // If the zero-LOD workaround is active and we want to update a level greater than zero, then we
    // should update the mipmapped texture, even if mapmaps are currently disabled.
    if (index.mipIndex > 0 && mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ANGLE_TRY(getMippedResource(context, &dstTexture));
    }
    else
    {
        ANGLE_TRY(getResource(context, &dstTexture));
    }

    unsigned int dstSubresource = getSubresourceIndex(index);

    ASSERT(dstTexture->valid());

    const d3d11::DXGIFormatSize &dxgiFormatSizeInfo =
        d3d11::GetDXGIFormatSizeInfo(mFormatInfo.texFormat);
    if (!fullCopy && mFormatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN)
    {
        // CopySubresourceRegion cannot copy partial depth stencils, use the blitter instead
        Blit11 *blitter        = mRenderer->getBlitter();
        return blitter->copyDepthStencil(srcTexture, sourceSubresource, copyArea, texSize,
                                         *dstTexture, dstSubresource, copyArea, texSize, nullptr);
    }

    D3D11_BOX srcBox;
    srcBox.left = copyArea.x;
    srcBox.top  = copyArea.y;
    srcBox.right =
        copyArea.x + roundUp(static_cast<UINT>(copyArea.width), dxgiFormatSizeInfo.blockWidth);
    srcBox.bottom =
        copyArea.y + roundUp(static_cast<UINT>(copyArea.height), dxgiFormatSizeInfo.blockHeight);
    srcBox.front = copyArea.z;
    srcBox.back  = copyArea.z + copyArea.depth;

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    deviceContext->CopySubresourceRegion(dstTexture->get(), dstSubresource, copyArea.x, copyArea.y,
                                         copyArea.z, srcTexture.get(), sourceSubresource,
                                         fullCopy ? nullptr : &srcBox);
    return gl::NoError();
}

gl::Error TextureStorage11::copySubresourceLevel(const gl::Context *context,
                                                 const TextureHelper11 &dstTexture,
                                                 unsigned int dstSubresource,
                                                 const gl::ImageIndex &index,
                                                 const gl::Box &region)
{
    ASSERT(dstTexture.valid());

    const TextureHelper11 *srcTexture = nullptr;

    // If the zero-LOD workaround is active and we want to update a level greater than zero, then we
    // should update the mipmapped texture, even if mapmaps are currently disabled.
    if (index.mipIndex > 0 && mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ANGLE_TRY(getMippedResource(context, &srcTexture));
    }
    else
    {
        ANGLE_TRY(getResource(context, &srcTexture));
    }

    ASSERT(srcTexture->valid());

    unsigned int srcSubresource = getSubresourceIndex(index);

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    // D3D11 can't perform partial CopySubresourceRegion on depth/stencil textures, so pSrcBox
    // should be nullptr.
    D3D11_BOX srcBox;
    D3D11_BOX *pSrcBox = nullptr;
    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        GLsizei width  = region.width;
        GLsizei height = region.height;
        d3d11::MakeValidSize(false, mFormatInfo.texFormat, &width, &height, nullptr);

        // Keep srcbox as nullptr if we're dealing with tiny mips of compressed textures.
        if (width == region.width && height == region.height)
        {
            // However, D3D10Level9 doesn't always perform CopySubresourceRegion correctly unless
            // the source box is specified. This is okay, since we don't perform
            // CopySubresourceRegion on depth/stencil textures on 9_3.
            ASSERT(mFormatInfo.dsvFormat == DXGI_FORMAT_UNKNOWN);
            srcBox.left   = region.x;
            srcBox.right  = region.x + region.width;
            srcBox.top    = region.y;
            srcBox.bottom = region.y + region.height;
            srcBox.front  = region.z;
            srcBox.back   = region.z + region.depth;
            pSrcBox       = &srcBox;
        }
    }

    deviceContext->CopySubresourceRegion(dstTexture.get(), dstSubresource, region.x, region.y,
                                         region.z, srcTexture->get(), srcSubresource, pSrcBox);

    return gl::NoError();
}

gl::Error TextureStorage11::generateMipmap(const gl::Context *context,
                                           const gl::ImageIndex &sourceIndex,
                                           const gl::ImageIndex &destIndex)
{
    ASSERT(sourceIndex.layerIndex == destIndex.layerIndex);

    markLevelDirty(destIndex.mipIndex);

    RenderTargetD3D *source = nullptr;
    ANGLE_TRY(getRenderTarget(context, sourceIndex, &source));

    RenderTargetD3D *dest = nullptr;
    ANGLE_TRY(getRenderTarget(context, destIndex, &dest));

    RenderTarget11 *rt11                   = GetAs<RenderTarget11>(source);
    const d3d11::SharedSRV &sourceSRV      = rt11->getBlitShaderResourceView();
    const d3d11::RenderTargetView &destRTV = rt11->getRenderTargetView();

    gl::Box sourceArea(0, 0, 0, source->getWidth(), source->getHeight(), source->getDepth());
    gl::Extents sourceSize(source->getWidth(), source->getHeight(), source->getDepth());

    gl::Box destArea(0, 0, 0, dest->getWidth(), dest->getHeight(), dest->getDepth());
    gl::Extents destSize(dest->getWidth(), dest->getHeight(), dest->getDepth());

    Blit11 *blitter = mRenderer->getBlitter();
    GLenum format   = gl::GetUnsizedFormat(source->getInternalFormat());
    return blitter->copyTexture(context, sourceSRV, sourceArea, sourceSize, format, destRTV,
                                destArea, destSize, nullptr, format, GL_LINEAR, false, false,
                                false);
}

void TextureStorage11::verifySwizzleExists(const gl::SwizzleState &swizzleState)
{
    for (unsigned int level = 0; level < mMipLevels; level++)
    {
        ASSERT(mSwizzleCache[level] == swizzleState);
    }
}

void TextureStorage11::clearSRVCache()
{
    markDirty();
    mSrvCache.clear();

    for (size_t level = 0; level < mLevelSRVs.size(); level++)
    {
        mLevelSRVs[level].reset();
        mLevelBlitSRVs[level].reset();
    }
}

gl::Error TextureStorage11::copyToStorage(const gl::Context *context, TextureStorage *destStorage)
{
    ASSERT(destStorage);

    const TextureHelper11 *sourceResouce = nullptr;
    ANGLE_TRY(getResource(context, &sourceResouce));

    TextureStorage11 *dest11     = GetAs<TextureStorage11>(destStorage);
    const TextureHelper11 *destResource = nullptr;
    ANGLE_TRY(dest11->getResource(context, &destResource));

    ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();
    immediateContext->CopyResource(destResource->get(), sourceResouce->get());

    dest11->markDirty();

    return gl::NoError();
}

gl::Error TextureStorage11::setData(const gl::Context *context,
                                    const gl::ImageIndex &index,
                                    ImageD3D *image,
                                    const gl::Box *destBox,
                                    GLenum type,
                                    const gl::PixelUnpackState &unpack,
                                    const uint8_t *pixelData)
{
    ASSERT(!image->isDirty());

    markLevelDirty(index.mipIndex);

    const TextureHelper11 *resource = nullptr;
    ANGLE_TRY(getResource(context, &resource));
    ASSERT(resource && resource->valid());

    UINT destSubresource = getSubresourceIndex(index);

    const gl::InternalFormat &internalFormatInfo =
        gl::GetInternalFormatInfo(image->getInternalFormat(), type);

    gl::Box levelBox(0, 0, 0, getLevelWidth(index.mipIndex), getLevelHeight(index.mipIndex),
                     getLevelDepth(index.mipIndex));
    bool fullUpdate = (destBox == nullptr || *destBox == levelBox);
    ASSERT(internalFormatInfo.depthBits == 0 || fullUpdate);

    // TODO(jmadill): Handle compressed formats
    // Compressed formats have different load syntax, so we'll have to handle them with slightly
    // different logic. Will implemnent this in a follow-up patch, and ensure we do not use SetData
    // with compressed formats in the calling logic.
    ASSERT(!internalFormatInfo.compressed);

    const int width    = destBox ? destBox->width : static_cast<int>(image->getWidth());
    const int height   = destBox ? destBox->height : static_cast<int>(image->getHeight());
    const int depth    = destBox ? destBox->depth : static_cast<int>(image->getDepth());
    GLuint srcRowPitch = 0;
    ANGLE_TRY_RESULT(
        internalFormatInfo.computeRowPitch(type, width, unpack.alignment, unpack.rowLength),
        srcRowPitch);
    GLuint srcDepthPitch = 0;
    ANGLE_TRY_RESULT(internalFormatInfo.computeDepthPitch(height, unpack.imageHeight, srcRowPitch),
                     srcDepthPitch);
    GLuint srcSkipBytes = 0;
    ANGLE_TRY_RESULT(
        internalFormatInfo.computeSkipBytes(srcRowPitch, srcDepthPitch, unpack, index.is3D()),
        srcSkipBytes);

    const d3d11::Format &d3d11Format =
        d3d11::Format::Get(image->getInternalFormat(), mRenderer->getRenderer11DeviceCaps());
    const d3d11::DXGIFormatSize &dxgiFormatInfo =
        d3d11::GetDXGIFormatSizeInfo(d3d11Format.texFormat);

    const size_t outputPixelSize = dxgiFormatInfo.pixelBytes;

    UINT bufferRowPitch   = static_cast<unsigned int>(outputPixelSize) * width;
    UINT bufferDepthPitch = bufferRowPitch * height;

    const size_t neededSize        = bufferDepthPitch * depth;
    angle::MemoryBuffer *conversionBuffer = nullptr;
    const uint8_t *data            = nullptr;

    LoadImageFunctionInfo loadFunctionInfo = d3d11Format.getLoadFunctions()(type);
    if (loadFunctionInfo.requiresConversion)
    {
        ANGLE_TRY(mRenderer->getScratchMemoryBuffer(neededSize, &conversionBuffer));
        loadFunctionInfo.loadFunction(width, height, depth, pixelData + srcSkipBytes, srcRowPitch,
                                      srcDepthPitch, conversionBuffer->data(), bufferRowPitch,
                                      bufferDepthPitch);
        data = conversionBuffer->data();
    }
    else
    {
        data             = pixelData + srcSkipBytes;
        bufferRowPitch   = srcRowPitch;
        bufferDepthPitch = srcDepthPitch;
    }

    ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();

    if (!fullUpdate)
    {
        ASSERT(destBox);

        D3D11_BOX destD3DBox;
        destD3DBox.left   = destBox->x;
        destD3DBox.right  = destBox->x + destBox->width;
        destD3DBox.top    = destBox->y;
        destD3DBox.bottom = destBox->y + destBox->height;
        destD3DBox.front  = destBox->z;
        destD3DBox.back   = destBox->z + destBox->depth;

        immediateContext->UpdateSubresource(resource->get(), destSubresource, &destD3DBox, data,
                                            bufferRowPitch, bufferDepthPitch);
    }
    else
    {
        immediateContext->UpdateSubresource(resource->get(), destSubresource, nullptr, data,
                                            bufferRowPitch, bufferDepthPitch);
    }

    return gl::NoError();
}

gl::ErrorOrResult<TextureStorage11::DropStencil> TextureStorage11::ensureDropStencilTexture(
    const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "Drop stencil texture not implemented.";
}

TextureStorage11_2D::TextureStorage11_2D(Renderer11 *renderer, SwapChain11 *swapchain)
    : TextureStorage11(renderer,
                       D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
                       0,
                       swapchain->getRenderTargetInternalFormat()),
      mTexture(swapchain->getOffscreenTexture()),
      mLevelZeroTexture(),
      mLevelZeroRenderTarget(nullptr),
      mUseLevelZeroTexture(false),
      mSwizzleTexture()
{
    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mAssociatedImages[i]     = nullptr;
        mRenderTarget[i]         = nullptr;
    }

    D3D11_TEXTURE2D_DESC texDesc;
    mTexture.getDesc(&texDesc);
    mMipLevels     = texDesc.MipLevels;
    mTextureWidth  = texDesc.Width;
    mTextureHeight = texDesc.Height;
    mTextureDepth  = 1;
    mHasKeyedMutex = (texDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0;
}

TextureStorage11_2D::TextureStorage11_2D(Renderer11 *renderer,
                                         GLenum internalformat,
                                         bool renderTarget,
                                         GLsizei width,
                                         GLsizei height,
                                         int levels,
                                         bool hintLevelZeroOnly)
    : TextureStorage11(
          renderer,
          GetTextureBindFlags(internalformat, renderer->getRenderer11DeviceCaps(), renderTarget),
          GetTextureMiscFlags(internalformat,
                              renderer->getRenderer11DeviceCaps(),
                              renderTarget,
                              levels),
          internalformat),
      mTexture(),
      mHasKeyedMutex(false),
      mLevelZeroTexture(),
      mLevelZeroRenderTarget(nullptr),
      mUseLevelZeroTexture(hintLevelZeroOnly && levels > 1),
      mSwizzleTexture()
{
    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mAssociatedImages[i]     = nullptr;
        mRenderTarget[i]         = nullptr;
    }

    d3d11::MakeValidSize(false, mFormatInfo.texFormat, &width, &height, &mTopLevel);
    mMipLevels     = mTopLevel + levels;
    mTextureWidth  = width;
    mTextureHeight = height;
    mTextureDepth  = 1;

    // The LevelZeroOnly hint should only be true if the zero max LOD workaround is active.
    ASSERT(!mUseLevelZeroTexture || mRenderer->getWorkarounds().zeroMaxLodWorkaround);
}

gl::Error TextureStorage11_2D::onDestroy(const gl::Context *context)
{
    for (unsigned i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        if (mAssociatedImages[i] != nullptr)
        {
            mAssociatedImages[i]->verifyAssociatedStorageValid(this);

            // We must let the Images recover their data before we delete it from the
            // TextureStorage.
            ANGLE_TRY(mAssociatedImages[i]->recoverFromAssociatedStorage(context));
        }
    }

    if (mHasKeyedMutex)
    {
        // If the keyed mutex is released that will unbind it and cause the state cache to become
        // desynchronized.
        mRenderer->getStateManager()->invalidateBoundViews();
    }

    // Invalidate RenderTargets.
    InvalidateRenderTargetContainer(context, &mRenderTarget);
    InvalidateRenderTarget(context, mLevelZeroRenderTarget.get());

    return gl::NoError();
}

TextureStorage11_2D::~TextureStorage11_2D()
{
}

gl::Error TextureStorage11_2D::copyToStorage(const gl::Context *context,
                                             TextureStorage *destStorage)
{
    ASSERT(destStorage);

    TextureStorage11_2D *dest11           = GetAs<TextureStorage11_2D>(destStorage);
    ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();

    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        // If either mTexture or mLevelZeroTexture exist, then we need to copy them into the
        // corresponding textures in destStorage.
        if (mTexture.valid())
        {
            ANGLE_TRY(dest11->useLevelZeroWorkaroundTexture(context, false));

            const TextureHelper11 *destResource = nullptr;
            ANGLE_TRY(dest11->getResource(context, &destResource));

            immediateContext->CopyResource(destResource->get(), mTexture.get());
        }

        if (mLevelZeroTexture.valid())
        {
            ANGLE_TRY(dest11->useLevelZeroWorkaroundTexture(context, true));

            const TextureHelper11 *destResource = nullptr;
            ANGLE_TRY(dest11->getResource(context, &destResource));

            immediateContext->CopyResource(destResource->get(), mLevelZeroTexture.get());
        }

        return gl::NoError();
    }

    const TextureHelper11 *sourceResouce = nullptr;
    ANGLE_TRY(getResource(context, &sourceResouce));

    const TextureHelper11 *destResource = nullptr;
    ANGLE_TRY(dest11->getResource(context, &destResource));

    immediateContext->CopyResource(destResource->get(), sourceResouce->get());
    dest11->markDirty();

    return gl::NoError();
}

gl::Error TextureStorage11_2D::useLevelZeroWorkaroundTexture(const gl::Context *context,
                                                             bool useLevelZeroTexture)
{
    bool lastSetting = mUseLevelZeroTexture;

    if (useLevelZeroTexture && mMipLevels > 1)
    {
        if (!mUseLevelZeroTexture && mTexture.valid())
        {
            ANGLE_TRY(ensureTextureExists(1));

            // Pull data back from the mipped texture if necessary.
            ASSERT(mLevelZeroTexture.valid());
            ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();
            deviceContext->CopySubresourceRegion(mLevelZeroTexture.get(), 0, 0, 0, 0,
                                                 mTexture.get(), 0, nullptr);
        }

        mUseLevelZeroTexture = true;
    }
    else
    {
        if (mUseLevelZeroTexture && mLevelZeroTexture.valid())
        {
            ANGLE_TRY(ensureTextureExists(mMipLevels));

            // Pull data back from the level zero texture if necessary.
            ASSERT(mTexture.valid());
            ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();
            deviceContext->CopySubresourceRegion(mTexture.get(), 0, 0, 0, 0,
                                                 mLevelZeroTexture.get(), 0, nullptr);
        }

        mUseLevelZeroTexture = false;
    }

    if (lastSetting != mUseLevelZeroTexture)
    {
        // Mark everything as dirty to be conservative.
        if (mLevelZeroRenderTarget)
        {
            mLevelZeroRenderTarget->signalDirty(context);
        }
        for (auto &renderTarget : mRenderTarget)
        {
            if (renderTarget)
            {
                renderTarget->signalDirty(context);
            }
        }
    }

    return gl::NoError();
}

void TextureStorage11_2D::associateImage(Image11 *image, const gl::ImageIndex &index)
{
    const GLint level = index.mipIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        mAssociatedImages[level] = image;
    }
}

void TextureStorage11_2D::verifyAssociatedImageValid(const gl::ImageIndex &index,
                                                     Image11 *expectedImage)
{
    const GLint level = index.mipIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    // This validation check should never return false. It means the Image/TextureStorage
    // association is broken.
    ASSERT(mAssociatedImages[level] == expectedImage);
}

// disassociateImage allows an Image to end its association with a Storage.
void TextureStorage11_2D::disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage)
{
    const GLint level = index.mipIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(mAssociatedImages[level] == expectedImage);
    mAssociatedImages[level] = nullptr;
}

// releaseAssociatedImage prepares the Storage for a new Image association. It lets the old Image
// recover its data before ending the association.
gl::Error TextureStorage11_2D::releaseAssociatedImage(const gl::Context *context,
                                                      const gl::ImageIndex &index,
                                                      Image11 *incomingImage)
{
    const GLint level = index.mipIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        // No need to let the old Image recover its data, if it is also the incoming Image.
        if (mAssociatedImages[level] != nullptr && mAssociatedImages[level] != incomingImage)
        {
            // Ensure that the Image is still associated with this TextureStorage.
            mAssociatedImages[level]->verifyAssociatedStorageValid(this);

            // Force the image to recover from storage before its data is overwritten.
            // This will reset mAssociatedImages[level] to nullptr too.
            ANGLE_TRY(mAssociatedImages[level]->recoverFromAssociatedStorage(context));
        }
    }

    return gl::NoError();
}

gl::Error TextureStorage11_2D::getResource(const gl::Context *context,
                                           const TextureHelper11 **outResource)
{
    if (mUseLevelZeroTexture && mMipLevels > 1)
    {
        ANGLE_TRY(ensureTextureExists(1));

        *outResource = &mLevelZeroTexture;
        return gl::NoError();
    }

    ANGLE_TRY(ensureTextureExists(mMipLevels));

    *outResource = &mTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_2D::getMippedResource(const gl::Context *context,
                                                 const TextureHelper11 **outResource)
{
    // This shouldn't be called unless the zero max LOD workaround is active.
    ASSERT(mRenderer->getWorkarounds().zeroMaxLodWorkaround);

    ANGLE_TRY(ensureTextureExists(mMipLevels));

    *outResource = &mTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_2D::ensureTextureExists(int mipLevels)
{
    // If mMipLevels = 1 then always use mTexture rather than mLevelZeroTexture.
    bool useLevelZeroTexture = mRenderer->getWorkarounds().zeroMaxLodWorkaround
                                   ? (mipLevels == 1) && (mMipLevels > 1)
                                   : false;
    TextureHelper11 *outputTexture = useLevelZeroTexture ? &mLevelZeroTexture : &mTexture;

    // if the width or height is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (!outputTexture->valid() && mTextureWidth > 0 && mTextureHeight > 0)
    {
        ASSERT(mipLevels > 0);

        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = mTextureWidth;  // Compressed texture size constraints?
        desc.Height             = mTextureHeight;
        desc.MipLevels          = mipLevels;
        desc.ArraySize          = 1;
        desc.Format             = mFormatInfo.texFormat;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = getBindFlags();
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = getMiscFlags();

        ANGLE_TRY(mRenderer->allocateTexture(desc, mFormatInfo, outputTexture));
        outputTexture->setDebugName("TexStorage2D.Texture");
    }

    return gl::NoError();
}

gl::Error TextureStorage11_2D::getRenderTarget(const gl::Context *context,
                                               const gl::ImageIndex &index,
                                               RenderTargetD3D **outRT)
{
    ASSERT(!index.hasLayer());

    const int level = index.mipIndex;
    ASSERT(level >= 0 && level < getLevelCount());

    // In GL ES 2.0, the application can only render to level zero of the texture (Section 4.4.3 of
    // the GLES 2.0 spec, page 113 of version 2.0.25). Other parts of TextureStorage11_2D could
    // create RTVs on non-zero levels of the texture (e.g. generateMipmap).
    // On Feature Level 9_3, this is unlikely to be useful. The renderer can't create SRVs on the
    // individual levels of the texture, so methods like generateMipmap can't do anything useful
    // with non-zero-level RTVs. Therefore if level > 0 on 9_3 then there's almost certainly
    // something wrong.
    ASSERT(
        !(mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3 && level > 0));
    ASSERT(outRT);
    if (mRenderTarget[level])
    {
        *outRT = mRenderTarget[level].get();
        return gl::NoError();
    }

    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ASSERT(index.mipIndex == 0);
        ANGLE_TRY(useLevelZeroWorkaroundTexture(context, true));
    }

    const TextureHelper11 *texture = nullptr;
    ANGLE_TRY(getResource(context, &texture));

    const d3d11::SharedSRV *srv = nullptr;
    ANGLE_TRY(getSRVLevel(context, level, false, &srv));

    const d3d11::SharedSRV *blitSRV = nullptr;
    ANGLE_TRY(getSRVLevel(context, level, true, &blitSRV));

    if (mUseLevelZeroTexture)
    {
        if (!mLevelZeroRenderTarget)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format             = mFormatInfo.rtvFormat;
            rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = mTopLevel + level;

            d3d11::RenderTargetView rtv;
            ANGLE_TRY(mRenderer->allocateResource(rtvDesc, mLevelZeroTexture.get(), &rtv));

            mLevelZeroRenderTarget.reset(new TextureRenderTarget11(
                std::move(rtv), mLevelZeroTexture, d3d11::SharedSRV(), d3d11::SharedSRV(),
                mFormatInfo.internalFormat, getFormatSet(), getLevelWidth(level),
                getLevelHeight(level), 1, 0));
        }

        *outRT = mLevelZeroRenderTarget.get();
        return gl::NoError();
    }

    if (mFormatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format             = mFormatInfo.rtvFormat;
        rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = mTopLevel + level;

        d3d11::RenderTargetView rtv;
        ANGLE_TRY(mRenderer->allocateResource(rtvDesc, texture->get(), &rtv));

        mRenderTarget[level].reset(new TextureRenderTarget11(
            std::move(rtv), *texture, *srv, *blitSRV, mFormatInfo.internalFormat, getFormatSet(),
            getLevelWidth(level), getLevelHeight(level), 1, 0));

        *outRT = mRenderTarget[level].get();
        return gl::NoError();
    }

    ASSERT(mFormatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Format             = mFormatInfo.dsvFormat;
    dsvDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = mTopLevel + level;
    dsvDesc.Flags              = 0;

    d3d11::DepthStencilView dsv;
    ANGLE_TRY(mRenderer->allocateResource(dsvDesc, texture->get(), &dsv));

    mRenderTarget[level].reset(new TextureRenderTarget11(
        std::move(dsv), *texture, *srv, mFormatInfo.internalFormat, getFormatSet(),
        getLevelWidth(level), getLevelHeight(level), 1, 0));

    *outRT = mRenderTarget[level].get();
    return gl::NoError();
}

gl::Error TextureStorage11_2D::createSRV(const gl::Context *context,
                                         int baseLevel,
                                         int mipLevels,
                                         DXGI_FORMAT format,
                                         const TextureHelper11 &texture,
                                         d3d11::SharedSRV *outSRV)
{
    ASSERT(outSRV);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                    = format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = mTopLevel + baseLevel;
    srvDesc.Texture2D.MipLevels       = mipLevels;

    const TextureHelper11 *srvTexture = &texture;

    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ASSERT(mTopLevel == 0);
        ASSERT(baseLevel == 0);
        // This code also assumes that the incoming texture equals either mLevelZeroTexture or
        // mTexture.

        if (mipLevels == 1 && mMipLevels > 1)
        {
            // We must use a SRV on the level-zero-only texture.
            ANGLE_TRY(ensureTextureExists(1));
            srvTexture = &mLevelZeroTexture;
        }
        else
        {
            ASSERT(mipLevels == static_cast<int>(mMipLevels));
            ASSERT(mTexture.valid() && texture == mTexture);
            srvTexture = &mTexture;
        }
    }

    ANGLE_TRY(mRenderer->allocateResource(srvDesc, srvTexture->get(), outSRV));
    outSRV->setDebugName("TexStorage2D.SRV");

    return gl::NoError();
}

gl::Error TextureStorage11_2D::getSwizzleTexture(const TextureHelper11 **outTexture)
{
    ASSERT(outTexture);

    if (!mSwizzleTexture.valid())
    {
        const auto &format = mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps());

        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = mTextureWidth;
        desc.Height             = mTextureHeight;
        desc.MipLevels          = mMipLevels;
        desc.ArraySize          = 1;
        desc.Format             = format.texFormat;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = 0;

        ANGLE_TRY(mRenderer->allocateTexture(desc, format, &mSwizzleTexture));
        mSwizzleTexture.setDebugName("TexStorage2D.SwizzleTexture");
    }

    *outTexture = &mSwizzleTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_2D::getSwizzleRenderTarget(int mipLevel,
                                                      const d3d11::RenderTargetView **outRTV)
{
    ASSERT(mipLevel >= 0 && mipLevel < getLevelCount());
    ASSERT(outRTV);

    if (!mSwizzleRenderTargets[mipLevel].valid())
    {
        const TextureHelper11 *swizzleTexture = nullptr;
        ANGLE_TRY(getSwizzleTexture(&swizzleTexture));

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format =
            mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps()).rtvFormat;
        rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = mTopLevel + mipLevel;

        ANGLE_TRY(mRenderer->allocateResource(rtvDesc, mSwizzleTexture.get(),
                                              &mSwizzleRenderTargets[mipLevel]));
    }

    *outRTV = &mSwizzleRenderTargets[mipLevel];
    return gl::NoError();
}

gl::ErrorOrResult<TextureStorage11::DropStencil> TextureStorage11_2D::ensureDropStencilTexture(
    const gl::Context *context)
{
    if (mDropStencilTexture.valid())
    {
        return DropStencil::ALREADY_EXISTS;
    }

    D3D11_TEXTURE2D_DESC dropDesc = {};
    dropDesc.ArraySize            = 1;
    dropDesc.BindFlags            = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    dropDesc.CPUAccessFlags       = 0;
    dropDesc.Format               = DXGI_FORMAT_R32_TYPELESS;
    dropDesc.Height               = mTextureHeight;
    dropDesc.MipLevels            = mMipLevels;
    dropDesc.MiscFlags            = 0;
    dropDesc.SampleDesc.Count     = 1;
    dropDesc.SampleDesc.Quality   = 0;
    dropDesc.Usage                = D3D11_USAGE_DEFAULT;
    dropDesc.Width                = mTextureWidth;

    const auto &format =
        d3d11::Format::Get(GL_DEPTH_COMPONENT32F, mRenderer->getRenderer11DeviceCaps());
    ANGLE_TRY(mRenderer->allocateTexture(dropDesc, format, &mDropStencilTexture));
    mDropStencilTexture.setDebugName("TexStorage2D.DropStencil");

    ANGLE_TRY(initDropStencilTexture(context, gl::ImageIndexIterator::Make2D(0, mMipLevels)));

    return DropStencil::CREATED;
}

TextureStorage11_External::TextureStorage11_External(
    Renderer11 *renderer,
    egl::Stream *stream,
    const egl::Stream::GLTextureDescription &glDesc)
    : TextureStorage11(renderer, D3D11_BIND_SHADER_RESOURCE, 0, glDesc.internalFormat)
{
    ASSERT(stream->getProducerType() == egl::Stream::ProducerType::D3D11TextureNV12);
    StreamProducerNV12 *producer = static_cast<StreamProducerNV12 *>(stream->getImplementation());
    mTexture.set(producer->getD3DTexture(), mFormatInfo);
    mSubresourceIndex            = producer->getArraySlice();
    mTexture.get()->AddRef();
    mMipLevels = 1;

    D3D11_TEXTURE2D_DESC desc;
    mTexture.getDesc(&desc);
    mTextureWidth  = desc.Width;
    mTextureHeight = desc.Height;
    mTextureDepth  = 1;
    mHasKeyedMutex = (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0;
}

gl::Error TextureStorage11_External::onDestroy(const gl::Context *context)
{
    if (mHasKeyedMutex)
    {
        // If the keyed mutex is released that will unbind it and cause the state cache to become
        // desynchronized.
        mRenderer->getStateManager()->invalidateBoundViews();
    }

    return gl::NoError();
}

TextureStorage11_External::~TextureStorage11_External()
{
}

gl::Error TextureStorage11_External::copyToStorage(const gl::Context *context,
                                                   TextureStorage *destStorage)
{
    UNIMPLEMENTED();
    return gl::NoError();
}

void TextureStorage11_External::associateImage(Image11 *image, const gl::ImageIndex &index)
{
    ASSERT(index.mipIndex == 0);
    mAssociatedImage = image;
}

void TextureStorage11_External::verifyAssociatedImageValid(const gl::ImageIndex &index,
                                                           Image11 *expectedImage)
{
    ASSERT(index.mipIndex == 0 && mAssociatedImage == expectedImage);
}

void TextureStorage11_External::disassociateImage(const gl::ImageIndex &index,
                                                  Image11 *expectedImage)
{
    ASSERT(index.mipIndex == 0);
    ASSERT(mAssociatedImage == expectedImage);
    mAssociatedImage = nullptr;
}

gl::Error TextureStorage11_External::releaseAssociatedImage(const gl::Context *context,
                                                            const gl::ImageIndex &index,
                                                            Image11 *incomingImage)
{
    ASSERT(index.mipIndex == 0);

    if (mAssociatedImage != nullptr && mAssociatedImage != incomingImage)
    {
        mAssociatedImage->verifyAssociatedStorageValid(this);

        ANGLE_TRY(mAssociatedImage->recoverFromAssociatedStorage(context));
    }

    return gl::NoError();
}

gl::Error TextureStorage11_External::getResource(const gl::Context *context,
                                                 const TextureHelper11 **outResource)
{
    *outResource = &mTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_External::getMippedResource(const gl::Context *context,
                                                       const TextureHelper11 **outResource)
{
    *outResource = &mTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_External::getRenderTarget(const gl::Context *context,
                                                     const gl::ImageIndex &index,
                                                     RenderTargetD3D **outRT)
{
    // Render targets are not supported for external textures
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureStorage11_External::createSRV(const gl::Context *context,
                                               int baseLevel,
                                               int mipLevels,
                                               DXGI_FORMAT format,
                                               const TextureHelper11 &texture,
                                               d3d11::SharedSRV *outSRV)
{
    // Since external textures are treates as non-mipmapped textures, we ignore mipmap levels and
    // use the specified subresource ID the storage was created with.
    ASSERT(mipLevels == 1);
    ASSERT(outSRV);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format        = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    // subresource index is equal to the mip level for 2D textures
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels       = 1;
    srvDesc.Texture2DArray.FirstArraySlice = mSubresourceIndex;
    srvDesc.Texture2DArray.ArraySize       = 1;

    ANGLE_TRY(mRenderer->allocateResource(srvDesc, texture.get(), outSRV));
    outSRV->setDebugName("TexStorage2D.SRV");

    return gl::NoError();
}

gl::Error TextureStorage11_External::getSwizzleTexture(const TextureHelper11 **outTexture)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureStorage11_External::getSwizzleRenderTarget(int mipLevel,
                                                            const d3d11::RenderTargetView **outRTV)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

TextureStorage11_EGLImage::TextureStorage11_EGLImage(Renderer11 *renderer,
                                                     EGLImageD3D *eglImage,
                                                     RenderTarget11 *renderTarget11)
    : TextureStorage11(renderer,
                       D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
                       0,
                       renderTarget11->getInternalFormat()),
      mImage(eglImage),
      mCurrentRenderTarget(0),
      mSwizzleTexture(),
      mSwizzleRenderTargets(gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
{
    mCurrentRenderTarget = reinterpret_cast<uintptr_t>(renderTarget11);

    mMipLevels     = 1;
    mTextureWidth  = renderTarget11->getWidth();
    mTextureHeight = renderTarget11->getHeight();
    mTextureDepth  = 1;
}

TextureStorage11_EGLImage::~TextureStorage11_EGLImage()
{
}

gl::Error TextureStorage11_EGLImage::getResource(const gl::Context *context,
                                                 const TextureHelper11 **outResource)
{
    ANGLE_TRY(checkForUpdatedRenderTarget(context));

    RenderTarget11 *renderTarget11 = nullptr;
    ANGLE_TRY(getImageRenderTarget(context, &renderTarget11));
    *outResource = &renderTarget11->getTexture();
    return gl::NoError();
}

gl::Error TextureStorage11_EGLImage::getSRV(const gl::Context *context,
                                            const gl::TextureState &textureState,
                                            const d3d11::SharedSRV **outSRV)
{
    ANGLE_TRY(checkForUpdatedRenderTarget(context));
    return TextureStorage11::getSRV(context, textureState, outSRV);
}

gl::Error TextureStorage11_EGLImage::getMippedResource(const gl::Context *context,
                                                       const TextureHelper11 **)
{
    // This shouldn't be called unless the zero max LOD workaround is active.
    // EGL images are unavailable in this configuration.
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureStorage11_EGLImage::getRenderTarget(const gl::Context *context,
                                                     const gl::ImageIndex &index,
                                                     RenderTargetD3D **outRT)
{
    ASSERT(!index.hasLayer());
    ASSERT(index.mipIndex == 0);

    ANGLE_TRY(checkForUpdatedRenderTarget(context));

    return mImage->getRenderTarget(context, outRT);
}

gl::Error TextureStorage11_EGLImage::copyToStorage(const gl::Context *context,
                                                   TextureStorage *destStorage)
{
    const TextureHelper11 *sourceResouce = nullptr;
    ANGLE_TRY(getResource(context, &sourceResouce));

    ASSERT(destStorage);
    TextureStorage11_2D *dest11  = GetAs<TextureStorage11_2D>(destStorage);
    const TextureHelper11 *destResource = nullptr;
    ANGLE_TRY(dest11->getResource(context, &destResource));

    ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();
    immediateContext->CopyResource(destResource->get(), sourceResouce->get());

    dest11->markDirty();

    return gl::NoError();
}

void TextureStorage11_EGLImage::associateImage(Image11 *, const gl::ImageIndex &)
{
}

void TextureStorage11_EGLImage::disassociateImage(const gl::ImageIndex &, Image11 *)
{
}

void TextureStorage11_EGLImage::verifyAssociatedImageValid(const gl::ImageIndex &, Image11 *)
{
}

gl::Error TextureStorage11_EGLImage::releaseAssociatedImage(const gl::Context *context,
                                                            const gl::ImageIndex &,
                                                            Image11 *)
{
    return gl::NoError();
}

gl::Error TextureStorage11_EGLImage::useLevelZeroWorkaroundTexture(const gl::Context *context, bool)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureStorage11_EGLImage::getSwizzleTexture(const TextureHelper11 **outTexture)
{
    ASSERT(outTexture);

    if (!mSwizzleTexture.valid())
    {
        const auto &format = mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps());

        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = mTextureWidth;
        desc.Height             = mTextureHeight;
        desc.MipLevels          = mMipLevels;
        desc.ArraySize          = 1;
        desc.Format             = format.texFormat;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = 0;

        ANGLE_TRY(mRenderer->allocateTexture(desc, format, &mSwizzleTexture));
        mSwizzleTexture.setDebugName("TexStorageEGLImage.SwizzleTexture");
    }

    *outTexture = &mSwizzleTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_EGLImage::getSwizzleRenderTarget(int mipLevel,
                                                            const d3d11::RenderTargetView **outRTV)
{
    ASSERT(mipLevel >= 0 && mipLevel < getLevelCount());
    ASSERT(outRTV);

    if (!mSwizzleRenderTargets[mipLevel].valid())
    {
        const TextureHelper11 *swizzleTexture = nullptr;
        ANGLE_TRY(getSwizzleTexture(&swizzleTexture));

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format =
            mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps()).rtvFormat;
        rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = mTopLevel + mipLevel;

        ANGLE_TRY(mRenderer->allocateResource(rtvDesc, mSwizzleTexture.get(),
                                              &mSwizzleRenderTargets[mipLevel]));
    }

    *outRTV = &mSwizzleRenderTargets[mipLevel];
    return gl::NoError();
}

gl::Error TextureStorage11_EGLImage::checkForUpdatedRenderTarget(const gl::Context *context)
{
    RenderTarget11 *renderTarget11 = nullptr;
    ANGLE_TRY(getImageRenderTarget(context, &renderTarget11));

    if (mCurrentRenderTarget != reinterpret_cast<uintptr_t>(renderTarget11))
    {
        clearSRVCache();
        mCurrentRenderTarget = reinterpret_cast<uintptr_t>(renderTarget11);
    }

    return gl::NoError();
}

gl::Error TextureStorage11_EGLImage::createSRV(const gl::Context *context,
                                               int baseLevel,
                                               int mipLevels,
                                               DXGI_FORMAT format,
                                               const TextureHelper11 &texture,
                                               d3d11::SharedSRV *outSRV)
{
    ASSERT(baseLevel == 0);
    ASSERT(mipLevels == 1);
    ASSERT(outSRV);

    // Create a new SRV only for the swizzle texture.  Otherwise just return the Image's
    // RenderTarget's SRV.
    if (texture == mSwizzleTexture)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format                    = format;
        srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = mTopLevel + baseLevel;
        srvDesc.Texture2D.MipLevels       = mipLevels;

        ANGLE_TRY(mRenderer->allocateResource(srvDesc, texture.get(), outSRV));
        outSRV->setDebugName("TexStorageEGLImage.SRV");
    }
    else
    {
        RenderTarget11 *renderTarget = nullptr;
        ANGLE_TRY(getImageRenderTarget(context, &renderTarget));

        ASSERT(texture == renderTarget->getTexture());

        *outSRV = renderTarget->getShaderResourceView().makeCopy();
    }

    return gl::NoError();
}

gl::Error TextureStorage11_EGLImage::getImageRenderTarget(const gl::Context *context,
                                                          RenderTarget11 **outRT) const
{
    RenderTargetD3D *renderTargetD3D = nullptr;
    ANGLE_TRY(mImage->getRenderTarget(context, &renderTargetD3D));
    *outRT = GetAs<RenderTarget11>(renderTargetD3D);
    return gl::NoError();
}

TextureStorage11_Cube::TextureStorage11_Cube(Renderer11 *renderer,
                                             GLenum internalformat,
                                             bool renderTarget,
                                             int size,
                                             int levels,
                                             bool hintLevelZeroOnly)
    : TextureStorage11(
          renderer,
          GetTextureBindFlags(internalformat, renderer->getRenderer11DeviceCaps(), renderTarget),
          GetTextureMiscFlags(internalformat,
                              renderer->getRenderer11DeviceCaps(),
                              renderTarget,
                              levels),
          internalformat),
      mTexture(),
      mLevelZeroTexture(),
      mUseLevelZeroTexture(hintLevelZeroOnly && levels > 1),
      mSwizzleTexture()
{
    for (unsigned int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        for (unsigned int face = 0; face < gl::CUBE_FACE_COUNT; face++)
        {
            mAssociatedImages[face][level] = nullptr;
            mRenderTarget[face][level]     = nullptr;
        }
    }

    for (unsigned int face = 0; face < gl::CUBE_FACE_COUNT; face++)
    {
        mLevelZeroRenderTarget[face] = nullptr;
    }

    // adjust size if needed for compressed textures
    int height = size;
    d3d11::MakeValidSize(false, mFormatInfo.texFormat, &size, &height, &mTopLevel);

    mMipLevels     = mTopLevel + levels;
    mTextureWidth  = size;
    mTextureHeight = size;
    mTextureDepth  = 1;

    // The LevelZeroOnly hint should only be true if the zero max LOD workaround is active.
    ASSERT(!mUseLevelZeroTexture || mRenderer->getWorkarounds().zeroMaxLodWorkaround);
}

gl::Error TextureStorage11_Cube::onDestroy(const gl::Context *context)
{
    for (unsigned int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        for (unsigned int face = 0; face < gl::CUBE_FACE_COUNT; face++)
        {
            if (mAssociatedImages[face][level] != nullptr)
            {
                mAssociatedImages[face][level]->verifyAssociatedStorageValid(this);

                // We must let the Images recover their data before we delete it from the
                // TextureStorage.
                ANGLE_TRY(mAssociatedImages[face][level]->recoverFromAssociatedStorage(context));
            }
        }
    }

    for (auto &faceRenderTargets : mRenderTarget)
    {
        InvalidateRenderTargetContainer(context, &faceRenderTargets);
    }
    InvalidateRenderTargetContainer(context, &mLevelZeroRenderTarget);

    return gl::NoError();
}

TextureStorage11_Cube::~TextureStorage11_Cube()
{
}

UINT TextureStorage11_Cube::getSubresourceIndex(const gl::ImageIndex &index) const
{
    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround && mUseLevelZeroTexture &&
        index.mipIndex == 0)
    {
        UINT arraySlice  = static_cast<UINT>(index.hasLayer() ? index.layerIndex : 0);
        UINT subresource = D3D11CalcSubresource(0, arraySlice, 1);
        ASSERT(subresource != std::numeric_limits<UINT>::max());
        return subresource;
    }
    else
    {
        UINT mipSlice    = static_cast<UINT>(index.mipIndex + mTopLevel);
        UINT arraySlice  = static_cast<UINT>(index.hasLayer() ? index.layerIndex : 0);
        UINT subresource = D3D11CalcSubresource(mipSlice, arraySlice, mMipLevels);
        ASSERT(subresource != std::numeric_limits<UINT>::max());
        return subresource;
    }
}

gl::Error TextureStorage11_Cube::copyToStorage(const gl::Context *context,
                                               TextureStorage *destStorage)
{
    ASSERT(destStorage);

    TextureStorage11_Cube *dest11 = GetAs<TextureStorage11_Cube>(destStorage);

    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();

        // If either mTexture or mLevelZeroTexture exist, then we need to copy them into the
        // corresponding textures in destStorage.
        if (mTexture.valid())
        {
            ANGLE_TRY(dest11->useLevelZeroWorkaroundTexture(context, false));

            const TextureHelper11 *destResource = nullptr;
            ANGLE_TRY(dest11->getResource(context, &destResource));

            immediateContext->CopyResource(destResource->get(), mTexture.get());
        }

        if (mLevelZeroTexture.valid())
        {
            ANGLE_TRY(dest11->useLevelZeroWorkaroundTexture(context, true));

            const TextureHelper11 *destResource = nullptr;
            ANGLE_TRY(dest11->getResource(context, &destResource));

            immediateContext->CopyResource(destResource->get(), mLevelZeroTexture.get());
        }
    }
    else
    {
        const TextureHelper11 *sourceResouce = nullptr;
        ANGLE_TRY(getResource(context, &sourceResouce));

        const TextureHelper11 *destResource = nullptr;
        ANGLE_TRY(dest11->getResource(context, &destResource));

        ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();
        immediateContext->CopyResource(destResource->get(), sourceResouce->get());
    }

    dest11->markDirty();

    return gl::NoError();
}

gl::Error TextureStorage11_Cube::useLevelZeroWorkaroundTexture(const gl::Context *context,
                                                               bool useLevelZeroTexture)
{
    if (useLevelZeroTexture && mMipLevels > 1)
    {
        if (!mUseLevelZeroTexture && mTexture.valid())
        {
            ANGLE_TRY(ensureTextureExists(1));

            // Pull data back from the mipped texture if necessary.
            ASSERT(mLevelZeroTexture.valid());
            ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

            for (int face = 0; face < 6; face++)
            {
                deviceContext->CopySubresourceRegion(mLevelZeroTexture.get(),
                                                     D3D11CalcSubresource(0, face, 1), 0, 0, 0,
                                                     mTexture.get(), face * mMipLevels, nullptr);
            }
        }

        mUseLevelZeroTexture = true;
    }
    else
    {
        if (mUseLevelZeroTexture && mLevelZeroTexture.valid())
        {
            ANGLE_TRY(ensureTextureExists(mMipLevels));

            // Pull data back from the level zero texture if necessary.
            ASSERT(mTexture.valid());
            ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

            for (int face = 0; face < 6; face++)
            {
                deviceContext->CopySubresourceRegion(mTexture.get(),
                                                     D3D11CalcSubresource(0, face, mMipLevels), 0,
                                                     0, 0, mLevelZeroTexture.get(), face, nullptr);
            }
        }

        mUseLevelZeroTexture = false;
    }

    return gl::NoError();
}

void TextureStorage11_Cube::associateImage(Image11 *image, const gl::ImageIndex &index)
{
    const GLint level       = index.mipIndex;
    const GLint layerTarget = index.layerIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(0 <= layerTarget && layerTarget < static_cast<GLint>(gl::CUBE_FACE_COUNT));

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        if (0 <= layerTarget && layerTarget < static_cast<GLint>(gl::CUBE_FACE_COUNT))
        {
            mAssociatedImages[layerTarget][level] = image;
        }
    }
}

void TextureStorage11_Cube::verifyAssociatedImageValid(const gl::ImageIndex &index,
                                                       Image11 *expectedImage)
{
    const GLint level       = index.mipIndex;
    const GLint layerTarget = index.layerIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(0 <= layerTarget && layerTarget < static_cast<GLint>(gl::CUBE_FACE_COUNT));
    // This validation check should never return false. It means the Image/TextureStorage
    // association is broken.
    ASSERT(mAssociatedImages[layerTarget][level] == expectedImage);
}

// disassociateImage allows an Image to end its association with a Storage.
void TextureStorage11_Cube::disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage)
{
    const GLint level       = index.mipIndex;
    const GLint layerTarget = index.layerIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(0 <= layerTarget && layerTarget < static_cast<GLint>(gl::CUBE_FACE_COUNT));
    ASSERT(mAssociatedImages[layerTarget][level] == expectedImage);
    mAssociatedImages[layerTarget][level] = nullptr;
}

// releaseAssociatedImage prepares the Storage for a new Image association. It lets the old Image
// recover its data before ending the association.
gl::Error TextureStorage11_Cube::releaseAssociatedImage(const gl::Context *context,
                                                        const gl::ImageIndex &index,
                                                        Image11 *incomingImage)
{
    const GLint level       = index.mipIndex;
    const GLint layerTarget = index.layerIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(0 <= layerTarget && layerTarget < static_cast<GLint>(gl::CUBE_FACE_COUNT));

    if ((0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS))
    {
        if (0 <= layerTarget && layerTarget < static_cast<GLint>(gl::CUBE_FACE_COUNT))
        {
            // No need to let the old Image recover its data, if it is also the incoming Image.
            if (mAssociatedImages[layerTarget][level] != nullptr &&
                mAssociatedImages[layerTarget][level] != incomingImage)
            {
                // Ensure that the Image is still associated with this TextureStorage.
                mAssociatedImages[layerTarget][level]->verifyAssociatedStorageValid(this);

                // Force the image to recover from storage before its data is overwritten.
                // This will reset mAssociatedImages[level] to nullptr too.
                ANGLE_TRY(
                    mAssociatedImages[layerTarget][level]->recoverFromAssociatedStorage(context));
            }
        }
    }

    return gl::NoError();
}

gl::Error TextureStorage11_Cube::getResource(const gl::Context *context,
                                             const TextureHelper11 **outResource)
{
    if (mUseLevelZeroTexture && mMipLevels > 1)
    {
        ANGLE_TRY(ensureTextureExists(1));
        *outResource = &mLevelZeroTexture;
        return gl::NoError();
    }
    else
    {
        ANGLE_TRY(ensureTextureExists(mMipLevels));
        *outResource = &mTexture;
        return gl::NoError();
    }
}

gl::Error TextureStorage11_Cube::getMippedResource(const gl::Context *context,
                                                   const TextureHelper11 **outResource)
{
    // This shouldn't be called unless the zero max LOD workaround is active.
    ASSERT(mRenderer->getWorkarounds().zeroMaxLodWorkaround);

    ANGLE_TRY(ensureTextureExists(mMipLevels));
    *outResource = &mTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_Cube::ensureTextureExists(int mipLevels)
{
    // If mMipLevels = 1 then always use mTexture rather than mLevelZeroTexture.
    bool useLevelZeroTexture = mRenderer->getWorkarounds().zeroMaxLodWorkaround
                                   ? (mipLevels == 1) && (mMipLevels > 1)
                                   : false;
    TextureHelper11 *outputTexture = useLevelZeroTexture ? &mLevelZeroTexture : &mTexture;

    // if the size is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (!outputTexture->valid() && mTextureWidth > 0 && mTextureHeight > 0)
    {
        ASSERT(mMipLevels > 0);

        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = mTextureWidth;
        desc.Height             = mTextureHeight;
        desc.MipLevels          = mipLevels;
        desc.ArraySize          = gl::CUBE_FACE_COUNT;
        desc.Format             = mFormatInfo.texFormat;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = getBindFlags();
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE | getMiscFlags();

        ANGLE_TRY(mRenderer->allocateTexture(desc, mFormatInfo, outputTexture));
        outputTexture->setDebugName("TexStorageCube.Texture");
    }

    return gl::NoError();
}

gl::Error TextureStorage11_Cube::createRenderTargetSRV(const TextureHelper11 &texture,
                                                       const gl::ImageIndex &index,
                                                       DXGI_FORMAT resourceFormat,
                                                       d3d11::SharedSRV *srv) const
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                         = resourceFormat;
    srvDesc.Texture2DArray.MostDetailedMip = mTopLevel + index.mipIndex;
    srvDesc.Texture2DArray.MipLevels       = 1;
    srvDesc.Texture2DArray.FirstArraySlice = index.layerIndex;
    srvDesc.Texture2DArray.ArraySize       = 1;

    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_10_0)
    {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    }
    else
    {
        // Will be used with Texture2D sampler, not TextureCube
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    }

    ANGLE_TRY(mRenderer->allocateResource(srvDesc, texture.get(), srv));
    return gl::NoError();
}

gl::Error TextureStorage11_Cube::getRenderTarget(const gl::Context *context,
                                                 const gl::ImageIndex &index,
                                                 RenderTargetD3D **outRT)
{
    const int faceIndex = index.layerIndex;
    const int level     = index.mipIndex;

    ASSERT(level >= 0 && level < getLevelCount());
    ASSERT(faceIndex >= 0 && faceIndex < static_cast<GLint>(gl::CUBE_FACE_COUNT));

    if (!mRenderTarget[faceIndex][level])
    {
        if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
        {
            ASSERT(index.mipIndex == 0);
            ANGLE_TRY(useLevelZeroWorkaroundTexture(context, true));
        }

        const TextureHelper11 *texture = nullptr;
        ANGLE_TRY(getResource(context, &texture));

        if (mUseLevelZeroTexture)
        {
            if (!mLevelZeroRenderTarget[faceIndex])
            {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
                rtvDesc.Format                         = mFormatInfo.rtvFormat;
                rtvDesc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.MipSlice        = mTopLevel + level;
                rtvDesc.Texture2DArray.FirstArraySlice = faceIndex;
                rtvDesc.Texture2DArray.ArraySize       = 1;

                d3d11::RenderTargetView rtv;
                ANGLE_TRY(mRenderer->allocateResource(rtvDesc, mLevelZeroTexture.get(), &rtv));

                mLevelZeroRenderTarget[faceIndex].reset(new TextureRenderTarget11(
                    std::move(rtv), mLevelZeroTexture, d3d11::SharedSRV(), d3d11::SharedSRV(),
                    mFormatInfo.internalFormat, getFormatSet(), getLevelWidth(level),
                    getLevelHeight(level), 1, 0));
            }

            ASSERT(outRT);
            *outRT = mLevelZeroRenderTarget[faceIndex].get();
            return gl::NoError();
        }

        d3d11::SharedSRV srv;
        ANGLE_TRY(createRenderTargetSRV(*texture, index, mFormatInfo.srvFormat, &srv));
        d3d11::SharedSRV blitSRV;
        if (mFormatInfo.blitSRVFormat != mFormatInfo.srvFormat)
        {
            ANGLE_TRY(createRenderTargetSRV(*texture, index, mFormatInfo.blitSRVFormat, &blitSRV));
        }
        else
        {
            blitSRV = srv.makeCopy();
        }

        srv.setDebugName("TexStorageCube.RenderTargetSRV");

        if (mFormatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format                         = mFormatInfo.rtvFormat;
            rtvDesc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice        = mTopLevel + level;
            rtvDesc.Texture2DArray.FirstArraySlice = faceIndex;
            rtvDesc.Texture2DArray.ArraySize       = 1;

            d3d11::RenderTargetView rtv;
            ANGLE_TRY(mRenderer->allocateResource(rtvDesc, texture->get(), &rtv));
            rtv.setDebugName("TexStorageCube.RenderTargetRTV");

            mRenderTarget[faceIndex][level].reset(new TextureRenderTarget11(
                std::move(rtv), *texture, srv, blitSRV, mFormatInfo.internalFormat, getFormatSet(),
                getLevelWidth(level), getLevelHeight(level), 1, 0));
        }
        else if (mFormatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Format                         = mFormatInfo.dsvFormat;
            dsvDesc.ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Flags                          = 0;
            dsvDesc.Texture2DArray.MipSlice        = mTopLevel + level;
            dsvDesc.Texture2DArray.FirstArraySlice = faceIndex;
            dsvDesc.Texture2DArray.ArraySize       = 1;

            d3d11::DepthStencilView dsv;
            ANGLE_TRY(mRenderer->allocateResource(dsvDesc, texture->get(), &dsv));
            dsv.setDebugName("TexStorageCube.RenderTargetDSV");

            mRenderTarget[faceIndex][level].reset(new TextureRenderTarget11(
                std::move(dsv), *texture, srv, mFormatInfo.internalFormat, getFormatSet(),
                getLevelWidth(level), getLevelHeight(level), 1, 0));
        }
        else
        {
            UNREACHABLE();
        }
    }

    ASSERT(outRT);
    *outRT = mRenderTarget[faceIndex][level].get();
    return gl::NoError();
}

gl::Error TextureStorage11_Cube::createSRV(const gl::Context *context,
                                           int baseLevel,
                                           int mipLevels,
                                           DXGI_FORMAT format,
                                           const TextureHelper11 &texture,
                                           d3d11::SharedSRV *outSRV)
{
    ASSERT(outSRV);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = format;

    // Unnormalized integer cube maps are not supported by DX11; we emulate them as an array of six
    // 2D textures
    const GLenum componentType = d3d11::GetComponentType(format);
    if (componentType == GL_INT || componentType == GL_UNSIGNED_INT)
    {
        srvDesc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = mTopLevel + baseLevel;
        srvDesc.Texture2DArray.MipLevels       = mipLevels;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize       = gl::CUBE_FACE_COUNT;
    }
    else
    {
        srvDesc.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels       = mipLevels;
        srvDesc.TextureCube.MostDetailedMip = mTopLevel + baseLevel;
    }

    const TextureHelper11 *srvTexture = &texture;

    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ASSERT(mTopLevel == 0);
        ASSERT(baseLevel == 0);
        // This code also assumes that the incoming texture equals either mLevelZeroTexture or
        // mTexture.

        if (mipLevels == 1 && mMipLevels > 1)
        {
            // We must use a SRV on the level-zero-only texture.
            ANGLE_TRY(ensureTextureExists(1));
            srvTexture = &mLevelZeroTexture;
        }
        else
        {
            ASSERT(mipLevels == static_cast<int>(mMipLevels));
            ASSERT(mTexture.valid() && texture == mTexture);
            srvTexture = &mTexture;
        }
    }

    ANGLE_TRY(mRenderer->allocateResource(srvDesc, srvTexture->get(), outSRV));
    outSRV->setDebugName("TexStorageCube.SRV");

    return gl::NoError();
}

gl::Error TextureStorage11_Cube::getSwizzleTexture(const TextureHelper11 **outTexture)
{
    ASSERT(outTexture);

    if (!mSwizzleTexture.valid())
    {
        const auto &format = mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps());

        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = mTextureWidth;
        desc.Height             = mTextureHeight;
        desc.MipLevels          = mMipLevels;
        desc.ArraySize          = gl::CUBE_FACE_COUNT;
        desc.Format             = format.texFormat;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE;

        ANGLE_TRY(mRenderer->allocateTexture(desc, format, &mSwizzleTexture));
        mSwizzleTexture.setDebugName("TexStorageCube.SwizzleTexture");
    }

    *outTexture = &mSwizzleTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_Cube::getSwizzleRenderTarget(int mipLevel,
                                                        const d3d11::RenderTargetView **outRTV)
{
    ASSERT(mipLevel >= 0 && mipLevel < getLevelCount());
    ASSERT(outRTV);

    if (!mSwizzleRenderTargets[mipLevel].valid())
    {
        const TextureHelper11 *swizzleTexture = nullptr;
        ANGLE_TRY(getSwizzleTexture(&swizzleTexture));

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format =
            mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps()).rtvFormat;
        rtvDesc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice        = mTopLevel + mipLevel;
        rtvDesc.Texture2DArray.FirstArraySlice = 0;
        rtvDesc.Texture2DArray.ArraySize       = gl::CUBE_FACE_COUNT;

        ANGLE_TRY(mRenderer->allocateResource(rtvDesc, mSwizzleTexture.get(),
                                              &mSwizzleRenderTargets[mipLevel]));
    }

    *outRTV = &mSwizzleRenderTargets[mipLevel];
    return gl::NoError();
}

gl::Error TextureStorage11::initDropStencilTexture(const gl::Context *context,
                                                   const gl::ImageIndexIterator &it)
{
    const TextureHelper11 *sourceTexture = nullptr;
    ANGLE_TRY(getResource(context, &sourceTexture));

    gl::ImageIndexIterator itCopy = it;

    while (itCopy.hasNext())
    {
        gl::ImageIndex index = itCopy.next();
        gl::Box wholeArea(0, 0, 0, getLevelWidth(index.mipIndex), getLevelHeight(index.mipIndex),
                          1);
        gl::Extents wholeSize(wholeArea.width, wholeArea.height, 1);
        UINT subresource = getSubresourceIndex(index);
        ANGLE_TRY(mRenderer->getBlitter()->copyDepthStencil(
            *sourceTexture, subresource, wholeArea, wholeSize, mDropStencilTexture, subresource,
            wholeArea, wholeSize, nullptr));
    }

    return gl::NoError();
}

gl::ErrorOrResult<TextureStorage11::DropStencil> TextureStorage11_Cube::ensureDropStencilTexture(
    const gl::Context *context)
{
    if (mDropStencilTexture.valid())
    {
        return DropStencil::ALREADY_EXISTS;
    }

    D3D11_TEXTURE2D_DESC dropDesc = {};
    dropDesc.ArraySize            = 6;
    dropDesc.BindFlags            = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    dropDesc.CPUAccessFlags       = 0;
    dropDesc.Format               = DXGI_FORMAT_R32_TYPELESS;
    dropDesc.Height               = mTextureHeight;
    dropDesc.MipLevels            = mMipLevels;
    dropDesc.MiscFlags            = D3D11_RESOURCE_MISC_TEXTURECUBE;
    dropDesc.SampleDesc.Count     = 1;
    dropDesc.SampleDesc.Quality   = 0;
    dropDesc.Usage                = D3D11_USAGE_DEFAULT;
    dropDesc.Width                = mTextureWidth;

    const auto &format =
        d3d11::Format::Get(GL_DEPTH_COMPONENT32F, mRenderer->getRenderer11DeviceCaps());
    ANGLE_TRY(mRenderer->allocateTexture(dropDesc, format, &mDropStencilTexture));
    mDropStencilTexture.setDebugName("TexStorageCube.DropStencil");

    ANGLE_TRY(initDropStencilTexture(context, gl::ImageIndexIterator::MakeCube(0, mMipLevels)));

    return DropStencil::CREATED;
}

TextureStorage11_3D::TextureStorage11_3D(Renderer11 *renderer,
                                         GLenum internalformat,
                                         bool renderTarget,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         int levels)
    : TextureStorage11(
          renderer,
          GetTextureBindFlags(internalformat, renderer->getRenderer11DeviceCaps(), renderTarget),
          GetTextureMiscFlags(internalformat,
                              renderer->getRenderer11DeviceCaps(),
                              renderTarget,
                              levels),
          internalformat)
{
    for (unsigned int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mAssociatedImages[i]     = nullptr;
        mLevelRenderTargets[i]   = nullptr;
    }

    // adjust size if needed for compressed textures
    d3d11::MakeValidSize(false, mFormatInfo.texFormat, &width, &height, &mTopLevel);

    mMipLevels     = mTopLevel + levels;
    mTextureWidth  = width;
    mTextureHeight = height;
    mTextureDepth  = depth;
}

gl::Error TextureStorage11_3D::onDestroy(const gl::Context *context)
{
    for (unsigned i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        if (mAssociatedImages[i] != nullptr)
        {
            mAssociatedImages[i]->verifyAssociatedStorageValid(this);

            // We must let the Images recover their data before we delete it from the
            // TextureStorage.
            ANGLE_TRY(mAssociatedImages[i]->recoverFromAssociatedStorage(context));
        }
    }

    InvalidateRenderTargetContainer(context, &mLevelRenderTargets);
    InvalidateRenderTargetContainer(context, &mLevelLayerRenderTargets);

    return gl::NoError();
}

TextureStorage11_3D::~TextureStorage11_3D()
{
}

void TextureStorage11_3D::associateImage(Image11 *image, const gl::ImageIndex &index)
{
    const GLint level = index.mipIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        mAssociatedImages[level] = image;
    }
}

void TextureStorage11_3D::verifyAssociatedImageValid(const gl::ImageIndex &index,
                                                     Image11 *expectedImage)
{
    const GLint level = index.mipIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    // This validation check should never return false. It means the Image/TextureStorage
    // association is broken.
    ASSERT(mAssociatedImages[level] == expectedImage);
}

// disassociateImage allows an Image to end its association with a Storage.
void TextureStorage11_3D::disassociateImage(const gl::ImageIndex &index, Image11 *expectedImage)
{
    const GLint level = index.mipIndex;

    ASSERT(0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(mAssociatedImages[level] == expectedImage);
    mAssociatedImages[level] = nullptr;
}

// releaseAssociatedImage prepares the Storage for a new Image association. It lets the old Image
// recover its data before ending the association.
gl::Error TextureStorage11_3D::releaseAssociatedImage(const gl::Context *context,
                                                      const gl::ImageIndex &index,
                                                      Image11 *incomingImage)
{
    const GLint level = index.mipIndex;

    ASSERT((0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS));

    if (0 <= level && level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        // No need to let the old Image recover its data, if it is also the incoming Image.
        if (mAssociatedImages[level] != nullptr && mAssociatedImages[level] != incomingImage)
        {
            // Ensure that the Image is still associated with this TextureStorage.
            mAssociatedImages[level]->verifyAssociatedStorageValid(this);

            // Force the image to recover from storage before its data is overwritten.
            // This will reset mAssociatedImages[level] to nullptr too.
            ANGLE_TRY(mAssociatedImages[level]->recoverFromAssociatedStorage(context));
        }
    }

    return gl::NoError();
}

gl::Error TextureStorage11_3D::getResource(const gl::Context *context,
                                           const TextureHelper11 **outResource)
{
    // If the width, height or depth are not positive this should be treated as an incomplete
    // texture. We handle that here by skipping the d3d texture creation.
    if (!mTexture.valid() && mTextureWidth > 0 && mTextureHeight > 0 && mTextureDepth > 0)
    {
        ASSERT(mMipLevels > 0);

        D3D11_TEXTURE3D_DESC desc;
        desc.Width          = mTextureWidth;
        desc.Height         = mTextureHeight;
        desc.Depth          = mTextureDepth;
        desc.MipLevels      = mMipLevels;
        desc.Format         = mFormatInfo.texFormat;
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = getBindFlags();
        desc.CPUAccessFlags = 0;
        desc.MiscFlags      = getMiscFlags();

        ANGLE_TRY(mRenderer->allocateTexture(desc, mFormatInfo, &mTexture));
        mTexture.setDebugName("TexStorage3D.Texture");
    }

    *outResource = &mTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_3D::createSRV(const gl::Context *context,
                                         int baseLevel,
                                         int mipLevels,
                                         DXGI_FORMAT format,
                                         const TextureHelper11 &texture,
                                         d3d11::SharedSRV *outSRV)
{
    ASSERT(outSRV);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                    = format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MostDetailedMip = baseLevel;
    srvDesc.Texture3D.MipLevels       = mipLevels;

    ANGLE_TRY(mRenderer->allocateResource(srvDesc, texture.get(), outSRV));
    outSRV->setDebugName("TexStorage3D.SRV");

    return gl::NoError();
}

gl::Error TextureStorage11_3D::getRenderTarget(const gl::Context *context,
                                               const gl::ImageIndex &index,
                                               RenderTargetD3D **outRT)
{
    const int mipLevel = index.mipIndex;
    ASSERT(mipLevel >= 0 && mipLevel < getLevelCount());

    ASSERT(mFormatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN);

    if (!index.hasLayer())
    {
        if (!mLevelRenderTargets[mipLevel])
        {
            const TextureHelper11 *texture = nullptr;
            ANGLE_TRY(getResource(context, &texture));

            const d3d11::SharedSRV *srv = nullptr;
            ANGLE_TRY(getSRVLevel(context, mipLevel, false, &srv));

            const d3d11::SharedSRV *blitSRV = nullptr;
            ANGLE_TRY(getSRVLevel(context, mipLevel, true, &blitSRV));

            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format                = mFormatInfo.rtvFormat;
            rtvDesc.ViewDimension         = D3D11_RTV_DIMENSION_TEXTURE3D;
            rtvDesc.Texture3D.MipSlice    = mTopLevel + mipLevel;
            rtvDesc.Texture3D.FirstWSlice = 0;
            rtvDesc.Texture3D.WSize       = static_cast<UINT>(-1);

            d3d11::RenderTargetView rtv;
            ANGLE_TRY(mRenderer->allocateResource(rtvDesc, texture->get(), &rtv));
            rtv.setDebugName("TexStorage3D.RTV");

            mLevelRenderTargets[mipLevel].reset(new TextureRenderTarget11(
                std::move(rtv), *texture, *srv, *blitSRV, mFormatInfo.internalFormat,
                getFormatSet(), getLevelWidth(mipLevel), getLevelHeight(mipLevel),
                getLevelDepth(mipLevel), 0));
        }

        ASSERT(outRT);
        *outRT = mLevelRenderTargets[mipLevel].get();
        return gl::NoError();
    }

    const int layer = index.layerIndex;

    LevelLayerKey key(mipLevel, layer);
    if (mLevelLayerRenderTargets.find(key) == mLevelLayerRenderTargets.end())
    {
        const TextureHelper11 *texture = nullptr;
        ANGLE_TRY(getResource(context, &texture));

        // TODO, what kind of SRV is expected here?
        const d3d11::SharedSRV srv;
        const d3d11::SharedSRV blitSRV;

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format                = mFormatInfo.rtvFormat;
        rtvDesc.ViewDimension         = D3D11_RTV_DIMENSION_TEXTURE3D;
        rtvDesc.Texture3D.MipSlice    = mTopLevel + mipLevel;
        rtvDesc.Texture3D.FirstWSlice = layer;
        rtvDesc.Texture3D.WSize       = 1;

        d3d11::RenderTargetView rtv;
        ANGLE_TRY(mRenderer->allocateResource(rtvDesc, texture->get(), &rtv));
        rtv.setDebugName("TexStorage3D.LayerRTV");

        mLevelLayerRenderTargets[key].reset(new TextureRenderTarget11(
            std::move(rtv), *texture, srv, blitSRV, mFormatInfo.internalFormat, getFormatSet(),
            getLevelWidth(mipLevel), getLevelHeight(mipLevel), 1, 0));
    }

    ASSERT(outRT);
    *outRT = mLevelLayerRenderTargets[key].get();
    return gl::NoError();
}

gl::Error TextureStorage11_3D::getSwizzleTexture(const TextureHelper11 **outTexture)
{
    ASSERT(outTexture);

    if (!mSwizzleTexture.valid())
    {
        const auto &format = mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps());

        D3D11_TEXTURE3D_DESC desc;
        desc.Width          = mTextureWidth;
        desc.Height         = mTextureHeight;
        desc.Depth          = mTextureDepth;
        desc.MipLevels      = mMipLevels;
        desc.Format         = format.texFormat;
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags      = 0;

        ANGLE_TRY(mRenderer->allocateTexture(desc, format, &mSwizzleTexture));
        mSwizzleTexture.setDebugName("TexStorage3D.SwizzleTexture");
    }

    *outTexture = &mSwizzleTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_3D::getSwizzleRenderTarget(int mipLevel,
                                                      const d3d11::RenderTargetView **outRTV)
{
    ASSERT(mipLevel >= 0 && mipLevel < getLevelCount());
    ASSERT(outRTV);

    if (!mSwizzleRenderTargets[mipLevel].valid())
    {
        const TextureHelper11 *swizzleTexture = nullptr;
        ANGLE_TRY(getSwizzleTexture(&swizzleTexture));

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format =
            mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps()).rtvFormat;
        rtvDesc.ViewDimension         = D3D11_RTV_DIMENSION_TEXTURE3D;
        rtvDesc.Texture3D.MipSlice    = mTopLevel + mipLevel;
        rtvDesc.Texture3D.FirstWSlice = 0;
        rtvDesc.Texture3D.WSize       = static_cast<UINT>(-1);

        ANGLE_TRY(mRenderer->allocateResource(rtvDesc, mSwizzleTexture.get(),
                                              &mSwizzleRenderTargets[mipLevel]));
        mSwizzleRenderTargets[mipLevel].setDebugName("TexStorage3D.SwizzleRTV");
    }

    *outRTV = &mSwizzleRenderTargets[mipLevel];
    return gl::NoError();
}

TextureStorage11_2DArray::TextureStorage11_2DArray(Renderer11 *renderer,
                                                   GLenum internalformat,
                                                   bool renderTarget,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei depth,
                                                   int levels)
    : TextureStorage11(
          renderer,
          GetTextureBindFlags(internalformat, renderer->getRenderer11DeviceCaps(), renderTarget),
          GetTextureMiscFlags(internalformat,
                              renderer->getRenderer11DeviceCaps(),
                              renderTarget,
                              levels),
          internalformat)
{
    // adjust size if needed for compressed textures
    d3d11::MakeValidSize(false, mFormatInfo.texFormat, &width, &height, &mTopLevel);

    mMipLevels     = mTopLevel + levels;
    mTextureWidth  = width;
    mTextureHeight = height;
    mTextureDepth  = depth;
}

gl::Error TextureStorage11_2DArray::onDestroy(const gl::Context *context)
{
    for (auto iter : mAssociatedImages)
    {
        if (iter.second)
        {
            iter.second->verifyAssociatedStorageValid(this);

            // We must let the Images recover their data before we delete it from the
            // TextureStorage.
            ANGLE_TRY(iter.second->recoverFromAssociatedStorage(context));
        }
    }
    mAssociatedImages.clear();

    InvalidateRenderTargetContainer(context, &mRenderTargets);

    return gl::NoError();
}

TextureStorage11_2DArray::~TextureStorage11_2DArray()
{
}

void TextureStorage11_2DArray::associateImage(Image11 *image, const gl::ImageIndex &index)
{
    const GLint level       = index.mipIndex;
    const GLint layerTarget = index.layerIndex;
    const GLint numLayers   = index.numLayers;

    ASSERT(0 <= level && level < getLevelCount());

    if (0 <= level && level < getLevelCount())
    {
        LevelLayerRangeKey key(level, layerTarget, numLayers);
        mAssociatedImages[key] = image;
    }
}

void TextureStorage11_2DArray::verifyAssociatedImageValid(const gl::ImageIndex &index,
                                                          Image11 *expectedImage)
{
    const GLint level       = index.mipIndex;
    const GLint layerTarget = index.layerIndex;
    const GLint numLayers   = index.numLayers;

    LevelLayerRangeKey key(level, layerTarget, numLayers);

    // This validation check should never return false. It means the Image/TextureStorage
    // association is broken.
    bool retValue = (mAssociatedImages.find(key) != mAssociatedImages.end() &&
                     (mAssociatedImages[key] == expectedImage));
    ASSERT(retValue);
}

// disassociateImage allows an Image to end its association with a Storage.
void TextureStorage11_2DArray::disassociateImage(const gl::ImageIndex &index,
                                                 Image11 *expectedImage)
{
    const GLint level       = index.mipIndex;
    const GLint layerTarget = index.layerIndex;
    const GLint numLayers   = index.numLayers;

    LevelLayerRangeKey key(level, layerTarget, numLayers);

    bool imageAssociationCorrect = (mAssociatedImages.find(key) != mAssociatedImages.end() &&
                                    (mAssociatedImages[key] == expectedImage));
    ASSERT(imageAssociationCorrect);
    mAssociatedImages[key] = nullptr;
}

// releaseAssociatedImage prepares the Storage for a new Image association. It lets the old Image
// recover its data before ending the association.
gl::Error TextureStorage11_2DArray::releaseAssociatedImage(const gl::Context *context,
                                                           const gl::ImageIndex &index,
                                                           Image11 *incomingImage)
{
    const GLint level       = index.mipIndex;
    const GLint layerTarget = index.layerIndex;
    const GLint numLayers   = index.numLayers;

    LevelLayerRangeKey key(level, layerTarget, numLayers);

    if (mAssociatedImages.find(key) != mAssociatedImages.end())
    {
        if (mAssociatedImages[key] != nullptr && mAssociatedImages[key] != incomingImage)
        {
            // Ensure that the Image is still associated with this TextureStorage.
            mAssociatedImages[key]->verifyAssociatedStorageValid(this);

            // Force the image to recover from storage before its data is overwritten.
            // This will reset mAssociatedImages[level] to nullptr too.
            ANGLE_TRY(mAssociatedImages[key]->recoverFromAssociatedStorage(context));
        }
    }

    return gl::NoError();
}

gl::Error TextureStorage11_2DArray::getResource(const gl::Context *context,
                                                const TextureHelper11 **outResource)
{
    // if the width, height or depth is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (!mTexture.valid() && mTextureWidth > 0 && mTextureHeight > 0 && mTextureDepth > 0)
    {
        ASSERT(mMipLevels > 0);

        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = mTextureWidth;
        desc.Height             = mTextureHeight;
        desc.MipLevels          = mMipLevels;
        desc.ArraySize          = mTextureDepth;
        desc.Format             = mFormatInfo.texFormat;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = getBindFlags();
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = getMiscFlags();

        ANGLE_TRY(mRenderer->allocateTexture(desc, mFormatInfo, &mTexture));
        mTexture.setDebugName("TexStorage2DArray.Texture");
    }

    *outResource = &mTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_2DArray::createSRV(const gl::Context *context,
                                              int baseLevel,
                                              int mipLevels,
                                              DXGI_FORMAT format,
                                              const TextureHelper11 &texture,
                                              d3d11::SharedSRV *outSRV)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                         = format;
    srvDesc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = mTopLevel + baseLevel;
    srvDesc.Texture2DArray.MipLevels       = mipLevels;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize       = mTextureDepth;

    ANGLE_TRY(mRenderer->allocateResource(srvDesc, texture.get(), outSRV));
    outSRV->setDebugName("TexStorage2DArray.SRV");

    return gl::NoError();
}

gl::Error TextureStorage11_2DArray::createRenderTargetSRV(const TextureHelper11 &texture,
                                                          const gl::ImageIndex &index,
                                                          DXGI_FORMAT resourceFormat,
                                                          d3d11::SharedSRV *srv) const
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                         = resourceFormat;
    srvDesc.ViewDimension                  = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = mTopLevel + index.mipIndex;
    srvDesc.Texture2DArray.MipLevels       = 1;
    srvDesc.Texture2DArray.FirstArraySlice = index.layerIndex;
    srvDesc.Texture2DArray.ArraySize       = index.numLayers;

    ANGLE_TRY(mRenderer->allocateResource(srvDesc, texture.get(), srv));

    return gl::NoError();
}

gl::Error TextureStorage11_2DArray::getRenderTarget(const gl::Context *context,
                                                    const gl::ImageIndex &index,
                                                    RenderTargetD3D **outRT)
{
    ASSERT(index.hasLayer());

    const int mipLevel = index.mipIndex;
    const int layer    = index.layerIndex;
    const int numLayers = index.numLayers;

    ASSERT(mipLevel >= 0 && mipLevel < getLevelCount());

    LevelLayerRangeKey key(mipLevel, layer, numLayers);
    if (mRenderTargets.find(key) == mRenderTargets.end())
    {
        const TextureHelper11 *texture = nullptr;
        ANGLE_TRY(getResource(context, &texture));
        d3d11::SharedSRV srv;
        ANGLE_TRY(createRenderTargetSRV(*texture, index, mFormatInfo.srvFormat, &srv));
        d3d11::SharedSRV blitSRV;
        if (mFormatInfo.blitSRVFormat != mFormatInfo.srvFormat)
        {
            ANGLE_TRY(createRenderTargetSRV(*texture, index, mFormatInfo.blitSRVFormat, &blitSRV));
        }
        else
        {
            blitSRV = srv.makeCopy();
        }

        srv.setDebugName("TexStorage2DArray.RenderTargetSRV");

        if (mFormatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format                         = mFormatInfo.rtvFormat;
            rtvDesc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice        = mTopLevel + mipLevel;
            rtvDesc.Texture2DArray.FirstArraySlice = layer;
            rtvDesc.Texture2DArray.ArraySize       = numLayers;

            d3d11::RenderTargetView rtv;
            ANGLE_TRY(mRenderer->allocateResource(rtvDesc, texture->get(), &rtv));
            rtv.setDebugName("TexStorage2DArray.RenderTargetRTV");

            mRenderTargets[key].reset(new TextureRenderTarget11(
                std::move(rtv), *texture, srv, blitSRV, mFormatInfo.internalFormat, getFormatSet(),
                getLevelWidth(mipLevel), getLevelHeight(mipLevel), 1, 0));
        }
        else
        {
            ASSERT(mFormatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN);

            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Format                         = mFormatInfo.dsvFormat;
            dsvDesc.ViewDimension                  = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice        = mTopLevel + mipLevel;
            dsvDesc.Texture2DArray.FirstArraySlice = layer;
            dsvDesc.Texture2DArray.ArraySize       = numLayers;
            dsvDesc.Flags                          = 0;

            d3d11::DepthStencilView dsv;
            ANGLE_TRY(mRenderer->allocateResource(dsvDesc, texture->get(), &dsv));
            dsv.setDebugName("TexStorage2DArray.RenderTargetDSV");

            mRenderTargets[key].reset(new TextureRenderTarget11(
                std::move(dsv), *texture, srv, mFormatInfo.internalFormat, getFormatSet(),
                getLevelWidth(mipLevel), getLevelHeight(mipLevel), 1, 0));
        }
    }

    ASSERT(outRT);
    *outRT = mRenderTargets[key].get();
    return gl::NoError();
}

gl::Error TextureStorage11_2DArray::getSwizzleTexture(const TextureHelper11 **outTexture)
{
    if (!mSwizzleTexture.valid())
    {
        const auto &format = mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps());

        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = mTextureWidth;
        desc.Height             = mTextureHeight;
        desc.MipLevels          = mMipLevels;
        desc.ArraySize          = mTextureDepth;
        desc.Format             = format.texFormat;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = 0;

        ANGLE_TRY(mRenderer->allocateTexture(desc, format, &mSwizzleTexture));
        mSwizzleTexture.setDebugName("TexStorage2DArray.SwizzleTexture");
    }

    *outTexture = &mSwizzleTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_2DArray::getSwizzleRenderTarget(int mipLevel,
                                                           const d3d11::RenderTargetView **outRTV)
{
    ASSERT(mipLevel >= 0 && mipLevel < getLevelCount());
    ASSERT(outRTV);

    if (!mSwizzleRenderTargets[mipLevel].valid())
    {
        const TextureHelper11 *swizzleTexture = nullptr;
        ANGLE_TRY(getSwizzleTexture(&swizzleTexture));

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format =
            mFormatInfo.getSwizzleFormat(mRenderer->getRenderer11DeviceCaps()).rtvFormat;
        rtvDesc.ViewDimension                  = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice        = mTopLevel + mipLevel;
        rtvDesc.Texture2DArray.FirstArraySlice = 0;
        rtvDesc.Texture2DArray.ArraySize       = mTextureDepth;

        ANGLE_TRY(mRenderer->allocateResource(rtvDesc, mSwizzleTexture.get(),
                                              &mSwizzleRenderTargets[mipLevel]));
    }

    *outRTV = &mSwizzleRenderTargets[mipLevel];
    return gl::NoError();
}

gl::ErrorOrResult<TextureStorage11::DropStencil> TextureStorage11_2DArray::ensureDropStencilTexture(
    const gl::Context *context)
{
    if (mDropStencilTexture.valid())
    {
        return DropStencil::ALREADY_EXISTS;
    }

    D3D11_TEXTURE2D_DESC dropDesc = {};
    dropDesc.ArraySize            = mTextureDepth;
    dropDesc.BindFlags            = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    dropDesc.CPUAccessFlags       = 0;
    dropDesc.Format               = DXGI_FORMAT_R32_TYPELESS;
    dropDesc.Height               = mTextureHeight;
    dropDesc.MipLevels            = mMipLevels;
    dropDesc.MiscFlags            = 0;
    dropDesc.SampleDesc.Count     = 1;
    dropDesc.SampleDesc.Quality   = 0;
    dropDesc.Usage                = D3D11_USAGE_DEFAULT;
    dropDesc.Width                = mTextureWidth;

    const auto &format =
        d3d11::Format::Get(GL_DEPTH_COMPONENT32F, mRenderer->getRenderer11DeviceCaps());
    ANGLE_TRY(mRenderer->allocateTexture(dropDesc, format, &mDropStencilTexture));
    mDropStencilTexture.setDebugName("TexStorage2DArray.DropStencil");

    std::vector<GLsizei> layerCounts(mMipLevels, mTextureDepth);

    ANGLE_TRY(initDropStencilTexture(
        context, gl::ImageIndexIterator::Make2DArray(0, mMipLevels, layerCounts.data())));

    return DropStencil::CREATED;
}

TextureStorage11_2DMultisample::TextureStorage11_2DMultisample(Renderer11 *renderer,
                                                               GLenum internalformat,
                                                               GLsizei width,
                                                               GLsizei height,
                                                               int levels,
                                                               int samples,
                                                               bool fixedSampleLocations)
    : TextureStorage11(
          renderer,
          GetTextureBindFlags(internalformat, renderer->getRenderer11DeviceCaps(), true),
          GetTextureMiscFlags(internalformat, renderer->getRenderer11DeviceCaps(), true, levels),
          internalformat),
      mTexture(),
      mRenderTarget(nullptr)
{
    // adjust size if needed for compressed textures
    d3d11::MakeValidSize(false, mFormatInfo.texFormat, &width, &height, &mTopLevel);

    mMipLevels            = 1;
    mTextureWidth         = width;
    mTextureHeight        = height;
    mTextureDepth         = 1;
    mSamples              = samples;
    mFixedSampleLocations = fixedSampleLocations;
}

gl::Error TextureStorage11_2DMultisample::onDestroy(const gl::Context *context)
{
    InvalidateRenderTarget(context, mRenderTarget.get());
    mRenderTarget.reset();
    return gl::NoError();
}

TextureStorage11_2DMultisample::~TextureStorage11_2DMultisample()
{
}

gl::Error TextureStorage11_2DMultisample::copyToStorage(const gl::Context *context,
                                                        TextureStorage *destStorage)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "copyToStorage is unimplemented";
}

void TextureStorage11_2DMultisample::associateImage(Image11 *image, const gl::ImageIndex &index)
{
}

void TextureStorage11_2DMultisample::verifyAssociatedImageValid(const gl::ImageIndex &index,
                                                                Image11 *expectedImage)
{
}

void TextureStorage11_2DMultisample::disassociateImage(const gl::ImageIndex &index,
                                                       Image11 *expectedImage)
{
}

gl::Error TextureStorage11_2DMultisample::releaseAssociatedImage(const gl::Context *context,
                                                                 const gl::ImageIndex &index,
                                                                 Image11 *incomingImage)
{
    return gl::NoError();
}

gl::Error TextureStorage11_2DMultisample::getResource(const gl::Context *context,
                                                      const TextureHelper11 **outResource)
{
    ANGLE_TRY(ensureTextureExists(1));

    *outResource = &mTexture;
    return gl::NoError();
}

gl::Error TextureStorage11_2DMultisample::ensureTextureExists(int mipLevels)
{
    // For Multisampled textures, mipLevels always equals 1.
    ASSERT(mipLevels == 1);

    // if the width or height is not positive this should be treated as an incomplete texture
    // we handle that here by skipping the d3d texture creation
    if (!mTexture.valid() && mTextureWidth > 0 && mTextureHeight > 0)
    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width          = mTextureWidth;  // Compressed texture size constraints?
        desc.Height         = mTextureHeight;
        desc.MipLevels      = mipLevels;
        desc.ArraySize      = 1;
        desc.Format         = mFormatInfo.texFormat;
        desc.Usage          = D3D11_USAGE_DEFAULT;
        desc.BindFlags      = getBindFlags();
        desc.CPUAccessFlags = 0;
        desc.MiscFlags      = getMiscFlags();

        const gl::TextureCaps &textureCaps =
            mRenderer->getNativeTextureCaps().get(mFormatInfo.internalFormat);
        GLuint supportedSamples = textureCaps.getNearestSamples(mSamples);
        desc.SampleDesc.Count   = (supportedSamples == 0) ? 1 : supportedSamples;
        desc.SampleDesc.Quality = static_cast<UINT>(D3D11_STANDARD_MULTISAMPLE_PATTERN);

        ANGLE_TRY(mRenderer->allocateTexture(desc, mFormatInfo, &mTexture));
        mTexture.setDebugName("TexStorage2DMS.Texture");
    }

    return gl::NoError();
}

gl::Error TextureStorage11_2DMultisample::getRenderTarget(const gl::Context *context,
                                                          const gl::ImageIndex &index,
                                                          RenderTargetD3D **outRT)
{
    ASSERT(!index.hasLayer());

    const int level = index.mipIndex;
    ASSERT(level == 0);

    ASSERT(outRT);
    if (mRenderTarget)
    {
        *outRT = mRenderTarget.get();
        return gl::NoError();
    }

    const TextureHelper11 *texture = nullptr;
    ANGLE_TRY(getResource(context, &texture));

    const d3d11::SharedSRV *srv = nullptr;
    ANGLE_TRY(getSRVLevel(context, level, false, &srv));

    const d3d11::SharedSRV *blitSRV = nullptr;
    ANGLE_TRY(getSRVLevel(context, level, true, &blitSRV));

    if (mFormatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format        = mFormatInfo.rtvFormat;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

        d3d11::RenderTargetView rtv;
        ANGLE_TRY(mRenderer->allocateResource(rtvDesc, texture->get(), &rtv));

        mRenderTarget.reset(new TextureRenderTarget11(
            std::move(rtv), *texture, *srv, *blitSRV, mFormatInfo.internalFormat, getFormatSet(),
            getLevelWidth(level), getLevelHeight(level), 1, mSamples));

        *outRT = mRenderTarget.get();
        return gl::NoError();
    }

    ASSERT(mFormatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Format        = mFormatInfo.dsvFormat;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
    dsvDesc.Flags         = 0;

    d3d11::DepthStencilView dsv;
    ANGLE_TRY(mRenderer->allocateResource(dsvDesc, texture->get(), &dsv));

    mRenderTarget.reset(new TextureRenderTarget11(
        std::move(dsv), *texture, *srv, mFormatInfo.internalFormat, getFormatSet(),
        getLevelWidth(level), getLevelHeight(level), 1, mSamples));

    *outRT = mRenderTarget.get();
    return gl::NoError();
}

gl::Error TextureStorage11_2DMultisample::createSRV(const gl::Context *context,
                                                    int baseLevel,
                                                    int mipLevels,
                                                    DXGI_FORMAT format,
                                                    const TextureHelper11 &texture,
                                                    d3d11::SharedSRV *outSRV)
{
    ASSERT(outSRV);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format        = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;

    ANGLE_TRY(mRenderer->allocateResource(srvDesc, texture.get(), outSRV));
    outSRV->setDebugName("TexStorage2DMS.SRV");
    return gl::NoError();
}

gl::Error TextureStorage11_2DMultisample::getSwizzleTexture(const TextureHelper11 **outTexture)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "getSwizzleTexture is unimplemented.";
}

gl::Error TextureStorage11_2DMultisample::getSwizzleRenderTarget(
    int mipLevel,
    const d3d11::RenderTargetView **outRTV)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "getSwizzleRenderTarget is unimplemented.";
}

gl::ErrorOrResult<TextureStorage11::DropStencil>
TextureStorage11_2DMultisample::ensureDropStencilTexture(const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "Drop stencil texture not implemented.";
}

}  // namespace rx
