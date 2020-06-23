/*
 * Copyright (C) 2016-2017 Apple Inc.  All rights reserved.
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

#if USE(DIRECT2D)
#include "GraphicsContext.h"
#include "PlatformContextDirect2D.h"
#endif

namespace WebCore {

ImageSource::ImageSource(BitmapImage* image, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : m_image(image)
    , m_alphaOption(alphaOption)
    , m_gammaAndColorProfileOption(gammaAndColorProfileOption)
    , m_runLoop(RunLoop::current())
{
}

ImageSource::ImageSource(NativeImagePtr&& nativeImage)
    : m_runLoop(RunLoop::current())
{
    m_frameCount = 1;
    m_encodedDataStatus = EncodedDataStatus::Complete;
    growFrames();

    setNativeImage(WTFMove(nativeImage));

    m_decodedSize = m_frames[0].frameBytes();

    m_size = m_frames[0].size();
    m_orientation = ImageOrientation(ImageOrientation::None);
}

ImageSource::~ImageSource()
{
    ASSERT(!hasAsyncDecodingQueue());
    ASSERT(&m_runLoop == &RunLoop::current());
}

bool ImageSource::ensureDecoderAvailable(SharedBuffer* data)
{
    if (!data || isDecoderAvailable())
        return true;

    m_decoder = ImageDecoder::create(*data, mimeType(), m_alphaOption, m_gammaAndColorProfileOption);
    if (!isDecoderAvailable())
        return false;

    m_decoder->setEncodedDataStatusChangeCallback([weakThis = makeWeakPtr(this)] (auto status) {
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

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
    if (!data || !ensureDecoderAvailable(data))
        return;

    m_decoder->setData(*data, allDataReceived);
}

void ImageSource::resetData(SharedBuffer* data)
{
    m_decoder = nullptr;
    setData(data, isAllDataReceived());
}

EncodedDataStatus ImageSource::dataChanged(SharedBuffer* data, bool allDataReceived)
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

void ImageSource::destroyDecodedData(size_t frameCount, size_t excludeFrame)
{
    unsigned decodedSize = 0;

    ASSERT(frameCount <= m_frames.size());

    for (size_t index = 0; index < frameCount; ++index) {
        if (index == excludeFrame)
            continue;
        decodedSize += m_frames[index].clearImage();
    }

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

void ImageSource::setNativeImage(NativeImagePtr&& nativeImage)
{
    ASSERT(m_frames.size() == 1);
    ImageFrame& frame = m_frames[0];

    ASSERT(!isDecoderAvailable());

    frame.m_nativeImage = WTFMove(nativeImage);

    frame.m_decodingStatus = DecodingStatus::Complete;
    frame.m_size = nativeImageSize(frame.m_nativeImage);
    frame.m_hasAlpha = nativeImageHasAlpha(frame.m_nativeImage);
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
        frame.m_size = nativeImageSize(frame.nativeImage());
    } else
        frame.m_size = m_decoder->frameSizeAtIndex(index, subsamplingLevel);

    frame.m_orientation = m_decoder->frameOrientationAtIndex(index);
    frame.m_hasAlpha = m_decoder->frameHasAlphaAtIndex(index);

    if (repetitionCount())
        frame.m_duration = m_decoder->frameDurationAtIndex(index);
}

void ImageSource::cacheNativeImageAtIndex(NativeImagePtr&& nativeImage, size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& decodingOptions, DecodingStatus decodingStatus)
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
    frame.m_nativeImage = WTFMove(nativeImage);
    frame.m_decodingOptions = decodingOptions;
    cacheMetadataAtIndex(index, subsamplingLevel, decodingStatus);

    // Update the observer with the new image frame bytes.
    decodedSizeIncreased(frame.frameBytes());
}

void ImageSource::cacheNativeImageAtIndexAsync(NativeImagePtr&& nativeImage, size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& decodingOptions, DecodingStatus decodingStatus)
{
    if (!isDecoderAvailable())
        return;

    ASSERT(index < m_frames.size());

    // Clean the old native image and set a new one
    cacheNativeImageAtIndex(WTFMove(nativeImage), index, subsamplingLevel, decodingOptions, decodingStatus);
    LOG(Images, "ImageSource::%s - %p - url: %s [frame %ld has been cached]", __FUNCTION__, this, sourceURL().string().utf8().data(), index);

    // Notify the image with the readiness of the new frame NativeImage.
    if (m_image)
        m_image->imageFrameAvailableAtIndex(index);
}

WorkQueue& ImageSource::decodingQueue()
{
    if (!m_decodingQueue)
        m_decodingQueue = WorkQueue::create("org.webkit.ImageDecoder", WorkQueue::Type::Serial, WorkQueue::QOS::Default);

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
    decodingQueue().dispatch([protectedThis = makeRef(*this), protectedDecodingQueue = makeRef(decodingQueue()), protectedFrameRequestQueue = makeRef(frameRequestQueue()), protectedDecoder = makeRef(*m_decoder), sourceURL = sourceURL().string().isolatedCopy()] () mutable {
        ImageFrameRequest frameRequest;
        Seconds minDecodingDuration = protectedThis->frameDecodingDurationForTesting();

        while (protectedFrameRequestQueue->dequeue(frameRequest)) {
            TraceScope tracingScope(AsyncImageDecodeStart, AsyncImageDecodeEnd);

            MonotonicTime startingTime;
            if (minDecodingDuration > 0_s)
                startingTime = MonotonicTime::now();

            // Get the frame NativeImage on the decoding thread.
            NativeImagePtr nativeImage = protectedDecoder->createFrameImageAtIndex(frameRequest.index, frameRequest.subsamplingLevel, frameRequest.decodingOptions);
            if (nativeImage)
                LOG(Images, "ImageSource::%s - %p - url: %s [frame %ld has been decoded]", __FUNCTION__, protectedThis.ptr(), sourceURL.utf8().data(), frameRequest.index);
            else {
                LOG(Images, "ImageSource::%s - %p - url: %s [decoding for frame %ld has failed]", __FUNCTION__, protectedThis.ptr(), sourceURL.utf8().data(), frameRequest.index);
                continue;
            }

            // Pretend as if the decoding takes minDecodingDuration.
            if (minDecodingDuration > 0_s)
                sleep(minDecodingDuration - (MonotonicTime::now() - startingTime));

            // Update the cached frames on the creation thread to avoid updating the MemoryCache from a different thread.
            callOnMainThread([protectedThis, protectedDecodingQueue, protectedDecoder, sourceURL = sourceURL.isolatedCopy(), nativeImage = WTFMove(nativeImage), frameRequest] () mutable {
                // The queue may have been closed if after we got the frame NativeImage, stopAsyncDecodingQueue() was called.
                if (protectedDecodingQueue.ptr() == protectedThis->m_decodingQueue && protectedDecoder.ptr() == protectedThis->m_decoder) {
                    ASSERT(protectedThis->m_frameCommitQueue.first() == frameRequest);
                    protectedThis->m_frameCommitQueue.removeFirst();
                    protectedThis->cacheNativeImageAtIndexAsync(WTFMove(nativeImage), frameRequest.index, frameRequest.subsamplingLevel, frameRequest.decodingOptions, frameRequest.decodingStatus);
                } else
                    LOG(Images, "ImageSource::%s - %p - url: %s [frame %ld will not cached]", __FUNCTION__, protectedThis.ptr(), sourceURL.utf8().data(), frameRequest.index);
            });
        }

        // Ensure destruction happens on creation thread.
        callOnMainThread([protectedThis = WTFMove(protectedThis), protectedQueue = WTFMove(protectedDecodingQueue), protectedDecoder = WTFMove(protectedDecoder)] () mutable { });
    });
}

void ImageSource::requestFrameAsyncDecodingAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const Optional<IntSize>& sizeForDrawing)
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

const ImageFrame& ImageSource::frameAtIndexCacheIfNeeded(size_t index, ImageFrame::Caching caching, const Optional<SubsamplingLevel>& subsamplingLevel)
{
    ASSERT(index < m_frames.size());
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
        NativeImagePtr nativeImage = m_decoder->createFrameImageAtIndex(index, subsamplingLevelValue);
        // Clean the old native image and set a new one.
        cacheNativeImageAtIndex(WTFMove(nativeImage), index, subsamplingLevelValue, DecodingOptions(DecodingMode::Synchronous));
        break;
    }

    return frame;
}

void ImageSource::clearMetadata()
{
    m_frameCount = WTF::nullopt;
    m_repetitionCount = WTF::nullopt;
    m_singlePixelSolidColor = WTF::nullopt;
    m_encodedDataStatus = WTF::nullopt;
    m_uti = WTF::nullopt;
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

template<typename T, T (ImageDecoder::*functor)() const>
T ImageSource::metadata(const T& defaultValue, Optional<T>* cachedValue)
{
    if (cachedValue && *cachedValue)
        return cachedValue->value();

    if (!isDecoderAvailable() || !m_decoder->isSizeAvailable())
        return defaultValue;

    if (!cachedValue)
        return (*m_decoder.*functor)();

    *cachedValue = (*m_decoder.*functor)();
    didDecodeProperties(m_decoder->bytesDecodedToDetermineProperties());
    return cachedValue->value();
}

template<typename T, typename... Args>
T ImageSource::frameMetadataAtIndex(size_t index, T (ImageFrame::*functor)(Args...) const, Args&&... args)
{
    const ImageFrame& frame = index < m_frames.size() ? m_frames[index] : ImageFrame::defaultFrame();
    return (frame.*functor)(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
T ImageSource::frameMetadataAtIndexCacheIfNeeded(size_t index, T (ImageFrame::*functor)() const, Optional<T>* cachedValue, Args&&... args)
{
    if (cachedValue && *cachedValue)
        return cachedValue->value();

    const ImageFrame& frame = index < m_frames.size() ? frameAtIndexCacheIfNeeded(index, std::forward<Args>(args)...) : ImageFrame::defaultFrame();

    // Don't cache any unavailable frame metadata.
    if (!frame.hasMetadata() || !cachedValue)
        return (frame.*functor)();

    *cachedValue = (frame.*functor)();
    return cachedValue->value();
}

EncodedDataStatus ImageSource::encodedDataStatus()
{
    return metadata<EncodedDataStatus, (&ImageDecoder::encodedDataStatus)>(EncodedDataStatus::Unknown, &m_encodedDataStatus);
}

size_t ImageSource::frameCount()
{
    return metadata<size_t, (&ImageDecoder::frameCount)>(m_frames.size(), &m_frameCount);
}

RepetitionCount ImageSource::repetitionCount()
{
    return metadata<RepetitionCount, (&ImageDecoder::repetitionCount)>(RepetitionCountNone, &m_repetitionCount);
}

String ImageSource::uti()
{
#if USE(CG)
    return metadata<String, (&ImageDecoder::uti)>(String(), &m_uti);
#else
    return String();
#endif
}

String ImageSource::filenameExtension()
{
    return metadata<String, (&ImageDecoder::filenameExtension)>(String(), &m_filenameExtension);
}

Optional<IntPoint> ImageSource::hotSpot()
{
    return metadata<Optional<IntPoint>, (&ImageDecoder::hotSpot)>(WTF::nullopt, &m_hotSpot);
}

ImageOrientation ImageSource::orientation()
{
    return frameMetadataAtIndexCacheIfNeeded<ImageOrientation>(0, (&ImageFrame::orientation), &m_orientation, ImageFrame::Caching::Metadata);
}

IntSize ImageSource::size(ImageOrientation orientation)
{
    IntSize size;
#if !USE(CG)
    // It's possible that we have decoded the metadata, but not frame contents yet. In that case ImageDecoder claims to
    // have the size available, but the frame cache is empty. Return the decoder size without caching in such case.
    if (m_frames.isEmpty() && isDecoderAvailable())
        size = m_decoder->size();
    else
#endif
        size = frameMetadataAtIndexCacheIfNeeded<IntSize>(0, (&ImageFrame::size), &m_size, ImageFrame::Caching::Metadata, SubsamplingLevel::Default);
    
    if (orientation == ImageOrientation::FromImage)
        orientation = this->orientation();

    return orientation.usesWidthAsHeight() ? size.transposedSize() : size;
}

Color ImageSource::singlePixelSolidColor()
{
    if (!m_singlePixelSolidColor && (size() != IntSize(1, 1) || frameCount() != 1))
        m_singlePixelSolidColor = Color();

    if (m_singlePixelSolidColor)
        return m_singlePixelSolidColor.value();

    return frameMetadataAtIndexCacheIfNeeded<Color>(0, (&ImageFrame::singlePixelSolidColor), &m_singlePixelSolidColor, ImageFrame::Caching::MetadataAndImage);
}

SubsamplingLevel ImageSource::maximumSubsamplingLevel()
{
    if (m_maximumSubsamplingLevel)
        return m_maximumSubsamplingLevel.value();

    if (!isDecoderAvailable() || !m_decoder->frameAllowSubsamplingAtIndex(0))
        return SubsamplingLevel::Default;

    // FIXME: this value was chosen to be appropriate for iOS since the image
    // subsampling is only enabled by default on iOS. Choose a different value
    // if image subsampling is enabled on other platform.
    const int maximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;
    SubsamplingLevel level = SubsamplingLevel::First;

    for (; level < SubsamplingLevel::Last; ++level) {
        if (frameSizeAtIndex(0, level).area().unsafeGet() < maximumImageAreaBeforeSubsampling)
            break;
    }

    m_maximumSubsamplingLevel = level;
    return m_maximumSubsamplingLevel.value();
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
    return frameMetadataAtIndexCacheIfNeeded<DecodingStatus>(index, (&ImageFrame::decodingStatus), nullptr, ImageFrame::Caching::Metadata);
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    return frameMetadataAtIndex<bool>(index, (&ImageFrame::hasAlpha));
}

bool ImageSource::frameHasFullSizeNativeImageAtIndex(size_t index, const Optional<SubsamplingLevel>& subsamplingLevel)
{
    return frameMetadataAtIndex<bool>(index, (&ImageFrame::hasFullSizeNativeImage), subsamplingLevel);
}

bool ImageSource::frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(size_t index, const Optional<SubsamplingLevel>& subsamplingLevel, const DecodingOptions& decodingOptions)
{
    return frameMetadataAtIndex<bool>(index, (&ImageFrame::hasDecodedNativeImageCompatibleWithOptions), subsamplingLevel, decodingOptions);
}

SubsamplingLevel ImageSource::frameSubsamplingLevelAtIndex(size_t index)
{
    return frameMetadataAtIndex<SubsamplingLevel>(index, (&ImageFrame::subsamplingLevel));
}

IntSize ImageSource::frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameMetadataAtIndexCacheIfNeeded<IntSize>(index, (&ImageFrame::size), nullptr, ImageFrame::Caching::Metadata, subsamplingLevel);
}

unsigned ImageSource::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameMetadataAtIndexCacheIfNeeded<unsigned>(index, (&ImageFrame::frameBytes), nullptr, ImageFrame::Caching::Metadata, subsamplingLevel);
}

Seconds ImageSource::frameDurationAtIndex(size_t index)
{
    return frameMetadataAtIndexCacheIfNeeded<Seconds>(index, (&ImageFrame::duration), nullptr, ImageFrame::Caching::Metadata);
}

ImageOrientation ImageSource::frameOrientationAtIndex(size_t index)
{
    return frameMetadataAtIndexCacheIfNeeded<ImageOrientation>(index, (&ImageFrame::orientation), nullptr, ImageFrame::Caching::Metadata);
}

#if USE(DIRECT2D)
void ImageSource::setTargetContext(const GraphicsContext* targetContext)
{
    if (isDecoderAvailable() && targetContext)
        m_decoder->setTargetContext(targetContext->platformContext()->renderTarget());
}
#endif

NativeImagePtr ImageSource::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    return isDecoderAvailable() ? m_decoder->createFrameImageAtIndex(index, subsamplingLevel) : nullptr;
}

NativeImagePtr ImageSource::frameImageAtIndex(size_t index)
{
    return frameMetadataAtIndex<NativeImagePtr>(index, (&ImageFrame::nativeImage));
}

NativeImagePtr ImageSource::frameImageAtIndexCacheIfNeeded(size_t index, SubsamplingLevel subsamplingLevel)
{
    return frameMetadataAtIndexCacheIfNeeded<NativeImagePtr>(index, (&ImageFrame::nativeImage), nullptr, ImageFrame::Caching::MetadataAndImage, subsamplingLevel);
}

void ImageSource::dump(TextStream& ts)
{
    ts.dumpProperty("type", filenameExtension());
    ts.dumpProperty("frame-count", frameCount());
    ts.dumpProperty("repetitions", repetitionCount());
    ts.dumpProperty("solid-color", singlePixelSolidColor());

    ImageOrientation orientation = frameOrientationAtIndex(0);
    if (orientation != ImageOrientation::None)
        ts.dumpProperty("orientation", orientation);
}

}
