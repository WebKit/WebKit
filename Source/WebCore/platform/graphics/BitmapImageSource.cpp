/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
#include "BitmapImageSource.h"

#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageDecoder.h"
#include "ImageFrameAnimator.h"
#include "ImageObserver.h"
#include "Logging.h"

namespace WebCore {

Ref<BitmapImageSource> BitmapImageSource::create(BitmapImage& bitmapImage, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    return adoptRef(*new BitmapImageSource(bitmapImage, alphaOption, gammaAndColorProfileOption));
}

BitmapImageSource::BitmapImageSource(BitmapImage& bitmapImage, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : m_bitmapImage(bitmapImage)
    , m_alphaOption(alphaOption)
    , m_gammaAndColorProfileOption(gammaAndColorProfileOption)
{
}

ImageDecoder* BitmapImageSource::decoder(FragmentedSharedBuffer* data) const
{
    if (m_decoder)
        return m_decoder.get();

    if (!data)
        return nullptr;

    m_decoder = ImageDecoder::create(*data, m_bitmapImage.mimeType(), m_alphaOption, m_gammaAndColorProfileOption);
    if (!m_decoder)
        return nullptr;

    m_decoder->setEncodedDataStatusChangeCallback([weakThis = ThreadSafeWeakPtr { *this }] (auto status) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->encodedDataStatusChanged(status);
    });

    if (auto expectedContentSize = m_bitmapImage.expectedContentLength())
        m_decoder->setExpectedContentSize(expectedContentSize);

    return m_decoder.get();
}

ImageFrameAnimator* BitmapImageSource::frameAnimator() const
{
    if (m_frameAnimator)
        return m_frameAnimator.get();

    if (!isAnimated())
        return nullptr;

    m_frameAnimator = ImageFrameAnimator::create(const_cast<BitmapImageSource&>(*this));
    return m_frameAnimator.get();
}

ImageFrameWorkQueue& BitmapImageSource::workQueue() const
{
    if (!m_workQueue)
        m_workQueue = ImageFrameWorkQueue::create(const_cast<BitmapImageSource&>(*this));
    return *m_workQueue;
}

void BitmapImageSource::encodedDataStatusChanged(EncodedDataStatus status)
{
    if (status >= EncodedDataStatus::SizeAvailable)
        m_frames.resizeToFit(m_decoder->frameCount());

    if (auto observer = m_bitmapImage.imageObserver())
        observer->encodedDataStatusChanged(m_bitmapImage, status);
}

EncodedDataStatus BitmapImageSource::dataChanged(FragmentedSharedBuffer* data, bool allDataReceived)
{
    m_cachedFlags = { };

    auto status = setData(data, allDataReceived);
    if (status < EncodedDataStatus::TypeAvailable)
        return status;

    encodedDataStatusChanged(status);
    return status;
}

void BitmapImageSource::destroyDecodedData(bool destroyAll)
{
    LOG(Images, "BitmapImageSource::%s - %p - url: %s. Decoded data with destroyAll = %d will be destroyed.", __FUNCTION__, this, sourceUTF8(), destroyAll);

    for (unsigned index = 0; index < frameCount(); ++index) {
        // Don't destroy a NativeImage betore it is drawn at least once.
        if (isPendingDrawingAtIndex(index))
            continue;

        if (!destroyAll && !canDestroyNativeImageAtIndex(index))
            continue;

        destroyNativeImageAtIndex(index);
    }

    // There's no need to throw away the decoder unless we're explicitly asked
    // to destroy all of the frames.
    if (destroyAll && isDecodingWorkQueueIdle())
        resetData();
    else
        clearFrameBufferCache();
}

void BitmapImageSource::decodedSizeChanged(long long decodedSize)
{
    if (!decodedSize)
        return;

    if (auto observer = m_bitmapImage.imageObserver())
        observer->decodedSizeChanged(m_bitmapImage, decodedSize);
}

void BitmapImageSource::decodedSizeIncreased(unsigned decodedSize)
{
    if (!decodedSize)
        return;

    m_decodedSize += decodedSize;

    decodedSizeChanged(decodedSize);

    // The fully-decoded frame will subsume the partially decoded data used
    // to determine image properties.
    decodedPropertiesSizeChanged(0);
}

void BitmapImageSource::decodedSizeDecreased(unsigned decodedSize)
{
    if (!decodedSize)
        return;

    ASSERT(m_decodedSize >= decodedSize);
    m_decodedSize -= decodedSize;

    decodedSizeChanged(-static_cast<long long>(decodedSize));
}

void BitmapImageSource::decodedPropertiesSizeChanged(unsigned decodedPropertiesSize)
{
    if (decodedPropertiesSize == m_decodedPropertiesSize)
        return;

    long long decodedSize = static_cast<long long>(decodedPropertiesSize) - m_decodedPropertiesSize;
    m_decodedPropertiesSize = decodedPropertiesSize;
    decodedSizeChanged(decodedSize);
}

bool BitmapImageSource::canDestroyNativeImageAtIndex(unsigned index)
{
    return !(isAnimated() && index == currentFrameIndex());
}

void BitmapImageSource::destroyNativeImageAtIndex(unsigned index)
{
    if (index >= m_frames.size())
        return;

    if (!m_frames[index].hasNativeImage())
        return;

    unsigned decodedSize = m_frames[index].clearImage();
    decodedSizeDecreased(decodedSize);

    if (m_workQueue)
        m_workQueue->didDestroyNativeImageAtIndex(index);
}

void BitmapImageSource::didDecodeProperties(unsigned decodedPropertiesSize)
{
    if (m_decodedSize)
        return;

    decodedPropertiesSizeChanged(decodedPropertiesSize);
}

void BitmapImageSource::didCacheNativeImageAtIndex(unsigned index)
{
    if (index >= m_frames.size())
        return;

    if (m_workQueue)
        m_workQueue->didCacheNativeImageAtIndex(index);
}

void BitmapImageSource::clearFrameBufferCache()
{
    if (!m_decoder)
        return;

    m_decoder->clearFrameBufferCache(currentFrameIndex());
}

EncodedDataStatus BitmapImageSource::setData(FragmentedSharedBuffer* data, bool allDataReceived)
{
    if (!data)
        return EncodedDataStatus::Unknown;

    RefPtr decoder = this->decoder(data);
    if (!decoder)
        return EncodedDataStatus::Unknown;

    decoder->setData(*data, allDataReceived);
    return encodedDataStatus();
}

void BitmapImageSource::resetData()
{
    if (!m_decoder || !isDecodingWorkQueueIdle())
        return;

    setData(m_bitmapImage.data(), m_decoder->isAllDataReceived());
}

void BitmapImageSource::startAnimation()
{
    startAnimation(SubsamplingLevel::Default, DecodingMode::Synchronous);
}

bool BitmapImageSource::startAnimation(SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    if (auto frameAnimator = this->frameAnimator())
        return frameAnimator->startAnimation(subsamplingLevel, options);
    return false;
}

void BitmapImageSource::stopAnimation()
{
    if (!m_frameAnimator)
        return;

    m_frameAnimator->stopAnimation();
    stopDecodingWorkQueue();
}

void BitmapImageSource::resetAnimation()
{
    if (!m_frameAnimator)
        return;

    m_frameAnimator->resetAnimation();
}

bool BitmapImageSource::isAnimated() const
{
    return frameCount() > 1 && repetitionCount() != RepetitionCountNone;
}

bool BitmapImageSource::isAnimating() const
{
    return m_frameAnimator && m_frameAnimator->isAnimating();
}

bool BitmapImageSource::isAnimationAllowed() const
{
    if (m_frameAnimator && !m_frameAnimator->isAnimationAllowed())
        return false;

    // ImageObserver may disallow animation.
    if (auto observer = m_bitmapImage.imageObserver())
        return observer->allowsAnimation(m_bitmapImage);

    return true;
}

bool BitmapImageSource::isLargeForDecoding() const
{
    size_t sizeInBytes = size(ImageOrientation::Orientation::None).area() * sizeof(uint32_t);
    return sizeInBytes > (isAnimated() ? 100 * KB : 500 * KB);
}

bool BitmapImageSource::isDecodingWorkQueueIdle() const
{
    return !m_workQueue || m_workQueue->isIdle();
}

void BitmapImageSource::stopDecodingWorkQueue()
{
    LOG(Images, "BitmapImageSource::%s - %p - url: %s. Decoding work queue will be stopped.", __FUNCTION__, this, sourceUTF8());

    if (isDecodingWorkQueueIdle())
        return;

    m_workQueue->stop();
}

bool BitmapImageSource::isPendingDecodingAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options) const
{
    if (!m_workQueue)
        return false;

    return m_workQueue->isPendingDecodingAtIndex(index, subsamplingLevel, options);
}

