/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ImageDecoderCG.h"

#if USE(CG)

#include "ImageOrientation.h"
#include "ImageSourceCG.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "SharedBuffer.h"
#include "UTIRegistry.h"
#include <pal/spi/cg/ImageIOSPI.h>
#include <ImageIO/ImageIO.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>

namespace WebCore {

const CFStringRef WebCoreCGImagePropertyAPNGUnclampedDelayTime = CFSTR("UnclampedDelayTime");
const CFStringRef WebCoreCGImagePropertyAPNGDelayTime = CFSTR("DelayTime");
const CFStringRef WebCoreCGImagePropertyAPNGLoopCount = CFSTR("LoopCount");

#if PLATFORM(WIN)
const CFStringRef kCGImageSourceShouldPreferRGB32 = CFSTR("kCGImageSourceShouldPreferRGB32");
const CFStringRef kCGImageSourceSkipMetadata = CFSTR("kCGImageSourceSkipMetadata");
const CFStringRef kCGImageSourceSubsampleFactor = CFSTR("kCGImageSourceSubsampleFactor");
const CFStringRef kCGImageSourceShouldCacheImmediately = CFSTR("kCGImageSourceShouldCacheImmediately");
#endif

static RetainPtr<CFMutableDictionaryRef> createImageSourceOptions()
{
    RetainPtr<CFMutableDictionaryRef> options = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(options.get(), kCGImageSourceShouldCache, kCFBooleanTrue);
    CFDictionarySetValue(options.get(), kCGImageSourceShouldPreferRGB32, kCFBooleanTrue);
    CFDictionarySetValue(options.get(), kCGImageSourceSkipMetadata, kCFBooleanTrue);
    return options;
}
    
static RetainPtr<CFMutableDictionaryRef> createImageSourceAsyncOptions()
{
    RetainPtr<CFMutableDictionaryRef> options = createImageSourceOptions();
    CFDictionarySetValue(options.get(), kCGImageSourceShouldCacheImmediately, kCFBooleanTrue);
    CFDictionarySetValue(options.get(), kCGImageSourceCreateThumbnailFromImageAlways, kCFBooleanTrue);
    return options;
}

static RetainPtr<CFMutableDictionaryRef> appendImageSourceOption(RetainPtr<CFMutableDictionaryRef>&& options, SubsamplingLevel subsamplingLevel)
{
    subsamplingLevel = std::min(SubsamplingLevel::Last, std::max(SubsamplingLevel::First, subsamplingLevel));
    int subsampleInt = 1 << static_cast<int>(subsamplingLevel); // [0..3] => [1, 2, 4, 8]
    auto subsampleNumber = adoptCF(CFNumberCreate(nullptr,  kCFNumberIntType,  &subsampleInt));
    CFDictionarySetValue(options.get(), kCGImageSourceSubsampleFactor, subsampleNumber.get());
    return WTFMove(options);
}

static RetainPtr<CFMutableDictionaryRef> appendImageSourceOption(RetainPtr<CFMutableDictionaryRef>&& options, const IntSize& sizeForDrawing)
{
    unsigned maxDimension = DecodingOptions::maxDimension(sizeForDrawing);
    RetainPtr<CFNumberRef> maxDimensionNumber = adoptCF(CFNumberCreate(nullptr, kCFNumberIntType, &maxDimension));
    CFDictionarySetValue(options.get(), kCGImageSourceThumbnailMaxPixelSize, maxDimensionNumber.get());
    return WTFMove(options);
}
    
static RetainPtr<CFMutableDictionaryRef> appendImageSourceOptions(RetainPtr<CFMutableDictionaryRef>&& options, SubsamplingLevel subsamplingLevel, const IntSize& sizeForDrawing)
{
    if (subsamplingLevel != SubsamplingLevel::Default)
        options = appendImageSourceOption(WTFMove(options), subsamplingLevel);
    
    options = appendImageSourceOption(WTFMove(options), sizeForDrawing);
    return WTFMove(options);
}
    
static RetainPtr<CFDictionaryRef> imageSourceOptions(SubsamplingLevel subsamplingLevel = SubsamplingLevel::Default)
{
    static const auto options = createImageSourceOptions().leakRef();
    if (subsamplingLevel == SubsamplingLevel::Default)
        return options;
    return appendImageSourceOption(adoptCF(CFDictionaryCreateMutableCopy(nullptr, 0, options)), subsamplingLevel);
}

static RetainPtr<CFDictionaryRef> imageSourceAsyncOptions(SubsamplingLevel subsamplingLevel, const IntSize& sizeForDrawing)
{
    static const auto options = createImageSourceAsyncOptions().leakRef();
    return appendImageSourceOptions(adoptCF(CFDictionaryCreateMutableCopy(nullptr, 0, options)), subsamplingLevel, sizeForDrawing);
}
    
static ImageOrientation orientationFromProperties(CFDictionaryRef imageProperties)
{
    ASSERT(imageProperties);
    CFNumberRef orientationProperty = (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyOrientation);
    if (!orientationProperty)
        return ImageOrientation();
    
    int exifValue;
    CFNumberGetValue(orientationProperty, kCFNumberIntType, &exifValue);
    return ImageOrientation::fromEXIFValue(exifValue);
}

#if !PLATFORM(COCOA)
size_t sharedBufferGetBytesAtPosition(void* info, void* buffer, off_t position, size_t count)
{
    SharedBuffer* sharedBuffer = static_cast<SharedBuffer*>(info);
    size_t sourceSize = sharedBuffer->size();
    if (position >= sourceSize)
        return 0;
    
    const char* source = sharedBuffer->data() + position;
    size_t amount = std::min<size_t>(count, sourceSize - position);
    memcpy(buffer, source, amount);
    return amount;
}

void sharedBufferRelease(void* info)
{
    SharedBuffer* sharedBuffer = static_cast<SharedBuffer*>(info);
    sharedBuffer->deref();
}
#endif

ImageDecoderCG::ImageDecoderCG(SharedBuffer& data, AlphaOption, GammaAndColorProfileOption)
{
    RetainPtr<CFStringRef> utiHint;
    if (data.size() >= 32)
        utiHint = adoptCF(CGImageSourceGetTypeWithData(data.createCFData().get(), nullptr, nullptr));
    
    if (utiHint) {
        const void* key = kCGImageSourceTypeIdentifierHint;
        const void* value = utiHint.get();
        auto options = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        m_nativeDecoder = adoptCF(CGImageSourceCreateIncremental(options.get()));
    } else
        m_nativeDecoder = adoptCF(CGImageSourceCreateIncremental(nullptr));
}

size_t ImageDecoderCG::bytesDecodedToDetermineProperties() const
{
    // Measured by tracing malloc/calloc calls on Mac OS 10.6.6, x86_64.
    // A non-zero value ensures cached images with no decoded frames still enter
    // the live decoded resources list when the CGImageSource decodes image
    // properties, allowing the cache to prune the partially decoded image.
    // This value is likely to be inaccurate on other platforms, but the overall
    // behavior is unchanged.
    return 13088;
}
    
String ImageDecoderCG::uti() const
{
    return CGImageSourceGetType(m_nativeDecoder.get());
}

String ImageDecoderCG::filenameExtension() const
{
    return WebCore::preferredExtensionForImageType(uti());
}

EncodedDataStatus ImageDecoderCG::encodedDataStatus() const
{
    String uti = this->uti();
    if (uti.isEmpty())
        return EncodedDataStatus::Unknown;

    switch (CGImageSourceGetStatus(m_nativeDecoder.get())) {
    case kCGImageStatusUnknownType:
        return EncodedDataStatus::Error;

    case kCGImageStatusUnexpectedEOF:
    case kCGImageStatusInvalidData:
    case kCGImageStatusReadingHeader:
        // Ragnaros yells: TOO SOON! You have awakened me TOO SOON, Executus!
        if (!m_isAllDataReceived)
            return EncodedDataStatus::Unknown;

        return EncodedDataStatus::Error;

    case kCGImageStatusIncomplete: {
        if (!isSupportedImageType(uti))
            return EncodedDataStatus::Error;

        RetainPtr<CFDictionaryRef> image0Properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), 0, imageSourceOptions().get()));
        if (!image0Properties)
            return EncodedDataStatus::TypeAvailable;
        
