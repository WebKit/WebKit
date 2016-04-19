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
      mStagingTexture(NULL),
      mStagingSubresource(0),
      mRecoverFromStorage(false),
      mAssociatedStorage(NULL),
      mAssociatedImageIndex(gl::ImageIndex::MakeInvalid()),
      mRecoveredFromStorageCount(0)
{
}

Image11::~Image11()
{
    disassociateStorage();
    releaseStagingTexture();
}

gl::Error Image11::generateMipmap(Image11 *dest,
                                  Image11 *src,
                                  const Renderer11DeviceCaps &rendererCaps)
{
    ASSERT(src->getDXGIFormat() == dest->getDXGIFormat());
    ASSERT(src->getWidth() == 1 || src->getWidth() / 2 == dest->getWidth());
    ASSERT(src->getHeight() == 1 || src->getHeight() / 2 == dest->getHeight());

    D3D11_MAPPED_SUBRESOURCE destMapped;
    gl::Error error = dest->map(D3D11_MAP_WRITE, &destMapped);
    if (error.isError())
    {
        return error;
    }

    D3D11_MAPPED_SUBRESOURCE srcMapped;
    error = src->map(D3D11_MAP_READ, &srcMapped);
    if (error.isError())
    {
        dest->unmap();
        return error;
    }

    const uint8_t *sourceData = reinterpret_cast<const uint8_t*>(srcMapped.pData);
    uint8_t *destData = reinterpret_cast<uint8_t*>(destMapped.pData);

    auto mipGenerationFunction = d3d11::GetTextureFormatInfo(src->getInternalFormat(), rendererCaps)
                                     .formatSet->mipGenerationFunction;
    mipGenerationFunction(src->getWidth(), src->getHeight(), src->getDepth(), sourceData,
                          srcMapped.RowPitch, srcMapped.DepthPitch, destData, destMapped.RowPitch,
                          destMapped.DepthPitch);

    dest->unmap();
    src->unmap();

    dest->markDirty();

    return gl::Error(GL_NO_ERROR);
}

bool Image11::isDirty() const
{
    // If mDirty is true
    // AND mStagingTexture doesn't exist AND mStagingTexture doesn't need to be recovered from TextureStorage
    // AND the texture doesn't require init data (i.e. a blank new texture will suffice)
    // then isDirty should still return false.
    if (mDirty && !mStagingTexture && !mRecoverFromStorage)
    {
        const Renderer11DeviceCaps &deviceCaps = mRenderer->getRenderer11DeviceCaps();
        const auto &formatInfo = d3d11::GetTextureFormatInfo(mInternalFormat, deviceCaps);
        if (formatInfo.dataInitializerFunction == nullptr)
        {
            return false;
        }
    }

    return mDirty;
}

gl::Error Image11::copyToStorage(TextureStorage *storage, const gl::ImageIndex &index, const gl::Box &region)
{
    TextureStorage11 *storage11 = GetAs<TextureStorage11>(storage);

    // If an app's behavior results in an Image11 copying its data to/from to a TextureStorage multiple times,
    // then we should just keep the staging texture around to prevent the copying from impacting perf.
    // We allow the Image11 to copy its data to/from TextureStorage once.
    // This accounts for an app making a late call to glGenerateMipmap.
    bool attemptToReleaseStagingTexture = (mRecoveredFromStorageCount < 2);

    if (attemptToReleaseStagingTexture)
    {
        // If another image is relying on this Storage for its data, then we must let it recover its data before we overwrite it.
        gl::Error error = storage11->releaseAssociatedImage(index, this);
        if (error.isError())
        {
            return error;
        }
    }

    ID3D11Resource *stagingTexture = NULL;
    unsigned int stagingSubresourceIndex = 0;
    gl::Error error = getStagingTexture(&stagingTexture, &stagingSubresourceIndex);
    if (error.isError())
    {
        return error;
    }

    error = storage11->updateSubresourceLevel(stagingTexture, stagingSubresourceIndex, index, region);
    if (error.isError())
    {
        return error;
    }

    // Once the image data has been copied into the Storage, we can release it locally.
    if (attemptToReleaseStagingTexture)
    {
        storage11->associateImage(this, index);
        releaseStagingTexture();
        mRecoverFromStorage = true;
        mAssociatedStorage = storage11;
        mAssociatedImageIndex = index;
    }

    return gl::Error(GL_NO_ERROR);
}