bool BitmapImageSource::isPendingDrawingAtIndex(unsigned index) const
{
    if (!m_workQueue)
        return false;

    return m_workQueue->isPendingDrawingAtIndex(index);
}

bool BitmapImageSource::isCompatibleWithOptionsAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options) const
{
    return frameAtIndex(index).hasDecodedNativeImageCompatibleWithOptions(subsamplingLevel, options);
}

void BitmapImageSource::clearFrameAtIndex(unsigned index)
{
    if (index >= m_frames.size())
        return;

    m_frames[index].clear();
}

void BitmapImageSource::decode(Function<void(DecodingStatus)>&& decodeCallback)
{
    m_decodeCallbacks.append(WTFMove(decodeCallback));
    unsigned index = currentFrameIndex();

    if (isPendingDecodingAtIndex(index, SubsamplingLevel::Default, DecodingMode::Asynchronous)) {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d is being decoded.", __FUNCTION__, this, sourceUTF8(), index);
        return;
    }

    bool isCompatibleNativeImage = isCompatibleWithOptionsAtIndex(index, SubsamplingLevel::Default, DecodingMode::Asynchronous);
    auto frameAnimator = this->frameAnimator();

    if (frameAnimator && (frameAnimator->hasEverAnimated() || isCompatibleNativeImage)) {
        // startAnimation() always decodes the nextFrame which is currentFrameIndex + 1.
        // If primaryFrameIndex = 0, then the sequence of decoding is { 1, 2, .., n, 0, 1, ...}.
        if (startAnimation(SubsamplingLevel::Default, DecodingMode::Asynchronous)) {
            LOG(Images, "BitmapImageSource::%s - %p - url: %s. Animator has requested decoding next frame at index = %d.", __FUNCTION__, this, sourceUTF8(), index);
            return;
        }
    }

    if (!isCompatibleNativeImage) {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Decoding for frame at index = %d will be requested.", __FUNCTION__, this, sourceUTF8(), index);
        requestNativeImageAtIndex(index, SubsamplingLevel::Default, ImageAnimatingState::No, { DecodingMode::Asynchronous });
        return;
    }

    LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d was decoded for natural size.", __FUNCTION__, this, sourceUTF8(), index);
    callDecodeCallbacks(DecodingStatus::Complete);
}

