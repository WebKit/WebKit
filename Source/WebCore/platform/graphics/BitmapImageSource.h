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

    // State
    ImageDecoder* decoder(FragmentedSharedBuffer* = nullptr) const;

    // Encoded and decoded data
    void destroyDecodedData(bool destroyAll) final;
    void resetData();
    unsigned decodedSize() const { return m_decodedSize; }

    // Animation
    bool isAnimationAllowed() const;

    // Decoding & animation
    bool isPendingDecodingAtIndex(unsigned index, SubsamplingLevel, const DecodingOptions&) const;
    void destroyNativeImageAtIndex(unsigned index);
    void imageFrameAtIndexAvailable(unsigned index, ImageAnimatingState, DecodingStatus);
    void imageFrameDecodeAtIndexHasFinished(unsigned index, SubsamplingLevel, ImageAnimatingState, const DecodingOptions&, RefPtr<NativeImage>&&);

    // ImageFrame
    unsigned primaryFrameIndex() const final;

    const ImageFrame& primaryImageFrame() final { return frameAtIndexCacheIfNeeded(primaryFrameIndex()); }

    // NativeImage
    DecodingStatus requestNativeImageAtIndexIfNeeded(unsigned index, SubsamplingLevel, ImageAnimatingState, const DecodingOptions&);

    RefPtr<NativeImage> primaryNativeImage() final { return nativeImageAtIndex(primaryFrameIndex()); }

    // Image Metadata
    unsigned frameCount() const final;
    RepetitionCount repetitionCount() const;

    // ImageFrame metadata
    Seconds frameDurationAtIndex(unsigned index) const final;
    DecodingStatus frameDecodingStatusAtIndex(unsigned index) const final;

    // Testing support
    const char* sourceUTF8() const;

private:
    enum class CachedFlag : uint16_t {
        EncodedDataStatus           = 1 << 0,
        Size                        = 1 << 1,
        DensityCorrectedSize        = 1 << 2,
        Orientation                 = 1 << 3,
        PrimaryFrameIndex           = 1 << 4,
        FrameCount                  = 1 << 5,
        RepetitionCount             = 1 << 6,
        ColorSpace                  = 1 << 7,
        SinglePixelSolidColor       = 1 << 8,

        UTI                         = 1 << 9,
        FilenameExtension           = 1 << 10,
        AccessibilityDescription    = 1 << 11,
        HotSpot                     = 1 << 12,
        MaximumSubsamplingLevel     = 1 << 13,
    };

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

    void didDecodeProperties(unsigned decodedPropertiesSize);
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
    const ImageFrame& frameAtIndexCacheIfNeeded(unsigned index, SubsamplingLevel = SubsamplingLevel::Default);
    const ImageFrame& currentImageFrame() final { return frameAtIndexCacheIfNeeded(currentFrameIndex()); }

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

    // Image Metadata
    template<typename MetadataType>
    MetadataType imageMetadata(MetadataType& cachedValue, const MetadataType& defaultValue, CachedFlag, MetadataType (ImageDecoder::*functor)() const) const;

    template<typename MetadataType>
    MetadataType primaryNativeImageMetadata(MetadataType& cachedValue, const MetadataType& defaultValue, CachedFlag, MetadataType (NativeImage::*functor)() const) const;

    template<typename MetadataType>
    MetadataType primaryImageFrameMetadata(MetadataType& cachedValue, CachedFlag, MetadataType (ImageFrame::*functor)() const) const;

    EncodedDataStatus encodedDataStatus() const;
    IntSize size(ImageOrientation = ImageOrientation::Orientation::FromImage) const final;
    IntSize sourceSize(ImageOrientation = ImageOrientation::Orientation::FromImage) const final;
    std::optional<IntSize> densityCorrectedSize() const;
    bool hasDensityCorrectedSize() const final { return densityCorrectedSize().has_value(); }
    ImageOrientation orientation() const final;
    DestinationColorSpace colorSpace() const final;
    std::optional<Color> singlePixelSolidColor() const final;

    String uti() const final;
    String filenameExtension() const final;
    String accessibilityDescription() const final;
    std::optional<IntPoint> hotSpot() const final;
    SubsamplingLevel maximumSubsamplingLevel() const;

    SubsamplingLevel subsamplingLevelForScaleFactor(GraphicsContext&, const FloatSize& scaleFactor, AllowImageSubsampling) final;

#if ENABLE(QUICKLOOK_FULLSCREEN)
    bool shouldUseQuickLookForFullscreen() const;
#endif

    // ImageFrame metadata
    IntSize frameSizeAtIndex(unsigned index, SubsamplingLevel = SubsamplingLevel::Default) const;
    ImageOrientation frameOrientationAtIndex(unsigned index) const final;

    // BitmapImage metadata
    RefPtr<ImageObserver> imageObserver() const;
    String mimeType() const;
    long long expectedContentLength() const;

    // Testing support
    unsigned decodeCountForTesting() const { return m_decodeCountForTesting; }
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

    mutable RefPtr<ImageDecoder> m_decoder;
    mutable std::unique_ptr<ImageFrameAnimator> m_frameAnimator;
    mutable RefPtr<ImageFrameWorkQueue> m_workQueue;
    Vector<Function<void(DecodingStatus)>> m_decodeCallbacks;

    // Metadata
    mutable OptionSet<CachedFlag> m_cachedFlags;

    mutable EncodedDataStatus m_encodedDataStatus { EncodedDataStatus::Unknown };
    mutable IntSize m_size;
    mutable std::optional<IntSize> m_densityCorrectedSize;
    mutable ImageOrientation m_orientation;
    mutable size_t m_primaryFrameIndex { 0 };
    mutable size_t m_frameCount { 0 };
    mutable RepetitionCount m_repetitionCount { RepetitionCountNone };
    mutable DestinationColorSpace m_colorSpace { DestinationColorSpace::SRGB() };
    mutable std::optional<Color> m_singlePixelSolidColor;

    mutable String m_uti;
    mutable String m_filenameExtension;
    mutable String m_accessibilityDescription;
    mutable std::optional<IntPoint> m_hotSpot;
    mutable SubsamplingLevel m_maximumSubsamplingLevel { SubsamplingLevel::Default };

    // ImageFrame
    Vector<ImageFrame> m_frames;
    unsigned m_decodedSize { 0 };
    unsigned m_decodedPropertiesSize { 0 };

    // Testing support
    unsigned m_decodeCountForTesting { 0 };
    bool m_isAsyncDecodingEnabledForTesting { false };
    bool m_clearDecoderAfterAsyncFrameRequestForTesting { false };
};

} // namespace WebCore
