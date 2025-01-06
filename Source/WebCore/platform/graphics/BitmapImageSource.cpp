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
#include "BitmapImageDescriptor.h"
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
    : m_bitmapImage(&bitmapImage)
    , m_alphaOption(alphaOption)
    , m_gammaAndColorProfileOption(gammaAndColorProfileOption)
    , m_descriptor(*this)
{
}

BitmapImageSource::~BitmapImageSource() = default;

ImageDecoder* BitmapImageSource::decoder(FragmentedSharedBuffer* data) const
{
    if (m_decoder)
        return m_decoder.get();

    if (!data)
        return nullptr;

    m_decoder = ImageDecoder::create(*data, mimeType(), m_alphaOption, m_gammaAndColorProfileOption);
    if (!m_decoder)
        return nullptr;

    m_decoder->setEncodedDataStatusChangeCallback([weakThis = ThreadSafeWeakPtr { *this }] (auto status) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->encodedDataStatusChanged(status);
    });

    if (auto expectedContentLength = this->expectedContentLength())
        m_decoder->setExpectedContentSize(expectedContentLength);

    return m_decoder.get();
}

ImageFrameAnimator* BitmapImageSource::frameAnimator() const
{
    if (m_frameAnimator)
        return m_frameAnimator.get();

    // Number of frames can only be known for sure when loadimg the image is complete.
    if (encodedDataStatus() != EncodedDataStatus::Complete)
        return nullptr;

    if (!isAnimated())
        return nullptr;

    m_frameAnimator = makeUniqueWithoutRefCountedCheck<ImageFrameAnimator>(const_cast<BitmapImageSource&>(*this));
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
    ASSERT(m_decoder);

    if (status >= EncodedDataStatus::SizeAvailable)
        m_frames.resizeToFit(m_decoder->frameCount());

    if (auto imageObserver = this->imageObserver())
        imageObserver->encodedDataStatusChanged(*m_bitmapImage, status);
}

EncodedDataStatus BitmapImageSource::dataChanged(FragmentedSharedBuffer* data, bool allDataReceived)
{
    m_descriptor.clear();

    auto status = setData(data, allDataReceived);
    if (status < EncodedDataStatus::TypeAvailable)
        return status;

    encodedDataStatusChanged(status);
    return status;
}

void BitmapImageSource::destroyDecodedData(bool destroyAll)
{
    LOG(Images, "BitmapImageSource::%s - %p - url: %s. Decoded data with destroyAll = %d will be destroyed.", __FUNCTION__, this, sourceUTF8(), destroyAll);

    bool canDestroyDecodedData = destroyAll && this->canDestroyDecodedData();

    unsigned primaryFrameIndex = this->primaryFrameIndex();
    unsigned currentFrameIndex = this->currentFrameIndex();
    unsigned decodedSize = 0;

    for (unsigned index = 0, framesSize = m_frames.size(); index < framesSize; ++index) {
        if (!canDestroyDecodedData && (index == primaryFrameIndex || index == currentFrameIndex))
            continue;

        if (!destroyAll && index > currentFrameIndex)
            break;

        decodedSize += m_frames[index].clearImage();
    }

    decodedSizeReset(decodedSize);

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

    if (auto imageObserver = this->imageObserver())
        imageObserver->decodedSizeChanged(*m_bitmapImage, decodedSize);
}

void BitmapImageSource::decodedSizeIncreased(unsigned decodedSize)
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

void BitmapImageSource::decodedSizeDecreased(unsigned decodedSize)
{
    if (!decodedSize)
        return;

    ASSERT(m_decodedSize >= decodedSize);
    m_decodedSize -= decodedSize;
    decodedSizeChanged(-static_cast<long long>(decodedSize));
}

void BitmapImageSource::decodedSizeReset(unsigned decodedSize)
{
    ASSERT(m_decodedSize >= decodedSize);
    m_decodedSize -= decodedSize;

    // Clearing the ImageSource destroys the extra decoded data used for
    // determining image properties.
    decodedSize += m_decodedPropertiesSize;
    m_decodedPropertiesSize = 0;
    decodedSizeChanged(-static_cast<long long>(decodedSize));
}

void BitmapImageSource::destroyNativeImageAtIndex(unsigned index)
{
    if (index >= m_frames.size())
        return;

    decodedSizeDecreased(m_frames[index].clearImage());
}

bool BitmapImageSource::canDestroyDecodedData() const
{
    // Animated images should preserve the current frame till the next one finishes decoding.
    if (!isDecodingWorkQueueIdle())
        return false;

    // Small image should be decoded synchronously. Deleting its decoded frame is fine.
    if (!isLargeForDecoding())
        return true;

    if (auto imageObserver = this->imageObserver())
        return imageObserver->canDestroyDecodedData(*m_bitmapImage);

    return true;
}

void BitmapImageSource::didDecodeProperties(unsigned decodedPropertiesSize)
{
    if (m_decodedSize)
        return;

    long long decodedSize = static_cast<long long>(decodedPropertiesSize) - m_decodedPropertiesSize;
    m_decodedPropertiesSize = decodedPropertiesSize;
    decodedSizeChanged(decodedSize);
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
    m_allDataReceived = allDataReceived;
    return encodedDataStatus();
}

