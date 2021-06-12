/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2008, 2015 Apple Inc. All rights reserved.
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
#include "BitmapImage.h"

#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "Logging.h"
#include "Settings.h"
#include "Timer.h"
#include <wtf/Vector.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

#if USE(CG)
#include <pal/spi/cg/CoreGraphicsSPI.h>
#endif

namespace WebCore {

BitmapImage::BitmapImage(ImageObserver* observer)
    : Image(observer)
    , m_source(ImageSource::create(this))
{
}

BitmapImage::BitmapImage(RefPtr<NativeImage>&& image, ImageObserver* observer)
    : Image(observer)
    , m_source(ImageSource::create(WTFMove(image)))
{
}

BitmapImage::~BitmapImage()
{
    invalidatePlatformData();
    clearTimer();
    m_source->clearImage();
    m_source->stopAsyncDecodingQueue();
}

void BitmapImage::updateFromSettings(const Settings& settings)
{
    m_allowSubsampling = settings.imageSubsamplingEnabled();
    m_allowAnimatedImageAsyncDecoding = settings.animatedImageAsyncDecodingEnabled();
    m_showDebugBackground = settings.showDebugBorders();
}

void BitmapImage::destroyDecodedData(bool destroyAll)
{
    LOG(Images, "BitmapImage::%s - %p - url: %s", __FUNCTION__, this, sourceURL().string().utf8().data());

    if (!destroyAll)
        m_source->destroyDecodedDataBeforeFrame(m_currentFrame);
    else if (!canDestroyDecodedData())
        m_source->destroyAllDecodedDataExcludeFrame(m_currentFrame);
    else {
        m_source->destroyAllDecodedData();
        m_currentFrameDecodingStatus = DecodingStatus::Invalid;
    }

    // There's no need to throw away the decoder unless we're explicitly asked
    // to destroy all of the frames.
    if (!destroyAll || m_source->hasAsyncDecodingQueue())
        m_source->clearFrameBufferCache(m_currentFrame);
    else
        m_source->resetData(data());

    invalidatePlatformData();
}

void BitmapImage::destroyDecodedDataIfNecessary(bool destroyAll)
{
    // If we have decoded frames but there is no encoded data, we shouldn't destroy
    // the decoded image since we won't be able to reconstruct it later.
    if (!data() && frameCount())
        return;

    if (m_source->decodedSize() < LargeAnimationCutoff)
        return;

    destroyDecodedData(destroyAll);
}

EncodedDataStatus BitmapImage::dataChanged(bool allDataReceived)
{
    if (m_source->decodedSize() && !canUseAsyncDecodingForLargeImages())
        m_source->destroyIncompleteDecodedData();

    m_currentFrameDecodingStatus = DecodingStatus::Invalid;
    return m_source->dataChanged(data(), allDataReceived);
}

void BitmapImage::setCurrentFrameDecodingStatusIfNecessary(DecodingStatus decodingStatus)
{
    // When new data is received, m_currentFrameDecodingStatus is set to DecodingStatus::Invalid
    // to force decoding the frame when it's drawn. m_currentFrameDecodingStatus should not be
    // changed in this case till draw() is called and sets its value to DecodingStatus::Decoding.
    if (m_currentFrameDecodingStatus != DecodingStatus::Decoding)
        return;
    m_currentFrameDecodingStatus = decodingStatus;
}

RefPtr<NativeImage> BitmapImage::frameImageAtIndexCacheIfNeeded(size_t index, SubsamplingLevel subsamplingLevel, const GraphicsContext* targetContext)
{
    if (!frameHasFullSizeNativeImageAtIndex(index, subsamplingLevel)) {
        LOG(Images, "BitmapImage::%s - %p - url: %s [subsamplingLevel was %d, resampling]", __FUNCTION__, this, sourceURL().string().utf8().data(), static_cast<int>(frameSubsamplingLevelAtIndex(index)));
        invalidatePlatformData();
    }
#if USE(DIRECT2D)
    m_source->setTargetContext(targetContext);
#else
    UNUSED_PARAM(targetContext);
#endif
    return m_source->frameImageAtIndexCacheIfNeeded(index, subsamplingLevel);
}

RefPtr<NativeImage> BitmapImage::nativeImage(const GraphicsContext* targetContext)
{
    return frameImageAtIndexCacheIfNeeded(0, SubsamplingLevel::Default, targetContext);
}

RefPtr<NativeImage> BitmapImage::nativeImageForCurrentFrame(const GraphicsContext* targetContext)
{
    return frameImageAtIndexCacheIfNeeded(m_currentFrame, SubsamplingLevel::Default, targetContext);
}

RefPtr<NativeImage> BitmapImage::preTransformedNativeImageForCurrentFrame(bool respectOrientation, const GraphicsContext* targetContext)
{
    auto image = nativeImageForCurrentFrame(targetContext);
    if (!image)
        return image;

    auto orientation = respectOrientation ? orientationForCurrentFrame() : ImageOrientation(ImageOrientation::None);
    auto correctedSize = m_source->densityCorrectedSize(orientation);
    if (orientation == ImageOrientation::None && !correctedSize)
        return image;

    auto correctedSizeFloat = correctedSize ? FloatSize(correctedSize.value()) : size();
    auto buffer = ImageBuffer::create(correctedSizeFloat, RenderingMode::Unaccelerated, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return image;

    auto sourceSize = this->sourceSize();

    FloatRect destRect(FloatPoint(), correctedSizeFloat);
    FloatRect sourceRect(FloatPoint(), sourceSize);

    buffer->context().drawNativeImage(*image, sourceSize, destRect, sourceRect, { orientation });
    return ImageBuffer::sinkIntoNativeImage(WTFMove(buffer));
}

#if USE(CG)
RefPtr<NativeImage> BitmapImage::nativeImageOfSize(const IntSize& size, const GraphicsContext* targetContext)
{
    size_t count = frameCount();

    for (size_t i = 0; i < count; ++i) {
        auto image = frameImageAtIndexCacheIfNeeded(i, SubsamplingLevel::Default, targetContext);
        if (image && image->size() == size)
            return image;
    }

    // Fallback to the first frame image if we can't find the right size
    return frameImageAtIndexCacheIfNeeded(0, SubsamplingLevel::Default, targetContext);
}

Vector<Ref<NativeImage>> BitmapImage::framesNativeImages()
{
    Vector<Ref<NativeImage>> images;
    size_t count = frameCount();

    for (size_t i = 0; i < count; ++i) {
        if (auto image = frameImageAtIndexCacheIfNeeded(i))
            images.append(makeRef(*image));
    }

    return images;
}
#endif

#if ASSERT_ENABLED
bool BitmapImage::notSolidColor()
{
    return size().width() != 1 || size().height() != 1 || frameCount() > 1;
}
#endif // ASSERT_ENABLED

static inline void drawNativeImage(NativeImage& image, GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, const IntSize& srcSize, const ImagePaintingOptions& options)
{
    // Subsampling may have given us an image that is smaller than size().
    IntSize subsampledImageSize = image.size();
    if (options.orientation().usesWidthAsHeight())
        subsampledImageSize = subsampledImageSize.transposedSize();

    // srcRect is in the coordinates of the unsubsampled image, so we have to map it to the subsampled image.
    FloatRect adjustedSrcRect = srcRect;
    if (subsampledImageSize != srcSize)
        adjustedSrcRect = mapRect(srcRect, FloatRect({ }, srcSize), FloatRect({ }, subsampledImageSize));

    context.drawNativeImage(image, subsampledImageSize, destRect, adjustedSrcRect, options);
}

ImageDrawResult BitmapImage::draw(GraphicsContext& context, const FloatRect& destRect, const FloatRect& requestedSrcRect, const ImagePaintingOptions& options)
{
    if (destRect.isEmpty() || requestedSrcRect.isEmpty())
        return ImageDrawResult::DidNothing;

    
    auto srcRect = requestedSrcRect;
    auto preferredSize = size();
    auto srcSize = sourceSize();

    if (preferredSize != srcSize)
        srcRect.scale(srcSize.width() / preferredSize.width(), srcSize.height() / preferredSize.height());

    auto scaleFactorForDrawing = context.scaleFactorForDrawing(destRect, srcRect);
    auto sizeForDrawing = expandedIntSize(srcSize * scaleFactorForDrawing);
    ImageDrawResult result = ImageDrawResult::DidDraw;

    m_currentSubsamplingLevel = m_allowSubsampling ? subsamplingLevelForScaleFactor(context, scaleFactorForDrawing) : SubsamplingLevel::Default;
    LOG(Images, "BitmapImage::%s - %p - url: %s [subsamplingLevel = %d scaleFactorForDrawing = (%.4f, %.4f)]", __FUNCTION__, this, sourceURL().string().utf8().data(), static_cast<int>(m_currentSubsamplingLevel), scaleFactorForDrawing.width(), scaleFactorForDrawing.height());

    RefPtr<NativeImage> image;
    if (options.decodingMode() == DecodingMode::Asynchronous) {
        ASSERT(!canAnimate());
        ASSERT(!m_currentFrame || m_animationFinished);

        bool frameIsCompatible = frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(m_currentFrame, m_currentSubsamplingLevel, DecodingOptions(sizeForDrawing));
        bool frameIsBeingDecoded = frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(m_currentFrame, DecodingOptions(sizeForDrawing));

        // If the current frame is incomplete, a new request for decoding this frame has to be made even if
        // it is currently being decoded. New data may have been received since the previous request was made.
        if ((!frameIsCompatible && !frameIsBeingDecoded) || m_currentFrameDecodingStatus == DecodingStatus::Invalid) {
            LOG(Images, "BitmapImage::%s - %p - url: %s [requesting large async decoding]", __FUNCTION__, this, sourceURL().string().utf8().data());
            m_source->requestFrameAsyncDecodingAtIndex(m_currentFrame, m_currentSubsamplingLevel, sizeForDrawing);
            m_currentFrameDecodingStatus = DecodingStatus::Decoding;
        }

        if (m_currentFrameDecodingStatus == DecodingStatus::Decoding)
            result = ImageDrawResult::DidRequestDecoding;

        if (!frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(m_currentFrame, m_currentSubsamplingLevel, DecodingOptions(DecodingMode::Asynchronous))) {
            if (m_showDebugBackground)
                fillWithSolidColor(context, destRect, Color::yellow.colorWithAlphaByte(128), options.compositeOperator());
            return result;
        }

        image = frameImageAtIndex(m_currentFrame);
        LOG(Images, "BitmapImage::%s - %p - url: %s [a decoded frame will be used for asynchronous drawing]", __FUNCTION__, this, sourceURL().string().utf8().data());
    } else {
        StartAnimationStatus status = internalStartAnimation();
        ASSERT_IMPLIES(status == StartAnimationStatus::DecodingActive, (!m_currentFrame && !m_repetitionsComplete) || frameHasFullSizeNativeImageAtIndex(m_currentFrame, m_currentSubsamplingLevel));

        if (status == StartAnimationStatus::DecodingActive && m_showDebugBackground) {
            fillWithSolidColor(context, destRect, Color::yellow.colorWithAlphaByte(128), options.compositeOperator());
            return result;
        }
        
        // If the decodingMode changes from asynchronous to synchronous and new data is received,
        // the current incomplete decoded frame has to be destroyed.
        if (m_currentFrameDecodingStatus == DecodingStatus::Invalid)
            m_source->destroyIncompleteDecodedData();
        
        bool frameIsCompatible = frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(m_currentFrame, m_currentSubsamplingLevel, DecodingOptions(sizeForDrawing));
        bool frameIsBeingDecoded = frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(m_currentFrame, DecodingOptions(DecodingMode::Asynchronous));
        
        if (frameIsCompatible) {
            image = frameImageAtIndex(m_currentFrame);
            LOG(Images, "BitmapImage::%s - %p - url: %s [a decoded frame will reused for synchronous drawing]", __FUNCTION__, this, sourceURL().string().utf8().data());
        } else if (frameIsBeingDecoded) {
            // FIXME: instead of showing the yellow rectangle and returning we need to wait for this frame to finish decoding.
            if (m_showDebugBackground) {
                fillWithSolidColor(context, destRect, Color::yellow.colorWithAlphaByte(128), options.compositeOperator());
                LOG(Images, "BitmapImage::%s - %p - url: %s [waiting for async decoding to finish]", __FUNCTION__, this, sourceURL().string().utf8().data());
            }
            return ImageDrawResult::DidRequestDecoding;
        } else {
            image = frameImageAtIndexCacheIfNeeded(m_currentFrame, m_currentSubsamplingLevel, &context);
            LOG(Images, "BitmapImage::%s - %p - url: %s [an image frame will be decoded synchronously]", __FUNCTION__, this, sourceURL().string().utf8().data());
        }
        
        if (!image) // If it's too early we won't have an image yet.
            return ImageDrawResult::DidNothing;

        if (m_currentFrameDecodingStatus != DecodingStatus::Complete)
            ++m_decodeCountForTesting;
    }

    ASSERT(image);
    Color color = singlePixelSolidColor();
    if (color.isValid()) {
        fillWithSolidColor(context, destRect, color, options.compositeOperator());
        return result;
    }

    auto orientation = options.orientation();
    if (orientation == ImageOrientation::FromImage) {
        orientation = frameOrientationAtIndex(m_currentFrame);
        drawNativeImage(*image, context, destRect, srcRect, IntSize(sourceSize(orientation)), { options, orientation });
    } else
        drawNativeImage(*image, context, destRect, srcRect, IntSize(sourceSize(orientation)), options);

    m_currentFrameDecodingStatus = frameDecodingStatusAtIndex(m_currentFrame);

    if (imageObserver())
        imageObserver()->didDraw(*this);

    return result;
}

void BitmapImage::drawPattern(GraphicsContext& ctxt, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    if (tileRect.isEmpty())
        return;

    if (!ctxt.drawLuminanceMask()) {
        // If new data is received, the current incomplete decoded frame has to be destroyed.
        if (m_currentFrameDecodingStatus == DecodingStatus::Invalid)
            m_source->destroyIncompleteDecodedData();
        
        Image::drawPattern(ctxt, destRect, tileRect, transform, phase, spacing, { options, ImageOrientation::FromImage });
        m_currentFrameDecodingStatus = frameDecodingStatusAtIndex(m_currentFrame);
        return;
    }

    if (!m_cachedImage) {
        auto buffer = ImageBuffer::createCompatibleBuffer(expandedIntSize(tileRect.size()), DestinationColorSpace::SRGB(), ctxt);
        if (!buffer)
            return;

        ImageObserver* observer = imageObserver();

        // Temporarily reset image observer, we don't want to receive any changeInRect() calls due to this relayout.
        setImageObserver(nullptr);

        draw(buffer->context(), tileRect, tileRect, { options, DecodingMode::Synchronous, ImageOrientation::FromImage });

        setImageObserver(observer);
        buffer->convertToLuminanceMask();

        m_cachedImage = ImageBuffer::sinkIntoImage(WTFMove(buffer), PreserveResolution::Yes);
        if (!m_cachedImage)
            return;
    }

    ctxt.setDrawLuminanceMask(false);
    m_cachedImage->drawPattern(ctxt, destRect, tileRect, transform, phase, spacing, { options, ImageOrientation::FromImage });
}

bool BitmapImage::shouldAnimate() const
{
    return repetitionCount() && !m_animationFinished && imageObserver();
}

bool BitmapImage::canAnimate() const
{
    return shouldAnimate() && frameCount() > 1;
}

bool BitmapImage::canUseAsyncDecodingForLargeImages() const
{
    return !canAnimate() && m_source->canUseAsyncDecoding();
}

bool BitmapImage::shouldUseAsyncDecodingForAnimatedImages() const
{
    return canAnimate() && m_allowAnimatedImageAsyncDecoding && (shouldUseAsyncDecodingForTesting() || m_source->canUseAsyncDecoding());
}

void BitmapImage::clearTimer()
{
    m_frameTimer = nullptr;
}

void BitmapImage::startTimer(Seconds delay)
{
    ASSERT(!m_frameTimer);
    m_frameTimer = makeUnique<Timer>(*this, &BitmapImage::advanceAnimation);
    m_frameTimer->startOneShot(delay);
}

SubsamplingLevel BitmapImage::subsamplingLevelForScaleFactor(GraphicsContext& context, const FloatSize& scaleFactor)
{
#if USE(CG)
    // Never use subsampled images for drawing into PDF contexts.
    if (context.hasPlatformContext() && CGContextGetType(context.platformContext()) == kCGContextTypePDF)
        return SubsamplingLevel::Default;

    float scale = std::min(float(1), std::max(scaleFactor.width(), scaleFactor.height()));
    if (!(scale > 0 && scale <= 1))
        return SubsamplingLevel::Default;

    int result = std::ceil(std::log2(1 / scale));
    return static_cast<SubsamplingLevel>(std::min(result, static_cast<int>(m_source->maximumSubsamplingLevel())));
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(scaleFactor);
    return SubsamplingLevel::Default;
#endif
}

bool BitmapImage::canDestroyDecodedData()
{
    // Animated images should preserve the current frame till the next one finishes decoding.
    if (m_source->hasAsyncDecodingQueue())
        return false;

    // Small image should be decoded synchronously. Deleting its decoded frame is fine.
    if (!canUseAsyncDecodingForLargeImages())
        return true;

    return !imageObserver() || imageObserver()->canDestroyDecodedData(*this);
}

BitmapImage::StartAnimationStatus BitmapImage::internalStartAnimation()
{
    LOG_WITH_STREAM(Images, stream << "BitmapImage " << this << " internalStartAnimation");

    if (!canAnimate())
        return StartAnimationStatus::CannotStart;

    if (m_frameTimer)
        return StartAnimationStatus::TimerActive;

    // Don't start a new animation until we draw the frame that is currently being decoded.
    size_t nextFrame = (m_currentFrame + 1) % frameCount();
    if (frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(nextFrame, DecodingOptions(DecodingMode::Asynchronous))) {
        LOG(Images, "BitmapImage::%s - %p - url: %s [nextFrame = %ld is being decoded]", __FUNCTION__, this, sourceURL().string().utf8().data(), nextFrame);
        return StartAnimationStatus::DecodingActive;
    }

    if (m_currentFrame >= frameCount() - 1) {
        // Don't advance past the last frame if we haven't decoded the whole image
        // yet and our repetition count is potentially unset. The repetition count
        // in a GIF can potentially come after all the rest of the image data, so
        // wait on it.
        if (!m_source->isAllDataReceived() && repetitionCount() == RepetitionCountOnce)
            return StartAnimationStatus::IncompleteData;

        ++m_repetitionsComplete;

        // Check for the end of animation.
        if (repetitionCount() != RepetitionCountInfinite && m_repetitionsComplete >= repetitionCount()) {
            m_animationFinished = true;
            destroyDecodedDataIfNecessary(false);
            return StartAnimationStatus::CannotStart;
        }

        destroyDecodedDataIfNecessary(true);
    }

    // Don't advance the animation to an incomplete frame.
    if (!m_source->isAllDataReceived() && !frameIsCompleteAtIndex(nextFrame))
        return StartAnimationStatus::IncompleteData;

    MonotonicTime time = MonotonicTime::now();

    // Handle initial state.
    if (!m_desiredFrameStartTime)
        m_desiredFrameStartTime = time;

    // Setting 'm_desiredFrameStartTime' to 'time' means we are late; otherwise we are early.
    m_desiredFrameStartTime = std::max(time, m_desiredFrameStartTime + Seconds { frameDurationAtIndex(m_currentFrame) });

    // Request async decoding for nextFrame only if this is required. If nextFrame is not in the frameCache,
    // it will be decoded on a separate work queue. When decoding nextFrame finishes, we will be notified
    // through the callback newFrameNativeImageAvailableAtIndex(). Otherwise, advanceAnimation() will be called
    // when the timer fires and m_currentFrame will be advanced to nextFrame since it is not being decoded.
    if (shouldUseAsyncDecodingForAnimatedImages()) {
        if (frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(nextFrame, m_currentSubsamplingLevel, DecodingOptions(std::optional<IntSize>())))
            LOG(Images, "BitmapImage::%s - %p - url: %s [cachedFrameCount = %ld nextFrame = %ld]", __FUNCTION__, this, sourceURL().string().utf8().data(), ++m_cachedFrameCount, nextFrame);
        else {
            m_source->requestFrameAsyncDecodingAtIndex(nextFrame, m_currentSubsamplingLevel);
            m_currentFrameDecodingStatus = DecodingStatus::Decoding;
            LOG(Images, "BitmapImage::%s - %p - url: %s [requesting async decoding for nextFrame = %ld]", __FUNCTION__, this, sourceURL().string().utf8().data(), nextFrame);
        }

        if (m_clearDecoderAfterAsyncFrameRequestForTesting)
            m_source->resetData(data());
    }

    ASSERT(!m_frameTimer);
    startTimer(m_desiredFrameStartTime - time);
    return StartAnimationStatus::Started;
}

void BitmapImage::advanceAnimation()
{
    clearTimer();

    // Don't advance to nextFrame unless its decoding has finished or was not required.
    size_t nextFrame = (m_currentFrame + 1) % frameCount();
    if (!frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(nextFrame, DecodingOptions(DecodingMode::Asynchronous)))
        internalAdvanceAnimation();
    else {
        // Force repaint if showDebugBackground() is on.
        if (m_showDebugBackground)
            imageObserver()->changedInRect(*this);
        LOG(Images, "BitmapImage::%s - %p - url: %s [lateFrameCount = %ld nextFrame = %ld]", __FUNCTION__, this, sourceURL().string().utf8().data(), ++m_lateFrameCount, nextFrame);
    }
}

void BitmapImage::internalAdvanceAnimation()
{
    m_currentFrame = (m_currentFrame + 1) % frameCount();
    ASSERT(!frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(m_currentFrame, DecodingOptions(DecodingMode::Asynchronous)));

    destroyDecodedDataIfNecessary(false);

    DecodingStatus decodingStatus = frameDecodingStatusAtIndex(m_currentFrame);
    setCurrentFrameDecodingStatusIfNecessary(decodingStatus);

    callDecodingCallbacks();

    if (imageObserver())
        imageObserver()->imageFrameAvailable(*this, ImageAnimatingState::Yes, nullptr, decodingStatus);

    LOG(Images, "BitmapImage::%s - %p - url: %s [m_currentFrame = %ld]", __FUNCTION__, this, sourceURL().string().utf8().data(), m_currentFrame);
}

bool BitmapImage::isAnimating() const
{
    return !!m_frameTimer;
}

void BitmapImage::stopAnimation()
{
    // This timer is used to animate all occurrences of this image. Don't invalidate
    // the timer unless all renderers have stopped drawing.
    clearTimer();
    if (canAnimate())
        m_source->stopAsyncDecodingQueue();
}

void BitmapImage::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = RepetitionCountNone;
    m_desiredFrameStartTime = { };
    m_animationFinished = false;

    // For extremely large animations, when the animation is reset, we just throw everything away.
    destroyDecodedDataIfNecessary(true);
}

void BitmapImage::decode(WTF::Function<void()>&& callback)
{
    if (!m_decodingCallbacks)
        m_decodingCallbacks = makeUnique<Vector<Function<void()>, 1>>();

    m_decodingCallbacks->append(WTFMove(callback));

    if (canAnimate())  {
        if (m_desiredFrameStartTime) {
            internalStartAnimation();
            return;
        }

        // The animated image has not been displayed. In this case, either the first frame has not been decoded yet or the animation has not started yet.
        bool frameIsCompatible = frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(m_currentFrame, m_currentSubsamplingLevel, std::optional<IntSize>());
        bool frameIsBeingDecoded = frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(m_currentFrame, std::optional<IntSize>());

        if (frameIsCompatible)
            internalStartAnimation();
        else if (!frameIsBeingDecoded) {
            m_source->requestFrameAsyncDecodingAtIndex(m_currentFrame, m_currentSubsamplingLevel, std::optional<IntSize>());
            m_currentFrameDecodingStatus = DecodingStatus::Decoding;
        }
        return;
    }

    bool frameIsCompatible = frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(m_currentFrame, m_currentSubsamplingLevel, std::optional<IntSize>());
    bool frameIsBeingDecoded = frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(m_currentFrame, std::optional<IntSize>());
    
    if (frameIsCompatible)
        callDecodingCallbacks();
    else if (!frameIsBeingDecoded) {
        m_source->requestFrameAsyncDecodingAtIndex(m_currentFrame, m_currentSubsamplingLevel, std::optional<IntSize>());
        m_currentFrameDecodingStatus = DecodingStatus::Decoding;
    }
}

void BitmapImage::callDecodingCallbacks()
{
    if (!m_decodingCallbacks)
        return;
    for (auto& decodingCallback : *m_decodingCallbacks)
        decodingCallback();
    m_decodingCallbacks = nullptr;
}

void BitmapImage::imageFrameAvailableAtIndex(size_t index)
{
    LOG(Images, "BitmapImage::%s - %p - url: %s [requested frame %ld is now available]", __FUNCTION__, this, sourceURL().string().utf8().data(), index);

    if (canAnimate()) {
        if (index == (m_currentFrame + 1) % frameCount()) {
            // Don't advance to nextFrame unless the timer was fired before its decoding finishes.
            if (!m_frameTimer)
                internalAdvanceAnimation();
            else
                LOG(Images, "BitmapImage::%s - %p - url: %s [earlyFrameCount = %ld nextFrame = %ld]", __FUNCTION__, this, sourceURL().string().utf8().data(), ++m_earlyFrameCount, index);
            return;
        }

        // Because of image partial loading, an image may start decoding as a large static image. But
        // when more data is received, frameCount() changes to be > 1 so the image starts animating.
        // The animation may even start before finishing the decoding of the first frame.
        ASSERT(!m_repetitionsComplete);
        LOG(Images, "BitmapImage::%s - %p - url: %s [More data makes frameCount() > 1]", __FUNCTION__, this, sourceURL().string().utf8().data());
    }

    ASSERT(index == m_currentFrame && !m_currentFrame);
    if (m_source->isAsyncDecodingQueueIdle())
        m_source->stopAsyncDecodingQueue();

    DecodingStatus decodingStatus = frameDecodingStatusAtIndex(m_currentFrame);
    setCurrentFrameDecodingStatusIfNecessary(decodingStatus);

    if (m_currentFrameDecodingStatus == DecodingStatus::Complete)
        ++m_decodeCountForTesting;

    // Call m_decodingCallbacks only if the image frame was decoded with the native size.
    if (frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(m_currentFrame, m_currentSubsamplingLevel, std::optional<IntSize>()))
        callDecodingCallbacks();

    if (imageObserver())
        imageObserver()->imageFrameAvailable(*this, ImageAnimatingState::No, nullptr, decodingStatus);
}

unsigned BitmapImage::decodeCountForTesting() const
{
    return m_decodeCountForTesting;
}

void BitmapImage::dump(TextStream& ts) const
{
    Image::dump(ts);
    
    if (isAnimated())
        ts.dumpProperty("current-frame", m_currentFrame);
    
    m_source->dump(ts);
}

}