        if (!CFDictionaryContainsKey(image0Properties.get(), kCGImagePropertyPixelWidth) || !CFDictionaryContainsKey(image0Properties.get(), kCGImagePropertyPixelHeight))
            return EncodedDataStatus::TypeAvailable;
        
        return EncodedDataStatus::SizeAvailable;
    }

    case kCGImageStatusComplete:
        if (!isSupportedImageType(uti))
            return EncodedDataStatus::Error;

        return EncodedDataStatus::Complete;
    }

    ASSERT_NOT_REACHED();
    return EncodedDataStatus::Unknown;
}

size_t ImageDecoderCG::frameCount() const
{
    return CGImageSourceGetCount(m_nativeDecoder.get());
}

RepetitionCount ImageDecoderCG::repetitionCount() const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyProperties(m_nativeDecoder.get(), imageSourceOptions().get()));
    if (!properties)
        return RepetitionCountOnce;
    
    CFDictionaryRef gifProperties = (CFDictionaryRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyGIFDictionary);
    if (gifProperties) {
        CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(gifProperties, kCGImagePropertyGIFLoopCount);
        
        // No property means loop once.
        if (!num)
            return RepetitionCountOnce;
        
        RepetitionCount loopCount;
        CFNumberGetValue(num, kCFNumberIntType, &loopCount);
        
        // A property with value 0 means loop forever.
        // For loopCount > 0, the specs is not clear about it. But it looks the meaning
        // is: play once + loop loopCount which is equivalent to play loopCount + 1.
        return loopCount ? loopCount + 1 : RepetitionCountInfinite;
    }
    
    CFDictionaryRef pngProperties = (CFDictionaryRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyPNGDictionary);
    if (pngProperties) {
        CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(pngProperties, WebCoreCGImagePropertyAPNGLoopCount);
        if (!num)
            return RepetitionCountOnce;
        
        RepetitionCount loopCount;
        CFNumberGetValue(num, kCFNumberIntType, &loopCount);
        return loopCount ? loopCount : RepetitionCountInfinite;
    }
    
    // Turns out we're not an animated image after all, so we don't animate.
    return RepetitionCountNone;
}

