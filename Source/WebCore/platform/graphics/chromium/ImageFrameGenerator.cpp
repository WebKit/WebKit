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
    , m_decodeFailedAndEmpty(false)
    , m_hasAlpha(true)
{
    setData(data.get(), allDataReceived);
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
    m_data.setData(data.get(), allDataReceived);
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

    DiscardablePixelRefAllocator allocator;
    // This call allocates the DiscardablePixelRef and lock/unlocks it
    // afterwards. So the memory allocated to the scaledBitmap can be
    // discarded after this call. Need to lock the scaledBitmap and
    // check the pixels before using it next time.
    SkBitmap scaledBitmap = skia::ImageOperations::Resize(fullSizeImage->bitmap(), resizeMethod(), scaledSize.width(), scaledSize.height(), &allocator);

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

    if (m_data.hasNewData()) {
        // For single frame images, the above ImageDecodingStore::lockCache()
        // call should lock the pixelRef. As a result, this lockFrameBuffers()
        // call should always succeed.
        // TODO: this does not work for the multiframe images, which are not
        // yet supported by this class.
        bool frameBuffersLocked = cachedDecoder->lockFrameBuffers();
        ASSERT_UNUSED(frameBuffersLocked, frameBuffersLocked);
        // Only do decoding if there is new data.
        OwnPtr<ScaledImageFragment> fullSizeImage = decode(&cachedDecoder);
        cachedImage = ImageDecodingStore::instance()->overwriteAndLockCache(this, cachedImage, fullSizeImage.release());
        // If the image is partially decoded, unlock the frames so that it
        // can be evicted from the memory. For fully decoded images,
        // ImageDecodingStore should have deleted the decoder here.
        if (!cachedImage->isComplete())
            cachedDecoder->unlockFrameBuffers();
    }

    if (m_fullSize == scaledSize)
        return cachedImage;
    return tryToScale(cachedImage, scaledSize);
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
    // The newly created SkBitmap in the decoder is locked. Unlock it here
    // if the image is partially decoded.
    if (!cachedFullSizeImage->isComplete())
        decoder->unlockFrameBuffers();

    if (m_fullSize == scaledSize)
        return cachedFullSizeImage;
    return tryToScale(cachedFullSizeImage, scaledSize);
}

PassOwnPtr<ScaledImageFragment> ImageFrameGenerator::decode(ImageDecoder** decoder)
{
    ASSERT(decoder);
    SharedBuffer* data = 0;
    bool allDataReceived = false;
    m_data.data(&data, &allDataReceived);

    // Try to create an ImageDecoder if we are not given one.
    if (!*decoder) {
        *decoder = ImageDecoder::create(*data, ImageSource::AlphaPremultiplied, ImageSource::GammaAndColorProfileApplied);

        if (!*decoder && m_imageDecoderFactory)
            *decoder = m_imageDecoderFactory->create().leakPtr();

        if (!*decoder)
            return nullptr;
    }

    // TODO: this is very ugly. We need to refactor the way how we can pass a
    // memory allocator to image decoders.
    (*decoder)->setMemoryAllocator(&m_allocator);
    (*decoder)->setData(data, allDataReceived);
    // If this call returns a newly allocated DiscardablePixelRef, then
    // ImageFrame::m_bitmap and the contained DiscardablePixelRef are locked.
    // They will be unlocked after the image fragment is inserted into
    // ImageDecodingStore.
    ImageFrame* frame = (*decoder)->frameBufferAtIndex(0);
    (*decoder)->setData(0, false); // Unref SharedBuffer from ImageDecoder.

    if (!frame || frame->status() == ImageFrame::FrameEmpty)
        return nullptr;

    bool isComplete = frame->status() == ImageFrame::FrameComplete;
    SkBitmap fullSizeBitmap = frame->getSkBitmap();
    {
        MutexLocker lock(m_alphaMutex);
        m_hasAlpha = !fullSizeBitmap.isOpaque();
    }
    ASSERT(fullSizeBitmap.width() == m_fullSize.width() && fullSizeBitmap.height() == m_fullSize.height());

    return ScaledImageFragment::create(m_fullSize, fullSizeBitmap, isComplete);
}

bool ImageFrameGenerator::hasAlpha()
{
    MutexLocker lock(m_alphaMutex);
    return m_hasAlpha;
}

} // namespace WebCore
