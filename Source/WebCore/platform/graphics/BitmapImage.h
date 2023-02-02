/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#pragma once

#include "Image.h"
#include "Color.h"
#include "ImageObserver.h"
#include "ImageOrientation.h"
#include "ImageSource.h"
#include "IntSize.h"

#if USE(CG) || USE(APPKIT)
#include <wtf/RetainPtr.h>
#endif

#if USE(APPKIT)
OBJC_CLASS NSImage;
#endif

#if PLATFORM(WIN)
typedef struct HBITMAP__ *HBITMAP;
#endif

namespace WebCore {

class Settings;
class Timer;

class BitmapImage final : public Image {
public:
    static RefPtr<BitmapImage> create(PlatformImagePtr&& platformImage)
    {
        return create(NativeImage::create(WTFMove(platformImage)));
    }
    static RefPtr<BitmapImage> create(RefPtr<NativeImage>&& nativeImage)
    {
        if (!nativeImage)
            return nullptr;
        return create(nativeImage.releaseNonNull());
    }
    static Ref<BitmapImage> create(Ref<NativeImage>&& nativeImage)
    {
        return adoptRef(*new BitmapImage(WTFMove(nativeImage)));
    }
    static Ref<BitmapImage> create(ImageObserver* observer = nullptr)
    {
        return adoptRef(*new BitmapImage(observer));
    }
#if PLATFORM(WIN)
    WEBCORE_EXPORT static RefPtr<BitmapImage> create(HBITMAP);
#endif
    virtual ~BitmapImage();

    void updateFromSettings(const Settings&);

    EncodedDataStatus dataChanged(bool allDataReceived) override;
    unsigned decodedSize() const { return m_source->decodedSize(); }

    EncodedDataStatus encodedDataStatus() const { return m_source->encodedDataStatus(); }
    size_t frameCount() const { return m_source->frameCount(); }
    RepetitionCount repetitionCount() const { return m_source->repetitionCount(); }
    String uti() const override { return m_source->uti(); }
    String filenameExtension() const override { return m_source->filenameExtension(); }
    String accessibilityDescription() const override { return m_source->accessibilityDescription(); }
    std::optional<IntPoint> hotSpot() const override { return m_source->hotSpot(); }

    // FloatSize due to override.
    FloatSize size(ImageOrientation orientation = ImageOrientation::Orientation::FromImage) const override { return m_source->size(orientation); }
    ImageOrientation orientation() const override { return m_source->orientation(); }
    Color singlePixelSolidColor() const override { return m_source->singlePixelSolidColor(); }
    bool frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(size_t index, const DecodingOptions& decodingOptions) const { return m_source->frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(index, decodingOptions); }
    DecodingStatus frameDecodingStatusAtIndex(size_t index) const { return m_source->frameDecodingStatusAtIndex(index); }
    bool frameIsCompleteAtIndex(size_t index) const { return frameDecodingStatusAtIndex(index) == DecodingStatus::Complete; }
    bool frameHasAlphaAtIndex(size_t index) const { return m_source->frameHasAlphaAtIndex(index); }

    bool frameHasFullSizeNativeImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel) { return m_source->frameHasFullSizeNativeImageAtIndex(index, subsamplingLevel); }
    bool frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(size_t index, const std::optional<SubsamplingLevel>& subsamplingLevel, const DecodingOptions& decodingOptions) { return m_source->frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(index, subsamplingLevel, decodingOptions); }

    SubsamplingLevel frameSubsamplingLevelAtIndex(size_t index) const { return m_source->frameSubsamplingLevelAtIndex(index); }

    Seconds frameDurationAtIndex(size_t index) const { return m_source->frameDurationAtIndex(index); }
    ImageOrientation frameOrientationAtIndex(size_t index) const { return m_source->frameOrientationAtIndex(index); }

    size_t currentFrame() const { return m_currentFrame; }
    bool currentFrameKnownToBeOpaque() const override { return !frameHasAlphaAtIndex(currentFrame()); }
    ImageOrientation orientationForCurrentFrame() const { return frameOrientationAtIndex(currentFrame()); }
    bool canAnimate() const;

    bool shouldUseAsyncDecodingForTesting() const { return m_source->frameDecodingDurationForTesting() > 0_s; }
    void setFrameDecodingDurationForTesting(Seconds duration) { m_source->setFrameDecodingDurationForTesting(duration); }
    bool canUseAsyncDecodingForLargeImages() const;
    bool shouldUseAsyncDecodingForAnimatedImages() const;
    void setClearDecoderAfterAsyncFrameRequestForTesting(bool value) { m_clearDecoderAfterAsyncFrameRequestForTesting = value; }
    void setLargeImageAsyncDecodingEnabledForTesting(bool enabled) { m_largeImageAsyncDecodingEnabledForTesting = enabled; }
    bool isLargeImageAsyncDecodingEnabledForTesting() const { return m_largeImageAsyncDecodingEnabledForTesting; }
    void stopAsyncDecodingQueue() { m_source->stopAsyncDecodingQueue(); }

    DestinationColorSpace colorSpace() final;

    WEBCORE_EXPORT unsigned decodeCountForTesting() const;

    // Accessors for native image formats.
#if USE(APPKIT)
    NSImage *nsImage() override;
    RetainPtr<NSImage> snapshotNSImage() override;
#endif

#if PLATFORM(COCOA)
    CFDataRef tiffRepresentation() override;
#endif

#if PLATFORM(WIN)
    bool getHBITMAP(HBITMAP) override;
    bool getHBITMAPOfSize(HBITMAP, const IntSize*) override;
#endif

#if PLATFORM(GTK)
    GdkPixbuf* getGdkPixbuf() override;
#if USE(GTK4)
    GdkTexture* gdkTexture() override;
#endif
#endif

