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

#pragma once

#include "BitmapImageDescriptor.h"
#include "ImageFrameWorkQueue.h"
#include "ImageSource.h"
#include <wtf/Expected.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class BitmapImage;
class ImageDecoder;
class ImageFrameAnimator;
class ImageObserver;

class BitmapImageSource : public ImageSource {
public:
    static Ref<BitmapImageSource> create(BitmapImage&, AlphaOption, GammaAndColorProfileOption);

    virtual ~BitmapImageSource();

    // State
    ImageDecoder* decoder(FragmentedSharedBuffer* = nullptr) const;
    ImageDecoder* decoderIfExists() const { return m_decoder.get(); }

    // Encoded and decoded data
    void destroyDecodedData(bool destroyAll) final;
    void resetData();
    unsigned decodedSize() const { return m_decodedSize; }
    void didDecodeProperties(unsigned decodedPropertiesSize);

    // Animation
    bool isAnimationAllowed() const;

    // Decoding & animation
    bool isPendingDecodingAtIndex(unsigned index, SubsamplingLevel, const DecodingOptions&) const;
    void destroyNativeImageAtIndex(unsigned index);
    void imageFrameAtIndexAvailable(unsigned index, ImageAnimatingState, DecodingStatus);
    void imageFrameDecodeAtIndexHasFinished(unsigned index, SubsamplingLevel, ImageAnimatingState, const DecodingOptions&, RefPtr<NativeImage>&&);

    // ImageFrame
    unsigned primaryFrameIndex() const final { return m_descriptor.primaryFrameIndex(); }

    const Vector<ImageFrame>& frames() const { return m_frames; }
    const ImageFrame& primaryImageFrame(const std::optional<SubsamplingLevel>& subsamplingLevel = std::nullopt) final { return frameAtIndexCacheIfNeeded(primaryFrameIndex(), subsamplingLevel); }

    // NativeImage
    DecodingStatus requestNativeImageAtIndexIfNeeded(unsigned index, SubsamplingLevel, ImageAnimatingState, const DecodingOptions&);

    RefPtr<NativeImage> primaryNativeImage() final { return nativeImageAtIndex(primaryFrameIndex()); }

    // Image Metadata
    unsigned frameCount() const final { return m_descriptor.frameCount(); }
    RepetitionCount repetitionCount() const { return m_descriptor.repetitionCount(); }

    // ImageFrame metadata
    IntSize frameSizeAtIndex(unsigned index, SubsamplingLevel = SubsamplingLevel::Default) const;
    Seconds frameDurationAtIndex(unsigned index) const final;
    DecodingStatus frameDecodingStatusAtIndex(unsigned index) const final;

    // Testing support
    const char* sourceUTF8() const;

private:
    BitmapImageSource(BitmapImage&, AlphaOption, GammaAndColorProfileOption);

    // State
    ImageFrameAnimator* frameAnimator() const;
    ImageFrameWorkQueue& workQueue() const;

    // Encoded and decoded data
    void encodedDataStatusChanged(EncodedDataStatus);
    EncodedDataStatus dataChanged(FragmentedSharedBuffer*, bool allDataReceived) final;
    EncodedDataStatus setData(FragmentedSharedBuffer*, bool allDataReceived);

    void decodedSizeChanged(long long decodedSize);
    void decodedSizeIncreased(unsigned decodedSize);
    void decodedSizeDecreased(unsigned decodedSize);
    void decodedSizeReset(unsigned decodedSize);
    bool canDestroyDecodedData() const;

    void clearFrameBufferCache();

    // Animation
    void startAnimation() final;
    bool startAnimation(SubsamplingLevel, const DecodingOptions&);
    void stopAnimation() final;
    void resetAnimation() final;
    bool isAnimated() const final;
    bool isAnimating() const final;
    bool hasEverAnimated() const final;

    // Decoding
    bool isLargeForDecoding() const final;
    bool isDecodingWorkQueueIdle() const;
    bool isCompatibleWithOptionsAtIndex(unsigned index, SubsamplingLevel, const DecodingOptions&) const;
    void stopDecodingWorkQueue() final;
    void decode(Function<void(DecodingStatus)>&& decodeCallback) final;
    void callDecodeCallbacks(DecodingStatus);
    void imageFrameDecodeAtIndexHasFinished(unsigned index, ImageAnimatingState, DecodingStatus);

    // ImageFrame
    unsigned currentFrameIndex() const final;

    void cacheMetadataAtIndex(unsigned index, SubsamplingLevel, const DecodingOptions&);
    void cacheNativeImageAtIndex(unsigned index, SubsamplingLevel, const DecodingOptions&, Ref<NativeImage>&&);

    const ImageFrame& frameAtIndex(unsigned index) const;
    const ImageFrame& frameAtIndexCacheIfNeeded(unsigned index, const std::optional<SubsamplingLevel>& = std::nullopt);
    const ImageFrame& currentImageFrame(const std::optional<SubsamplingLevel>& subsamplingLevel = std::nullopt) final { return frameAtIndexCacheIfNeeded(currentFrameIndex(), subsamplingLevel); }

    // NativeImage
    DecodingStatus requestNativeImageAtIndex(unsigned index, SubsamplingLevel, ImageAnimatingState, const DecodingOptions&);

