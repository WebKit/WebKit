/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include "MIMETypeRegistry.h"
#include "PixelBuffer.h"
#include <ImageIO/ImageIO.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/ScopedLambda.h>
#include <wtf/text/Base64.h>

#if PLATFORM(COCOA)
#include "UTIUtilities.h"
#endif

namespace WebCore {

using PutBytesCallback = size_t(const void*, size_t);

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
    // FIXME: Add Windows support for all the supported UTIs when a way to convert from MIMEType to UTI reliably is found.
    // For now, only support PNG, JPEG, and GIF. See <rdar://problem/6095286>.
    static CFStringRef kUTTypePNG;
    static CFStringRef kUTTypeGIF;

    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        kUTTypePNG = CFSTR("public.png");
        kUTTypeGIF = CFSTR("com.compuserve.gif");
    });

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

static bool encode(CGImageRef image, CFStringRef destinationUTI, std::optional<double> quality, const ScopedLambda<PutBytesCallback>& function)
{
    if (!image || !destinationUTI)
        return false;

    CGDataConsumerCallbacks callbacks {
        [](void* context, const void* buffer, size_t count) -> size_t {
            auto functor = *static_cast<const ScopedLambda<PutBytesCallback>*>(context);
            return functor(buffer, count);
        },
        nullptr
    };

    auto consumer = adoptCF(CGDataConsumerCreate(const_cast<ScopedLambda<PutBytesCallback>*>(&function), &callbacks));
    auto destination = adoptCF(CGImageDestinationCreateWithDataConsumer(consumer.get(), destinationUTI, 1, nullptr));
    
    auto imageProperties = [&] () -> RetainPtr<CFDictionaryRef> {
        if (CFEqual(destinationUTI, jpegUTI()) && quality && *quality >= 0.0 && *quality <= 1.0) {
            // Apply the compression quality to the JPEG image destination.
            auto compressionQuality = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &*quality));
            const void* key = kCGImageDestinationLossyCompressionQuality;
            const void* value = compressionQuality.get();
            return adoptCF(CFDictionaryCreate(0, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        }
        return nullptr;
    }();

    // FIXME: Setting kCGImageDestinationBackgroundColor to black for JPEG images in imageProperties would save some math
    // in the calling functions, but it doesn't seem to work.

    CGImageDestinationAddImage(destination.get(), image, imageProperties.get());

    return CGImageDestinationFinalize(destination.get());
}

static bool encode(const PixelBuffer& source, const String& mimeType, std::optional<double> quality, const ScopedLambda<PutBytesCallback>& function)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);

    CGImageAlphaInfo dataAlphaInfo = kCGImageAlphaLast;
    
    auto data = source.data().data();
    auto dataSize = source.data().byteLength();

    Vector<uint8_t> premultipliedData;

    if (CFEqual(destinationUTI.get(), jpegUTI())) {
        // FIXME: Use PixelBufferConversion for this once it supports RGBX.

        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        if (!premultipliedData.tryReserveCapacity(dataSize))
            return false;

        premultipliedData.grow(dataSize);
        unsigned char* buffer = premultipliedData.data();
        for (size_t i = 0; i < dataSize; i += 4) {
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

    verifyImageBufferIsBigEnough(data, dataSize);

    auto dataProvider = adoptCF(CGDataProviderCreateWithData(nullptr, data, dataSize, nullptr));
    if (!dataProvider)
        return nullptr;

    auto imageSize = source.size();
    auto image = adoptCF(CGImageCreate(imageSize.width(), imageSize.height(), 8, 32, 4 * imageSize.width(), source.format().colorSpace.platformColorSpace(), kCGBitmapByteOrderDefault | dataAlphaInfo, dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    return encode(image.get(), destinationUTI.get(), quality, function);
}

template<typename Source, typename SourceDescription> static Vector<uint8_t> encodeToVector(Source&& source, SourceDescription&& sourceDescription, std::optional<double> quality)
{
    Vector<uint8_t> result;

    bool success = encode(std::forward<Source>(source), std::forward<SourceDescription>(sourceDescription), quality, scopedLambdaRef<PutBytesCallback>([&] (const void* data, size_t length) {
        result.append(static_cast<const uint8_t*>(data), length);
        return length;
    }));
    if (!success)
        return { };

    return result;
}

template<typename Source, typename SourceDescription> static String encodeToDataURL(Source&& source, SourceDescription&& sourceDescription, const String& mimeType, std::optional<double> quality)
{
    // FIXME: This could be done more efficiently with a streaming base64 encoder.

    auto encodedData = encodeToVector(std::forward<Source>(source), std::forward<SourceDescription>(sourceDescription), quality);
    if (encodedData.isEmpty())
        return "data:,"_s;

    return makeString("data:", mimeType, ";base64,", base64Encoded(encodedData));
}

Vector<uint8_t> data(CGImageRef image, CFStringRef destinationUTI, std::optional<double> quality)
{
    return encodeToVector(image, destinationUTI, quality);
}

Vector<uint8_t> data(const PixelBuffer& pixelBuffer, const String& mimeType, std::optional<double> quality)
{
    return encodeToVector(pixelBuffer, mimeType, quality);
}

String dataURL(CGImageRef image, CFStringRef destinationUTI, const String& mimeType, std::optional<double> quality)
{
    return encodeToDataURL(image, destinationUTI, mimeType, quality);
}

String dataURL(const PixelBuffer& pixelBuffer, const String& mimeType, std::optional<double> quality)
{
    return encodeToDataURL(pixelBuffer, mimeType, mimeType, quality);
}

} // namespace WebCore

#endif // USE(CG)