bool Image11::isAssociatedStorageValid(TextureStorage11* textureStorage) const
{
    return (mAssociatedStorage == textureStorage);
}

gl::Error Image11::recoverFromAssociatedStorage()
{
    if (mRecoverFromStorage)
    {
        gl::Error error = createStagingTexture();
        if (error.isError())
        {
            return error;
        }

        bool textureStorageCorrect = mAssociatedStorage->isAssociatedImageValid(mAssociatedImageIndex, this);

        // This means that the cached TextureStorage has been modified after this Image11 released its copy of its data. 
        // This should not have happened. The TextureStorage should have told this Image11 to recover its data before it was overwritten.
        ASSERT(textureStorageCorrect);

        if (textureStorageCorrect)
        {
            // CopySubResource from the Storage to the Staging texture
            gl::Box region(0, 0, 0, mWidth, mHeight, mDepth);
            error = mAssociatedStorage->copySubresourceLevel(mStagingTexture, mStagingSubresource, mAssociatedImageIndex, region);
            if (error.isError())
            {
                return error;
            }

            mRecoveredFromStorageCount += 1;
        }

        // Reset all the recovery parameters, even if the texture storage association is broken.
        disassociateStorage();
    }

    return gl::Error(GL_NO_ERROR);
}

void Image11::disassociateStorage()
{
    if (mRecoverFromStorage)
    {
        // Make the texturestorage release the Image11 too
        mAssociatedStorage->disassociateImage(mAssociatedImageIndex, this);

        mRecoverFromStorage = false;
        mAssociatedStorage = NULL;
        mAssociatedImageIndex = gl::ImageIndex::MakeInvalid();
    }
}

