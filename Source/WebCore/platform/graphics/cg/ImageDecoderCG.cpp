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
#include "IntPoint.h"
#include "IntSize.h"
#include "SharedBuffer.h"
#include <wtf/NeverDestroyed.h>

#if !PLATFORM(IOS)
#include <ApplicationServices/ApplicationServices.h>
#else
#include "CoreGraphicsSPI.h"
#include <ImageIO/ImageIO.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#import <ImageIO/CGImageSourcePrivate.h>
#else
const CFStringRef kCGImageSourceSubsampleFactor = CFSTR("kCGImageSourceSubsampleFactor");
#endif

namespace WebCore {

const CFStringRef WebCoreCGImagePropertyAPNGUnclampedDelayTime = CFSTR("UnclampedDelayTime");
const CFStringRef WebCoreCGImagePropertyAPNGDelayTime = CFSTR("DelayTime");
const CFStringRef WebCoreCGImagePropertyAPNGLoopCount = CFSTR("LoopCount");

const CFStringRef kCGImageSourceShouldPreferRGB32 = CFSTR("kCGImageSourceShouldPreferRGB32");
const CFStringRef kCGImageSourceSkipMetadata = CFSTR("kCGImageSourceSkipMetadata");

static RetainPtr<CFDictionaryRef> createImageSourceOptions(SubsamplingLevel subsamplingLevel)
{
    if (!subsamplingLevel) {
        const unsigned numOptions = 3;
        const void* keys[numOptions] = { kCGImageSourceShouldCache, kCGImageSourceShouldPreferRGB32, kCGImageSourceSkipMetadata };
        const void* values[numOptions] = { kCFBooleanTrue, kCFBooleanTrue, kCFBooleanTrue };
        return CFDictionaryCreate(nullptr, keys, values, numOptions, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    
    short constrainedSubsamplingLevel = std::min<short>(3, std::max<short>(0, subsamplingLevel));
    int subsampleInt = 1 << constrainedSubsamplingLevel; // [0..3] => [1, 2, 4, 8]
    
    RetainPtr<CFNumberRef> subsampleNumber = adoptCF(CFNumberCreate(nullptr,  kCFNumberIntType,  &subsampleInt));
    const CFIndex numOptions = 4;
    const void* keys[numOptions] = { kCGImageSourceShouldCache, kCGImageSourceShouldPreferRGB32, kCGImageSourceSkipMetadata, kCGImageSourceSubsampleFactor };
    const void* values[numOptions] = { kCFBooleanTrue, kCFBooleanTrue, kCFBooleanTrue, subsampleNumber.get() };
    return adoptCF(CFDictionaryCreate(nullptr, keys, values, numOptions, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

static RetainPtr<CFDictionaryRef> imageSourceOptions(SubsamplingLevel subsamplingLevel = 0)
{
    if (subsamplingLevel)
        return createImageSourceOptions(subsamplingLevel);
    
    static NeverDestroyed<RetainPtr<CFDictionaryRef>> options = createImageSourceOptions(0);
    return options;
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

ImageDecoder::ImageDecoder()
{
    m_nativeDecoder = adoptCF(CGImageSourceCreateIncremental(nullptr));
}

size_t ImageDecoder::bytesDecodedToDetermineProperties()
{
    // Measured by tracing malloc/calloc calls on Mac OS 10.6.6, x86_64.
    // A non-zero value ensures cached images with no decoded frames still enter
    // the live decoded resources list when the CGImageSource decodes image
    // properties, allowing the cache to prune the partially decoded image.
    // This value is likely to be inaccurate on other platforms, but the overall
    // behavior is unchanged.
    return 13088;
}

String ImageDecoder::filenameExtension() const
{
    CFStringRef imageSourceType = CGImageSourceGetType(m_nativeDecoder.get());
    return WebCore::preferredExtensionForImageSourceType(imageSourceType);
}

bool ImageDecoder::isSizeAvailable() const
{
    // Ragnaros yells: TOO SOON! You have awakened me TOO SOON, Executus!
    if (CGImageSourceGetStatus(m_nativeDecoder.get()) < kCGImageStatusIncomplete)
        return false;
    
    RetainPtr<CFDictionaryRef> image0Properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), 0, imageSourceOptions().get()));
    if (!image0Properties)
        return false;
    
    return CFDictionaryContainsKey(image0Properties.get(), kCGImagePropertyPixelWidth)
    && CFDictionaryContainsKey(image0Properties.get(), kCGImagePropertyPixelHeight);
}

IntSize ImageDecoder::size() const
{
    if (m_size.isEmpty())
        m_size = frameSizeAtIndex(0, 0);
    return m_size;
}

size_t ImageDecoder::frameCount() const
{
    return CGImageSourceGetCount(m_nativeDecoder.get());
}

int ImageDecoder::repetitionCount() const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyProperties(m_nativeDecoder.get(), imageSourceOptions().get()));
    if (!properties)
        return cAnimationLoopOnce;
    
    CFDictionaryRef gifProperties = (CFDictionaryRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyGIFDictionary);
    if (gifProperties) {
        CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(gifProperties, kCGImagePropertyGIFLoopCount);
        
        // No property means loop once.
        if (!num)
            return cAnimationLoopOnce;
        
        int loopCount;
        CFNumberGetValue(num, kCFNumberIntType, &loopCount);
        
        // A property with value 0 means loop forever.
        return loopCount ? loopCount : cAnimationLoopInfinite;
    }
    
    CFDictionaryRef pngProperties = (CFDictionaryRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyPNGDictionary);
    if (pngProperties) {
        CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(pngProperties, WebCoreCGImagePropertyAPNGLoopCount);
        if (!num)
            return cAnimationLoopOnce;
        
        int loopCount;
        CFNumberGetValue(num, kCFNumberIntType, &loopCount);
        return loopCount ? loopCount : cAnimationLoopInfinite;
    }
    
    // Turns out we're not an animated image after all, so we don't animate.
    return cAnimationNone;
}

Optional<IntPoint> ImageDecoder::hotSpot() const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), 0, imageSourceOptions().get()));
    if (!properties)
        return Nullopt;
    
    int x = -1, y = -1;
    CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(properties.get(), CFSTR("hotspotX"));
    if (!num || !CFNumberGetValue(num, kCFNumberIntType, &x))
        return Nullopt;
    
    num = (CFNumberRef)CFDictionaryGetValue(properties.get(), CFSTR("hotspotY"));
    if (!num || !CFNumberGetValue(num, kCFNumberIntType, &y))
        return Nullopt;
    
    if (x < 0 || y < 0)
        return Nullopt;
    
    return IntPoint(x, y);
}