void BitmapImageSource::resetData()
{
    m_decoder = nullptr;

    if (m_bitmapImage)
        setData(m_bitmapImage->data(), m_allDataReceived);
}

void BitmapImageSource::startAnimation()
{
    startAnimation(SubsamplingLevel::Default, DecodingMode::Synchronous);
}

bool BitmapImageSource::startAnimation(SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    RefPtr frameAnimator = this->frameAnimator();
    if (!frameAnimator)
        return false;

    return frameAnimator->startAnimation(subsamplingLevel, options);
}

void BitmapImageSource::stopAnimation()
{
    if (!m_frameAnimator || !m_frameAnimator->isAnimating())
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
    if (auto imageObserver = this->imageObserver())
        return imageObserver->allowsAnimation(*m_bitmapImage);

    return true;
}

bool BitmapImageSource::hasEverAnimated() const
{
    return m_frameAnimator && m_frameAnimator->hasEverAnimated();
}

bool BitmapImageSource::isLargeForDecoding() const
{
    auto sizeInBytes = size(ImageOrientation::Orientation::None).unclampedArea() * sizeof(uint32_t);
    return sizeInBytes > (isAnimated() ? 100 * KB : 500 * KB);
}

bool BitmapImageSource::isDecodingWorkQueueIdle() const
{
    return !m_workQueue || m_workQueue->isIdle();
}

void BitmapImageSource::stopDecodingWorkQueue()
{
    LOG(Images, "BitmapImageSource::%s - %p - url: %s. Decoding work queue will be stopped.", __FUNCTION__, this, sourceUTF8());

    if (!m_workQueue || !m_workQueue->isIdle())
        return;

    m_workQueue->stop();
}

bool BitmapImageSource::isPendingDecodingAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options) const
{
    if (!m_workQueue)
        return false;

    return m_workQueue->isPendingDecodingAtIndex(index, subsamplingLevel, options);
}

bool BitmapImageSource::isCompatibleWithOptionsAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options) const
{
    return frameAtIndex(index).hasDecodedNativeImageCompatibleWithOptions(subsamplingLevel, options);
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
    RefPtr frameAnimator = this->frameAnimator();

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

    if (auto imageObserver = this->imageObserver())
        imageObserver->imageFrameAvailable(*m_bitmapImage, animatingState, nullptr, decodingStatus);
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

    if (!nativeImage || !m_decoder) {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d has failed.", __FUNCTION__, this, sourceUTF8(), index);

        destroyNativeImageAtIndex(index);
        imageFrameDecodeAtIndexHasFinished(index, animatingState, DecodingStatus::Invalid);
    } else {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d has been decoded.", __FUNCTION__, this, sourceUTF8(), index);

        cacheNativeImageAtIndex(index, subsamplingLevel, options, nativeImage.releaseNonNull());

        if (frameAtIndex(index).isComplete())
            ++m_decodeCountForTesting;

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
    ASSERT(m_decoder);

    if (index >= m_frames.size())
        return;

    auto& frame = m_frames[index];

    m_decoder->fetchFrameMetaDataAtIndex(index, subsamplingLevel, options, frame);

    if (repetitionCount())
        frame.m_duration = m_decoder->frameDurationAtIndex(index);
}

void BitmapImageSource::cacheNativeImageAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options, Ref<NativeImage>&& nativeImage)
{
    ASSERT(m_decoder);

    if (index >= m_frames.size())
        return;

    destroyNativeImageAtIndex(index);

    // Do not cache NativeImage if adding its frameByes to MemoryCache will cause numerical overflow.
    auto frameBytes = nativeImage->size().unclampedArea() * sizeof(uint32_t);
    if (!isInBounds<unsigned>(frameBytes + m_decodedSize))
        return;

    auto& frame = m_frames[index];
    frame.m_nativeImage = WTFMove(nativeImage);

    cacheMetadataAtIndex(index, subsamplingLevel, options);
    decodedSizeIncreased(frame.frameBytes());
}

const ImageFrame& BitmapImageSource::frameAtIndex(unsigned index) const
{
    if (index >= m_frames.size())
        return ImageFrame::defaultFrame();

    return m_frames[index];
}

const ImageFrame& BitmapImageSource::frameAtIndexCacheIfNeeded(unsigned index, const std::optional<SubsamplingLevel>& subsamplingLevel)
{
    if (!m_decoder)
        return ImageFrame::defaultFrame();

    if (index >= m_frames.size())
        return ImageFrame::defaultFrame();

    auto& frame = m_frames[index];
    auto subsamplingLevelValue = subsamplingLevel.value_or(frame.subsamplingLevel());

    if (frame.isComplete() && subsamplingLevelValue == frame.subsamplingLevel())
        return frame;

    destroyNativeImageAtIndex(index);

    // Retrieve the metadata from ImageDecoder if the ImageFrame isn't complete.
    cacheMetadataAtIndex(index, subsamplingLevelValue, { });
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

    // Never decode the same frame from two different threads.
    if (isPendingDecodingAtIndex(index, subsamplingLevel, options)) {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d is being decoded.", __FUNCTION__, this, sourceUTF8(), index);
        ++m_blankDrawCountForTesting;
        return DecodingStatus::Decoding;
    }

    // isCompatibleWithOptionsAtIndex() returns true only if the frame is complete.
    if (isCompatibleWithOptionsAtIndex(index, subsamplingLevel, options))
        return DecodingStatus::Complete;

    return requestNativeImageAtIndex(index, subsamplingLevel, animatingState, options);
}