std::optional<IntPoint> ImageDecoderCG::hotSpot() const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), 0, imageSourceOptions().get()));
    if (!properties)
        return std::nullopt;
    
    int x = -1, y = -1;
    CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(properties.get(), CFSTR("hotspotX"));
    if (!num || !CFNumberGetValue(num, kCFNumberIntType, &x))
        return std::nullopt;
    
    num = (CFNumberRef)CFDictionaryGetValue(properties.get(), CFSTR("hotspotY"));
    if (!num || !CFNumberGetValue(num, kCFNumberIntType, &y))
        return std::nullopt;
    
    if (x < 0 || y < 0)
        return std::nullopt;
    
    return IntPoint(x, y);
}

IntSize ImageDecoderCG::frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), index, imageSourceOptions(subsamplingLevel).get()));
    
    if (!properties)
        return { };
    
    int width = 0;
    int height = 0;
    CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyPixelWidth);
    if (num)
        CFNumberGetValue(num, kCFNumberIntType, &width);
    
    num = (CFNumberRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyPixelHeight);
    if (num)
        CFNumberGetValue(num, kCFNumberIntType, &height);
    
    return IntSize(width, height);
}

bool ImageDecoderCG::frameIsCompleteAtIndex(size_t index) const
{
    ASSERT(frameCount());
    // CGImageSourceGetStatusAtIndex() changes the return status value from kCGImageStatusIncomplete
    // to kCGImageStatusComplete only if (index > 1 && index < frameCount() - 1). To get an accurate
    // result for the last frame (or the single frame of the static image) use CGImageSourceGetStatus()
    // instead for this frame.
    if (index == frameCount() - 1)
        return CGImageSourceGetStatus(m_nativeDecoder.get()) == kCGImageStatusComplete;
    return CGImageSourceGetStatusAtIndex(m_nativeDecoder.get(), index) == kCGImageStatusComplete;
}

ImageOrientation ImageDecoderCG::frameOrientationAtIndex(size_t index) const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), index, imageSourceOptions().get()));
    if (!properties)
        return ImageOrientation();
    
    return orientationFromProperties(properties.get());
}

Seconds ImageDecoderCG::frameDurationAtIndex(size_t index) const
{
    float value = 0;
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), index, imageSourceOptions().get()));
    if (properties) {
        CFDictionaryRef gifProperties = (CFDictionaryRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyGIFDictionary);
        if (gifProperties) {
            if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(gifProperties, kCGImagePropertyGIFUnclampedDelayTime)) {
                // Use the unclamped frame delay if it exists.
                CFNumberGetValue(num, kCFNumberFloatType, &value);
            } else if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(gifProperties, kCGImagePropertyGIFDelayTime)) {
                // Fall back to the clamped frame delay if the unclamped frame delay does not exist.
                CFNumberGetValue(num, kCFNumberFloatType, &value);
            }
        }
        
        CFDictionaryRef pngProperties = (CFDictionaryRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyPNGDictionary);
        if (pngProperties) {
            if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(pngProperties, WebCoreCGImagePropertyAPNGUnclampedDelayTime))
                CFNumberGetValue(num, kCFNumberFloatType, &value);
            else if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(pngProperties, WebCoreCGImagePropertyAPNGDelayTime))
                CFNumberGetValue(num, kCFNumberFloatType, &value);
        }
    }

    Seconds duration(value);

    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See <rdar://problem/7689300> and <http://webkit.org/b/36082>
    // for more information.
    if (duration < 11_ms)
        return 100_ms;
    return duration;
}