IntSize ImageDecoder::frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
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

bool ImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    ASSERT(frameCount());
    return CGImageSourceGetStatusAtIndex(m_nativeDecoder.get(), index) == kCGImageStatusComplete;
}

ImageOrientation ImageDecoder::orientationAtIndex(size_t index) const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), index, imageSourceOptions().get()));
    if (!properties)
        return ImageOrientation();
    
    return orientationFromProperties(properties.get());
}

float ImageDecoder::frameDurationAtIndex(size_t index) const
{
    float duration = 0;
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), index, imageSourceOptions().get()));
    if (properties) {
        CFDictionaryRef gifProperties = (CFDictionaryRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyGIFDictionary);
        if (gifProperties) {
            if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(gifProperties, kCGImagePropertyGIFUnclampedDelayTime)) {
                // Use the unclamped frame delay if it exists.
                CFNumberGetValue(num, kCFNumberFloatType, &duration);
            } else if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(gifProperties, kCGImagePropertyGIFDelayTime)) {
                // Fall back to the clamped frame delay if the unclamped frame delay does not exist.
                CFNumberGetValue(num, kCFNumberFloatType, &duration);
            }
        }
        
        CFDictionaryRef pngProperties = (CFDictionaryRef)CFDictionaryGetValue(properties.get(), kCGImagePropertyPNGDictionary);
        if (pngProperties) {
            if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(pngProperties, WebCoreCGImagePropertyAPNGUnclampedDelayTime))
                CFNumberGetValue(num, kCFNumberFloatType, &duration);
            else if (CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(pngProperties, WebCoreCGImagePropertyAPNGDelayTime))
                CFNumberGetValue(num, kCFNumberFloatType, &duration);
        }
    }
    
    // Many annoying ads specify a 0 duration to make an image flash as quickly as possible.
    // We follow Firefox's behavior and use a duration of 100 ms for any frames that specify
    // a duration of <= 10 ms. See <rdar://problem/7689300> and <http://webkit.org/b/36082>
    // for more information.
    if (duration < 0.011f)
        return 0.1f;
    return duration;
}

