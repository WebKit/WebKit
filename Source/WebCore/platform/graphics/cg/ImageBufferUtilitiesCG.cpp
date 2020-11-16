/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageBufferUtilitiesCG.h"

#if USE(CG)

#include "GraphicsContextCG.h"
#include "ImageData.h"
#include "MIMETypeRegistry.h"
#include <ImageIO/ImageIO.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/Base64.h>

#if PLATFORM(COCOA)
#include "UTIUtilities.h"
#endif

namespace WebCore {

uint8_t verifyImageBufferIsBigEnough(const void* buffer, size_t bufferSize)
{
    RELEASE_ASSERT(bufferSize);

    uintptr_t lastByte;
    bool isSafe = WTF::safeAdd((uintptr_t)buffer, bufferSize - 1, lastByte);
    RELEASE_ASSERT(isSafe);

    return *(uint8_t*)lastByte;
}

CFStringRef jpegUTI()
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#if PLATFORM(IOS_FAMILY) || PLATFORM(WIN)
    static const CFStringRef kUTTypeJPEG = CFSTR("public.jpeg");
#endif
    return kUTTypeJPEG;
ALLOW_DEPRECATED_DECLARATIONS_END
}

RetainPtr<CFStringRef> utiFromImageBufferMIMEType(const String& mimeType)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: Why doesn't iOS use the CoreServices version?
#if PLATFORM(MAC)
    return UTIFromMIMEType(mimeType).createCFString();
#else
    ASSERT(isMainThread()); // It is unclear if CFSTR is threadsafe.

    // FIXME: Add Windows support for all the supported UTIs when a way to convert from MIMEType to UTI reliably is found.
    // For now, only support PNG, JPEG, and GIF. See <rdar://problem/6095286>.
    static const CFStringRef kUTTypePNG = CFSTR("public.png");
    static const CFStringRef kUTTypeGIF = CFSTR("com.compuserve.gif");

    if (equalLettersIgnoringASCIICase(mimeType, "image/png"))
        return kUTTypePNG;
    if (equalLettersIgnoringASCIICase(mimeType, "image/jpeg"))
        return jpegUTI();
    if (equalLettersIgnoringASCIICase(mimeType, "image/gif"))
        return kUTTypeGIF;

    ASSERT_NOT_REACHED();
    return kUTTypePNG;
#endif
ALLOW_DEPRECATED_DECLARATIONS_END
}

bool encodeImage(CGImageRef image, CFStringRef uti, Optional<double> quality, CFMutableDataRef data)
{
    if (!image || !uti || !data)
        return false;

    auto destination = adoptCF(CGImageDestinationCreateWithData(data, uti, 1, 0));
    if (!destination)
        return false;

    RetainPtr<CFDictionaryRef> imageProperties;
    if (CFEqual(uti, jpegUTI()) && quality && *quality >= 0.0 && *quality <= 1.0) {
        // Apply the compression quality to the JPEG image destination.
        auto compressionQuality = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &*quality));
        const void* key = kCGImageDestinationLossyCompressionQuality;
        const void* value = compressionQuality.get();
        imageProperties = adoptCF(CFDictionaryCreate(0, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }

    // Setting kCGImageDestinationBackgroundColor to black for JPEG images in imageProperties would save some math
    // in the calling functions, but it doesn't seem to work.

    CGImageDestinationAddImage(destination.get(), image, imageProperties.get());
    return CGImageDestinationFinalize(destination.get());
}

static RetainPtr<CFDataRef> cfData(const ImageData& source, const String& mimeType, Optional<double> quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    auto uti = utiFromImageBufferMIMEType(mimeType);
    ASSERT(uti);

    CGImageAlphaInfo dataAlphaInfo = kCGImageAlphaLast;
    unsigned char* data = source.data()->data();
    Vector<uint8_t> premultipliedData;

    if (CFEqual(uti.get(), jpegUTI())) {
        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        size_t size = 4 * source.width() * source.height();
        if (!premultipliedData.tryReserveCapacity(size))
            return nullptr;

        premultipliedData.grow(size);
        unsigned char* buffer = premultipliedData.data();
        for (size_t i = 0; i < size; i += 4) {
            unsigned alpha = data[i + 3];
            if (alpha != 255) {
                buffer[i + 0] = data[i + 0] * alpha / 255;
                buffer[i + 1] = data[i + 1] * alpha / 255;
                buffer[i + 2] = data[i + 2] * alpha / 255;
            } else {
                buffer[i + 0] = data[i + 0];
                buffer[i + 1] = data[i + 1];
                buffer[i + 2] = data[i + 2];
            }
        }

        dataAlphaInfo = kCGImageAlphaNoneSkipLast; // Ignore the alpha channel.
        data = premultipliedData.data();
    }

    verifyImageBufferIsBigEnough(data, 4 * source.width() * source.height());
    auto dataProvider = adoptCF(CGDataProviderCreateWithData(0, data, 4 * source.width() * source.height(), 0));
    if (!dataProvider)
        return nullptr;

    auto image = adoptCF(CGImageCreate(source.width(), source.height(), 8, 32, 4 * source.width(), sRGBColorSpaceRef(), kCGBitmapByteOrderDefault | dataAlphaInfo, dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    auto cfData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!encodeImage(image.get(), uti.get(), quality, cfData.get()))
        return nullptr;

    return WTFMove(cfData);
}

String dataURL(CFDataRef data, const String& mimeType)
{
    Vector<char> base64Data;
    base64Encode(CFDataGetBytePtr(data), CFDataGetLength(data), base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

String dataURL(const ImageData& source, const String& mimeType, Optional<double> quality)
{
    if (auto data = cfData(source, mimeType, quality))
        return dataURL(data.get(), mimeType);
    return "data:,"_s;
}

Vector<uint8_t> dataVector(CFDataRef cfData)
{
    Vector<uint8_t> data;
    data.append(CFDataGetBytePtr(cfData), CFDataGetLength(cfData));
    return data;
}

Vector<uint8_t> data(const ImageData& source, const String& mimeType, Optional<double> quality)
{
    if (auto data = cfData(source, mimeType, quality))
        return dataVector(data.get());
    return { };
}

} // namespace WebCore

#endif // USE(CG)
