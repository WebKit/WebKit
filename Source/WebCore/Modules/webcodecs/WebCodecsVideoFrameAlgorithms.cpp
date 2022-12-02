/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebCodecsVideoFrameAlgorithms.h"

#if ENABLE(WEB_CODECS)

#include "DOMRectReadOnly.h"
#include "VideoColorSpace.h"

namespace WebCore {

// https://w3c.github.io/webcodecs/#valid-videoframebufferinit
bool isValidVideoFrameBufferInit(const WebCodecsVideoFrame::BufferInit& init)
{
    if (!init.codedWidth || !init.codedHeight)
        return false;

    if (init.visibleRect) {
        if (init.visibleRect->x < 0 || init.visibleRect->y < 0 || init.visibleRect->width < 0 || init.visibleRect->height < 0)
            return false;
        if (!std::isfinite(init.visibleRect->x) || !std::isfinite(init.visibleRect->y) || !std::isfinite(init.visibleRect->width) || !std::isfinite(init.visibleRect->height))
            return false;

        if (init.visibleRect->y + init.visibleRect->height > init.codedHeight)
            return false;

        if (init.visibleRect->x + init.visibleRect->width > init.codedWidth)
            return false;
    }

    if (init.displayWidth && !init.displayWidth)
        return false;
    if (init.displayHeight && !init.displayHeight)
        return false;
    return true;
}

static bool isMultiple(double value, unsigned factor)
{
     return !(static_cast<unsigned>(value) % factor);
}

// https://w3c.github.io/webcodecs/#videoframe-verify-rect-offset-alignment
bool verifyRectOffsetAlignment(VideoPixelFormat format, const DOMRectInit& rect)
{
    switch (format) {
    case VideoPixelFormat::I420:
    case VideoPixelFormat::I420A:
    case VideoPixelFormat::I422:
    case VideoPixelFormat::NV12:
        return isMultiple(rect.x, 2) && isMultiple(rect.y, 2);
    case VideoPixelFormat::I444:
    case VideoPixelFormat::RGBA:
    case VideoPixelFormat::RGBX:
    case VideoPixelFormat::BGRA:
    case VideoPixelFormat::BGRX:
        return true;
    }
    return false;
}

// https://w3c.github.io/webcodecs/#videoframe-verify-rect-size-alignment
bool verifyRectSizeAlignment(VideoPixelFormat format, const DOMRectInit& rect)
{
    switch (format) {
    case VideoPixelFormat::I420:
    case VideoPixelFormat::I420A:
        return !(static_cast<unsigned>(rect.width) % 2) && !(static_cast<unsigned>(rect.height) % 2);
    case VideoPixelFormat::I422:
        return !(static_cast<unsigned>(rect.width) % 2) && !(static_cast<unsigned>(rect.height) % 2);
    case VideoPixelFormat::I444:
        return true;
    case VideoPixelFormat::NV12:
        return !(static_cast<unsigned>(rect.width) % 2) && !(static_cast<unsigned>(rect.height) % 2);
    case VideoPixelFormat::RGBA:
    case VideoPixelFormat::RGBX:
    case VideoPixelFormat::BGRA:
    case VideoPixelFormat::BGRX:
        return true;
    }
    return false;
}

// https://w3c.github.io/webcodecs/#videoframe-parse-visible-rect
ExceptionOr<DOMRectInit> parseVisibleRect(const DOMRectInit& defaultRect, const std::optional<DOMRectInit>& overrideRect, size_t codedWidth, size_t codedHeight, VideoPixelFormat format)
{
    auto sourceRect = defaultRect;
    if (overrideRect) {
        if (!overrideRect->width || !overrideRect->height)
            return Exception { TypeError, "overrideRect is not valid"_s };
        if (overrideRect->x + overrideRect->width > codedWidth)
            return Exception { TypeError, "overrideRect is not valid"_s };
        if (overrideRect->y + overrideRect->height > codedHeight)
            return Exception { TypeError, "overrideRect is not valid"_s };
        sourceRect = *overrideRect;
    }
    if (!verifyRectOffsetAlignment(format, sourceRect))
        return Exception { TypeError, "offset alignment is invalid"_s };
    return sourceRect;
}

size_t videoPixelFormatToPlaneCount(VideoPixelFormat format)
{
    switch (format) {
    case VideoPixelFormat::I420:
    case VideoPixelFormat::I420A:
    case VideoPixelFormat::I444:
    case VideoPixelFormat::I422:
        return 3;
    case VideoPixelFormat::NV12:
        return 2;
    case VideoPixelFormat::RGBA:
    case VideoPixelFormat::RGBX:
    case VideoPixelFormat::BGRA:
    case VideoPixelFormat::BGRX:
        return 1;
    }
    return 1;
}

size_t videoPixelFormatToSampleByteSizePerPlane()
{
    return 1;
}

static inline size_t sampleCountPerPixel(VideoPixelFormat format, size_t planeNumber)
{
    switch (format) {
    case VideoPixelFormat::I420:
    case VideoPixelFormat::I420A:
    case VideoPixelFormat::I444:
    case VideoPixelFormat::I422:
        return 1;
    case VideoPixelFormat::NV12:
        return planeNumber ? 2 : 1;
    case VideoPixelFormat::RGBA:
    case VideoPixelFormat::RGBX:
    case VideoPixelFormat::BGRA:
    case VideoPixelFormat::BGRX:
        return 4;
    }
    return 1;
}

size_t videoPixelFormatToSubSampling(VideoPixelFormat format, size_t planeNumber)
{
    switch (format) {
    case VideoPixelFormat::I420:
    case VideoPixelFormat::I420A:
    case VideoPixelFormat::I444:
    case VideoPixelFormat::I422:
    case VideoPixelFormat::NV12:
        return planeNumber ? 2 : 1;
    case VideoPixelFormat::RGBA:
    case VideoPixelFormat::RGBX:
    case VideoPixelFormat::BGRA:
    case VideoPixelFormat::BGRX:
        return 1;
    }
    return 1;
}

// https://w3c.github.io/webcodecs/#videoframe-compute-layout-and-allocation-size
ExceptionOr<CombinedPlaneLayout> computeLayoutAndAllocationSize(const DOMRectInit& parsedRect, const std::optional<Vector<PlaneLayout>>& layout, VideoPixelFormat format)
{
    auto planeCount = videoPixelFormatToPlaneCount(format);
    if (layout && layout->size() != planeCount)
        return Exception { TypeError, "layout size is invalid"_s };

    size_t minAllocationSize = 0;
    Vector<ComputedPlaneLayout> computedLayouts;
    computedLayouts.reserveInitialCapacity(planeCount);
    Vector<size_t> endOffsets;
    endOffsets.reserveInitialCapacity(planeCount);
    for (size_t i = 0; i < planeCount; ++i) {
        size_t pixelSampleCount = sampleCountPerPixel(format, i);

        auto sampleBytes = videoPixelFormatToSampleByteSizePerPlane();
        auto sampleWidth = videoPixelFormatToSubSampling(format, i);
        auto sampleHeight = videoPixelFormatToSubSampling(format, i);
        auto sampleWidthBytes = sampleWidth * sampleBytes;

        ComputedPlaneLayout computedLayout;
        computedLayout.sourceTop = parsedRect.y / sampleHeight;
        computedLayout.sourceHeight = parsedRect.height / sampleHeight;
        computedLayout.sourceLeftBytes = pixelSampleCount * parsedRect.x / sampleWidthBytes;
        computedLayout.sourceWidthBytes = pixelSampleCount * parsedRect.width / sampleWidthBytes;

        if (layout) {
            if (layout.value()[i].stride < computedLayout.sourceWidthBytes)
                return Exception { TypeError, "layout stride is invalid"_s };

            computedLayout.destinationOffset = layout.value()[i].offset;
            computedLayout.destinationStride = layout.value()[i].stride;
        } else {
            computedLayout.destinationOffset = minAllocationSize;
            computedLayout.destinationStride = computedLayout.sourceWidthBytes;
        }

        size_t planeSize, planeEnd;
        if (!WTF::safeMultiply(computedLayout.destinationStride, computedLayout.sourceHeight, planeSize) || planeSize > std::numeric_limits<uint32_t>::max())
            return Exception { TypeError, "planeSize is too big"_s };

        if (!WTF::safeAdd(planeSize, computedLayout.destinationOffset, planeEnd) || planeEnd > std::numeric_limits<uint32_t>::max())
            return Exception { TypeError, "planeEnd is too big"_s };

        endOffsets.uncheckedAppend(planeEnd);
        minAllocationSize = std::max(minAllocationSize, planeEnd);

        for (size_t j = 1; j < i; ++j) {
            if (planeEnd > computedLayouts[j].destinationOffset && endOffsets[j] > computedLayout.destinationOffset)
                return Exception { TypeError, "planes are overlapping"_s };
        }

        computedLayouts.uncheckedAppend(computedLayout);
    }

    return CombinedPlaneLayout { minAllocationSize, WTFMove(computedLayouts) };
}

// https://w3c.github.io/webcodecs/#videoframe-parse-videoframecopytooptions
ExceptionOr<CombinedPlaneLayout> parseVideoFrameCopyToOptions(const WebCodecsVideoFrame& frame, const WebCodecsVideoFrame::CopyToOptions& options)
{
    ASSERT(!frame.isDetached());
    ASSERT(frame.format());

    if (options.rect && !verifyRectSizeAlignment(*frame.format(), *options.rect))
        return Exception { TypeError, "rect size alignment is invalid"_s };

    auto& visibleRect = *frame.visibleRect();
    auto parsedRect = parseVisibleRect({ visibleRect.x(), visibleRect.y(), visibleRect.width(), visibleRect.height() }, options.rect, frame.codedWidth(), frame.codedHeight(), *frame.format());

    if (parsedRect.hasException())
        return parsedRect.releaseException();

    return computeLayoutAndAllocationSize(parsedRect.returnValue(), options.layout, *frame.format());
}

// https://w3c.github.io/webcodecs/#videoframe-initialize-visible-rect-and-display-size
void initializeVisibleRectAndDisplaySize(WebCodecsVideoFrame& frame, const WebCodecsVideoFrame::Init& init, const DOMRectInit& defaultVisibleRect, size_t defaultDisplayWidth, size_t defaultDisplayHeight)
{
    auto visibleRect = init.visibleRect.value_or(defaultVisibleRect);
    frame.setVisibleRect(visibleRect);
    if (init.displayWidth && init.displayHeight)
        frame.setDisplaySize(*init.displayWidth, *init.displayHeight);
    else {
        auto widthScale = defaultDisplayWidth / defaultVisibleRect.width;
        auto heightScale = defaultDisplayHeight / defaultVisibleRect.height;
        frame.setDisplaySize(visibleRect.width * widthScale, visibleRect.height * heightScale);
    }
}

// https://w3c.github.io/webcodecs/#videoframe-pick-color-space
VideoColorSpaceInit videoFramePickColorSpace(const std::optional<VideoColorSpaceInit>& overrideColorSpace, VideoPixelFormat format)
{
    if (overrideColorSpace)
        return *overrideColorSpace;

    if (isRGBVideoPixelFormat(format))
        return { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Iec6196621, PlatformVideoMatrixCoefficients::Rgb, true };

    return { PlatformVideoColorPrimaries::Bt709, PlatformVideoTransferCharacteristics::Bt709, PlatformVideoMatrixCoefficients::Bt709, false };
}

// https://w3c.github.io/webcodecs/#validate-videoframeinit
static bool isNegativeOrNonFinite(double value)
{
    return value < 0 || !std::isfinite(value);
}

bool validateVideoFrameInit(const WebCodecsVideoFrame::Init& init, size_t codedWidth, size_t codedHeight, VideoPixelFormat format)
{
    if (init.visibleRect) {
        auto& visibleRect = *init.visibleRect;
        if (!verifyRectOffsetAlignment(format, visibleRect))
            return false;

        if (isNegativeOrNonFinite(visibleRect.x) || isNegativeOrNonFinite(visibleRect.y) || isNegativeOrNonFinite(visibleRect.width) || isNegativeOrNonFinite(visibleRect.height))
            return false;
        if (!visibleRect.width || !visibleRect.height)
            return false;

        if (visibleRect.y + visibleRect.height > codedHeight)
            return false;
        if (visibleRect.x + visibleRect.width > codedWidth)
            return false;
    }

    if (!codedWidth || !codedHeight)
        return false;

    if (!!init.displayWidth != !!init.displayHeight)
        return false;
    if (init.displayWidth && (!*init.displayWidth || !*init.displayHeight))
        return false;

    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