void BitmapImageSource::callDecodeCallbacks(DecodingStatus status)
{
    if (m_decodeCallbacks.isEmpty())
        return;
    for (auto& decodeCallback : m_decodeCallbacks)
        decodeCallback(status);
    m_decodeCallbacks.clear();
}

void BitmapImageSource::imageFrameAtIndexAvailable(unsigned index, ImageAnimatingState animatingState, DecodingStatus decodingStatus)
{
    ASSERT_UNUSED(index, index < frameCount());

    // Decode callbacks have to called regardless of the decoding status.
    // Decoding promises have to resolved or rejected.
    callDecodeCallbacks(decodingStatus);

    if (decodingStatus == DecodingStatus::Invalid)
        return;

    if (auto observer = m_bitmapImage.imageObserver())
        observer->imageFrameAvailable(m_bitmapImage, animatingState, nullptr, decodingStatus);
}

void BitmapImageSource::imageFrameDecodeAtIndexHasFinished(unsigned index, ImageAnimatingState animatingState, DecodingStatus decodingStatus)
{
    // Depending on its timer, the animator may call or postpone calling imageFrameAvailable().
    if (m_frameAnimator && m_frameAnimator->imageFrameDecodeAtIndexHasFinished(index, animatingState, decodingStatus))
        return;

    imageFrameAtIndexAvailable(index, animatingState, decodingStatus);
}