    Expected<Ref<NativeImage>, DecodingStatus> nativeImageAtIndexCacheIfNeeded(unsigned index, SubsamplingLevel = SubsamplingLevel::Default, const DecodingOptions& = { });
    Expected<Ref<NativeImage>, DecodingStatus> nativeImageAtIndexRequestIfNeeded(unsigned index, SubsamplingLevel, const DecodingOptions&);
    Expected<Ref<NativeImage>, DecodingStatus> nativeImageAtIndexForDrawing(unsigned index, SubsamplingLevel, const DecodingOptions&);

    Expected<Ref<NativeImage>, DecodingStatus> currentNativeImageForDrawing(SubsamplingLevel, const DecodingOptions&) final;

    RefPtr<NativeImage> nativeImageAtIndex(unsigned index) final;
    RefPtr<NativeImage> preTransformedNativeImageAtIndex(unsigned index, ImageOrientation);

    RefPtr<NativeImage> currentNativeImage() final { return nativeImageAtIndex(currentFrameIndex()); }
    RefPtr<NativeImage> currentPreTransformedNativeImage(ImageOrientation orientation) final { return preTransformedNativeImageAtIndex(currentFrameIndex(), orientation); }

    EncodedDataStatus encodedDataStatus() const { return m_descriptor.encodedDataStatus(); }
    IntSize size(ImageOrientation orientation = ImageOrientation::Orientation::FromImage) const final { return m_descriptor.size(orientation); }
    IntSize sourceSize(ImageOrientation orientation = ImageOrientation::Orientation::FromImage) const final { return m_descriptor.sourceSize(orientation); }
    std::optional<IntSize> densityCorrectedSize() const { return m_descriptor.densityCorrectedSize(); }
    bool hasDensityCorrectedSize() const final { return densityCorrectedSize().has_value(); }
    ImageOrientation orientation() const final { return m_descriptor.orientation(); }
    DestinationColorSpace colorSpace() const final { return m_descriptor.colorSpace(); }
    std::optional<Color> singlePixelSolidColor() const final { return m_descriptor.singlePixelSolidColor(); }

    String uti() const final { return m_descriptor.uti(); }
    String filenameExtension() const final { return m_descriptor.filenameExtension(); }
    String accessibilityDescription() const final { return m_descriptor.accessibilityDescription(); }
    std::optional<IntPoint> hotSpot() const final { return m_descriptor.hotSpot(); }
    SubsamplingLevel maximumSubsamplingLevel() const { return m_descriptor.maximumSubsamplingLevel(); }
    SubsamplingLevel subsamplingLevelForScaleFactor(GraphicsContext& context, const FloatSize& scaleFactor, AllowImageSubsampling allowImageSubsampling) final { return m_descriptor.subsamplingLevelForScaleFactor(context, scaleFactor, allowImageSubsampling); }

#if ENABLE(QUICKLOOK_FULLSCREEN)
    bool shouldUseQuickLookForFullscreen() const final { return m_descriptor.shouldUseQuickLookForFullscreen(); }
#endif

#if ENABLE(SPATIAL_IMAGE_DETECTION)
    bool isSpatial() const final { return m_descriptor.isSpatial(); }
#endif

    // ImageFrame metadata
    ImageOrientation frameOrientationAtIndex(unsigned index) const final;

    // BitmapImage metadata
    RefPtr<ImageObserver> imageObserver() const;
    String mimeType() const;
    long long expectedContentLength() const;

    // Testing support
    unsigned decodeCountForTesting() const final { return m_decodeCountForTesting; }
    unsigned blankDrawCountForTesting() const final { return m_blankDrawCountForTesting; }
    void setMinimumDecodingDurationForTesting(Seconds) final;
    void setClearDecoderAfterAsyncFrameRequestForTesting(bool enabled) final { m_clearDecoderAfterAsyncFrameRequestForTesting = enabled; }
    void setAsyncDecodingEnabledForTesting(bool enabled) final { m_isAsyncDecodingEnabledForTesting = enabled; }
    bool isAsyncDecodingEnabledForTesting() const final { return m_isAsyncDecodingEnabledForTesting; }

    void dump(TextStream&) const final;

    // State
    WeakPtr<BitmapImage> m_bitmapImage;
    AlphaOption m_alphaOption { AlphaOption::Premultiplied };
    GammaAndColorProfileOption m_gammaAndColorProfileOption { GammaAndColorProfileOption::Applied };
    bool m_allDataReceived { false };

    BitmapImageDescriptor m_descriptor;
    mutable RefPtr<ImageDecoder> m_decoder;
    mutable std::unique_ptr<ImageFrameAnimator> m_frameAnimator;
    mutable RefPtr<ImageFrameWorkQueue> m_workQueue;
    Vector<Function<void(DecodingStatus)>> m_decodeCallbacks;

    // ImageFrame
    Vector<ImageFrame> m_frames;
    unsigned m_decodedSize { 0 };
    unsigned m_decodedPropertiesSize { 0 };

    // Testing support
    unsigned m_decodeCountForTesting { 0 };
    unsigned m_blankDrawCountForTesting { 0 };
    bool m_isAsyncDecodingEnabledForTesting { false };
    bool m_clearDecoderAfterAsyncFrameRequestForTesting { false };
};

} // namespace WebCore
