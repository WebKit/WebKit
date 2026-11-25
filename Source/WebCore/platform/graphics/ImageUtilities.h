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

Vector<uint8_t> encodeData(const PixelBuffer&, StringImpl* mimeType, std::optional<double> quality = std::nullopt);
Vector<uint8_t> encodeData(const NativeImage&, StringImpl* mimeType, std::optional<double> quality = std::nullopt);
Vector<uint8_t> encodeData(const RefPtr<NativeImage>&, StringImpl* mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT Vector<uint8_t> encodeData(RefPtr<ImageBuffer>&&, StringImpl* mimeType, std::optional<double> quality = std::nullopt);

RefPtr<StringImpl> encodeDataURL(const PixelBuffer&, StringImpl* mimeType, std::optional<double> quality = std::nullopt);
RefPtr<StringImpl> encodeDataURL(const NativeImage&, StringImpl* mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT RefPtr<StringImpl> encodeDataURL(const RefPtr<NativeImage>&, StringImpl* mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT RefPtr<StringImpl> encodeDataURL(RefPtr<ImageBuffer>&&, StringImpl* mimeType, std::optional<double> quality = std::nullopt);

#if USE(CG)
WEBCORE_EXPORT WorkQueue& sharedImageTranscodingQueueSingleton();

// Given a list of files' 'paths' and 'allowedMIMETypes', the function returns a list
// of strings whose size is the same as the size of 'paths' and its entries are all
// null strings except the ones whose MIME types are not in 'allowedMIMETypes'.
WEBCORE_EXPORT Vector<RefPtr<StringImpl>> findImagesForTranscoding(const Vector<RefPtr<StringImpl>>& paths, const Vector<RefPtr<StringImpl>>& allowedMIMETypes);

// Given a list of images' 'paths', this function transcodes these images to a new
// format whose UTI is destinationUTI. The result of the transcoding will be written
// to temporary files whose extensions are 'destinationExtension'. It returns a list
// of paths to the result temporary files. If an entry in 'paths' is null or an error
// happens while transcoding, a null string will be added to the returned list.
WEBCORE_EXPORT Vector<RefPtr<StringImpl>> transcodeImages(const Vector<RefPtr<StringImpl>>& paths, StringImpl* destinationUTI, StringImpl* destinationExtension);

// Same as transcodeImages, but performs the work on a background queue and invokes the completion handler on the main thread with the transcoded paths.
WEBCORE_EXPORT void transcodeImagesInBackgroundQueue(Vector<RefPtr<StringImpl>>&& paths, RefPtr<StringImpl>&& destinationUTI, RefPtr<StringImpl>&& destinationExtension, CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&&);

enum class ImageDecodingError : uint8_t {
    Internal,
    BadData,
    UnsupportedType
};
WEBCORE_EXPORT RefPtr<StringImpl> descriptionString(ImageDecodingError);
WEBCORE_EXPORT std::expected<std::pair<RefPtr<StringImpl>, Vector<IntSize>>, ImageDecodingError> utiAndAvailableSizesFromImageData(std::span<const uint8_t>);
WEBCORE_EXPORT std::expected<Vector<std::pair<RefPtr<StringImpl>, float>>, ImageDecodingError> imageMetadataFromImageData(std::span<const uint8_t>);
WEBCORE_EXPORT RefPtr<SharedBuffer> createIconDataFromBitmaps(Vector<Ref<ShareableBitmap>>&&);

Vector<uint8_t> encodeData(CGImageRef, StringImpl* mimeType, std::optional<double> quality = std::nullopt);
WEBCORE_EXPORT RefPtr<StringImpl> encodeDataURL(CGImageRef, StringImpl* mimeType, std::optional<double> quality = std::nullopt);
NODELETE WEBCORE_EXPORT uint8_t verifyImageBufferIsBigEnough(std::span<const uint8_t> buffer);
RetainPtr<CFStringRef> utiFromImageBufferMIMEType(StringImpl* mimeType);
NODELETE CFStringRef jpegUTI();
#endif

// For the implementations, not to be called directly.
Vector<uint8_t> platformEncodeData(const NativeImage&, StringImpl* mimeType, std::optional<double> quality);

} // namespace WebCore