    WEBCORE_EXPORT RefPtr<NativeImage> nativeImage(const DestinationColorSpace& = DestinationColorSpace::SRGB()) override;
    RefPtr<NativeImage> nativeImageForCurrentFrame() override;
    RefPtr<NativeImage> preTransformedNativeImageForCurrentFrame(bool respectOrientation) override;
#if USE(CG)
    RefPtr<NativeImage> nativeImageOfSize(const IntSize&) override;
    Vector<Ref<NativeImage>> framesNativeImages();
#endif

    void imageFrameAvailableAtIndex(size_t);
    void decode(Function<void()>&&);

private:
    WEBCORE_EXPORT BitmapImage(Ref<NativeImage>&&);
    WEBCORE_EXPORT BitmapImage(ImageObserver* = nullptr);

    RefPtr<NativeImage> frameImageAtIndex(size_t index) { return m_source->frameImageAtIndex(index); }
    RefPtr<NativeImage> frameImageAtIndexCacheIfNeeded(size_t, SubsamplingLevel = SubsamplingLevel::Default);

    // Called to invalidate cached data. When |destroyAll| is true, we wipe out
    // the entire frame buffer cache and tell the image source to destroy
    // everything; this is used when e.g. we want to free some room in the image
    // cache. If |destroyAll| is false, we only delete frames up to the current
    // one; this is used while animating large images to keep memory footprint
    // low without redecoding the whole image on every frame.
    void destroyDecodedData(bool destroyAll = true) override;

    // If the image is large enough, calls destroyDecodedData() and passes
    // |destroyAll| along.
    void destroyDecodedDataIfNecessary(bool destroyAll = true);

    FloatSize sourceSize(ImageOrientation orientation = ImageOrientation::Orientation::FromImage) const final { return m_source->sourceSize(orientation); }
    bool hasDensityCorrectedSize() const override { return m_source->hasDensityCorrectedSize(); }

    ImageDrawResult draw(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, const ImagePaintingOptions& = { }) override;
    void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { }) override;
#if PLATFORM(WIN)
    void drawFrameMatchingSourceSize(GraphicsContext&, const FloatRect& dstRect, const IntSize& srcSize, CompositeOperator) override;
#endif

    // Animation.
    enum class StartAnimationStatus { CannotStart, IncompleteData, TimerActive, DecodingActive, Started };
    bool isAnimated() const override { return m_source->frameCount() > 1; }
    bool shouldAnimate() const;
    void startAnimation() override { internalStartAnimation(); }
    StartAnimationStatus internalStartAnimation();
    void advanceAnimation();
    void internalAdvanceAnimation();
    bool isAnimating() const final;

    // It may look unusual that there is no start animation call as public API. This is because
    // we start and stop animating lazily. Animation begins whenever someone draws the image. It will
    // automatically pause once all observers no longer want to render the image anywhere.
    void stopAnimation() override;
    void resetAnimation() override;
    
    // Handle platform-specific data
    void invalidatePlatformData();

#if ASSERT_ENABLED
    bool notSolidColor() override;
#endif

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> tiffRepresentation(const Vector<Ref<NativeImage>>&);
#endif

    void clearTimer();
    void startTimer(Seconds delay);
    SubsamplingLevel subsamplingLevelForScaleFactor(GraphicsContext&, const FloatSize& scaleFactor);
    bool canDestroyDecodedData();
    void setCurrentFrameDecodingStatusIfNecessary(DecodingStatus);
    bool isBitmapImage() const override { return true; }
    void callDecodingCallbacks();
    void dump(WTF::TextStream&) const override;

    // Animated images over a certain size are considered large enough that we'll only hang on to one frame at a time.
    static const unsigned LargeAnimationCutoff = 30 * 1024 * 1024;

    mutable Ref<ImageSource> m_source;

    size_t m_currentFrame { 0 }; // The index of the current frame of animation.
    SubsamplingLevel m_currentSubsamplingLevel { SubsamplingLevel::Default };
    DecodingStatus m_currentFrameDecodingStatus { DecodingStatus::Invalid };
    std::unique_ptr<Timer> m_frameTimer;
    RepetitionCount m_repetitionsComplete { RepetitionCountNone }; // How many repetitions we've finished.
    MonotonicTime m_desiredFrameStartTime; // The system time at which we hope to see the next call to startAnimation().

    std::unique_ptr<Vector<Function<void()>, 1>> m_decodingCallbacks;

    bool m_animationFinished { false };

    // The default value of m_allowSubsampling should be the same as default value of the ImageSubsamplingEnabled setting.
#if PLATFORM(IOS_FAMILY)
    bool m_allowSubsampling { true };
#else
    bool m_allowSubsampling { false };
#endif
    bool m_allowAnimatedImageAsyncDecoding { false };
    bool m_showDebugBackground { false };

    bool m_clearDecoderAfterAsyncFrameRequestForTesting { false };
    bool m_largeImageAsyncDecodingEnabledForTesting { false };

#if ASSERT_ENABLED || !LOG_DISABLED
    size_t m_lateFrameCount { 0 };
    size_t m_earlyFrameCount { 0 };
    size_t m_cachedFrameCount { 0 };
#endif

    unsigned m_decodeCountForTesting { 0 };

#if USE(APPKIT)
    mutable RetainPtr<NSImage> m_nsImage; // A cached NSImage of all the frames. Only built lazily if someone actually queries for one.
#endif
#if USE(CG)
    mutable RetainPtr<CFDataRef> m_tiffRep; // Cached TIFF rep for all the frames. Only built lazily if someone queries for one.
#endif
    RefPtr<Image> m_cachedImage;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_IMAGE(BitmapImage)
