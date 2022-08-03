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
#include "ImageResolution.h"
#include "ImageSourceCG.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "ProcessCapabilities.h"
#include "SharedBuffer.h"
#include "UTIRegistry.h"
#include <pal/spi/cg/ImageIOSPI.h>
#include <ImageIO/ImageIO.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>

#include "MediaAccessibilitySoftLink.h"

namespace WebCore {

const CFStringRef WebCoreCGImagePropertyAVISDictionary = CFSTR("{AVIS}");
const CFStringRef WebCoreCGImagePropertyHEICSDictionary = CFSTR("{HEICS}");
const CFStringRef WebCoreCGImagePropertyFrameInfoArray = CFSTR("FrameInfo");

const CFStringRef WebCoreCGImagePropertyUnclampedDelayTime = CFSTR("UnclampedDelayTime");
const CFStringRef WebCoreCGImagePropertyDelayTime = CFSTR("DelayTime");
const CFStringRef WebCoreCGImagePropertyLoopCount = CFSTR("LoopCount");

#if PLATFORM(WIN)
const CFStringRef kCGImageSourceShouldPreferRGB32 = CFSTR("kCGImageSourceShouldPreferRGB32");
const CFStringRef kCGImageSourceSkipMetadata = CFSTR("kCGImageSourceSkipMetadata");
const CFStringRef kCGImageSourceSubsampleFactor = CFSTR("kCGImageSourceSubsampleFactor");
const CFStringRef kCGImageSourceShouldCacheImmediately = CFSTR("kCGImageSourceShouldCacheImmediately");
const CFStringRef kCGImageSourceUseHardwareAcceleration = CFSTR("kCGImageSourceUseHardwareAcceleration");
#endif

const CFStringRef kCGImageSourceEnableRestrictedDecoding = CFSTR("kCGImageSourceEnableRestrictedDecoding");

#if HAVE(IMAGEIO_CREATE_UNPREMULTIPLIED_PNG)
const CFStringRef kCGImageSourceCreateUnpremultipliedPNG = CFSTR("kCGImageSourceCreateUnpremultipliedPNG");
#endif

static RetainPtr<CFMutableDictionaryRef> createImageSourceOptions()
{
    RetainPtr<CFMutableDictionaryRef> options = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(options.get(), kCGImageSourceShouldCache, kCFBooleanTrue);
    CFDictionarySetValue(options.get(), kCGImageSourceShouldPreferRGB32, kCFBooleanTrue);
    CFDictionarySetValue(options.get(), kCGImageSourceSkipMetadata, kCFBooleanTrue);

    if (ProcessCapabilities::isHardwareAcceleratedDecodingDisabled())
        CFDictionarySetValue(options.get(), kCGImageSourceUseHardwareAcceleration, kCFBooleanFalse);

#if HAVE(IMAGE_RESTRICTED_DECODING) && USE(APPLE_INTERNAL_SDK)
    if (ProcessCapabilities::isHEICDecodingEnabled() || ProcessCapabilities::isAVIFDecodingEnabled())
        CFDictionarySetValue(options.get(), kCGImageSourceEnableRestrictedDecoding, kCFBooleanTrue);
#endif

#if HAVE(IMAGEIO_CREATE_UNPREMULTIPLIED_PNG)
    CFDictionarySetValue(options.get(), kCGImageSourceCreateUnpremultipliedPNG, kCFBooleanTrue);
#endif
    return options;
}

static RetainPtr<CFMutableDictionaryRef> createImageSourceMetadataOptions()
{
    auto options = createImageSourceOptions();
    CFDictionarySetValue(options.get(), kCGImageSourceSkipMetadata, kCFBooleanFalse);
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

static CFDictionaryRef animationPropertiesFromProperties(CFDictionaryRef properties)
{
    if (!properties)
        return nullptr;

    if (auto animationProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyGIFDictionary))
        return animationProperties;
#if HAVE(WEBP)
    if (auto animationProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyWebPDictionary))
        return animationProperties;
#endif
    if (auto animationProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, kCGImagePropertyPNGDictionary))
        return animationProperties;

    if (auto animationProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, WebCoreCGImagePropertyAVISDictionary))
        return animationProperties;

    return (CFDictionaryRef)CFDictionaryGetValue(properties, WebCoreCGImagePropertyHEICSDictionary);
}

