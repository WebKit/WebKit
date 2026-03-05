/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "ImageUtilities.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageDecoderCG.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "NativeImage.h"
#include "PixelBuffer.h"
#include "SVGImage.h"
#include "SVGImageForContainer.h"
#include "UTIRegistry.h"
#include "UTIUtilities.h"
#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/CompletionHandler.h>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/ScopedLambda.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WorkQueue& sharedImageTranscodingQueueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("com.apple.WebKit.ImageTranscoding"_s));
    return queue.get();
}

static String transcodeImage(const String& path, const String& destinationUTI, const String& destinationExtension)
{
    auto sourceURL = adoptCF(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path.createCFString().get(), kCFURLPOSIXPathStyle, false));
    auto source = adoptCF(CGImageSourceCreateWithURL(sourceURL.get(), nullptr));
    if (!source)
        return nullString();

    auto sourceUTI = String(CGImageSourceGetType(source.get()));
    if (!sourceUTI || sourceUTI == destinationUTI)
        return nullString();

    // It is important to add the appropriate file extension to the temporary file path.
    // The File object depends solely on the extension to know the MIME type of the file.
    // Extract the original filename (without extension) to preserve it in the transcoded file.
    auto originalFilename = FileSystem::pathFileName(path);
    auto dotPosition = originalFilename.reverseFind('.');
    auto baseName = (dotPosition != notFound) ? originalFilename.left(dotPosition) : originalFilename;
    if (baseName.isEmpty())
        baseName = "image"_s;
    auto suffix = makeString('.', destinationExtension);
    auto [destinationPath, destinationFileHandle] = FileSystem::openTemporaryFile(baseName, suffix);
    if (!destinationFileHandle) {
        RELEASE_LOG_ERROR(Images, "transcodeImage: Destination image could not be created: %s %s\n", path.utf8().data(), destinationUTI.utf8().data());
        return nullString();
    }

    CGDataConsumerCallbacks callbacks = {
        [](void* info, const void* buffer, size_t count) -> size_t {
            auto& handle = *static_cast<FileSystem::FileHandle*>(info);
            return handle.write(unsafeMakeSpan(static_cast<const uint8_t*>(buffer), count)).value_or(0);
        },
        nullptr
    };

    auto consumer = adoptCF(CGDataConsumerCreate(&destinationFileHandle, &callbacks));
    auto destination = adoptCF(CGImageDestinationCreateWithDataConsumer(consumer.get(), destinationUTI.createCFString().get(), 1, nullptr));

    CGImageDestinationAddImageFromSource(destination.get(), source.get(), 0, nullptr);

    if (!CGImageDestinationFinalize(destination.get())) {
        RELEASE_LOG_ERROR(Images, "transcodeImage: Image transcoding fails: %s %s\n", path.utf8().data(), destinationUTI.utf8().data());
        destinationFileHandle = { };
        FileSystem::deleteFile(destinationPath);
        return nullString();
    }

    return destinationPath;
}

Vector<String> findImagesForTranscoding(const Vector<String>& paths, const Vector<String>& allowedMIMETypes)
{
    bool allowAllImages = allowedMIMETypes.contains("image/*"_s);

    bool needsTranscoding = false;
    auto transcodingPaths = paths.map([&](auto& path) {
        auto mimeType = WebCore::MIMETypeRegistry::mimeTypeForPath(path);

        if (allowAllImages && mimeType.startsWith("image/"_s))
            return nullString();

        if (!allowedMIMETypes.contains(mimeType)) {
            needsTranscoding = true;
            return path;
        }
        return nullString();
    });

    // If none of the files needs image transcoding, return an empty Vector.
    return needsTranscoding ? transcodingPaths : Vector<String>();
}

