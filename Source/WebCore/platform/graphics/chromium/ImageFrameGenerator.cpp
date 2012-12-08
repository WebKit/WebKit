/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ImageFrameGenerator.h"

#include "ImageDecoder.h"
#include "ImageDecodingStore.h"
#include "ScaledImageFragment.h"
#include "SharedBuffer.h"

#include "skia/ext/image_operations.h"

namespace WebCore {

namespace {

skia::ImageOperations::ResizeMethod resizeMethod()
{
    return skia::ImageOperations::RESIZE_LANCZOS3;
}

} // namespace

ImageFrameGenerator::ImageFrameGenerator(const SkISize& fullSize, PassRefPtr<SharedBuffer> data, bool allDataReceived)
    : m_fullSize(fullSize)
    , m_allDataReceived(false)
    , m_decodeFailedAndEmpty(false)
{
    setData(data, allDataReceived);
}

ImageFrameGenerator::~ImageFrameGenerator()
{
    // FIXME: This check is not really thread-safe. This should be changed to:
    // ImageDecodingStore::removeCacheFromInstance(this);
    // Which uses a lock internally.
    if (ImageDecodingStore::instance())
        ImageDecodingStore::instance()->removeCacheIndexedByGenerator(this);
}

void ImageFrameGenerator::setData(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    // FIXME: Doing a full copy is expensive, instead copy only new data.
    RefPtr<SharedBuffer> dataCopy = data->copy();

    MutexLocker lock(m_dataMutex);
    m_data = dataCopy;
    m_allDataReceived = allDataReceived;
}

const ScaledImageFragment* ImageFrameGenerator::decodeAndScale(const SkISize& scaledSize)
{
    // Prevents concurrent decode or scale operations on the same image data.
    // Multiple LazyDecodingPixelRefs can call this method at the same time.
    MutexLocker lock(m_decodeMutex);
    if (m_decodeFailedAndEmpty)
        return 0;

    const ScaledImageFragment* cachedImage = 0;

    cachedImage = tryToLockCompleteCache(scaledSize);
    if (cachedImage)
        return cachedImage;

    cachedImage = tryToScale(0, scaledSize);
    if (cachedImage)
        return cachedImage;

    cachedImage = tryToResumeDecodeAndScale(scaledSize);
    if (cachedImage)
        return cachedImage;

    cachedImage = tryToDecodeAndScale(scaledSize);
    if (cachedImage)
        return cachedImage;
    return 0;
}

const ScaledImageFragment* ImageFrameGenerator::tryToLockCompleteCache(const SkISize& scaledSize)
{
    const ScaledImageFragment* cachedImage = 0;
    if (ImageDecodingStore::instance()->lockCache(this, scaledSize, ImageDecodingStore::CacheMustBeComplete, &cachedImage))
        return cachedImage;
    return 0;
}

const ScaledImageFragment* ImageFrameGenerator::tryToScale(const ScaledImageFragment* fullSizeImage, const SkISize& scaledSize)
{
    // If the requested scaled size is the same as the full size then exit
    // early. This saves a cache lookup.
    if (scaledSize == m_fullSize)
        return 0;

    if (!fullSizeImage && !ImageDecodingStore::instance()->lockCache(this, m_fullSize, ImageDecodingStore::CacheMustBeComplete, &fullSizeImage))
        return 0;

    SkBitmap scaledBitmap = skia::ImageOperations::Resize(
        fullSizeImage->bitmap(), resizeMethod(), scaledSize.width(), scaledSize.height());
    OwnPtr<ScaledImageFragment> scaledImage = ScaledImageFragment::create(scaledSize, scaledBitmap, fullSizeImage->isComplete());

    ImageDecodingStore::instance()->unlockCache(this, fullSizeImage);

    const ScaledImageFragment* cachedImage = 0;

    // If a cache lookup is successful then the cache entry must be incomplete
    // therefore it is safe to overwrite.
    if (ImageDecodingStore::instance()->lockCache(this, scaledSize, ImageDecodingStore::CacheCanBeIncomplete, &cachedImage))
        return ImageDecodingStore::instance()->overwriteAndLockCache(this, cachedImage, scaledImage.release());
    return ImageDecodingStore::instance()->insertAndLockCache(this, scaledImage.release());
}

const ScaledImageFragment* ImageFrameGenerator::tryToResumeDecodeAndScale(const SkISize& scaledSize)
{
    const ScaledImageFragment* cachedImage = 0;
    ImageDecoder* cachedDecoder = 0;
    if (!ImageDecodingStore::instance()->lockCache(this, m_fullSize, ImageDecodingStore::CacheCanBeIncomplete, &cachedImage, &cachedDecoder))
        return 0;
    ASSERT(cachedDecoder);

    OwnPtr<ScaledImageFragment> fullSizeImage = decode(&cachedDecoder);

    const ScaledImageFragment* cachedFullSizeImage = ImageDecodingStore::instance()->overwriteAndLockCache(
        this, cachedImage, fullSizeImage.release());

    if (m_fullSize == scaledSize)
        return cachedFullSizeImage;
    return tryToScale(cachedFullSizeImage, scaledSize);
}

const ScaledImageFragment* ImageFrameGenerator::tryToDecodeAndScale(const SkISize& scaledSize)
{
    ImageDecoder* decoder = 0;
    OwnPtr<ScaledImageFragment> fullSizeImage = decode(&decoder);

    if (!decoder)
        return 0;

    OwnPtr<ImageDecoder> decoderContainer = adoptPtr(decoder);

    if (!fullSizeImage) {
        // If decode has failed and resulted an empty image we need to make
        // sure we don't do wasted work in the future.
        m_decodeFailedAndEmpty = decoderContainer->failed();
        return 0;
    }

    const ScaledImageFragment* cachedFullSizeImage = ImageDecodingStore::instance()->insertAndLockCache(
        this, fullSizeImage.release(), decoderContainer.release());

    if (m_fullSize == scaledSize)
        return cachedFullSizeImage;
    return tryToScale(cachedFullSizeImage, scaledSize);
}

PassOwnPtr<ScaledImageFragment> ImageFrameGenerator::decode(ImageDecoder** decoder)
{
    ASSERT(decoder);
    RefPtr<SharedBuffer> data;
    bool allDataReceived = false;
    prepareData(&data, &allDataReceived);

    // Try to create an ImageDecoder if we are not given one.
    if (!*decoder) {
        *decoder = ImageDecoder::create(*data.get(), ImageSource::AlphaPremultiplied, ImageSource::GammaAndColorProfileApplied);

        if (!*decoder && m_imageDecoderFactory)
            *decoder = m_imageDecoderFactory->create().leakPtr();

        if (!*decoder)
            return nullptr;
    }

    (*decoder)->setData(data.get(), allDataReceived);
    ImageFrame* frame = (*decoder)->frameBufferAtIndex(0);
    (*decoder)->setData(0, false); // Unref SharedBuffer from ImageDecoder.

    if (!frame || frame->status() == ImageFrame::FrameEmpty)
        return nullptr;

    bool isComplete = frame->status() == ImageFrame::FrameComplete;
    SkBitmap fullSizeBitmap = frame->getSkBitmap();
    ASSERT(fullSizeBitmap.width() == m_fullSize.width() && fullSizeBitmap.height() == m_fullSize.height());

    return ScaledImageFragment::create(m_fullSize, fullSizeBitmap, isComplete);
}

void ImageFrameGenerator::prepareData(RefPtr<SharedBuffer>* data, bool* allDataReceived)
{
    MutexLocker lock(m_dataMutex);

    // FIXME: We should do a shallow copy instead. Now we're restricted by the API of SharedBuffer.
    *data = m_data->copy();
    *allDataReceived = m_allDataReceived;
}

} // namespace WebCore