static CFDictionaryRef animationPropertiesFromProperties(CFDictionaryRef properties, const CFStringRef animationDictionaryName, size_t index)
{
    if (!properties)
        return nullptr;

    // For HEIF container images, ImageIO does not create a properties dictionary for each frame.
    // Instead it maintains all frames' information in the image properties dictionary. Here is how
    // ImageIO structures the properties dictionary for HEICS image:
    //  "{HEICS}" =  {
    //      FrameInfo = ( { DelayTime = "0.1"; }, { DelayTime = "0.1"; }, ... );
    //      LoopCount = 0;
    //      ...
    //  };
    auto animationProperties = (CFDictionaryRef)CFDictionaryGetValue(properties, animationDictionaryName);
    if (!animationProperties)
        return nullptr;

    auto frameInfoArray = (CFArrayRef)CFDictionaryGetValue(animationProperties, WebCoreCGImagePropertyFrameInfoArray);
    if (!frameInfoArray)
        return nullptr;

    return (CFDictionaryRef)CFArrayGetValueAtIndex(frameInfoArray, index);
}

static ImageOrientation orientationFromProperties(CFDictionaryRef imageProperties)
{
    ASSERT(imageProperties);
    CFNumberRef orientationProperty = (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyOrientation);
    if (!orientationProperty)
        return ImageOrientation::None;
    
    int exifValue;
    CFNumberGetValue(orientationProperty, kCFNumberIntType, &exifValue);
    return ImageOrientation::fromEXIFValue(exifValue);
}

static bool mayHaveDensityCorrectedSize(CFDictionaryRef imageProperties)
{
    ASSERT(imageProperties);
    auto resolutionXProperty = (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyDPIWidth);
    auto resolutionYProperty = (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyDPIHeight);
    if (!resolutionXProperty || !resolutionYProperty)
        return false;

    float resolutionX, resolutionY;
    return CFNumberGetValue(resolutionXProperty, kCFNumberFloat32Type, &resolutionX)
        && CFNumberGetValue(resolutionYProperty, kCFNumberFloat32Type, &resolutionY)
        && (resolutionX != ImageResolution::DefaultResolution || resolutionY != ImageResolution::DefaultResolution);
}

static std::optional<IntSize> densityCorrectedSizeFromProperties(CFDictionaryRef imageProperties)
{
    ASSERT(imageProperties);
    auto exifDictionary = (CFDictionaryRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyExifDictionary);
    auto tiffDictionary = (CFDictionaryRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyTIFFDictionary);

    if (!exifDictionary || !tiffDictionary)
        return std::nullopt;

    auto widthProperty = (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyPixelWidth);
    auto heightProperty = (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyPixelHeight);
    auto preferredWidthProperty = (CFNumberRef)CFDictionaryGetValue(exifDictionary, kCGImagePropertyExifPixelXDimension);
    auto preferredHeightProperty = (CFNumberRef)CFDictionaryGetValue(exifDictionary, kCGImagePropertyExifPixelYDimension);
    auto resolutionXProperty = (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyDPIWidth);
    auto resolutionYProperty = (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyDPIHeight);
    auto resolutionUnitProperty = (CFNumberRef)CFDictionaryGetValue(tiffDictionary, kCGImagePropertyTIFFResolutionUnit);

    if (!preferredWidthProperty || !preferredHeightProperty || !resolutionXProperty || !resolutionYProperty || !resolutionUnitProperty)
        return std::nullopt;

    int resolutionUnit;
    float sourceWidth, sourceHeight, preferredWidth, preferredHeight, resolutionWidth, resolutionHeight;
    if (!CFNumberGetValue(widthProperty, kCFNumberFloat32Type, &sourceWidth)
        || !CFNumberGetValue(heightProperty, kCFNumberFloat32Type, &sourceHeight)
        || !CFNumberGetValue(preferredWidthProperty, kCFNumberFloat32Type, &preferredWidth)
        || !CFNumberGetValue(preferredHeightProperty, kCFNumberFloat32Type, &preferredHeight)
        || !CFNumberGetValue(resolutionXProperty, kCFNumberFloat32Type, &resolutionWidth)
        || !CFNumberGetValue(resolutionYProperty, kCFNumberFloat32Type, &resolutionHeight)
        || !CFNumberGetValue(resolutionUnitProperty, kCFNumberIntType, &resolutionUnit)) {
        return std::nullopt;
    }

    return ImageResolution::densityCorrectedSize(FloatSize(sourceWidth, sourceHeight), {
        { preferredWidth, preferredHeight },
        { resolutionWidth, resolutionHeight },
        static_cast<ImageResolution::ResolutionUnit>(resolutionUnit)
    });
}

#if !PLATFORM(COCOA)
size_t sharedBufferGetBytesAtPosition(void* info, void* buffer, off_t position, size_t count)
{
    SharedBuffer* sharedBuffer = static_cast<SharedBuffer*>(info);
    size_t sourceSize = sharedBuffer->size();
    if (position >= sourceSize)
        return 0;
    
    auto* source = sharedBuffer->data() + position;
    size_t amount = std::min<size_t>(count, sourceSize - position);
    memcpy(buffer, source, amount);
    return amount;
}