void BitmapImageSource::imageFrameDecodeAtIndexHasFinished(unsigned index, SubsamplingLevel subsamplingLevel, ImageAnimatingState animatingState, const DecodingOptions& options, RefPtr<NativeImage>&& nativeImage)
{
    ASSERT(index < m_frames.size());

    if (!nativeImage) {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d has been failed.", __FUNCTION__, this, sourceUTF8(), index);
        clearFrameAtIndex(index);
        imageFrameDecodeAtIndexHasFinished(index, animatingState, DecodingStatus::Invalid);
    } else {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d has been decoded.", __FUNCTION__, this, sourceUTF8(), index);

        cacheNativeImageAtIndex(index, subsamplingLevel, options, nativeImage.releaseNonNull());
        didCacheNativeImageAtIndex(index);
        imageFrameDecodeAtIndexHasFinished(index, animatingState, frameDecodingStatusAtIndex(index));
    }

    // Do not leave any decoding work queue idle for static images.
    if (animatingState == ImageAnimatingState::No)
        stopDecodingWorkQueue();
}

unsigned BitmapImageSource::currentFrameIndex() const
{
    return m_frameAnimator ? m_frameAnimator->currentFrameIndex() : primaryFrameIndex();
}

void BitmapImageSource::cacheMetadataAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    if (index >= m_frames.size())
        return;

    ImageFrame& frame = m_frames[index];

    if (frame.m_decodingOptions.hasSizeForDrawing()) {
        ASSERT(frame.hasNativeImage());
        frame.m_size = frame.nativeImage()->size();
    } else
        frame.m_size = m_decoder->frameSizeAtIndex(index, subsamplingLevel);

    frame.m_densityCorrectedSize = m_decoder->densityCorrectedSizeAtIndex(index);
    frame.m_subsamplingLevel = subsamplingLevel;
    frame.m_decodingOptions = options;
    frame.m_hasAlpha = m_decoder->frameHasAlphaAtIndex(index);
    frame.m_orientation = m_decoder->frameOrientationAtIndex(index);
    frame.m_decodingStatus = m_decoder->frameIsCompleteAtIndex(index) ? DecodingStatus::Complete : DecodingStatus::Partial;

    if (repetitionCount())
        frame.m_duration = m_decoder->frameDurationAtIndex(index);
}

void BitmapImageSource::cacheNativeImageAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options, Ref<NativeImage>&& nativeImage)
{
    if (index >= m_frames.size())
        return;

    destroyNativeImageAtIndex(index);

    // Do not cache NativeImage if adding its frameByes to MemoryCache will cause numerical overflow.
    size_t frameBytes = nativeImage->size().unclampedArea() * sizeof(uint32_t);
    if (!isInBounds<unsigned>(frameBytes + m_decodedSize))
        return;

    auto& frame = m_frames[index];
    frame.m_nativeImage = WTFMove(nativeImage);

    cacheMetadataAtIndex(index, subsamplingLevel, options);
    decodedSizeIncreased(frame.frameBytes());

    if (frameAtIndex(index).isComplete())
        ++m_decodeCountForTesting;
}

const ImageFrame& BitmapImageSource::frameAtIndex(unsigned index) const
{
    if (index >= m_frames.size())
        return ImageFrame::defaultFrame();

    return m_frames[index];
}

