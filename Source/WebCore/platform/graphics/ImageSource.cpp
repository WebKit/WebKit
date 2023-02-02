/*
 * Copyright (C) 2016-2021 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageSource.h"

#include "BitmapImage.h"
#include "ImageDecoder.h"
#include "ImageObserver.h"
#include "Logging.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/SystemTracing.h>
#include <wtf/URL.h>

namespace WebCore {

ImageSource::ImageSource(BitmapImage* image, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : m_image(image)
    , m_alphaOption(alphaOption)
    , m_gammaAndColorProfileOption(gammaAndColorProfileOption)
    , m_runLoop(RunLoop::current())
{
}

ImageSource::ImageSource(Ref<NativeImage>&& nativeImage)
    : m_runLoop(RunLoop::current())
{
    m_frameCount = 1;
    m_encodedDataStatus = EncodedDataStatus::Complete;
    m_cachedMetadata.add({ MetadataType::EncodedDataStatus, MetadataType::FrameCount });
    growFrames();

    setNativeImage(WTFMove(nativeImage));

    m_decodedSize = m_frames[0].frameBytes();

    m_size = m_frames[0].size();
    m_orientation = ImageOrientation(ImageOrientation::Orientation::None);
    m_cachedMetadata.add({ MetadataType::Orientation, MetadataType::Size });
}

ImageSource::~ImageSource()
{
    ASSERT(!hasAsyncDecodingQueue());
    ASSERT(&m_runLoop == &RunLoop::current());
}

bool ImageSource::ensureDecoderAvailable(FragmentedSharedBuffer* data)
{
    if (!data || isDecoderAvailable())
        return true;

    m_decoder = ImageDecoder::create(*data, mimeType(), m_alphaOption, m_gammaAndColorProfileOption);
    if (!isDecoderAvailable())
        return false;

    m_decoder->setEncodedDataStatusChangeCallback([weakThis = WeakPtr { *this }] (auto status) {
        if (weakThis)
            weakThis->encodedDataStatusChanged(status);
    });

    if (auto expectedContentSize = expectedContentLength())
        m_decoder->setExpectedContentSize(expectedContentSize);

    // Changing the decoder has to stop the decoding thread. The current frame will
    // continue decoding safely because the decoding thread has its own
    // reference of the old decoder.
    stopAsyncDecodingQueue();
    return true;
}

void ImageSource::setData(FragmentedSharedBuffer* data, bool allDataReceived)
{
    if (!data || !ensureDecoderAvailable(data))
        return;

    m_decoder->setData(*data, allDataReceived);
}

void ImageSource::resetData(FragmentedSharedBuffer* data)
{
    m_decoder = nullptr;
    setData(data, isAllDataReceived());
}

EncodedDataStatus ImageSource::dataChanged(FragmentedSharedBuffer* data, bool allDataReceived)
{
    setData(data, allDataReceived);
    clearMetadata();
    EncodedDataStatus status = encodedDataStatus();
    if (status >= EncodedDataStatus::SizeAvailable)
        growFrames();
    return status;
}

bool ImageSource::isAllDataReceived()
{
    return isDecoderAvailable() ? m_decoder->isAllDataReceived() : frameCount();
}

void ImageSource::destroyDecodedData(size_t begin, size_t end)
{
    if (begin >= end)
        return;

    ASSERT(end <= m_frames.size());

    unsigned decodedSize = 0;

    for (size_t index = begin; index < end; ++index)
        decodedSize += m_frames[index].clearImage();

    decodedSizeReset(decodedSize);
}

void ImageSource::destroyIncompleteDecodedData()
{
    unsigned decodedSize = 0;

    for (auto& frame : m_frames) {
        if (!frame.hasMetadata() || frame.isComplete())
            continue;

        decodedSize += frame.clear();
    }

    decodedSizeDecreased(decodedSize);
}

void ImageSource::clearFrameBufferCache(size_t beforeFrame)
{
    if (!isDecoderAvailable())
        return;
    m_decoder->clearFrameBufferCache(beforeFrame);
}

void ImageSource::encodedDataStatusChanged(EncodedDataStatus status)
{
    if (status == m_encodedDataStatus)
        return;

    m_encodedDataStatus = status;

    if (status >= EncodedDataStatus::SizeAvailable)
        growFrames();

    if (m_image && m_image->imageObserver())
        m_image->imageObserver()->encodedDataStatusChanged(*m_image, status);
}

void ImageSource::decodedSizeChanged(long long decodedSize)
{
    if (!decodedSize || !m_image || !m_image->imageObserver())
        return;

    m_image->imageObserver()->decodedSizeChanged(*m_image, decodedSize);
}

void ImageSource::decodedSizeIncreased(unsigned decodedSize)
{
    if (!decodedSize)
        return;

    m_decodedSize += decodedSize;

    // The fully-decoded frame will subsume the partially decoded data used
    // to determine image properties.
    long long changeSize = static_cast<long long>(decodedSize) - m_decodedPropertiesSize;
    m_decodedPropertiesSize = 0;
    decodedSizeChanged(changeSize);
}

void ImageSource::decodedSizeDecreased(unsigned decodedSize)
{
    if (!decodedSize)
        return;

    ASSERT(m_decodedSize >= decodedSize);
    m_decodedSize -= decodedSize;
    decodedSizeChanged(-static_cast<long long>(decodedSize));
}

void ImageSource::decodedSizeReset(unsigned decodedSize)
{
    ASSERT(m_decodedSize >= decodedSize);
    m_decodedSize -= decodedSize;

    // Clearing the ImageSource destroys the extra decoded data used for
    // determining image properties.
    decodedSize += m_decodedPropertiesSize;
    m_decodedPropertiesSize = 0;
    decodedSizeChanged(-static_cast<long long>(decodedSize));
}

void ImageSource::didDecodeProperties(unsigned decodedPropertiesSize)
{
    if (m_decodedSize)
        return;

    long long decodedSize = static_cast<long long>(decodedPropertiesSize) - m_decodedPropertiesSize;
    m_decodedPropertiesSize = decodedPropertiesSize;
    decodedSizeChanged(decodedSize);
}

void ImageSource::growFrames()
{
    ASSERT(isSizeAvailable());
    auto newSize = frameCount();
    if (newSize > m_frames.size())
        m_frames.grow(newSize);
}

void ImageSource::setNativeImage(Ref<NativeImage>&& nativeImage)
{
    ASSERT(m_frames.size() == 1);
    ImageFrame& frame = m_frames[0];

    ASSERT(!isDecoderAvailable());

    frame.m_nativeImage = WTFMove(nativeImage);

    frame.m_decodingStatus = DecodingStatus::Complete;
    frame.m_size = frame.m_nativeImage->size();
    frame.m_hasAlpha = frame.m_nativeImage->hasAlpha();
}

void ImageSource::cacheMetadataAtIndex(size_t index, SubsamplingLevel subsamplingLevel, DecodingStatus decodingStatus)
{
    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];

    ASSERT(isDecoderAvailable());
    if (decodingStatus == DecodingStatus::Invalid)
        frame.m_decodingStatus = m_decoder->frameIsCompleteAtIndex(index) ? DecodingStatus::Complete : DecodingStatus::Partial;
    else
        frame.m_decodingStatus = decodingStatus;

    if (frame.hasMetadata())
        return;

    frame.m_subsamplingLevel = subsamplingLevel;

    if (frame.m_decodingOptions.hasSizeForDrawing()) {
        ASSERT(frame.hasNativeImage());
        frame.m_size = frame.nativeImage()->size();
    } else
        frame.m_size = m_decoder->frameSizeAtIndex(index, subsamplingLevel);

    auto metadata = m_decoder->frameMetadataAtIndex(index);
    frame.m_orientation = metadata.orientation;
    frame.m_densityCorrectedSize = metadata.densityCorrectedSize;
    frame.m_hasAlpha = m_decoder->frameHasAlphaAtIndex(index);

    if (repetitionCount())
        frame.m_duration = m_decoder->frameDurationAtIndex(index);
}

void ImageSource::cachePlatformImageAtIndex(PlatformImagePtr&& platformImage, size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& decodingOptions, DecodingStatus decodingStatus)
{
    ASSERT(index < m_frames.size());
    ImageFrame& frame = m_frames[index];

    // Clear the current image frame and update the observer with this clearance.
    decodedSizeDecreased(frame.clear());

    // Do not cache the NativeImage if adding its frameByes to the MemoryCache will cause numerical overflow.
    size_t frameBytes = size().unclampedArea() * sizeof(uint32_t);
    if (!isInBounds<unsigned>(frameBytes + decodedSize()))
        return;

    // Move the new image to the cache.
    frame.m_nativeImage = NativeImage::create(WTFMove(platformImage));
    frame.m_decodingOptions = decodingOptions;
    cacheMetadataAtIndex(index, subsamplingLevel, decodingStatus);

    // Update the observer with the new image frame bytes.
    decodedSizeIncreased(frame.frameBytes());
}

void ImageSource::cachePlatformImageAtIndexAsync(PlatformImagePtr&& platformImage, size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& decodingOptions, DecodingStatus decodingStatus)
{
    if (!isDecoderAvailable())
        return;

    ASSERT(index < m_frames.size());

    // Clean the old native image and set a new one
    cachePlatformImageAtIndex(WTFMove(platformImage), index, subsamplingLevel, decodingOptions, decodingStatus);
    LOG(Images, "ImageSource::%s - %p - url: %s [frame %ld has been cached]", __FUNCTION__, this, sourceURL().string().utf8().data(), index);

    // Notify the image with the readiness of the new frame NativeImage.
    if (m_image)
        m_image->imageFrameAvailableAtIndex(index);
}

WorkQueue& ImageSource::decodingQueue()
{
    if (!m_decodingQueue)
        m_decodingQueue = WorkQueue::create("org.webkit.ImageDecoder", WorkQueue::QOS::Default);

    return *m_decodingQueue;
}

ImageSource::FrameRequestQueue& ImageSource::frameRequestQueue()
{
    if (!m_frameRequestQueue)
        m_frameRequestQueue = FrameRequestQueue::create();

    return *m_frameRequestQueue;
}

bool ImageSource::canUseAsyncDecoding()
{
    if (!isDecoderAvailable())
        return false;
    // FIXME: figure out the best heuristic for enabling async image decoding.
    return size().area() * sizeof(uint32_t) >= (frameCount() > 1 ? 100 * KB : 500 * KB);
}

void ImageSource::startAsyncDecodingQueue()
{
    if (hasAsyncDecodingQueue() || !isDecoderAvailable())
        return;

    // Async decoding is only enabled for HTMLImageElement and CSS background images.
    ASSERT(isMainThread());

    // We need to protect this, m_decodingQueue and m_decoder from being deleted while we are in the decoding loop.
    decodingQueue().dispatch([protectedThis = Ref { *this }, protectedDecodingQueue = Ref { decodingQueue() }, protectedFrameRequestQueue = Ref { frameRequestQueue() }, protectedDecoder = Ref { *m_decoder }, sourceURL = sourceURL().string().isolatedCopy()] () mutable {
        ImageFrameRequest frameRequest;
        Seconds minDecodingDuration = protectedThis->frameDecodingDurationForTesting();

        while (protectedFrameRequestQueue->dequeue(frameRequest)) {
            TraceScope tracingScope(AsyncImageDecodeStart, AsyncImageDecodeEnd);

            MonotonicTime startingTime;
            if (minDecodingDuration > 0_s)
                startingTime = MonotonicTime::now();

            // Get the frame NativeImage on the decoding thread.
            auto platformImage = protectedDecoder->createFrameImageAtIndex(frameRequest.index, frameRequest.subsamplingLevel, frameRequest.decodingOptions);
            if (platformImage)
                LOG(Images, "ImageSource::%s - %p - url: %s [frame %ld has been decoded]", __FUNCTION__, protectedThis.ptr(), sourceURL.utf8().data(), frameRequest.index);
            else {
                LOG(Images, "ImageSource::%s - %p - url: %s [decoding for frame %ld has failed]", __FUNCTION__, protectedThis.ptr(), sourceURL.utf8().data(), frameRequest.index);
                continue;
            }

            // Pretend as if the decoding takes minDecodingDuration.
            if (minDecodingDuration > 0_s)
                sleep(minDecodingDuration - (MonotonicTime::now() - startingTime));

            // Update the cached frames on the creation thread to avoid updating the MemoryCache from a different thread.
            callOnMainThread([protectedThis, protectedDecodingQueue, protectedDecoder, sourceURL = WTFMove(sourceURL).isolatedCopy(), platformImage = WTFMove(platformImage), frameRequest] () mutable {
                // The queue may have been closed if after we got the frame NativeImage, stopAsyncDecodingQueue() was called.
                if (protectedDecodingQueue.ptr() == protectedThis->m_decodingQueue && protectedDecoder.ptr() == protectedThis->m_decoder) {
                    ASSERT(protectedThis->m_frameCommitQueue.first() == frameRequest);
                    protectedThis->m_frameCommitQueue.removeFirst();
                    protectedThis->cachePlatformImageAtIndexAsync(WTFMove(platformImage), frameRequest.index, frameRequest.subsamplingLevel, frameRequest.decodingOptions, frameRequest.decodingStatus);
                } else
                    LOG(Images, "ImageSource::%s - %p - url: %s [frame %ld will not cached]", __FUNCTION__, protectedThis.ptr(), sourceURL.utf8().data(), frameRequest.index);
            });
        }

        // Ensure destruction happens on creation thread.
        callOnMainThread([protectedThis = WTFMove(protectedThis), protectedQueue = WTFMove(protectedDecodingQueue), protectedDecoder = WTFMove(protectedDecoder)] () mutable { });
    });
}

void ImageSource::requestFrameAsyncDecodingAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const std::optional<IntSize>& sizeForDrawing)
{
    ASSERT(isDecoderAvailable());
    if (!hasAsyncDecodingQueue())
        startAsyncDecodingQueue();

    ASSERT(index < m_frames.size());
    DecodingStatus decodingStatus = m_decoder->frameIsCompleteAtIndex(index) ? DecodingStatus::Complete : DecodingStatus::Partial;

    LOG(Images, "ImageSource::%s - %p - url: %s [enqueuing frame %ld for decoding]", __FUNCTION__, this, sourceURL().string().utf8().data(), index);
    m_frameRequestQueue->enqueue({ index, subsamplingLevel, sizeForDrawing, decodingStatus });
    m_frameCommitQueue.append({ index, subsamplingLevel, sizeForDrawing, decodingStatus });
}

bool ImageSource::isAsyncDecodingQueueIdle() const
{
    return m_frameCommitQueue.isEmpty();
}

void ImageSource::stopAsyncDecodingQueue()
{
    if (!hasAsyncDecodingQueue())
        return;

    std::for_each(m_frameCommitQueue.begin(), m_frameCommitQueue.end(), [this](const ImageFrameRequest& frameRequest) {
        ImageFrame& frame = m_frames[frameRequest.index];
        if (!frame.isInvalid()) {
            LOG(Images, "ImageSource::%s - %p - url: %s [decoding has been cancelled for frame %ld]", __FUNCTION__, this, sourceURL().string().utf8().data(), frameRequest.index);
            frame.clear();
        }
    });

    // Close m_frameRequestQueue then set it to nullptr. A new decoding thread might start and a
    // new m_frameRequestQueue will be created. So the terminating thread will not have access to it.
    m_frameRequestQueue->close();
    m_frameRequestQueue = nullptr;
    m_frameCommitQueue.clear();
    m_decodingQueue = nullptr;
    LOG(Images, "ImageSource::%s - %p - url: %s [decoding has been stopped]", __FUNCTION__, this, sourceURL().string().utf8().data());
}

const ImageFrame& ImageSource::frameAtIndexCacheIfNeeded(size_t index, ImageFrame::Caching caching, const std::optional<SubsamplingLevel>& subsamplingLevel)
{
    if (index >= m_frames.size())
        return ImageFrame::defaultFrame();
    
    ImageFrame& frame = m_frames[index];
    if (!isDecoderAvailable() || frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(index, DecodingOptions(DecodingMode::Asynchronous)))
        return frame;

    SubsamplingLevel subsamplingLevelValue = subsamplingLevel ? subsamplingLevel.value() : frame.subsamplingLevel();

    switch (caching) {
    case ImageFrame::Caching::Metadata:
        // Retrieve the metadata from ImageDecoder if the ImageFrame isn't complete.
        if (frame.isComplete())
            break;
        cacheMetadataAtIndex(index, subsamplingLevelValue);
        break;

    case ImageFrame::Caching::MetadataAndImage:
        // Cache the image and retrieve the metadata from ImageDecoder only if there was not valid image stored.
        if (frame.hasFullSizeNativeImage(subsamplingLevel))
            break;
        // We have to perform synchronous image decoding in this code.
        auto platformImage = m_decoder->createFrameImageAtIndex(index, subsamplingLevelValue);
        // Clean the old native image and set a new one.
        cachePlatformImageAtIndex(WTFMove(platformImage), index, subsamplingLevelValue, DecodingOptions(DecodingMode::Synchronous));
        break;
    }

    return frame;
}

void ImageSource::clearMetadata()
{
    m_cachedMetadata.remove({
        MetadataType::EncodedDataStatus,
        MetadataType::FrameCount,
        MetadataType::RepetitionCount,
        MetadataType::SinglePixelSolidColor,
        MetadataType::UTI
    });
}

URL ImageSource::sourceURL() const
{
    return m_image ? m_image->sourceURL() : URL();
}

String ImageSource::mimeType() const
{
    return m_image ? m_image->mimeType() : emptyString();
}

long long ImageSource::expectedContentLength() const
{
    return m_image ? m_image->expectedContentLength() : 0;
}

template<typename T>
T ImageSource::metadataCacheIfNeeded(T& cachedValue, const T& defaultValue, MetadataType metadataType, T (ImageDecoder::*functor)() const)
{
    if (m_cachedMetadata.contains(metadataType))
        return cachedValue;

    if (!isDecoderAvailable() || !m_decoder->isSizeAvailable())
        return defaultValue;

    cachedValue = (*m_decoder.*functor)();
    m_cachedMetadata.add(metadataType);
    didDecodeProperties(m_decoder->bytesDecodedToDetermineProperties());
    return cachedValue;
}

template<typename T>
T ImageSource::firstFrameMetadataCacheIfNeeded(T& cachedValue, MetadataType metadataType, T (ImageFrame::*functor)() const, ImageFrame::Caching caching, const std::optional<SubsamplingLevel>& subsamplingLevel)
{
    if (m_cachedMetadata.contains(metadataType))
        return cachedValue;

    const ImageFrame& frame = frameAtIndexCacheIfNeeded(0, caching, subsamplingLevel);

    // Don't cache any unavailable frame metadata.
    if (!frame.hasMetadata())
        return (frame.*functor)();

    cachedValue = (frame.*functor)();
    m_cachedMetadata.add(metadataType);
    return cachedValue;
}

EncodedDataStatus ImageSource::encodedDataStatus()
{
    return metadataCacheIfNeeded(m_encodedDataStatus, EncodedDataStatus::Unknown, MetadataType::EncodedDataStatus, &ImageDecoder::encodedDataStatus);
}

size_t ImageSource::frameCount()
{
    return metadataCacheIfNeeded(m_frameCount, m_frames.size(), MetadataType::FrameCount, &ImageDecoder::frameCount);
}

RepetitionCount ImageSource::repetitionCount()
{
    return metadataCacheIfNeeded(m_repetitionCount, static_cast<RepetitionCount>(RepetitionCountNone), MetadataType::RepetitionCount, &ImageDecoder::repetitionCount);
}

String ImageSource::uti()
{
#if USE(CG)
    return metadataCacheIfNeeded(m_uti, String(), MetadataType::UTI, &ImageDecoder::uti);
#else
    return String();
#endif
}

String ImageSource::filenameExtension()
{
    return metadataCacheIfNeeded(m_filenameExtension, String(), MetadataType::FileNameExtension, &ImageDecoder::filenameExtension);
}

String ImageSource::accessibilityDescription()
{
    return metadataCacheIfNeeded(m_accessibilityDescription, String(), MetadataType::AccessibilityDescription, &ImageDecoder::accessibilityDescription);
}

std::optional<IntPoint> ImageSource::hotSpot()
{
    return metadataCacheIfNeeded(m_hotSpot, { }, MetadataType::HotSpot, &ImageDecoder::hotSpot);
}

ImageOrientation ImageSource::orientation()
{
    return firstFrameMetadataCacheIfNeeded(m_orientation, MetadataType::Orientation, &ImageFrame::orientation, ImageFrame::Caching::Metadata);
}

std::optional<IntSize> ImageSource::densityCorrectedSize(ImageOrientation orientation)
{
    auto size = firstFrameMetadataCacheIfNeeded(m_densityCorrectedSize, MetadataType::DensityCorrectedSize, &ImageFrame::densityCorrectedSize, ImageFrame::Caching::Metadata);
    if (!size)
        return std::nullopt;

    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = this->orientation();

    return orientation.usesWidthAsHeight() ? std::optional<IntSize>(size.value().transposedSize()) : size;
}

IntSize ImageSource::size(ImageOrientation orientation)
{
    auto preferredSize = densityCorrectedSize(orientation);
    return preferredSize ? preferredSize.value() : sourceSize(orientation);
}

IntSize ImageSource::sourceSize(ImageOrientation orientation)
{
    IntSize size;
#if !USE(CG)
    // It's possible that we have decoded the metadata, but not frame contents yet. In that case ImageDecoder claims to
    // have the size available, but the frame cache is empty. Return the decoder size without caching in such case.
    if (m_frames.isEmpty() && isDecoderAvailable())
        size = m_decoder->size();
    else
#endif
        size = firstFrameMetadataCacheIfNeeded(m_size, MetadataType::Size, &ImageFrame::size, ImageFrame::Caching::Metadata, SubsamplingLevel::Default);
    
    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = this->orientation();

    return orientation.usesWidthAsHeight() ? size.transposedSize() : size;
}

Color ImageSource::singlePixelSolidColor()
{
    if (!m_cachedMetadata.contains(MetadataType::SinglePixelSolidColor) && (size() != IntSize(1, 1) || frameCount() != 1)) {
        m_singlePixelSolidColor = Color();
        m_cachedMetadata.add(MetadataType::SinglePixelSolidColor);
    }

    return firstFrameMetadataCacheIfNeeded(m_singlePixelSolidColor, MetadataType::SinglePixelSolidColor, &ImageFrame::singlePixelSolidColor, ImageFrame::Caching::MetadataAndImage);
}

SubsamplingLevel ImageSource::maximumSubsamplingLevel()
{
    if (m_cachedMetadata.contains(MetadataType::MaximumSubsamplingLevel))
        return m_maximumSubsamplingLevel;

    if (!isDecoderAvailable() || !m_decoder->frameAllowSubsamplingAtIndex(0))
        return SubsamplingLevel::Default;

    // FIXME: this value was chosen to be appropriate for iOS since the image
    // subsampling is only enabled by default on iOS. Choose a different value
    // if image subsampling is enabled on other platform.
    const int maximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;
    SubsamplingLevel level = SubsamplingLevel::First;

    for (; level < SubsamplingLevel::Last; ++level) {
        if (frameSizeAtIndex(0, level).area() < maximumImageAreaBeforeSubsampling)
            break;
    }

    m_maximumSubsamplingLevel = level;
    m_cachedMetadata.add(MetadataType::MaximumSubsamplingLevel);
    return m_maximumSubsamplingLevel;
}

bool ImageSource::frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(size_t index, const DecodingOptions& decodingOptions)
{
    auto it = std::find_if(m_frameCommitQueue.begin(), m_frameCommitQueue.end(), [index, &decodingOptions](const ImageFrameRequest& frameRequest) {
        return frameRequest.index == index && frameRequest.decodingOptions.isAsynchronousCompatibleWith(decodingOptions);
    });
    return it != m_frameCommitQueue.end();
}

DecodingStatus ImageSource::frameDecodingStatusAtIndex(size_t index)
{
    return frameAtIndexCacheIfNeeded(index, ImageFrame::Caching::Metadata).decodingStatus();
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    return frameAtIndex(index).hasAlpha();
}

bool ImageSource::frameHasFullSizeNativeImageAtIndex(size_t index, const std::optional<SubsamplingLevel>& subsamplingLevel)
{
    return frameAtIndex(index).hasFullSizeNativeImage(subsamplingLevel);
}

bool ImageSource::frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(size_t index, const std::optional<SubsamplingLevel>& subsamplingLevel, const DecodingOptions& decodingOptions)
{
    return frameAtIndex(index).hasDecodedNativeImageCompatibleWithOptions(subsamplingLevel, decodingOptions);
}

SubsamplingLevel ImageSource::frameSubsamplingLevelAtIndex(size_t index)
{
    return frameAtIndex(index).subsamplingLevel();
}

IntSize ImageSource::frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameAtIndexCacheIfNeeded(index, ImageFrame::Caching::Metadata, subsamplingLevel).size();
}

unsigned ImageSource::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameAtIndexCacheIfNeeded(index, ImageFrame::Caching::Metadata, subsamplingLevel).frameBytes();
}

Seconds ImageSource::frameDurationAtIndex(size_t index)
{
    return frameAtIndexCacheIfNeeded(index, ImageFrame::Caching::Metadata).duration();
}

ImageOrientation ImageSource::frameOrientationAtIndex(size_t index)
{
    return frameAtIndexCacheIfNeeded(index, ImageFrame::Caching::Metadata).orientation();
}

RefPtr<NativeImage> ImageSource::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    if (!isDecoderAvailable())
        return nullptr;
    return NativeImage::create(m_decoder->createFrameImageAtIndex(index, subsamplingLevel));
}

RefPtr<NativeImage> ImageSource::frameImageAtIndex(size_t index)
{
    return frameAtIndex(index).nativeImage();
}

RefPtr<NativeImage> ImageSource::frameImageAtIndexCacheIfNeeded(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameAtIndexCacheIfNeeded(index, ImageFrame::Caching::MetadataAndImage, subsamplingLevel).nativeImage();
}

void ImageSource::dump(TextStream& ts)
{
    ts.dumpProperty("type", filenameExtension());
    ts.dumpProperty("frame-count", frameCount());
    ts.dumpProperty("repetitions", repetitionCount());
    ts.dumpProperty("solid-color", singlePixelSolidColor());

    ImageOrientation orientation = frameOrientationAtIndex(0);
    if (orientation != ImageOrientation::Orientation::None)
        ts.dumpProperty("orientation", orientation);
}

}
