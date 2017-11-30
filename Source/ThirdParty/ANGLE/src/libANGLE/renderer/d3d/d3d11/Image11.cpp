//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image11.h: Implements the rx::Image11 class, which acts as the interface to
// the actual underlying resources of a Texture

#include "libANGLE/renderer/d3d/d3d11/Image11.h"

#include "common/utilities.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"
#include "libANGLE/renderer/d3d/d3d11/TextureStorage11.h"

namespace rx
{

Image11::Image11(Renderer11 *renderer)
    : mRenderer(renderer),
      mDXGIFormat(DXGI_FORMAT_UNKNOWN),
      mStagingTexture(),
      mStagingSubresource(0),
      mRecoverFromStorage(false),
      mAssociatedStorage(nullptr),
      mAssociatedImageIndex(gl::ImageIndex::MakeInvalid()),
      mRecoveredFromStorageCount(0)
{
}

Image11::~Image11()
{
    disassociateStorage();
    releaseStagingTexture();
}

// static
gl::Error Image11::GenerateMipmap(const gl::Context *context,
                                  Image11 *dest,
                                  Image11 *src,
                                  const Renderer11DeviceCaps &rendererCaps)
{
    ASSERT(src->getDXGIFormat() == dest->getDXGIFormat());
    ASSERT(src->getWidth() == 1 || src->getWidth() / 2 == dest->getWidth());
    ASSERT(src->getHeight() == 1 || src->getHeight() / 2 == dest->getHeight());

    D3D11_MAPPED_SUBRESOURCE destMapped;
    ANGLE_TRY(dest->map(context, D3D11_MAP_WRITE, &destMapped));

    D3D11_MAPPED_SUBRESOURCE srcMapped;
    gl::Error error = src->map(context, D3D11_MAP_READ, &srcMapped);
    if (error.isError())
    {
        dest->unmap();
        return error;
    }

    const uint8_t *sourceData = reinterpret_cast<const uint8_t *>(srcMapped.pData);
    uint8_t *destData         = reinterpret_cast<uint8_t *>(destMapped.pData);

    auto mipGenerationFunction =
        d3d11::Format::Get(src->getInternalFormat(), rendererCaps).format().mipGenerationFunction;
    mipGenerationFunction(src->getWidth(), src->getHeight(), src->getDepth(), sourceData,
                          srcMapped.RowPitch, srcMapped.DepthPitch, destData, destMapped.RowPitch,
                          destMapped.DepthPitch);

    dest->unmap();
    src->unmap();

    dest->markDirty();

    return gl::NoError();
}

// static
gl::Error Image11::CopyImage(const gl::Context *context,
                             Image11 *dest,
                             Image11 *source,
                             const gl::Rectangle &sourceRect,
                             const gl::Offset &destOffset,
                             bool unpackFlipY,
                             bool unpackPremultiplyAlpha,
                             bool unpackUnmultiplyAlpha,
                             const Renderer11DeviceCaps &rendererCaps)
{
    D3D11_MAPPED_SUBRESOURCE destMapped;
    ANGLE_TRY(dest->map(context, D3D11_MAP_WRITE, &destMapped));

    D3D11_MAPPED_SUBRESOURCE srcMapped;
    gl::Error error = source->map(context, D3D11_MAP_READ, &srcMapped);
    if (error.isError())
    {
        dest->unmap();
        return error;
    }

    const auto &sourceFormat =
        d3d11::Format::Get(source->getInternalFormat(), rendererCaps).format();
    GLuint sourcePixelBytes =
        gl::GetSizedInternalFormatInfo(sourceFormat.fboImplementationInternalFormat).pixelBytes;

    GLenum destUnsizedFormat = gl::GetUnsizedFormat(dest->getInternalFormat());
    const auto &destFormat = d3d11::Format::Get(dest->getInternalFormat(), rendererCaps).format();
    const auto &destFormatInfo =
        gl::GetSizedInternalFormatInfo(destFormat.fboImplementationInternalFormat);
    GLuint destPixelBytes = destFormatInfo.pixelBytes;

    const uint8_t *sourceData = reinterpret_cast<const uint8_t *>(srcMapped.pData) +
                                sourceRect.x * sourcePixelBytes + sourceRect.y * srcMapped.RowPitch;
    uint8_t *destData = reinterpret_cast<uint8_t *>(destMapped.pData) +
                        destOffset.x * destPixelBytes + destOffset.y * destMapped.RowPitch;

    CopyImageCHROMIUM(sourceData, srcMapped.RowPitch, sourcePixelBytes,
                      sourceFormat.colorReadFunction, destData, destMapped.RowPitch, destPixelBytes,
                      destFormat.colorWriteFunction, destUnsizedFormat,
                      destFormatInfo.componentType, sourceRect.width, sourceRect.height,
                      unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha);

    dest->unmap();
    source->unmap();

    dest->markDirty();

    return gl::NoError();
}

bool Image11::isDirty() const
{
    // If mDirty is true AND mStagingTexture doesn't exist AND mStagingTexture doesn't need to be
    // recovered from TextureStorage AND the texture doesn't require init data (i.e. a blank new
    // texture will suffice) AND robust resource initialization is not enabled then isDirty should
    // still return false.
    if (mDirty && !mStagingTexture.valid() && !mRecoverFromStorage)
    {
        const Renderer11DeviceCaps &deviceCaps = mRenderer->getRenderer11DeviceCaps();
        const auto &formatInfo                 = d3d11::Format::Get(mInternalFormat, deviceCaps);
        if (formatInfo.dataInitializerFunction == nullptr)
        {
            return false;
        }
    }

    return mDirty;
}

gl::Error Image11::copyToStorage(const gl::Context *context,
                                 TextureStorage *storage,
                                 const gl::ImageIndex &index,
                                 const gl::Box &region)
{
    TextureStorage11 *storage11 = GetAs<TextureStorage11>(storage);

    // If an app's behavior results in an Image11 copying its data to/from to a TextureStorage
    // multiple times, then we should just keep the staging texture around to prevent the copying
    // from impacting perf. We allow the Image11 to copy its data to/from TextureStorage once. This
    // accounts for an app making a late call to glGenerateMipmap.
    bool attemptToReleaseStagingTexture = (mRecoveredFromStorageCount < 2);

    if (attemptToReleaseStagingTexture)
    {
        // If another image is relying on this Storage for its data, then we must let it recover its
        // data before we overwrite it.
        ANGLE_TRY(storage11->releaseAssociatedImage(context, index, this));
    }

    const TextureHelper11 *stagingTexture = nullptr;
    unsigned int stagingSubresourceIndex = 0;
    ANGLE_TRY(getStagingTexture(&stagingTexture, &stagingSubresourceIndex));
    ANGLE_TRY(storage11->updateSubresourceLevel(context, *stagingTexture, stagingSubresourceIndex,
                                                index, region));

    // Once the image data has been copied into the Storage, we can release it locally.
    if (attemptToReleaseStagingTexture)
    {
        storage11->associateImage(this, index);
        releaseStagingTexture();
        mRecoverFromStorage   = true;
        mAssociatedStorage    = storage11;
        mAssociatedImageIndex = index;
    }

    return gl::NoError();
}

void Image11::verifyAssociatedStorageValid(TextureStorage11 *textureStorage) const
{
    ASSERT(mAssociatedStorage == textureStorage);
}

gl::Error Image11::recoverFromAssociatedStorage(const gl::Context *context)
{
    if (mRecoverFromStorage)
    {
        ANGLE_TRY(createStagingTexture());

        mAssociatedStorage->verifyAssociatedImageValid(mAssociatedImageIndex, this);

        // CopySubResource from the Storage to the Staging texture
        gl::Box region(0, 0, 0, mWidth, mHeight, mDepth);
        ANGLE_TRY(mAssociatedStorage->copySubresourceLevel(
            context, mStagingTexture, mStagingSubresource, mAssociatedImageIndex, region));
        mRecoveredFromStorageCount += 1;

        // Reset all the recovery parameters, even if the texture storage association is broken.
        disassociateStorage();
    }

    return gl::NoError();
}

void Image11::disassociateStorage()
{
    if (mRecoverFromStorage)
    {
        // Make the texturestorage release the Image11 too
        mAssociatedStorage->disassociateImage(mAssociatedImageIndex, this);

        mRecoverFromStorage   = false;
        mAssociatedStorage    = nullptr;
        mAssociatedImageIndex = gl::ImageIndex::MakeInvalid();
    }
}

bool Image11::redefine(GLenum target,
                       GLenum internalformat,
                       const gl::Extents &size,
                       bool forceRelease)
{
    if (mWidth != size.width || mHeight != size.height || mInternalFormat != internalformat ||
        forceRelease)
    {
        // End the association with the TextureStorage, since that data will be out of date.
        // Also reset mRecoveredFromStorageCount since this Image is getting completely redefined.
        disassociateStorage();
        mRecoveredFromStorageCount = 0;

        mWidth          = size.width;
        mHeight         = size.height;
        mDepth          = size.depth;
        mInternalFormat = internalformat;
        mTarget         = target;

        // compute the d3d format that will be used
        const d3d11::Format &formatInfo =
            d3d11::Format::Get(internalformat, mRenderer->getRenderer11DeviceCaps());
        mDXGIFormat = formatInfo.texFormat;
        mRenderable = (formatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN);

        releaseStagingTexture();
        mDirty = (formatInfo.dataInitializerFunction != nullptr);

        return true;
    }

    return false;
}

DXGI_FORMAT Image11::getDXGIFormat() const
{
    // this should only happen if the image hasn't been redefined first
    // which would be a bug by the caller
    ASSERT(mDXGIFormat != DXGI_FORMAT_UNKNOWN);

    return mDXGIFormat;
}

// Store the pixel rectangle designated by xoffset,yoffset,width,height with pixels stored as
// format/type at input
// into the target pixel rectangle.
gl::Error Image11::loadData(const gl::Context *context,
                            const gl::Box &area,
                            const gl::PixelUnpackState &unpack,
                            GLenum type,
                            const void *input,
                            bool applySkipImages)
{
    const gl::InternalFormat &formatInfo = gl::GetSizedInternalFormatInfo(mInternalFormat);
    GLuint inputRowPitch                 = 0;
    ANGLE_TRY_RESULT(
        formatInfo.computeRowPitch(type, area.width, unpack.alignment, unpack.rowLength),
        inputRowPitch);
    GLuint inputDepthPitch = 0;
    ANGLE_TRY_RESULT(formatInfo.computeDepthPitch(area.height, unpack.imageHeight, inputRowPitch),
                     inputDepthPitch);
    GLuint inputSkipBytes = 0;
    ANGLE_TRY_RESULT(
        formatInfo.computeSkipBytes(inputRowPitch, inputDepthPitch, unpack, applySkipImages),
        inputSkipBytes);

    const d3d11::DXGIFormatSize &dxgiFormatInfo = d3d11::GetDXGIFormatSizeInfo(mDXGIFormat);
    GLuint outputPixelSize                      = dxgiFormatInfo.pixelBytes;

    const d3d11::Format &d3dFormatInfo =
        d3d11::Format::Get(mInternalFormat, mRenderer->getRenderer11DeviceCaps());
    LoadImageFunction loadFunction = d3dFormatInfo.getLoadFunctions()(type).loadFunction;

    D3D11_MAPPED_SUBRESOURCE mappedImage;
    ANGLE_TRY(map(context, D3D11_MAP_WRITE, &mappedImage));

    uint8_t *offsetMappedData = (reinterpret_cast<uint8_t *>(mappedImage.pData) +
                                 (area.y * mappedImage.RowPitch + area.x * outputPixelSize +
                                  area.z * mappedImage.DepthPitch));
    loadFunction(area.width, area.height, area.depth,
                 reinterpret_cast<const uint8_t *>(input) + inputSkipBytes, inputRowPitch,
                 inputDepthPitch, offsetMappedData, mappedImage.RowPitch, mappedImage.DepthPitch);

    unmap();

    return gl::NoError();
}

gl::Error Image11::loadCompressedData(const gl::Context *context,
                                      const gl::Box &area,
                                      const void *input)
{
    const gl::InternalFormat &formatInfo = gl::GetSizedInternalFormatInfo(mInternalFormat);
    GLsizei inputRowPitch                = 0;
    ANGLE_TRY_RESULT(formatInfo.computeRowPitch(GL_UNSIGNED_BYTE, area.width, 1, 0), inputRowPitch);
    GLsizei inputDepthPitch = 0;
    ANGLE_TRY_RESULT(formatInfo.computeDepthPitch(area.height, 0, inputRowPitch), inputDepthPitch);

    const d3d11::DXGIFormatSize &dxgiFormatInfo = d3d11::GetDXGIFormatSizeInfo(mDXGIFormat);
    GLuint outputPixelSize                      = dxgiFormatInfo.pixelBytes;
    GLuint outputBlockWidth                     = dxgiFormatInfo.blockWidth;
    GLuint outputBlockHeight                    = dxgiFormatInfo.blockHeight;

    ASSERT(area.x % outputBlockWidth == 0);
    ASSERT(area.y % outputBlockHeight == 0);

    const d3d11::Format &d3dFormatInfo =
        d3d11::Format::Get(mInternalFormat, mRenderer->getRenderer11DeviceCaps());
    LoadImageFunction loadFunction =
        d3dFormatInfo.getLoadFunctions()(GL_UNSIGNED_BYTE).loadFunction;

    D3D11_MAPPED_SUBRESOURCE mappedImage;
    ANGLE_TRY(map(context, D3D11_MAP_WRITE, &mappedImage));

    uint8_t *offsetMappedData =
        reinterpret_cast<uint8_t *>(mappedImage.pData) +
        ((area.y / outputBlockHeight) * mappedImage.RowPitch +
         (area.x / outputBlockWidth) * outputPixelSize + area.z * mappedImage.DepthPitch);

    loadFunction(area.width, area.height, area.depth, reinterpret_cast<const uint8_t *>(input),
                 inputRowPitch, inputDepthPitch, offsetMappedData, mappedImage.RowPitch,
                 mappedImage.DepthPitch);

    unmap();

    return gl::NoError();
}

gl::Error Image11::copyFromTexStorage(const gl::Context *context,
                                      const gl::ImageIndex &imageIndex,
                                      TextureStorage *source)
{
    TextureStorage11 *storage11 = GetAs<TextureStorage11>(source);

    const TextureHelper11 *textureHelper = nullptr;
    ANGLE_TRY(storage11->getResource(context, &textureHelper));

    UINT subresourceIndex = storage11->getSubresourceIndex(imageIndex);

    gl::Box sourceBox(0, 0, 0, mWidth, mHeight, mDepth);
    return copyWithoutConversion(gl::Offset(), sourceBox, *textureHelper, subresourceIndex);
}

gl::Error Image11::copyFromFramebuffer(const gl::Context *context,
                                       const gl::Offset &destOffset,
                                       const gl::Rectangle &sourceArea,
                                       const gl::Framebuffer *sourceFBO)
{
    const gl::FramebufferAttachment *srcAttachment = sourceFBO->getReadColorbuffer();
    ASSERT(srcAttachment);

    GLenum sourceInternalFormat = srcAttachment->getFormat().info->sizedInternalFormat;
    const auto &d3d11Format =
        d3d11::Format::Get(sourceInternalFormat, mRenderer->getRenderer11DeviceCaps());

    if (d3d11Format.texFormat == mDXGIFormat && sourceInternalFormat == mInternalFormat)
    {
        RenderTarget11 *rt11 = nullptr;
        ANGLE_TRY(srcAttachment->getRenderTarget(context, &rt11));
        ASSERT(rt11->getTexture().get());

        TextureHelper11 textureHelper  = rt11->getTexture();
        unsigned int sourceSubResource = rt11->getSubresourceIndex();

        gl::Box sourceBox(sourceArea.x, sourceArea.y, 0, sourceArea.width, sourceArea.height, 1);
        return copyWithoutConversion(destOffset, sourceBox, textureHelper, sourceSubResource);
    }

    // This format requires conversion, so we must copy the texture to staging and manually convert
    // via readPixels
    D3D11_MAPPED_SUBRESOURCE mappedImage;
    ANGLE_TRY(map(context, D3D11_MAP_WRITE, &mappedImage));

    // determine the offset coordinate into the destination buffer
    const auto &dxgiFormatInfo = d3d11::GetDXGIFormatSizeInfo(mDXGIFormat);
    GLsizei rowOffset          = dxgiFormatInfo.pixelBytes * destOffset.x;

    uint8_t *dataOffset = static_cast<uint8_t *>(mappedImage.pData) +
                          mappedImage.RowPitch * destOffset.y + rowOffset +
                          destOffset.z * mappedImage.DepthPitch;

    const gl::InternalFormat &destFormatInfo = gl::GetSizedInternalFormatInfo(mInternalFormat);
    const auto &destD3D11Format =
        d3d11::Format::Get(mInternalFormat, mRenderer->getRenderer11DeviceCaps());

    auto loadFunction = destD3D11Format.getLoadFunctions()(destFormatInfo.type);
    gl::Error error   = gl::NoError();
    if (loadFunction.requiresConversion)
    {
        size_t bufferSize = destFormatInfo.pixelBytes * sourceArea.width * sourceArea.height;
        angle::MemoryBuffer *memoryBuffer = nullptr;
        error = mRenderer->getScratchMemoryBuffer(bufferSize, &memoryBuffer);

        if (!error.isError())
        {
            GLuint memoryBufferRowPitch = destFormatInfo.pixelBytes * sourceArea.width;

            error = mRenderer->readFromAttachment(
                context, *srcAttachment, sourceArea, destFormatInfo.format, destFormatInfo.type,
                memoryBufferRowPitch, gl::PixelPackState(), memoryBuffer->data());

            loadFunction.loadFunction(sourceArea.width, sourceArea.height, 1, memoryBuffer->data(),
                                      memoryBufferRowPitch, 0, dataOffset, mappedImage.RowPitch,
                                      mappedImage.DepthPitch);
        }
    }
    else
    {
        error = mRenderer->readFromAttachment(
            context, *srcAttachment, sourceArea, destFormatInfo.format, destFormatInfo.type,
            mappedImage.RowPitch, gl::PixelPackState(), dataOffset);
    }

    unmap();
    mDirty = true;

    return error;
}

gl::Error Image11::copyWithoutConversion(const gl::Offset &destOffset,
                                         const gl::Box &sourceArea,
                                         const TextureHelper11 &textureHelper,
                                         UINT sourceSubResource)
{
    // No conversion needed-- use copyback fastpath
    const TextureHelper11 *stagingTexture = nullptr;
    unsigned int stagingSubresourceIndex = 0;
    ANGLE_TRY(getStagingTexture(&stagingTexture, &stagingSubresourceIndex));

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    const gl::Extents &extents = textureHelper.getExtents();

    D3D11_BOX srcBox;
    srcBox.left   = sourceArea.x;
    srcBox.right  = sourceArea.x + sourceArea.width;
    srcBox.top    = sourceArea.y;
    srcBox.bottom = sourceArea.y + sourceArea.height;
    srcBox.front  = sourceArea.z;
    srcBox.back   = sourceArea.z + sourceArea.depth;

    if (textureHelper.is2D() && textureHelper.getSampleCount() > 1)
    {
        D3D11_TEXTURE2D_DESC resolveDesc;
        resolveDesc.Width              = extents.width;
        resolveDesc.Height             = extents.height;
        resolveDesc.MipLevels          = 1;
        resolveDesc.ArraySize          = 1;
        resolveDesc.Format             = textureHelper.getFormat();
        resolveDesc.SampleDesc.Count   = 1;
        resolveDesc.SampleDesc.Quality = 0;
        resolveDesc.Usage              = D3D11_USAGE_DEFAULT;
        resolveDesc.BindFlags          = 0;
        resolveDesc.CPUAccessFlags     = 0;
        resolveDesc.MiscFlags          = 0;

        d3d11::Texture2D resolveTex;
        ANGLE_TRY(mRenderer->allocateResource(resolveDesc, &resolveTex));

        deviceContext->ResolveSubresource(resolveTex.get(), 0, textureHelper.get(),
                                          sourceSubResource, textureHelper.getFormat());

        deviceContext->CopySubresourceRegion(stagingTexture->get(), stagingSubresourceIndex,
                                             destOffset.x, destOffset.y, destOffset.z,
                                             resolveTex.get(), 0, &srcBox);
    }
    else
    {
        deviceContext->CopySubresourceRegion(stagingTexture->get(), stagingSubresourceIndex,
                                             destOffset.x, destOffset.y, destOffset.z,
                                             textureHelper.get(), sourceSubResource, &srcBox);
    }

    mDirty = true;
    return gl::NoError();
}

gl::Error Image11::getStagingTexture(const TextureHelper11 **outStagingTexture,
                                     unsigned int *outSubresourceIndex)
{
    ANGLE_TRY(createStagingTexture());

    *outStagingTexture   = &mStagingTexture;
    *outSubresourceIndex = mStagingSubresource;
    return gl::NoError();
}

void Image11::releaseStagingTexture()
{
    mStagingTexture.reset();
}

gl::Error Image11::createStagingTexture()
{
    if (mStagingTexture.valid())
    {
        return gl::NoError();
    }

    ASSERT(mWidth > 0 && mHeight > 0 && mDepth > 0);

    const DXGI_FORMAT dxgiFormat = getDXGIFormat();
    const auto &formatInfo =
        d3d11::Format::Get(mInternalFormat, mRenderer->getRenderer11DeviceCaps());

    int lodOffset  = 1;
    GLsizei width  = mWidth;
    GLsizei height = mHeight;

    // adjust size if needed for compressed textures
    d3d11::MakeValidSize(false, dxgiFormat, &width, &height, &lodOffset);

    if (mTarget == GL_TEXTURE_3D)
    {
        D3D11_TEXTURE3D_DESC desc;
        desc.Width          = width;
        desc.Height         = height;
        desc.Depth          = mDepth;
        desc.MipLevels      = lodOffset + 1;
        desc.Format         = dxgiFormat;
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.BindFlags      = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags      = 0;

        if (formatInfo.dataInitializerFunction != nullptr)
        {
            std::vector<D3D11_SUBRESOURCE_DATA> initialData;
            std::vector<std::vector<BYTE>> textureData;
            d3d11::GenerateInitialTextureData(mInternalFormat, mRenderer->getRenderer11DeviceCaps(),
                                              width, height, mDepth, lodOffset + 1, &initialData,
                                              &textureData);

            ANGLE_TRY(
                mRenderer->allocateTexture(desc, formatInfo, initialData.data(), &mStagingTexture));
        }
        else
        {
            ANGLE_TRY(mRenderer->allocateTexture(desc, formatInfo, &mStagingTexture));
        }

        mStagingTexture.setDebugName("Image11::StagingTexture3D");
        mStagingSubresource = D3D11CalcSubresource(lodOffset, 0, lodOffset + 1);
    }
    else if (mTarget == GL_TEXTURE_2D || mTarget == GL_TEXTURE_2D_ARRAY ||
             mTarget == GL_TEXTURE_CUBE_MAP)
    {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = width;
        desc.Height             = height;
        desc.MipLevels          = lodOffset + 1;
        desc.ArraySize          = 1;
        desc.Format             = dxgiFormat;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_STAGING;
        desc.BindFlags          = 0;
        desc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags          = 0;

        if (formatInfo.dataInitializerFunction != nullptr)
        {
            std::vector<D3D11_SUBRESOURCE_DATA> initialData;
            std::vector<std::vector<BYTE>> textureData;
            d3d11::GenerateInitialTextureData(mInternalFormat, mRenderer->getRenderer11DeviceCaps(),
                                              width, height, 1, lodOffset + 1, &initialData,
                                              &textureData);

            ANGLE_TRY(
                mRenderer->allocateTexture(desc, formatInfo, initialData.data(), &mStagingTexture));
        }
        else
        {
            ANGLE_TRY(mRenderer->allocateTexture(desc, formatInfo, &mStagingTexture));
        }

        mStagingTexture.setDebugName("Image11::StagingTexture2D");
        mStagingSubresource = D3D11CalcSubresource(lodOffset, 0, lodOffset + 1);
    }
    else
    {
        UNREACHABLE();
    }

    mDirty = false;
    return gl::NoError();
}

gl::Error Image11::map(const gl::Context *context, D3D11_MAP mapType, D3D11_MAPPED_SUBRESOURCE *map)
{
    // We must recover from the TextureStorage if necessary, even for D3D11_MAP_WRITE.
    ANGLE_TRY(recoverFromAssociatedStorage(context));

    const TextureHelper11 *stagingTexture = nullptr;
    unsigned int subresourceIndex  = 0;
    ANGLE_TRY(getStagingTexture(&stagingTexture, &subresourceIndex));

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    ASSERT(stagingTexture && stagingTexture->valid());
    HRESULT result = deviceContext->Map(stagingTexture->get(), subresourceIndex, mapType, 0, map);

    if (FAILED(result))
    {
        // this can fail if the device is removed (from TDR)
        if (d3d11::isDeviceLostError(result))
        {
            mRenderer->notifyDeviceLost();
        }
        return gl::OutOfMemory() << "Failed to map staging texture, " << gl::FmtHR(result);
    }

    mDirty = true;

    return gl::NoError();
}

void Image11::unmap()
{
    if (mStagingTexture.valid())
    {
        ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();
        deviceContext->Unmap(mStagingTexture.get(), mStagingSubresource);
    }
}

}  // namespace rx