const ImageFrame& BitmapImageSource::frameAtIndexCacheIfNeeded(unsigned index, SubsamplingLevel subsamplingLevel)
{
    if (!m_decoder)
        return ImageFrame::defaultFrame();

    if (index >= m_frames.size())
        return ImageFrame::defaultFrame();

    ImageFrame& frame = m_frames[index];
    if (frame.isComplete() && frame.subsamplingLevel() == subsamplingLevel)
        return frame;

    destroyNativeImageAtIndex(index);

    // Retrieve the metadata from ImageDecoder if the ImageFrame isn't complete.
    cacheMetadataAtIndex(index, subsamplingLevel, { });
    return frame;
}

DecodingStatus BitmapImageSource::requestNativeImageAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, ImageAnimatingState animatingState, const DecodingOptions& options)
{
    if (index >= m_frames.size())
        return DecodingStatus::Invalid;

    LOG(Images, "BitmapImageSource::%s - %p - url: %s. Decoding for frame at index = %d will be requested.", __FUNCTION__, this, sourceUTF8(), index);

    workQueue().dispatch({ index, subsamplingLevel, animatingState, options });

    if (m_clearDecoderAfterAsyncFrameRequestForTesting)
        resetData();

    return DecodingStatus::Decoding;
}

DecodingStatus BitmapImageSource::requestNativeImageAtIndexIfNeeded(unsigned index, SubsamplingLevel subsamplingLevel, ImageAnimatingState animatingState, const DecodingOptions& options)
{
    if (index >= m_frames.size())
        return DecodingStatus::Invalid;

    if (isPendingDecodingAtIndex(index, subsamplingLevel, options)) {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d is being decoded.", __FUNCTION__, this, sourceUTF8(), index);
        return DecodingStatus::Decoding;
    }

    // isCompatibleWithOptionsAtIndex() returns true only if the frame is complete.
    if (isCompatibleWithOptionsAtIndex(index, subsamplingLevel, options))
        return DecodingStatus::Complete;

    return requestNativeImageAtIndex(index, subsamplingLevel, animatingState, options);
}

Expected<Ref<NativeImage>, DecodingStatus> BitmapImageSource::nativeImageAtIndexCacheIfNeeded(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    if (index >= m_frames.size())
        return makeUnexpected(DecodingStatus::Invalid);

    // Never decode the same frame from two different threads.
    if (isPendingDecodingAtIndex(index, subsamplingLevel, options)) {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d is being decoded.", __FUNCTION__, this, sourceUTF8(), index);
        return makeUnexpected(DecodingStatus::Decoding);
    }

    if (!isCompatibleWithOptionsAtIndex(index, subsamplingLevel, options)) {
        PlatformImagePtr platformImage = m_decoder->createFrameImageAtIndex(index, subsamplingLevel, DecodingMode::Synchronous);

        RefPtr nativeImage = NativeImage::create(WTFMove(platformImage));
        if (!nativeImage)
            return makeUnexpected(DecodingStatus::Invalid);

        cacheNativeImageAtIndex(index, subsamplingLevel, DecodingMode::Synchronous, nativeImage.releaseNonNull());
    }

    if (RefPtr nativeImage = frameAtIndex(index).nativeImage())
        return nativeImage.releaseNonNull();

    return makeUnexpected(DecodingStatus::Invalid);
}

Expected<Ref<NativeImage>, DecodingStatus> BitmapImageSource::nativeImageAtIndexRequestIfNeeded(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    if (index >= m_frames.size())
        return makeUnexpected(DecodingStatus::Invalid);

    auto status = requestNativeImageAtIndexIfNeeded(index, subsamplingLevel, ImageAnimatingState::No, options);
    if (status == DecodingStatus::Invalid || status == DecodingStatus::Decoding)
        return makeUnexpected(status);

    if (RefPtr nativeImage = frameAtIndex(index).nativeImage())
        return nativeImage.releaseNonNull();

    return makeUnexpected(DecodingStatus::Invalid);
}