Expected<Ref<NativeImage>, DecodingStatus> BitmapImageSource::nativeImageAtIndexCacheIfNeeded(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    if (!m_decoder)
        return makeUnexpected(DecodingStatus::Invalid);

    if (index >= m_frames.size())
        return makeUnexpected(DecodingStatus::Invalid);

    // FIXME: Remove this for CG; ImageIO should be thread safe when decoding the same frame from multiple threads.
    // Never decode the same frame from two different threads.
    if (isPendingDecodingAtIndex(index, subsamplingLevel, options)) {
        LOG(Images, "BitmapImageSource::%s - %p - url: %s. Frame at index = %d is being decoded.", __FUNCTION__, this, sourceUTF8(), index);
        ++m_blankDrawCountForTesting;
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
    if (!m_decoder)
        return makeUnexpected(DecodingStatus::Invalid);

    ASSERT(!isAnimated());

    auto status = requestNativeImageAtIndexIfNeeded(index, subsamplingLevel, ImageAnimatingState::No, options);
    if (status == DecodingStatus::Invalid || status == DecodingStatus::Decoding)
        return makeUnexpected(status);

    if (RefPtr nativeImage = frameAtIndex(index).nativeImage())
        return nativeImage.releaseNonNull();

    return makeUnexpected(DecodingStatus::Invalid);
}

Expected<Ref<NativeImage>, DecodingStatus> BitmapImageSource::nativeImageAtIndexForDrawing(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    // If this is an animated image and the frame is not available, we have no
    // choice but to decode it synchronously. Otherwise, a flicker will happen.
    if (options.decodingMode() == DecodingMode::Asynchronous && !isAnimated())
        return nativeImageAtIndexRequestIfNeeded(index, subsamplingLevel, options);
    return nativeImageAtIndexCacheIfNeeded(index, subsamplingLevel, options);
}

Expected<Ref<NativeImage>, DecodingStatus> BitmapImageSource::currentNativeImageForDrawing(SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    startAnimation(subsamplingLevel, options);

    auto effectiveOptions = options;

    // If frame0 is displayed for the first time, startAnimation() has to request decoding frame1
    // asynchronously. A flicker will occur if we request decoding frame0 also asynchronously.
    if (options.decodingMode() == DecodingMode::Asynchronous && isAnimated() && !hasEverAnimated())
        effectiveOptions = { DecodingMode::Synchronous, options.sizeForDrawing() };

    return nativeImageAtIndexForDrawing(currentFrameIndex(), subsamplingLevel, effectiveOptions);
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

    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = frameOrientationAtIndex(index);

    if (orientation == ImageOrientation::Orientation::None && size == sourceSize)
        return nativeImage;

    RefPtr buffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), ImageBufferPixelFormat::BGRA8);
    if (!buffer)
        return nativeImage;

    auto destinationRect = FloatRect { FloatPoint(), size };
    auto sourceRect = FloatRect { FloatPoint(), sourceSize };

    buffer->context().drawNativeImage(*nativeImage, destinationRect, sourceRect, { orientation });
    return ImageBuffer::sinkIntoNativeImage(WTFMove(buffer));
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

Headroom BitmapImageSource::frameHeadroomAtIndex(unsigned index) const
{
    return const_cast<BitmapImageSource&>(*this).frameAtIndexCacheIfNeeded(index).headroom();
}

DecodingStatus BitmapImageSource::frameDecodingStatusAtIndex(unsigned index) const
{
    return const_cast<BitmapImageSource&>(*this).frameAtIndexCacheIfNeeded(index).decodingStatus();
}

RefPtr<ImageObserver> BitmapImageSource::imageObserver() const
{
    return m_bitmapImage ? m_bitmapImage->imageObserver() : nullptr;
}

String BitmapImageSource::mimeType() const
{
    return m_bitmapImage ? m_bitmapImage->mimeType() : emptyString();
}

long long BitmapImageSource::expectedContentLength() const
{
    return m_bitmapImage ? m_bitmapImage->expectedContentLength() : 0;
}

const char* BitmapImageSource::sourceUTF8() const
{
    constexpr const char* emptyString = "";
    return m_bitmapImage ? m_bitmapImage->sourceUTF8() : emptyString;
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

    m_descriptor.dump(ts);

    ts.dumpProperty("decoded-size", m_decodedSize);
    ts.dumpProperty("decode-count-for-testing", m_decodeCountForTesting);
}

} // namespace WebCore