bool ImageDecoderCG::frameAllowSubsamplingAtIndex(size_t) const
{
    return true;
}

bool ImageDecoderCG::frameHasAlphaAtIndex(size_t index) const
{
    if (!frameIsCompleteAtIndex(index))
        return true;
    
    String uti = this->uti();
    
    // Return false if there is no image type or the image type is JPEG, because
    // JPEG does not support alpha transparency.
    if (uti.isEmpty() || uti == "public.jpeg")
        return false;
    
    // FIXME: Could return false for other non-transparent image formats.
    // FIXME: Could maybe return false for a GIF Frame if we have enough info in the GIF properties dictionary
    // to determine whether or not a transparent color was defined.
    return true;
}

unsigned ImageDecoderCG::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    IntSize frameSize = frameSizeAtIndex(index, subsamplingLevel);
    return (frameSize.area() * 4).unsafeGet();
}

NativeImagePtr ImageDecoderCG::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& decodingOptions)
{
    LOG(Images, "ImageDecoder %p createFrameImageAtIndex %lu", this, index);
    RetainPtr<CFDictionaryRef> options;
    RetainPtr<CGImageRef> image;

    if (!decodingOptions.isSynchronous()) {
        // Don't consider the subsamplingLevel when comparing the image native size with sizeForDrawing.
        IntSize size = frameSizeAtIndex(index, SubsamplingLevel::Default);
        
        if (decodingOptions.hasSizeForDrawing()) {
            // See which size is smaller: the image native size or the sizeForDrawing.
            std::optional<IntSize> sizeForDrawing = decodingOptions.sizeForDrawing();
            if (sizeForDrawing.value().unclampedArea() < size.unclampedArea())
                size = sizeForDrawing.value();
        }
        
        options = imageSourceAsyncOptions(subsamplingLevel, size);
        image = adoptCF(CGImageSourceCreateThumbnailAtIndex(m_nativeDecoder.get(), index, options.get()));
    } else {
        // Decode an image synchronously for its native size.
        options = imageSourceOptions(subsamplingLevel);
        image = adoptCF(CGImageSourceCreateImageAtIndex(m_nativeDecoder.get(), index, options.get()));
    }
    
#if PLATFORM(IOS_FAMILY)
    // <rdar://problem/7371198> - CoreGraphics changed the default caching behaviour in iOS 4.0 to kCGImageCachingTransient
    // which caused a performance regression for us since the images had to be resampled/recreated every time we called
    // CGContextDrawImage. We now tell CG to cache the drawn images. See also <rdar://problem/14366755> -
    // CoreGraphics needs to un-deprecate kCGImageCachingTemporary since it's still not the default.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CGImageSetCachingFlags(image.get(), kCGImageCachingTemporary);
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif // PLATFORM(IOS_FAMILY)
    
    String uti = this->uti();
    if (uti.isEmpty() || uti != "public.xbitmap-image")
        return image;
    
    // If it is an xbm image, mask out all the white areas to render them transparent.
    const CGFloat maskingColors[6] = {255, 255,  255, 255, 255, 255};
    RetainPtr<CGImageRef> maskedImage = adoptCF(CGImageCreateWithMaskingColors(image.get(), maskingColors));
    return maskedImage ? maskedImage : image;
}

void ImageDecoderCG::setData(SharedBuffer& data, bool allDataReceived)
{
    m_isAllDataReceived = allDataReceived;

#if PLATFORM(COCOA)
    // On Mac the NSData inside the SharedBuffer can be secretly appended to without the SharedBuffer's knowledge.
    // We use SharedBuffer's ability to wrap itself inside CFData to get around this, ensuring that ImageIO is
    // really looking at the SharedBuffer.
    CGImageSourceUpdateData(m_nativeDecoder.get(), data.createCFData().get(), allDataReceived);
#else
    // Create a CGDataProvider to wrap the SharedBuffer.
    data.ref();
    // We use the GetBytesAtPosition callback rather than the GetBytePointer one because SharedBuffer
    // does not provide a way to lock down the byte pointer and guarantee that it won't move, which
    // is a requirement for using the GetBytePointer callback.
    CGDataProviderDirectCallbacks providerCallbacks = { 0, 0, 0, sharedBufferGetBytesAtPosition, sharedBufferRelease };
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateDirect(&data, data.size(), &providerCallbacks));
    CGImageSourceUpdateDataProvider(m_nativeDecoder.get(), dataProvider.get(), allDataReceived);
#endif
}

bool ImageDecoderCG::canDecodeType(const String& mimeType)
{
    return MIMETypeRegistry::isSupportedImageMIMEType(mimeType);
}

}

#endif // USE(CG)