void sharedBufferRelease(void* info)
{
    FragmentedSharedBuffer* sharedBuffer = static_cast<FragmentedSharedBuffer*>(info);
    sharedBuffer->deref();
}
#endif

ImageDecoderCG::ImageDecoderCG(FragmentedSharedBuffer& data, AlphaOption, GammaAndColorProfileOption)
{
    RetainPtr<CFStringRef> utiHint;
    if (data.size() >= 32)
        utiHint = adoptCF(CGImageSourceGetTypeWithData(data.makeContiguous()->createCFData().get(), nullptr, nullptr));
    
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

String ImageDecoderCG::accessibilityDescription() const
{
    if (!MediaAccessibilityLibrary() || !canLoad_MediaAccessibility_MAImageCaptioningCopyCaptionWithSource())
        return { };
    
    auto description = adoptCF(MAImageCaptioningCopyCaptionWithSource(m_nativeDecoder.get(), nullptr));
    if (!description)
        return { };
    return description.get();
}

EncodedDataStatus ImageDecoderCG::encodedDataStatus() const
{
    if (m_encodedDataStatus == EncodedDataStatus::Error || m_encodedDataStatus == EncodedDataStatus::Complete)
        return m_encodedDataStatus;

    // The image source UTI can be changed while receiving more encoded data.
    String uti = this->uti();
    if (uti.isEmpty())
        return EncodedDataStatus::Unknown;

    if (!isSupportedImageType(uti)) {
        m_encodedDataStatus = EncodedDataStatus::Error;
        return m_encodedDataStatus;
    }

    switch (CGImageSourceGetStatus(m_nativeDecoder.get())) {
    case kCGImageStatusUnknownType:
        m_encodedDataStatus = EncodedDataStatus::Error;
        break;

    case kCGImageStatusUnexpectedEOF:
    case kCGImageStatusInvalidData:
    case kCGImageStatusReadingHeader:
        // Ragnaros yells: TOO SOON! You have awakened me TOO SOON, Executus!
        if (!m_isAllDataReceived)
            m_encodedDataStatus = EncodedDataStatus::Unknown;
        else
            m_encodedDataStatus = EncodedDataStatus::Error;
        break;

    case kCGImageStatusIncomplete: {
        if (m_encodedDataStatus == EncodedDataStatus::SizeAvailable)
            break;

        auto image0Properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), 0, imageSourceOptions().get()));
        if (!image0Properties || !CFDictionaryContainsKey(image0Properties.get(), kCGImagePropertyPixelWidth) || !CFDictionaryContainsKey(image0Properties.get(), kCGImagePropertyPixelHeight)) {
            m_encodedDataStatus = EncodedDataStatus::TypeAvailable;
            break;
        }

        m_encodedDataStatus = EncodedDataStatus::SizeAvailable;
        break;
    }

    case kCGImageStatusComplete:
        m_encodedDataStatus = EncodedDataStatus::Complete;
        break;
    }

    return m_encodedDataStatus; 
}

size_t ImageDecoderCG::frameCount() const
{
    return CGImageSourceGetCount(m_nativeDecoder.get());
}

RepetitionCount ImageDecoderCG::repetitionCount() const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyProperties(m_nativeDecoder.get(), imageSourceOptions().get()));
    CFDictionaryRef animationProperties = animationPropertiesFromProperties(properties.get());

    // Turns out we're not an animated image after all, so we don't animate.
    if (!animationProperties)
        return RepetitionCountNone;

    CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(animationProperties, WebCoreCGImagePropertyLoopCount);

    // No property means loop once.
    if (!num)
        return RepetitionCountOnce;

    RepetitionCount loopCount;
    CFNumberGetValue(num, kCFNumberIntType, &loopCount);

    // A property with value 0 means loop forever.
    if (!loopCount)
        return RepetitionCountInfinite;

#if HAVE(CGIMAGESOURCE_WITH_ACCURATE_LOOP_COUNT)
    return loopCount;
#else
    if (!isGIFImageType(uti()))
        return loopCount;

    // For GIF and loopCount > 0, the specs is not clear about it. But it looks the meaning
    // is: play once + loop loopCount which is equivalent to play loopCount + 1.
    return loopCount + 1;
#endif
}

std::optional<IntPoint> ImageDecoderCG::hotSpot() const
{
    auto properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), 0, imageSourceOptions().get()));
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

