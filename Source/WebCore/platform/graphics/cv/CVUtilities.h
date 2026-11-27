/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <CoreVideo/CoreVideo.h>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <wtf/RetainPtr.h>

using CGColorSpaceRef = struct CGColorSpace*;
using CVPixelBufferPoolRef = struct __CVPixelBufferPool*;
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;

namespace WebCore {
class ColorSpace;
class ProcessIdentity;
enum class VideoPixelFormat;
struct PlatformVideoColorSpace;
namespace PlanarImageConverter {
struct SampleFormat;
}

// Creates CVPixelBufferPool that creates CVPixelBuffers backed by IOSurfaces.
// These buffers can be for example sent through IPC.
WEBCORE_EXPORT std::expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createIOSurfaceCVPixelBufferPool(size_t width, size_t height, OSType pixelFormat, unsigned minimumBufferCount = 0u, bool isCGImageCompatible = false);

WEBCORE_EXPORT std::expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createInMemoryCVPixelBufferPool(size_t width, size_t height, OSType pixelFormat, unsigned minimumBufferCount = 0u, bool isCGImageCompatible = false);

WEBCORE_EXPORT std::expected<RetainPtr<CVPixelBufferPoolRef>, CVReturn> createCVPixelBufferPool(size_t width, size_t height, OSType pixelFormat, unsigned minimumBufferCount = 0u, bool isCGImageCompatible = false, bool shouldUseIOSUrface = true);

WEBCORE_EXPORT std::expected<RetainPtr<CVPixelBufferRef>, CVReturn> createCVPixelBufferFromPool(CVPixelBufferPoolRef, unsigned maximumBufferCount = 0u);

WEBCORE_EXPORT std::expected<RetainPtr<CVPixelBufferRef>, CVReturn> createCVPixelBuffer(IOSurfaceRef);

WEBCORE_EXPORT RetainPtr<CGColorSpaceRef> createCGColorSpaceForCVPixelBuffer(CVPixelBufferRef);

WEBCORE_EXPORT CGImageAlphaInfo alphaInfoForCVPixelBuffer(CVPixelBufferRef);

// Returns the color space of a video frame of 8-bit RGB contents in the color space, or nullopt when
// a video frame cannot describe it.
WEBCORE_EXPORT std::optional<PlatformVideoColorSpace> videoColorSpaceForRGBContents(const ColorSpace&);
// Attaches the color space of a video frame of RGB contents, which the readers of the frame take it from.
WEBCORE_EXPORT void attachVideoColorSpaceForRGBContents(const PlatformVideoColorSpace&, CVPixelBufferRef);
WEBCORE_EXPORT void setAlphaInfoForCVPixelBuffer(CVPixelBufferRef, CGImageAlphaInfo);
// Returns a copy of a premultiplied 32BGRA or 32ARGB pixel buffer with straight alpha, which keeps its
// other attachments.
RetainPtr<CVPixelBufferRef> createUnpremultipliedPixelBuffer(CVPixelBufferRef);

std::span<const OSType> pixelFormatsWithAlphaPlane();
WEBCORE_EXPORT bool isPixelFormatWithAlphaPlane(OSType);
std::optional<int> bitDepthOfPixelFormatWithAlphaPlane(OSType);
std::optional<OSType> pixelFormatWithAlpha(OSType colorFormat);
std::expected<RetainPtr<CVPixelBufferRef>, CVReturn> createPixelBufferWithAlpha(CVPixelBufferRef colorBuffer, CVPixelBufferRef alphaBuffer, CVPixelBufferPoolRef);
bool copyAlphaIntoAlphaPlane(CVPixelBufferRef destination, CVPixelBufferRef alphaBuffer);
bool copyAlphaFromLumaPlane(CVPixelBufferRef alphaBuffer, std::span<uint8_t> destination, PlanarImageConverter::SampleFormat);
bool copyPlanarYUVToPixelBuffer(std::span<const uint8_t> source, VideoPixelFormat sourceFormat, bool isFullRange, CVPixelBufferRef destination);
// The color planes of a pixel buffer with an alpha plane, in the bi-planar format with the same layout.
WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> createPixelBufferWithoutAlphaPlane(CVPixelBufferRef);
// VideoToolbox doesn't support the pixel formats WebKit registers: those are replaced by their color planes, without alpha.
// Returns null when the buffer is null or the copy fails.
RetainPtr<CVPixelBufferRef> pixelBufferForVideoToolbox(CVPixelBufferRef);
bool copyAlphaPlaneToAlphaChannel(CVPixelBufferRef source, CVPixelBufferRef destination);

// Should be called with non-empty ProcessIdentity.
WEBCORE_EXPORT void setOwnershipIdentityForCVPixelBuffer(CVPixelBufferRef, const ProcessIdentity&);

WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> createBlackPixelBuffer(size_t width, size_t height, bool shouldUseIOSurface = false);

}