Vector<String> transcodeImages(const Vector<String>& paths, const String& destinationUTI, const String& destinationExtension)
{
    ASSERT(!destinationUTI.isNull());
    ASSERT(!destinationExtension.isNull());

    return paths.map([&](auto& path) {
        // Append the transcoded path if the image needs transcoding. Otherwise append a null string.
        return path.isNull() ? nullString() : transcodeImage(path, destinationUTI, destinationExtension);
    });
}

String descriptionString(ImageDecodingError error)
{
    switch (error) {
    case ImageDecodingError::Internal:
        return "Internal error"_s;
    case ImageDecodingError::BadData:
        return "Bad data"_s;
    case ImageDecodingError::UnsupportedType:
        return "Unsupported image type"_s;
    }

    return "Unkown error"_s;
}

Expected<std::pair<String, Vector<IntSize>>, ImageDecodingError> utiAndAvailableSizesFromImageData(std::span<const uint8_t> data)
{
    Ref buffer = SharedBuffer::create(data);
    Ref imageDecoder = ImageDecoderCG::create(buffer.get(), AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    imageDecoder->setData(buffer.get(), true);
    if (imageDecoder->encodedDataStatus() == EncodedDataStatus::Error)
        return makeUnexpected(ImageDecodingError::BadData);

    auto uti = imageDecoder->uti();
    if (!isSupportedImageType(uti))
        return makeUnexpected(ImageDecodingError::UnsupportedType);

    size_t frameCount = imageDecoder->frameCount();
    // Do not support animated image.
    if (imageDecoder->repetitionCount() != RepetitionCountNone && frameCount > 1)
        return makeUnexpected(ImageDecodingError::UnsupportedType);

    Vector<IntSize> sizes;
    sizes.reserveInitialCapacity(frameCount);
    for (size_t index = 0; index < frameCount; ++index)
        sizes.append(imageDecoder->frameSizeAtIndex(index));

    return std::make_pair(WTF::move(uti), WTF::move(sizes));
}

static RefPtr<NativeImage> tryCreateNativeImageFromBitmapImageData(std::span<const uint8_t> data, std::optional<FloatSize> preferredSize)
{
    Ref buffer = SharedBuffer::create(data);
    Ref imageDecoder = ImageDecoderCG::create(buffer.get(), AlphaOption::Premultiplied, GammaAndColorProfileOption::Applied);
    imageDecoder->setData(buffer.get(), true);
    if (imageDecoder->encodedDataStatus() == EncodedDataStatus::Error)
        return nullptr;

    auto sourceUTI = imageDecoder->uti();
    if (!isSupportedImageType(sourceUTI))
        return nullptr;

    auto preferredIndex = [&]() -> size_t {
        if (!preferredSize)
            return imageDecoder->primaryFrameIndex();
        size_t frameCount = imageDecoder->frameCount();
        for (size_t index = 0; index < frameCount; ++index) {
            if (imageDecoder->frameSizeAtIndex(index) == *preferredSize)
                return index;
        }
        return imageDecoder->primaryFrameIndex();
    };
    RetainPtr image = imageDecoder->createFrameImageAtIndex(preferredIndex());
    if (!image)
        return nullptr;

    return NativeImage::create(WTF::move(image));
}

static void tryCreateNativeImageFromData(std::span<const uint8_t> data, std::optional<FloatSize> preferredSize, CompletionHandler<void(RefPtr<NativeImage>&&)>&& completionHandler)
{
    if (RefPtr nativeImage = tryCreateNativeImageFromBitmapImageData(data, preferredSize)) {
        completionHandler(WTF::move(nativeImage));
        return;
    }
    SVGImage::tryCreateFromData(data, [completionHandler = WTF::move(completionHandler)](auto svgImage) mutable {
        if (!svgImage) {
            completionHandler(nullptr);
            return;
        }
        completionHandler(svgImage->nativeImage(svgImage->size()));
    });
}

static Vector<Ref<ShareableBitmap>> createBitmapsFromNativeImage(NativeImage& image, std::span<const unsigned> lengths)
{
    Vector<Ref<ShareableBitmap>> bitmaps;
    auto sourceColorSpace = image.colorSpace();
    // The conversion could lead to loss of HDR contents.
    auto destinationColorSpace = sourceColorSpace.supportsOutput() ? sourceColorSpace : DestinationColorSpace::SRGB();
    for (auto length : lengths) {
        RefPtr bitmap = ShareableBitmap::createFromImageDraw(image, destinationColorSpace, { (int)length, (int)length }, image.size());
        if (!bitmap)
            return { };

        bitmaps.append(bitmap.releaseNonNull());
    }

    return bitmaps;
}

static RefPtr<NativeImage> createNativeImageFromSVGImage(SVGImage& image, const IntSize& size)
{
    RefPtr buffer = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return nullptr;

    Ref svgImageContainer = SVGImageForContainer::create(&image, size, 1, { });
    buffer->context().drawImage(svgImageContainer.get(), FloatPoint::zero());

    return ImageBuffer::sinkIntoNativeImage(WTF::move(buffer));
}

static Vector<Ref<ShareableBitmap>> createBitmapsFromSVGImage(SVGImage& image, std::span<const unsigned> lengths)
{
    Vector<Ref<ShareableBitmap>> bitmaps;
    for (auto length : lengths) {
        IntSize size { (int)length, (int)length };
        RefPtr nativeImage = createNativeImageFromSVGImage(image, size);
        if (!nativeImage)
            return { };

        RefPtr bitmap = ShareableBitmap::createFromImageDraw(*nativeImage, DestinationColorSpace::SRGB());
        if (!bitmap)
            return { };

        bitmaps.append(bitmap.releaseNonNull());
    }

    return bitmaps;
}

void createBitmapsFromImageData(std::span<const uint8_t> data, std::span<const unsigned> lengths, CompletionHandler<void(Vector<Ref<ShareableBitmap>>&&)>&& completionHandler)
{
    if (RefPtr nativeImage = tryCreateNativeImageFromBitmapImageData(data, std::nullopt)) {
        completionHandler(createBitmapsFromNativeImage(*nativeImage, lengths));
        return;
    }

    SVGImage::tryCreateFromData(data, [lengthsVector = Vector<unsigned> { lengths }, completionHandler = WTF::move(completionHandler)](auto svgImage) mutable {
        if (!svgImage) {
            completionHandler({ });
            return;
        }
        completionHandler(createBitmapsFromSVGImage(*svgImage, lengthsVector.span()));
    });
}

RefPtr<SharedBuffer> createIconDataFromBitmaps(Vector<Ref<ShareableBitmap>>&& bitmaps)
{
    if (bitmaps.isEmpty())
        return nullptr;

    constexpr auto icoUTI = "com.microsoft.ico"_s;
    RetainPtr cfUTI = icoUTI.createCFString();
    RetainPtr colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    RetainPtr destinationData = adoptCF(CFDataCreateMutable(0, 0));
    RetainPtr destination = adoptCF(CGImageDestinationCreateWithData(destinationData.get(), cfUTI.get(), bitmaps.size(), nullptr));

    for (Ref bitmap : bitmaps) {
        RetainPtr cgImage = bitmap->createPlatformImage();
        if (!cgImage) {
            RELEASE_LOG_ERROR(Images, "createIconDataFromBitmaps: Fails to create CGImage with size { %d , %d }", bitmap->size().width(), bitmap->size().height());
            return nullptr;
        }

        CGImageDestinationAddImage(destination.get(), cgImage.get(), nullptr);
    }

    if (!CGImageDestinationFinalize(destination.get()))
        return nullptr;

    return SharedBuffer::create(destinationData.get());
}

// FIXME: This does not implement preferredSize for SVG at the moment as there are no callers that pass preferredSize.
void decodeImageWithSize(std::span<const uint8_t> data, std::optional<FloatSize> preferredSize, CompletionHandler<void(RefPtr<ShareableBitmap>&&)>&& completionHandler)
{
    tryCreateNativeImageFromData(data, preferredSize, [completionHandler = WTF::move(completionHandler)](auto nativeImage) mutable {
        if (!nativeImage) {
            completionHandler(nullptr);
            return;
        }

        auto sourceColorSpace = nativeImage->colorSpace();
        auto destinationColorSpace = sourceColorSpace.supportsOutput() ? sourceColorSpace : DestinationColorSpace::SRGB();
        RefPtr bitmap = ShareableBitmap::create({ nativeImage->size(), destinationColorSpace });
        if (!bitmap) {
            completionHandler(nullptr);
            return;
        }

        auto context = bitmap->createGraphicsContext();
        if (!context) {
            completionHandler(nullptr);
            return;
        }

        FloatRect rect { { }, nativeImage->size() };
        context->drawNativeImage(*nativeImage, rect, rect, { CompositeOperator::Copy });
        completionHandler(WTF::move(bitmap));
    });
}

using PutBytesCallback = size_t(std::span<const uint8_t>);

uint8_t verifyImageBufferIsBigEnough(std::span<const uint8_t> buffer)
{
    RELEASE_ASSERT(!buffer.empty());

    uintptr_t lastByte;
    bool isSafe = WTF::safeAdd((uintptr_t)buffer.data(), buffer.size() - 1, lastByte);
    RELEASE_ASSERT(isSafe);

    return *(uint8_t*)lastByte;
}

CFStringRef jpegUTI()
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
#if PLATFORM(IOS_FAMILY)
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

    if (equalLettersIgnoringASCIICase(mimeType, "image/png"_s))
        return kUTTypePNG;
    if (equalLettersIgnoringASCIICase(mimeType, "image/jpeg"_s) || equalLettersIgnoringASCIICase(mimeType, "image/jpg"_s))
        return jpegUTI();
    if (equalLettersIgnoringASCIICase(mimeType, "image/gif"_s))
        return kUTTypeGIF;

    ASSERT_NOT_REACHED();
    return kUTTypePNG;
#endif
ALLOW_DEPRECATED_DECLARATIONS_END
}

