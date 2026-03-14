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

#pragma once

#include <WebCore/IntSize.h>
#include <WebCore/PlatformImage.h>
#include <optional>
#include <wtf/Forward.h>

#if USE(CG)
#include <CoreFoundation/CoreFoundation.h>
#include <span>
#include <wtf/WorkQueue.h>
#endif

namespace WebCore {

class ImageBuffer;
class NativeImage;
class PixelBuffer;
class ShareableBitmap;
class SharedBuffer;

Vector<uint8_t> encodeData(const PixelBuffer&, const String& mimeType, std::optional<double> quality = std::nullopt);
Vector<uint8_t> encodeData(const NativeImage&, const String& mimeType, std::optional<double> quality = std::nullopt);
Vector<uint8_t> encodeData(const RefPtr<NativeImage>&, const String& mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT Vector<uint8_t> encodeData(RefPtr<ImageBuffer>&&, const String& mimeType, std::optional<double> quality = std::nullopt);

String encodeDataURL(const PixelBuffer&, const String& mimeType, std::optional<double> quality = std::nullopt);
String encodeDataURL(const NativeImage&, const String& mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT String encodeDataURL(const RefPtr<NativeImage>&, const String& mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT String encodeDataURL(RefPtr<ImageBuffer>&&, const String& mimeType, std::optional<double> quality = std::nullopt);

#if USE(CG)
WEBCORE_EXPORT WorkQueue& sharedImageTranscodingQueueSingleton();

// Given a list of files' 'paths' and 'allowedMIMETypes', the function returns a list
// of strings whose size is the same as the size of 'paths' and its entries are all
// null strings except the ones whose MIME types are not in 'allowedMIMETypes'.
WEBCORE_EXPORT Vector<String> findImagesForTranscoding(const Vector<String>& paths, const Vector<String>& allowedMIMETypes);

// Given a list of images' 'paths', this function transcodes these images to a new
// format whose UTI is destinationUTI. The result of the transcoding will be written
// to temporary files whose extensions are 'destinationExtension'. It returns a list
// of paths to the result temporary files. If an entry in 'paths' is null or an error
// happens while transcoding, a null string will be added to the returned list.
WEBCORE_EXPORT Vector<String> transcodeImages(const Vector<String>& paths, const String& destinationUTI, const String& destinationExtension);

enum class ImageDecodingError : uint8_t {
    Internal,
    BadData,
    UnsupportedType
};
WEBCORE_EXPORT String descriptionString(ImageDecodingError);
WEBCORE_EXPORT Expected<std::pair<String, Vector<IntSize>>, ImageDecodingError> utiAndAvailableSizesFromImageData(std::span<const uint8_t>);
WEBCORE_EXPORT void createBitmapsFromImageData(std::span<const uint8_t> data, std::span<const unsigned> lengths, CompletionHandler<void(Vector<Ref<ShareableBitmap>>&&)>&&);
WEBCORE_EXPORT RefPtr<SharedBuffer> createIconDataFromBitmaps(Vector<Ref<ShareableBitmap>>&&);
WEBCORE_EXPORT void decodeImageWithSize(std::span<const uint8_t> data, std::optional<FloatSize>, CompletionHandler<void(RefPtr<ShareableBitmap>&&)>&&);

Vector<uint8_t> encodeData(CGImageRef, const String& mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT String encodeDataURL(CGImageRef, const String& mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT uint8_t NODELETE verifyImageBufferIsBigEnough(std::span<const uint8_t> buffer);
RetainPtr<CFStringRef> utiFromImageBufferMIMEType(const String& mimeType);
CFStringRef NODELETE jpegUTI();
#endif

// For the implementations, not to be called directly.
Vector<uint8_t> platformEncodeData(const NativeImage&, const String& mimeType, std::optional<double> quality);

} // namespace WebCore