ImageDecoder::FrameMetadata ImageDecoderCG::frameMetadataAtIndex(size_t index) const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), index, imageSourceOptions().get()));
    if (!properties)
        return { };
    
    auto orientation = orientationFromProperties(properties.get());
    if (!mayHaveDensityCorrectedSize(properties.get()))
        return { orientation, std::nullopt };

    auto propertiesWithMetadata = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), index, createImageSourceMetadataOptions().get()));
    if (!propertiesWithMetadata)
        return { orientation, std::nullopt };
    
    return { orientation, densityCorrectedSizeFromProperties(propertiesWithMetadata.get()) };
}

Seconds ImageDecoderCG::frameDurationAtIndex(size_t index) const
{
    RetainPtr<CFDictionaryRef> properties = nullptr;
    RetainPtr<CFDictionaryRef> frameProperties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), index, imageSourceOptions().get()));
    CFDictionaryRef animationProperties = animationPropertiesFromProperties(frameProperties.get());

    if (frameProperties && !animationProperties) {
        properties = adoptCF(CGImageSourceCopyProperties(m_nativeDecoder.get(), imageSourceOptions().get()));
        animationProperties = animationPropertiesFromProperties(properties.get(), WebCoreCGImagePropertyAVISDictionary, index);
        if (!animationProperties)
            animationProperties = animationPropertiesFromProperties(properties.get(), WebCoreCGImagePropertyHEICSDictionary, index);
    }

    // Use the unclamped frame delay if it exists. Otherwise use the clamped frame delay.
    float value = 0;
    if (animationProperties) {
        if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(animationProperties, WebCoreCGImagePropertyUnclampedDelayTime))
            CFNumberGetValue(num, kCFNumberFloatType, &value);
        else if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(animationProperties, WebCoreCGImagePropertyDelayTime))
            CFNumberGetValue(num, kCFNumberFloatType, &value);
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
    if (uti.isEmpty() || uti == "public.jpeg"_s)
        return false;
    
    // FIXME: Could return false for other non-transparent image formats.
    // FIXME: Could maybe return false for a GIF Frame if we have enough info in the GIF properties dictionary
    // to determine whether or not a transparent color was defined.
    return true;
}

unsigned ImageDecoderCG::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    return frameSizeAtIndex(index, subsamplingLevel).area() * 4;
}

PlatformImagePtr ImageDecoderCG::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions& decodingOptions)
{
    LOG(Images, "ImageDecoder %p createFrameImageAtIndex %lu", this, index);
    RetainPtr<CFDictionaryRef> options;
    RetainPtr<CGImageRef> image;

    auto size = frameSizeAtIndex(index, SubsamplingLevel::Default);

    if (!decodingOptions.isSynchronous()) {
        // Don't consider the subsamplingLevel when comparing the image native size with sizeForDrawing.
        
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
    if (uti.isEmpty() || uti != "public.xbitmap-image"_s)
        return image;
    
    // If it is an xbm image, mask out all the white areas to render them transparent.
    const CGFloat maskingColors[6] = {255, 255,  255, 255, 255, 255};
    RetainPtr<CGImageRef> maskedImage = adoptCF(CGImageCreateWithMaskingColors(image.get(), maskingColors));
    return maskedImage ? maskedImage : image;
}

void ImageDecoderCG::setData(const FragmentedSharedBuffer& data, bool allDataReceived)
{
    m_isAllDataReceived = allDataReceived;

#if PLATFORM(COCOA)
    // On Mac the NSData inside the FragmentedSharedBuffer can be secretly appended to without the FragmentedSharedBuffer's knowledge.
    // We use FragmentedSharedBuffer's ability to wrap itself inside CFData to get around this, ensuring that ImageIO is
    // really looking at the FragmentedSharedBuffer.
    CGImageSourceUpdateData(m_nativeDecoder.get(), data.makeContiguous()->createCFData().get(), allDataReceived);
#else
    // Create a CGDataProvider to wrap the FragmentedSharedBuffer.
    auto contiguousData = data.makeContiguous();
    contiguousData.get().ref();
    // We use the GetBytesAtPosition callback rather than the GetBytePointer one because FragmentedSharedBuffer
    // does not provide a way to lock down the byte pointer and guarantee that it won't move, which
    // is a requirement for using the GetBytePointer callback.
    CGDataProviderDirectCallbacks providerCallbacks = { 0, 0, 0, sharedBufferGetBytesAtPosition, sharedBufferRelease };
    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateDirect(contiguousData.ptr(), data.size(), &providerCallbacks));
    CGImageSourceUpdateDataProvider(m_nativeDecoder.get(), dataProvider.get(), allDataReceived);
#endif
}

bool ImageDecoderCG::canDecodeType(const String& mimeType)
{
    return MIMETypeRegistry::isSupportedImageMIMEType(mimeType);
}

}

#endif // USE(CG)