static RetainPtr<CFDictionaryRef> imagePropertiesForDestinationUTIAndQuality(CFStringRef destinationUTI, std::optional<double> quality)
{
    if (CFEqual(destinationUTI, jpegUTI()) && quality && *quality >= 0.0 && *quality <= 1.0) {
        // Apply the compression quality to the JPEG image destination.
        auto compressionQuality = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &*quality));
        const void* key = kCGImageDestinationLossyCompressionQuality;
        const void* value = compressionQuality.get();
        return adoptCF(CFDictionaryCreate(0, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }
    return nullptr;

    // FIXME: Setting kCGImageDestinationBackgroundColor to black for JPEG images in imageProperties would save some math
    // in the calling functions, but it doesn't seem to work.
}

static bool encode(CGImageRef image, const String& mimeType, std::optional<double> quality, const ScopedLambda<PutBytesCallback>& function)
{
    if (!image)
        return false;

    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);
    if (!destinationUTI)
        return false;

    CGDataConsumerCallbacks callbacks {
        [](void* context, const void* buffer, size_t count) -> size_t {
            auto functor = *static_cast<const ScopedLambda<PutBytesCallback>*>(context);
            return functor(unsafeMakeSpan(static_cast<const uint8_t*>(buffer), count));
        },
        nullptr
    };

    auto consumer = adoptCF(CGDataConsumerCreate(const_cast<ScopedLambda<PutBytesCallback>*>(&function), &callbacks));
    auto destination = adoptCF(CGImageDestinationCreateWithDataConsumer(consumer.get(), destinationUTI.get(), 1, nullptr));

    auto imageProperties = imagePropertiesForDestinationUTIAndQuality(destinationUTI.get(), quality);
    CGImageDestinationAddImage(destination.get(), image, imageProperties.get());

    return CGImageDestinationFinalize(destination.get());
}

