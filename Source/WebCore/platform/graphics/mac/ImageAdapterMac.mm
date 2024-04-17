/*
 * Copyright (C) 2004-2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ImageAdapter.h"

#import "BitmapImage.h"
#import "FloatRect.h"
#import "GraphicsContext.h"
#import "SharedBuffer.h"
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import "UIFoundationSoftLink.h"
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import <MobileCoreServices/MobileCoreServices.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebMultiRepresentationHEICAttachmentAdditions.h>
#endif

@interface WebCoreBundleFinder : NSObject
@end

@implementation WebCoreBundleFinder
@end

namespace WebCore {

Ref<Image> ImageAdapter::loadPlatformResource(const char *name)
{
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreBundleFinder class]];
    NSString *imagePath = [bundle pathForResource:[NSString stringWithUTF8String:name] ofType:@"png"];
    NSData *namedImageData = [NSData dataWithContentsOfFile:imagePath];
    if (namedImageData) {
        auto image = BitmapImage::create();
        image->setData(SharedBuffer::create(namedImageData), true);
        return WTFMove(image);
    }

    // We have reports indicating resource loads are failing, but we don't yet know the root cause(s).
    // Two theories are bad installs (image files are missing), and too-many-open-files.
    // See rdar://5607381
    ASSERT_NOT_REACHED();
    return Image::nullImage();
}

RetainPtr<CFDataRef> ImageAdapter::tiffRepresentation(const Vector<Ref<NativeImage>>& nativeImages)
{
    // If nativeImages.size() is zero, we know for certain this image doesn't have valid data
    // Even though the call to CGImageDestinationCreateWithData will fail and we'll handle it gracefully,
    // in certain circumstances that call will spam the console with an error message
    if (!nativeImages.size())
        return nullptr;

    RetainPtr<CFMutableDataRef> data = adoptCF(CFDataCreateMutable(0, 0));

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<CGImageDestinationRef> destination = adoptCF(CGImageDestinationCreateWithData(data.get(), kUTTypeTIFF, nativeImages.size(), 0));
ALLOW_DEPRECATED_DECLARATIONS_END

    if (!destination)
        return nullptr;

    for (const auto& nativeImage : nativeImages)
        CGImageDestinationAddImage(destination.get(), nativeImage->platformImage().get(), 0);

    CGImageDestinationFinalize(destination.get());
    return data;
}

#if ENABLE(MULTI_REPRESENTATION_HEIC)
WebMultiRepresentationHEICAttachment *ImageAdapter::multiRepresentationHEIC()
{
    if (m_multiRepHEIC)
        return m_multiRepHEIC.get();

    auto buffer = image().data();
    if (!buffer)
        return nullptr;

    Vector<uint8_t> data = buffer->copyData();

    RetainPtr nsData = adoptNS([[NSData alloc] initWithBytes:data.data() length:data.size()]);
    m_multiRepHEIC = adoptNS([[PlatformWebMultiRepresentationHEICAttachment alloc] initWithImageContent:nsData.get()]);

    return m_multiRepHEIC.get();
}
#endif

void ImageAdapter::invalidate()
{
#if USE(APPKIT)
    m_nsImage = nullptr;
#endif
    m_tiffRep = nullptr;
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    m_multiRepHEIC = nullptr;
#endif
}

CFDataRef ImageAdapter::tiffRepresentation()
{
    if (m_tiffRep)
        return m_tiffRep.get();

    auto data = tiffRepresentation(allNativeImages());
    if (!data)
        return nullptr;

    m_tiffRep = data;
    return m_tiffRep.get();
}

#if USE(APPKIT)
NSImage* ImageAdapter::nsImage()
{
    if (m_nsImage)
        return m_nsImage.get();

    CFDataRef data = tiffRepresentation();
    if (!data)
        return nullptr;

    m_nsImage = adoptNS([[NSImage alloc] initWithData:(__bridge NSData *)data]);
    return m_nsImage.get();
}

RetainPtr<NSImage> ImageAdapter::snapshotNSImage()
{
    RefPtr nativeImage =  image().currentNativeImage();
    if (!nativeImage)
        return nullptr;

    auto data = tiffRepresentation({ nativeImage.releaseNonNull() });
    if (!data)
        return nullptr;

    return adoptNS([[NSImage alloc] initWithData:(__bridge NSData *)data.get()]);
}
#endif

} // namespace WebCore