bool ImageDecoder::allowSubsamplingOfFrameAtIndex(size_t) const
{
    RetainPtr<CFDictionaryRef> properties = adoptCF(CGImageSourceCopyPropertiesAtIndex(m_nativeDecoder.get(), 0, imageSourceOptions().get()));
    if (!properties)
        return false;
    
    CFDictionaryRef jfifProperties = static_cast<CFDictionaryRef>(CFDictionaryGetValue(properties.get(), kCGImagePropertyJFIFDictionary));
    if (jfifProperties) {
        CFBooleanRef isProgCFBool = static_cast<CFBooleanRef>(CFDictionaryGetValue(jfifProperties, kCGImagePropertyJFIFIsProgressive));
        if (isProgCFBool) {
            bool isProgressive = CFBooleanGetValue(isProgCFBool);
            // Workaround for <rdar://problem/5184655> - Hang rendering very large progressive JPEG. Decoding progressive
            // images hangs for a very long time right now. Until this is fixed, don't sub-sample progressive images. This
            // will cause them to fail our large image check and they won't be decoded.
            // FIXME: Remove once underlying issue is fixed (<rdar://problem/5191418>)
            return !isProgressive;
        }
    }
    
    return true;
}

bool ImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    if (!frameIsCompleteAtIndex(index))
        return true;
    
    CFStringRef imageType = CGImageSourceGetType(m_nativeDecoder.get());
    
    // Return false if there is no image type or the image type is JPEG, because
    // JPEG does not support alpha transparency.
    if (!imageType || CFEqual(imageType, CFSTR("public.jpeg")))
        return false;
    
    // FIXME: Could return false for other non-transparent image formats.
    // FIXME: Could maybe return false for a GIF Frame if we have enough info in the GIF properties dictionary
    // to determine whether or not a transparent color was defined.
    return true;
}

unsigned ImageDecoder::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    IntSize frameSize = frameSizeAtIndex(index, subsamplingLevel);
    return frameSize.area() * 4;
}

NativeImagePtr ImageDecoder::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    RetainPtr<CGImageRef> image = adoptCF(CGImageSourceCreateImageAtIndex(m_nativeDecoder.get(), index, imageSourceOptions(subsamplingLevel).get()));
    
#if PLATFORM(IOS)
    // <rdar://problem/7371198> - CoreGraphics changed the default caching behaviour in iOS 4.0 to kCGImageCachingTransient
    // which caused a performance regression for us since the images had to be resampled/recreated every time we called
    // CGContextDrawImage. We now tell CG to cache the drawn images. See also <rdar://problem/14366755> -
    // CoreGraphics needs to un-deprecate kCGImageCachingTemporary since it's still not the default.
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    CGImageSetCachingFlags(image.get(), kCGImageCachingTemporary);
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif
#endif // PLATFORM(IOS)
    
    CFStringRef imageUTI = CGImageSourceGetType(m_nativeDecoder.get());
    static const CFStringRef xbmUTI = CFSTR("public.xbitmap-image");
    
    if (!imageUTI)
        return image;
    
    if (!CFEqual(imageUTI, xbmUTI))
        return image;
    
    // If it is an xbm image, mask out all the white areas to render them transparent.
    const CGFloat maskingColors[6] = {255, 255,  255, 255, 255, 255};
    RetainPtr<CGImageRef> maskedImage = adoptCF(CGImageCreateWithMaskingColors(image.get(), maskingColors));
    return maskedImage ? maskedImage : image;
}

void ImageDecoder::setData(CFDataRef data, bool allDataReceived)
{
    CGImageSourceUpdateData(m_nativeDecoder.get(), data, allDataReceived);
}

void ImageDecoder::setData(SharedBuffer& data, bool allDataReceived)
{
#if PLATFORM(COCOA)
    // On Mac the NSData inside the SharedBuffer can be secretly appended to without the SharedBuffer's knowledge.
    // We use SharedBuffer's ability to wrap itself inside CFData to get around this, ensuring that ImageIO is
    // really looking at the SharedBuffer.
    setData(data.createCFData().get(), allDataReceived);
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

}

#endif // USE(CG)