static bool encode(const PixelBuffer& source, const String& mimeType, std::optional<double> quality, const ScopedLambda<PutBytesCallback>& function)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    auto destinationUTI = utiFromImageBufferMIMEType(mimeType);

    CGImageAlphaInfo dataAlphaInfo = kCGImageAlphaLast;

    auto data = source.bytes();
    auto dataSize = data.size();

    Vector<uint8_t> premultipliedData;

    if (CFEqual(destinationUTI.get(), jpegUTI())) {
        // FIXME: Use PixelBufferConversion for this once it supports RGBX.

        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        if (!premultipliedData.tryReserveCapacity(dataSize))
            return false;

        premultipliedData.grow(dataSize);
        for (size_t i = 0; i < dataSize; i += 4) {
            unsigned alpha = data[i + 3];
            if (alpha != 255) {
                premultipliedData[i + 0] = data[i + 0] * alpha / 255;
                premultipliedData[i + 1] = data[i + 1] * alpha / 255;
                premultipliedData[i + 2] = data[i + 2] * alpha / 255;
            } else {
                premultipliedData[i + 0] = data[i + 0];
                premultipliedData[i + 1] = data[i + 1];
                premultipliedData[i + 2] = data[i + 2];
            }
        }

        dataAlphaInfo = kCGImageAlphaNoneSkipLast; // Ignore the alpha channel.
        data = premultipliedData.mutableSpan();
    }

    verifyImageBufferIsBigEnough(data);

    auto dataProvider = adoptCF(CGDataProviderCreateWithData(nullptr, data.data(), dataSize, nullptr));
    if (!dataProvider)
        return false;

    auto imageSize = source.size();
    auto image = adoptCF(CGImageCreate(imageSize.width(), imageSize.height(), 8, 32, 4 * imageSize.width(), protect(source.format().colorSpace.platformColorSpace()).get(), static_cast<uint32_t>(kCGBitmapByteOrderDefault) | static_cast<uint32_t>(dataAlphaInfo), dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    return encode(image.get(), mimeType, quality, function);
}

template<typename Source> static Vector<uint8_t> encodeToVector(Source&& source, const String& mimeType, std::optional<double> quality)
{
    Vector<uint8_t> result;

    bool success = encode(std::forward<Source>(source), mimeType, quality, scopedLambdaRef<PutBytesCallback>([&] (std::span<const uint8_t> data) {
        result.append(data);
        return data.size();
    }));
    if (!success)
        return { };

    return result;
}

Vector<uint8_t> encodeData(CGImageRef image, const String& mimeType, std::optional<double> quality)
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=308704): The encoding should take in background color and not use drawing,
    // so we would not need to create the temp NativeImage.
    return encodeData(NativeImage::create(image), mimeType, quality);
}

Vector<uint8_t> encodeData(const PixelBuffer& pixelBuffer, const String& mimeType, std::optional<double> quality)
{
    return encodeToVector(pixelBuffer, mimeType, quality);
}

String encodeDataURL(CGImageRef image, const String& mimeType, std::optional<double> quality)
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=308704): The encoding should take in background color and not use drawing,
    // so we would not need to create the temp NativeImage.
    return encodeDataURL(NativeImage::create(image), mimeType, quality);
}

String encodeDataURL(const PixelBuffer& pixelBuffer, const String& mimeType, std::optional<double> quality)
{
    auto encodedData = encodeToVector(pixelBuffer, mimeType, quality);
    if (encodedData.isEmpty())
        return "data:,"_s;
    return makeString("data:"_s, mimeType, ";base64,"_s, base64Encoded(encodedData));
}

Vector<uint8_t> platformEncodeData(const NativeImage& image, const String& mimeType, std::optional<double> quality)
{
    return encodeToVector(image.platformImage().get(), mimeType, quality);
}

} // namespace WebCore
