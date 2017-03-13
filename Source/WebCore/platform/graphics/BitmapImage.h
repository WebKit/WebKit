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
#include "URL.h"

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

class Timer;

class BitmapImage final : public Image {
public:
    static Ref<BitmapImage> create(NativeImagePtr&& nativeImage, ImageObserver* observer = nullptr)
    {
        return adoptRef(*new BitmapImage(WTFMove(nativeImage), observer));
    }
    static Ref<BitmapImage> create(ImageObserver* observer = nullptr)
    {
        return adoptRef(*new BitmapImage(observer));
    }
#if PLATFORM(WIN)
    WEBCORE_EXPORT static RefPtr<BitmapImage> create(HBITMAP);
#endif
    virtual ~BitmapImage();
    
    bool hasSingleSecurityOrigin() const override { return true; }

    bool dataChanged(bool allDataReceived) override;
    unsigned decodedSize() const { return m_source.decodedSize(); }

    bool isSizeAvailable() const { return m_source.isSizeAvailable(); }
    size_t frameCount() const { return m_source.frameCount(); }
    RepetitionCount repetitionCount() const { return m_source.repetitionCount(); }
    String filenameExtension() const override { return m_source.filenameExtension(); }
    std::optional<IntPoint> hotSpot() const override { return m_source.hotSpot(); }

    // FloatSize due to override.
    FloatSize size() const override { return m_source.size(); }
    IntSize sizeRespectingOrientation() const { return m_source.sizeRespectingOrientation(); }
    Color singlePixelSolidColor() const override { return m_source.singlePixelSolidColor(); }

    bool frameIsBeingDecodedAtIndex(size_t index, const std::optional<IntSize>& sizeForDrawing) const { return m_source.frameIsBeingDecodedAtIndex(index, sizeForDrawing); }
    bool frameHasDecodedNativeImage(size_t index) const { return m_source.frameHasDecodedNativeImage(index); }
    bool frameIsCompleteAtIndex(size_t index) const { return m_source.frameIsCompleteAtIndex(index); }
    bool frameHasAlphaAtIndex(size_t index) const { return m_source.frameHasAlphaAtIndex(index); }
    bool frameHasValidNativeImageAtIndex(size_t index, const std::optional<SubsamplingLevel>& subsamplingLevel, const std::optional<IntSize>& sizeForDrawing) const { return m_source.frameHasValidNativeImageAtIndex(index, subsamplingLevel, sizeForDrawing); }
    SubsamplingLevel frameSubsamplingLevelAtIndex(size_t index) const { return m_source.frameSubsamplingLevelAtIndex(index); }

    float frameDurationAtIndex(size_t index) const { return m_source.frameDurationAtIndex(index); }
    ImageOrientation frameOrientationAtIndex(size_t index) const { return m_source.frameOrientationAtIndex(index); }

    size_t currentFrame() const { return m_currentFrame; }
    bool currentFrameKnownToBeOpaque() const override { return !frameHasAlphaAtIndex(currentFrame()); }
    ImageOrientation orientationForCurrentFrame() const override { return frameOrientationAtIndex(currentFrame()); }

    bool isAsyncDecodingForcedForTesting() const { return m_frameDecodingDurationForTesting > 0; }
    void setFrameDecodingDurationForTesting(float duration) { m_frameDecodingDurationForTesting = duration; }
    bool isLargeImageAsyncDecodingRequired();
    bool isAnimatedImageAsyncDecodingRequired();

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
#endif

    WEBCORE_EXPORT NativeImagePtr nativeImage(const GraphicsContext* = nullptr) override;
    NativeImagePtr nativeImageForCurrentFrame(const GraphicsContext* = nullptr) override;
#if USE(CG)
    NativeImagePtr nativeImageOfSize(const IntSize&, const GraphicsContext* = nullptr) override;
    Vector<NativeImagePtr> framesNativeImages() override;
#endif

protected:
    WEBCORE_EXPORT BitmapImage(NativeImagePtr&&, ImageObserver* = nullptr);
    WEBCORE_EXPORT BitmapImage(ImageObserver* = nullptr);

    NativeImagePtr frameImageAtIndex(size_t, const std::optional<SubsamplingLevel>& = { }, const std::optional<IntSize>& sizeForDrawing = { }, const GraphicsContext* = nullptr);

    String sourceURL() const { return imageObserver() ? imageObserver()->sourceUrl().string() : emptyString(); }
    bool allowSubsampling() const { return imageObserver() && imageObserver()->allowSubsampling(); }
    bool allowLargeImageAsyncDecoding() const { return imageObserver() && imageObserver()->allowLargeImageAsyncDecoding(); }
    bool allowAnimatedImageAsyncDecoding() const { return imageObserver() && imageObserver()->allowAnimatedImageAsyncDecoding(); }
    bool showDebugBackground() const { return imageObserver() && imageObserver()->showDebugBackground(); }

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

    void draw(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator, BlendMode, ImageOrientationDescription) override;
    void drawPattern(GraphicsContext&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator, BlendMode = BlendModeNormal) override;
#if PLATFORM(WIN)
    void drawFrameMatchingSourceSize(GraphicsContext&, const FloatRect& dstRect, const IntSize& srcSize, CompositeOperator) override;
#endif

    // Animation.
    enum class StartAnimationResult { CannotStart, IncompleteData, TimerActive, DecodingActive, Started };
    bool isAnimated() const override { return m_source.frameCount() > 1; }
    bool shouldAnimate();
    bool canAnimate();
    void startAnimation() override { internalStartAnimation(); }
    StartAnimationResult internalStartAnimation();
    void advanceAnimation();
    void internalAdvanceAnimation();

    // It may look unusual that there is no start animation call as public API. This is because
    // we start and stop animating lazily. Animation begins whenever someone draws the image. It will
    // automatically pause once all observers no longer want to render the image anywhere.
    void stopAnimation() override;
    void resetAnimation() override;
    void newFrameNativeImageAvailableAtIndex(size_t) override;

    // Handle platform-specific data
    void invalidatePlatformData();

#if !ASSERT_DISABLED
    bool notSolidColor() override;
#endif

#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> tiffRepresentation(const Vector<NativeImagePtr>&);
#endif

private:
    void clearTimer();
    void startTimer(double delay);
    bool isBitmapImage() const override { return true; }
    void dump(TextStream&) const override;

    // Animated images over a certain size are considered large enough that we'll only hang on to one frame at a time.
#if !PLATFORM(IOS)
    static const unsigned LargeAnimationCutoff = 5242880;
#else
    static const unsigned LargeAnimationCutoff = 2097152;
#endif

    mutable ImageSource m_source;

    size_t m_currentFrame { 0 }; // The index of the current frame of animation.
    SubsamplingLevel m_currentSubsamplingLevel { SubsamplingLevel::Default };
    std::optional<IntSize> m_sizeForDrawing;
    std::unique_ptr<Timer> m_frameTimer;
    RepetitionCount m_repetitionsComplete { RepetitionCountNone }; // How many repetitions we've finished.
    double m_desiredFrameStartTime { 0 }; // The system time at which we hope to see the next call to startAnimation().
    bool m_animationFinished { false };

    float m_frameDecodingDurationForTesting { 0 };
    double m_desiredFrameDecodeTimeForTesting { 0 };
#if !LOG_DISABLED
    size_t m_lateFrameCount { 0 };
    size_t m_earlyFrameCount { 0 };
    size_t m_cachedFrameCount { 0 };
#endif

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