bool Image11::redefine(GLenum target, GLenum internalformat, const gl::Extents &size, bool forceRelease)
{
    if (mWidth != size.width ||
        mHeight != size.height ||
        mInternalFormat != internalformat ||
        forceRelease)
    {
        // End the association with the TextureStorage, since that data will be out of date.
        // Also reset mRecoveredFromStorageCount since this Image is getting completely redefined.
        disassociateStorage();
        mRecoveredFromStorageCount = 0;

        mWidth = size.width;
        mHeight = size.height;
        mDepth = size.depth;
        mInternalFormat = internalformat;
        mTarget = target;

        // compute the d3d format that will be used
        const d3d11::TextureFormat &formatInfo =
            d3d11::GetTextureFormatInfo(internalformat, mRenderer->getRenderer11DeviceCaps());
        mDXGIFormat = formatInfo.formatSet->texFormat;
        mRenderable = (formatInfo.formatSet->rtvFormat != DXGI_FORMAT_UNKNOWN);

        releaseStagingTexture();
        mDirty = (formatInfo.dataInitializerFunction != NULL);

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

// Store the pixel rectangle designated by xoffset,yoffset,width,height with pixels stored as format/type at input
// into the target pixel rectangle.
gl::Error Image11::loadData(const gl::Box &area, const gl::PixelUnpackState &unpack, GLenum type, const void *input)
{
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(mInternalFormat);
    GLsizei inputRowPitch = formatInfo.computeRowPitch(type, area.width, unpack.alignment, unpack.rowLength);
    GLsizei inputDepthPitch = formatInfo.computeDepthPitch(
        type, area.width, area.height, unpack.alignment, unpack.rowLength, unpack.imageHeight);
    GLsizei inputSkipBytes = formatInfo.computeSkipPixels(
        inputRowPitch, inputDepthPitch, unpack.skipImages, unpack.skipRows, unpack.skipPixels);

    const d3d11::DXGIFormatSize &dxgiFormatInfo = d3d11::GetDXGIFormatSizeInfo(mDXGIFormat);
    GLuint outputPixelSize = dxgiFormatInfo.pixelBytes;

    const d3d11::TextureFormat &d3dFormatInfo = d3d11::GetTextureFormatInfo(mInternalFormat, mRenderer->getRenderer11DeviceCaps());
    LoadImageFunction loadFunction = d3dFormatInfo.loadFunctions.at(type).loadFunction;

    D3D11_MAPPED_SUBRESOURCE mappedImage;
    gl::Error error = map(D3D11_MAP_WRITE, &mappedImage);
    if (error.isError())
    {
        return error;
    }

    uint8_t *offsetMappedData = (reinterpret_cast<uint8_t*>(mappedImage.pData) + (area.y * mappedImage.RowPitch + area.x * outputPixelSize + area.z * mappedImage.DepthPitch));
    loadFunction(area.width, area.height, area.depth,
                 reinterpret_cast<const uint8_t *>(input) + inputSkipBytes, inputRowPitch,
                 inputDepthPitch, offsetMappedData, mappedImage.RowPitch, mappedImage.DepthPitch);

    unmap();

    return gl::Error(GL_NO_ERROR);
}

gl::Error Image11::loadCompressedData(const gl::Box &area, const void *input)
{
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(mInternalFormat);
    GLsizei inputRowPitch = formatInfo.computeRowPitch(GL_UNSIGNED_BYTE, area.width, 1, 0);
    GLsizei inputDepthPitch =
        formatInfo.computeDepthPitch(GL_UNSIGNED_BYTE, area.width, area.height, 1, 0, 0);

    const d3d11::DXGIFormatSize &dxgiFormatInfo = d3d11::GetDXGIFormatSizeInfo(mDXGIFormat);
    GLuint outputPixelSize = dxgiFormatInfo.pixelBytes;
    GLuint outputBlockWidth = dxgiFormatInfo.blockWidth;
    GLuint outputBlockHeight = dxgiFormatInfo.blockHeight;

    ASSERT(area.x % outputBlockWidth == 0);
    ASSERT(area.y % outputBlockHeight == 0);

    const d3d11::TextureFormat &d3dFormatInfo = d3d11::GetTextureFormatInfo(mInternalFormat, mRenderer->getRenderer11DeviceCaps());
    LoadImageFunction loadFunction = d3dFormatInfo.loadFunctions.at(GL_UNSIGNED_BYTE).loadFunction;

    D3D11_MAPPED_SUBRESOURCE mappedImage;
    gl::Error error = map(D3D11_MAP_WRITE, &mappedImage);
    if (error.isError())
    {
        return error;
    }

    uint8_t* offsetMappedData = reinterpret_cast<uint8_t*>(mappedImage.pData) + ((area.y / outputBlockHeight) * mappedImage.RowPitch +
                                                                           (area.x / outputBlockWidth) * outputPixelSize +
                                                                           area.z * mappedImage.DepthPitch);

    loadFunction(area.width, area.height, area.depth,
                 reinterpret_cast<const uint8_t*>(input), inputRowPitch, inputDepthPitch,
                 offsetMappedData, mappedImage.RowPitch, mappedImage.DepthPitch);

    unmap();

    return gl::Error(GL_NO_ERROR);
}

gl::Error Image11::copyFromTexStorage(const gl::ImageIndex &imageIndex, TextureStorage *source)
{
    TextureStorage11 *storage11 = GetAs<TextureStorage11>(source);

    ID3D11Resource *resource = nullptr;
    gl::Error error = storage11->getResource(&resource);
    if (error.isError())
    {
        return error;
    }

    UINT subresourceIndex         = storage11->getSubresourceIndex(imageIndex);
    TextureHelper11 textureHelper =
        TextureHelper11::MakeAndReference(resource, storage11->getANGLEFormat());

    gl::Box sourceBox(0, 0, 0, mWidth, mHeight, mDepth);
    return copyWithoutConversion(gl::Offset(), sourceBox, textureHelper, subresourceIndex);
}

gl::Error Image11::copyFromFramebuffer(const gl::Offset &destOffset,
                                       const gl::Rectangle &sourceArea,
                                       const gl::Framebuffer *sourceFBO)
{
    const gl::FramebufferAttachment *srcAttachment = sourceFBO->getReadColorbuffer();
    ASSERT(srcAttachment);

    const auto &d3d11Format = d3d11::GetTextureFormatInfo(srcAttachment->getInternalFormat(),
                                                          mRenderer->getRenderer11DeviceCaps());

    if (d3d11Format.formatSet->texFormat == mDXGIFormat)
    {
        RenderTargetD3D *renderTarget = nullptr;
        gl::Error error = srcAttachment->getRenderTarget(&renderTarget);
        if (error.isError())
        {
            return error;
        }

        RenderTarget11 *rt11 = GetAs<RenderTarget11>(renderTarget);
        ASSERT(rt11->getTexture());

        TextureHelper11 textureHelper =
            TextureHelper11::MakeAndReference(rt11->getTexture(), rt11->getANGLEFormat());
        unsigned int sourceSubResource = rt11->getSubresourceIndex();

        gl::Box sourceBox(sourceArea.x, sourceArea.y, 0, sourceArea.width, sourceArea.height, 1);
        return copyWithoutConversion(destOffset, sourceBox, textureHelper, sourceSubResource);
    }

    // This format requires conversion, so we must copy the texture to staging and manually convert
    // via readPixels
    D3D11_MAPPED_SUBRESOURCE mappedImage;
    gl::Error error = map(D3D11_MAP_WRITE, &mappedImage);
    if (error.isError())
    {
        return error;
    }

    // determine the offset coordinate into the destination buffer
    const auto &dxgiFormatInfo = d3d11::GetDXGIFormatSizeInfo(mDXGIFormat);
    GLsizei rowOffset          = dxgiFormatInfo.pixelBytes * destOffset.x;

    uint8_t *dataOffset = static_cast<uint8_t *>(mappedImage.pData) +
                          mappedImage.RowPitch * destOffset.y + rowOffset +
                          destOffset.z * mappedImage.DepthPitch;

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(mInternalFormat);

    error = mRenderer->readFromAttachment(*srcAttachment, sourceArea, formatInfo.format,
                                          formatInfo.type, mappedImage.RowPitch,
                                          gl::PixelPackState(), dataOffset);

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
    ID3D11Resource *stagingTexture       = nullptr;
    unsigned int stagingSubresourceIndex = 0;
    gl::Error error = getStagingTexture(&stagingTexture, &stagingSubresourceIndex);
    if (error.isError())
    {
        return error;
    }

    ID3D11Device *device               = mRenderer->getDevice();
    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    UINT subresourceAfterResolve = sourceSubResource;

    ID3D11Resource *srcTex     = nullptr;
    const gl::Extents &extents = textureHelper.getExtents();

    bool needResolve =
        (textureHelper.getTextureType() == GL_TEXTURE_2D && textureHelper.getSampleCount() > 1);

    if (needResolve)
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

        ID3D11Texture2D *srcTex2D = NULL;
        HRESULT result = device->CreateTexture2D(&resolveDesc, NULL, &srcTex2D);
        if (FAILED(result))
        {
            return gl::Error(GL_OUT_OF_MEMORY,
                             "Failed to create resolve texture for Image11::copy, HRESULT: 0x%X.",
                             result);
        }
        srcTex = srcTex2D;

        deviceContext->ResolveSubresource(srcTex, 0, textureHelper.getTexture2D(),
                                          sourceSubResource, textureHelper.getFormat());
        subresourceAfterResolve = 0;
    }
    else
    {
        srcTex = textureHelper.getResource();
    }

    D3D11_BOX srcBox;
    srcBox.left   = sourceArea.x;
    srcBox.right  = sourceArea.x + sourceArea.width;
    srcBox.top    = sourceArea.y;
    srcBox.bottom = sourceArea.y + sourceArea.height;
    srcBox.front  = sourceArea.z;
    srcBox.back   = sourceArea.z + sourceArea.depth;

    deviceContext->CopySubresourceRegion(stagingTexture, stagingSubresourceIndex, destOffset.x,
                                         destOffset.y, destOffset.z, srcTex,
                                         subresourceAfterResolve, &srcBox);

    if (needResolve)
    {
        SafeRelease(srcTex);
    }

    mDirty = true;
    return gl::Error(GL_NO_ERROR);
}

gl::Error Image11::getStagingTexture(ID3D11Resource **outStagingTexture, unsigned int *outSubresourceIndex)
{
    gl::Error error = createStagingTexture();
    if (error.isError())
    {
        return error;
    }

    *outStagingTexture = mStagingTexture;
    *outSubresourceIndex = mStagingSubresource;
    return gl::Error(GL_NO_ERROR);
}

void Image11::releaseStagingTexture()
{
    SafeRelease(mStagingTexture);
}

gl::Error Image11::createStagingTexture()
{
    if (mStagingTexture)
    {
        return gl::Error(GL_NO_ERROR);
    }

    ASSERT(mWidth > 0 && mHeight > 0 && mDepth > 0);

    const DXGI_FORMAT dxgiFormat = getDXGIFormat();

    ID3D11Device *device = mRenderer->getDevice();
    HRESULT result;

    int lodOffset = 1;
    GLsizei width = mWidth;
    GLsizei height = mHeight;

    // adjust size if needed for compressed textures
    d3d11::MakeValidSize(false, dxgiFormat, &width, &height, &lodOffset);

    if (mTarget == GL_TEXTURE_3D)
    {
        ID3D11Texture3D *newTexture = NULL;

        D3D11_TEXTURE3D_DESC desc;
        desc.Width = width;
        desc.Height = height;
        desc.Depth = mDepth;
        desc.MipLevels = lodOffset + 1;
        desc.Format = dxgiFormat;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        if (d3d11::GetTextureFormatInfo(mInternalFormat, mRenderer->getRenderer11DeviceCaps()).dataInitializerFunction != NULL)
        {
            std::vector<D3D11_SUBRESOURCE_DATA> initialData;
            std::vector<std::vector<BYTE>> textureData;
            d3d11::GenerateInitialTextureData(mInternalFormat, mRenderer->getRenderer11DeviceCaps(), width, height, mDepth,
                                              lodOffset + 1, &initialData, &textureData);

            result = device->CreateTexture3D(&desc, initialData.data(), &newTexture);
        }
        else
        {
            result = device->CreateTexture3D(&desc, NULL, &newTexture);
        }

        if (FAILED(result))
        {
            ASSERT(result == E_OUTOFMEMORY);
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to create staging texture, result: 0x%X.", result);
        }

        mStagingTexture = newTexture;
        mStagingSubresource = D3D11CalcSubresource(lodOffset, 0, lodOffset + 1);
    }
    else if (mTarget == GL_TEXTURE_2D || mTarget == GL_TEXTURE_2D_ARRAY || mTarget == GL_TEXTURE_CUBE_MAP)
    {
        ID3D11Texture2D *newTexture = NULL;

        D3D11_TEXTURE2D_DESC desc;
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = lodOffset + 1;
        desc.ArraySize = 1;
        desc.Format = dxgiFormat;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        if (d3d11::GetTextureFormatInfo(mInternalFormat, mRenderer->getRenderer11DeviceCaps()).dataInitializerFunction != NULL)
        {
            std::vector<D3D11_SUBRESOURCE_DATA> initialData;
            std::vector<std::vector<BYTE>> textureData;
            d3d11::GenerateInitialTextureData(mInternalFormat, mRenderer->getRenderer11DeviceCaps(), width, height, 1,
                                              lodOffset + 1, &initialData, &textureData);

            result = device->CreateTexture2D(&desc, initialData.data(), &newTexture);
        }
        else
        {
            result = device->CreateTexture2D(&desc, NULL, &newTexture);
        }

        if (FAILED(result))
        {
            ASSERT(result == E_OUTOFMEMORY);
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to create staging texture, result: 0x%X.", result);
        }

        mStagingTexture = newTexture;
        mStagingSubresource = D3D11CalcSubresource(lodOffset, 0, lodOffset + 1);
    }
    else
    {
        UNREACHABLE();
    }

    mDirty = false;
    return gl::Error(GL_NO_ERROR);
}

gl::Error Image11::map(D3D11_MAP mapType, D3D11_MAPPED_SUBRESOURCE *map)
{
    // We must recover from the TextureStorage if necessary, even for D3D11_MAP_WRITE.
    gl::Error error = recoverFromAssociatedStorage();
    if (error.isError())
    {
        return error;
    }

    ID3D11Resource *stagingTexture = NULL;
    unsigned int subresourceIndex = 0;
    error = getStagingTexture(&stagingTexture, &subresourceIndex);
    if (error.isError())
    {
        return error;
    }

    ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();

    ASSERT(mStagingTexture);
    HRESULT result = deviceContext->Map(stagingTexture, subresourceIndex, mapType, 0, map);

    if (FAILED(result))
    {
        // this can fail if the device is removed (from TDR)
        if (d3d11::isDeviceLostError(result))
        {
            mRenderer->notifyDeviceLost();
        }
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to map staging texture, result: 0x%X.", result);
    }

    mDirty = true;

    return gl::Error(GL_NO_ERROR);
}

void Image11::unmap()
{
    if (mStagingTexture)
    {
        ID3D11DeviceContext *deviceContext = mRenderer->getDeviceContext();
        deviceContext->Unmap(mStagingTexture, mStagingSubresource);
    }
}

}  // namespace rx