Expected<Ref<NativeImage>, DecodingStatus> BitmapImageSource::nativeImageAtIndexForDrawing(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    if (options.decodingMode() == DecodingMode::Asynchronous)
        return nativeImageAtIndexRequestIfNeeded(index, subsamplingLevel, options);
    return nativeImageAtIndexCacheIfNeeded(index, subsamplingLevel, options);
}

Expected<Ref<NativeImage>, DecodingStatus> BitmapImageSource::currentNativeImageForDrawing(SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    startAnimation(subsamplingLevel, options);
    return nativeImageAtIndexForDrawing(currentFrameIndex(), subsamplingLevel, options);
}

RefPtr<NativeImage> BitmapImageSource::nativeImageAtIndex(unsigned index)
{
    auto nativeImage = nativeImageAtIndexCacheIfNeeded(index);
    if (!nativeImage)
        return nullptr;
    return RefPtr { nativeImage->ptr() };
}

RefPtr<NativeImage> BitmapImageSource::preTransformedNativeImageAtIndex(unsigned index, ImageOrientation orientation)
{
    RefPtr nativeImage = nativeImageAtIndex(index);
    if (!nativeImage)
        return nullptr;

    auto size =  this->size();
    auto sourceSize = this->sourceSize();

    if (orientation == ImageOrientation::Orientation::None && size == sourceSize)
        return nativeImage;

    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = frameOrientationAtIndex(index);

    RefPtr buffer = ImageBuffer::create(size, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return nativeImage;

    auto destinationRect = FloatRect { FloatPoint(), size };
    auto sourceRect = FloatRect { FloatPoint(), sourceSize };

    buffer->context().drawNativeImage(*nativeImage, destinationRect, sourceRect, { orientation });
    return ImageBuffer::sinkIntoNativeImage(WTFMove(buffer));
}

template<typename MetadataType>
MetadataType BitmapImageSource::imageMetadata(MetadataType& cachedValue, const MetadataType& defaultValue, CachedFlag cachedFlag, MetadataType (ImageDecoder::*functor)() const) const
{
    if (m_cachedFlags.contains(cachedFlag))
        return cachedValue;

    if (!m_decoder)
        return defaultValue;

    if (!m_decoder->isSizeAvailable())
        return defaultValue;

    cachedValue = (*m_decoder.*functor)();
    m_cachedFlags.add(cachedFlag);
    const_cast<BitmapImageSource&>(*this).didDecodeProperties(m_decoder->bytesDecodedToDetermineProperties());
    return cachedValue;
}

template<typename MetadataType>
MetadataType BitmapImageSource::primaryNativeImageMetadata(MetadataType& cachedValue, const MetadataType& defaultValue, CachedFlag cachedFlag, MetadataType (NativeImage::*functor)() const) const
{
    if (m_cachedFlags.contains(cachedFlag))
        return cachedValue;

    RefPtr nativeImage = const_cast<BitmapImageSource&>(*this).primaryNativeImage();
    if (!nativeImage)
        return defaultValue;

    cachedValue = (*nativeImage.*functor)();
    m_cachedFlags.add(cachedFlag);
    return cachedValue;
}

template<typename MetadataType>
MetadataType BitmapImageSource::primaryImageFrameMetadata(MetadataType& cachedValue, CachedFlag cachedFlag, MetadataType (ImageFrame::*functor)() const) const
{
    if (m_cachedFlags.contains(cachedFlag))
        return cachedValue;

    auto& frame = const_cast<BitmapImageSource&>(*this).primaryImageFrame();

    // Don't cache any unavailable frame metadata. Just return the default metadata.
    if (!frame.hasMetadata())
        return (frame.*functor)();

    cachedValue = (frame.*functor)();
    m_cachedFlags.add(cachedFlag);
    return cachedValue;
}

EncodedDataStatus BitmapImageSource::encodedDataStatus() const
{
    return imageMetadata(m_encodedDataStatus, EncodedDataStatus::Unknown, CachedFlag::EncodedDataStatus, &ImageDecoder::encodedDataStatus);
}

IntSize BitmapImageSource::size(ImageOrientation orientation) const
{
    auto densityCorrectedSize = this->densityCorrectedSize();
    if (!densityCorrectedSize)
        return sourceSize(orientation);

    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = this->orientation();

    return orientation.usesWidthAsHeight() ? densityCorrectedSize->transposedSize() : *densityCorrectedSize;
}

IntSize BitmapImageSource::sourceSize(ImageOrientation orientation) const
{
    IntSize size;

#if !USE(CG)
    // It's possible that we have decoded the metadata, but not frame contents yet. In that case ImageDecoder claims to
    // have the size available, but the frame cache is empty. Return the decoder size without caching in such case.
    if (m_decoder && m_frames.isEmpty())
        size = m_decoder->size();
    else
#endif
        size = primaryImageFrameMetadata(m_size, CachedFlag::Size, &ImageFrame::size);

    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = this->orientation();

    return orientation.usesWidthAsHeight() ? size.transposedSize() : size;
}

std::optional<IntSize> BitmapImageSource::densityCorrectedSize() const
{
    return primaryImageFrameMetadata(m_densityCorrectedSize, CachedFlag::DensityCorrectedSize, &ImageFrame::densityCorrectedSize);
}

ImageOrientation BitmapImageSource::orientation() const
{
    return primaryImageFrameMetadata(m_orientation, CachedFlag::Orientation, &ImageFrame::orientation);
}

unsigned BitmapImageSource::primaryFrameIndex() const
{
    return imageMetadata(m_primaryFrameIndex, std::size_t(0), CachedFlag::PrimaryFrameIndex, &ImageDecoder::primaryFrameIndex);
}

unsigned BitmapImageSource::frameCount() const
{
    return imageMetadata(m_frameCount, std::size_t(0), CachedFlag::FrameCount, &ImageDecoder::frameCount);
}

RepetitionCount BitmapImageSource::repetitionCount() const
{
    return imageMetadata(m_repetitionCount, static_cast<RepetitionCount>(RepetitionCountNone), CachedFlag::RepetitionCount, &ImageDecoder::repetitionCount);
}

DestinationColorSpace BitmapImageSource::colorSpace() const
{
    return primaryNativeImageMetadata(m_colorSpace, DestinationColorSpace::SRGB(), CachedFlag::ColorSpace, &NativeImage::colorSpace);
}

std::optional<Color> BitmapImageSource::singlePixelSolidColor() const
{
    if (!hasSolidColor())
        return std::nullopt;

    return primaryNativeImageMetadata(m_singlePixelSolidColor, std::optional<Color>(), CachedFlag::SinglePixelSolidColor, &NativeImage::singlePixelSolidColor);
}

String BitmapImageSource::uti() const
{
#if USE(CG)
    return imageMetadata(m_uti, String(), CachedFlag::UTI, &ImageDecoder::uti);
#else
    return String();
#endif
}

String BitmapImageSource::filenameExtension() const
{
    return imageMetadata(m_filenameExtension, String(), CachedFlag::FilenameExtension, &ImageDecoder::filenameExtension);
}

String BitmapImageSource::accessibilityDescription() const
{
    return imageMetadata(m_accessibilityDescription, String(), CachedFlag::AccessibilityDescription, &ImageDecoder::accessibilityDescription);
}

std::optional<IntPoint> BitmapImageSource::hotSpot() const
{
    return imageMetadata(m_hotSpot, std::optional<IntPoint>(), CachedFlag::HotSpot, &ImageDecoder::hotSpot);
}

SubsamplingLevel BitmapImageSource::maximumSubsamplingLevel() const
{
    if (m_cachedFlags.contains(CachedFlag::MaximumSubsamplingLevel))
        return m_maximumSubsamplingLevel;

    if (!m_decoder)
        return SubsamplingLevel::Default;

    if (!m_decoder->isSizeAvailable())
        return SubsamplingLevel::Default;

    // FIXME: this value was chosen to be appropriate for iOS since the image
    // subsampling is only enabled by default on iOS. Choose a different value
    // if image subsampling is enabled on other platform.
    static constexpr int maximumImageAreaBeforeSubsampling = 5 * 1024 * 1024;
    auto level = SubsamplingLevel::First;

    for (; level < SubsamplingLevel::Last; ++level) {
        if (frameSizeAtIndex(0, level).area() < maximumImageAreaBeforeSubsampling)
            break;
    }

    m_maximumSubsamplingLevel = level;
    m_cachedFlags.add(CachedFlag::MaximumSubsamplingLevel);
    return m_maximumSubsamplingLevel;
}

SubsamplingLevel BitmapImageSource::subsamplingLevelForScaleFactor(GraphicsContext& context, const FloatSize& scaleFactor, AllowImageSubsampling allowImageSubsampling)
{
#if USE(CG)
    if (allowImageSubsampling == AllowImageSubsampling::No)
        return SubsamplingLevel::Default;

    // Never use subsampled images for drawing into PDF contexts.
    if (context.hasPlatformContext() && CGContextGetType(context.platformContext()) == kCGContextTypePDF)
        return SubsamplingLevel::Default;

    float scale = std::min(float(1), std::max(scaleFactor.width(), scaleFactor.height()));
    if (!(scale > 0 && scale <= 1))
        return SubsamplingLevel::Default;

    int result = std::ceil(std::log2(1 / scale));
    return static_cast<SubsamplingLevel>(std::min(result, static_cast<int>(maximumSubsamplingLevel())));
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(scaleFactor);
    UNUSED_PARAM(allowImageSubsampling);
    return SubsamplingLevel::Default;
#endif
}

IntSize BitmapImageSource::frameSizeAtIndex(unsigned index, SubsamplingLevel subsamplingLevel) const
{
    return const_cast<BitmapImageSource&>(*this).frameAtIndexCacheIfNeeded(index, subsamplingLevel).size();
}

Seconds BitmapImageSource::frameDurationAtIndex(unsigned index) const
{
    return const_cast<BitmapImageSource&>(*this).frameAtIndexCacheIfNeeded(index).duration();
}

ImageOrientation BitmapImageSource::frameOrientationAtIndex(unsigned index) const
{
    return const_cast<BitmapImageSource&>(*this).frameAtIndexCacheIfNeeded(index).orientation();
}

DecodingStatus BitmapImageSource::frameDecodingStatusAtIndex(unsigned index) const
{
    return const_cast<BitmapImageSource&>(*this).frameAtIndexCacheIfNeeded(index).decodingStatus();
}

const char* BitmapImageSource::sourceUTF8() const
{
    return m_bitmapImage.sourceUTF8();
}

void BitmapImageSource::setMinimumDecodingDurationForTesting(Seconds duration)
{
    workQueue().setMinimumDecodingDurationForTesting(duration);
}

void BitmapImageSource::dump(TextStream& ts) const
{
    ts.dumpProperty("source-utf8", sourceUTF8());

    if (m_workQueue)
        m_workQueue->dump(ts);

    if (m_frameAnimator)
        m_frameAnimator->dump(ts);

    ts.dumpProperty("size", size());
    ts.dumpProperty("density-corrected-size", densityCorrectedSize());
    ts.dumpProperty("primary-frame-index", primaryFrameIndex());
    ts.dumpProperty("frame-count", frameCount());
    ts.dumpProperty("repetition-count", repetitionCount());

    ts.dumpProperty("uti", uti());
    ts.dumpProperty("filename-extension", filenameExtension());
    ts.dumpProperty("accessibility-description", accessibilityDescription());

    ts.dumpProperty("decoded-size", m_decodedSize);
    ts.dumpProperty("decode-count-for-testing", m_decodeCountForTesting);
}

} // namespace WebCore
