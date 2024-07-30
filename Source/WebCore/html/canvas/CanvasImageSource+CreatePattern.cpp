/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "CanvasImageSource+CreatePattern.h"

#include "CSSStyleImageValue.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBitmap.h"
#include "NaturalDimensions.h"
#include "SVGImageElement.h"
#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif
#if ENABLE(VIDEO)
#include "HTMLVideoElement.h"
#endif
#if ENABLE(WEB_CODECS)
#include "WebCodecsVideoFrame.h"
#endif

namespace WebCore {

// MARK: - Image Preprocessing for "CanvasRenderingContext2D createPattern"

// Helper used by sources that have `Ref<Image>` values (HTMLImageElement, SVGImageElement, CSSStyleImageValue).
template<typename T> static ExceptionOr<PreprocessedForCreatePatternOrBad<T>> preprocessImageForCreatePattern(ImageUsabilityGood<T>&& usability)
{
    // FIXME: The spec is not consistent on when to throw/return "bad". The choices made to throw vs. return "bad" are purely
    // because some existing tests depend on it.

    auto naturalDimensions = usability.source->naturalDimensions();
    if (!naturalDimensions.width || !naturalDimensions.height)
        return Exception { ExceptionCode::InvalidStateError };

    RefPtr nativeImage = usability.source->nativeImage();
    if (!nativeImage)
        return Exception { ExceptionCode::InvalidStateError };

    return { PreprocessedForCreatePattern<T> { WTFMove(usability), SourceImage { nativeImage.releaseNonNull() } } };
}

// Helper used by sources that are canvases (HTMLCanvasElement, OffscreenCanvas).
template<typename T> static ExceptionOr<PreprocessedForCreatePatternOrBad<T>> preprocessCanvasForCreatePattern(ImageUsabilityGood<T>&& usability)
{
    // FIXME: The spec is not consistent on when to throw/return "bad". The choices made to throw vs. return "bad" are purely
    // because some existing tests depend on it.

    auto* copiedImage = usability.image->copiedImage();
    if (!copiedImage)
        return Exception { ExceptionCode::InvalidStateError };

    auto nativeImage = copiedImage->nativeImage();
    if (!nativeImage)
        return Exception { ExceptionCode::InvalidStateError };

    return { PreprocessedForCreatePattern<T> { WTFMove(usability), SourceImage { nativeImage.releaseNonNull() } } };
}

ExceptionOr<PreprocessedForCreatePatternOrBad<HTMLImageElement>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<HTMLImageElement>&& usability)
{
    return preprocessImageForCreatePattern(WTFMove(usability));
}

ExceptionOr<PreprocessedForCreatePatternOrBad<SVGImageElement>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<SVGImageElement>&& usability)
{
    return preprocessImageForCreatePattern(WTFMove(usability));
}

ExceptionOr<PreprocessedForCreatePatternOrBad<CSSStyleImageValue>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<CSSStyleImageValue>&& usability)
{
    return preprocessImageForCreatePattern(WTFMove(usability));
}

ExceptionOr<PreprocessedForCreatePatternOrBad<ImageBitmap>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<ImageBitmap>&& usability)
{
    auto sourceImage = usability.source.copyRef();
    return { PreprocessedForCreatePattern<ImageBitmap> { WTFMove(usability), SourceImage { WTFMove(sourceImage) } } };
}

ExceptionOr<PreprocessedForCreatePatternOrBad<HTMLCanvasElement>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<HTMLCanvasElement>&& usability)
{
    return preprocessCanvasForCreatePattern(WTFMove(usability));
}

#if ENABLE(OFFSCREEN_CANVAS)
ExceptionOr<PreprocessedForCreatePatternOrBad<OffscreenCanvas>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<OffscreenCanvas>&& usability)
{
    return preprocessCanvasForCreatePattern(WTFMove(usability));
}
#endif

#if ENABLE(VIDEO)
ExceptionOr<PreprocessedForCreatePatternOrBad<HTMLVideoElement>> preprocessForCreatePattern(const CanvasBase& canvasBase, ImageUsabilityGood<HTMLVideoElement>&& usability)
{
    // FIXME: The spec is not consistent on when to throw/return "bad". The choices made to throw vs. return "bad" are purely
    // because some existing tests depend on it.

#if USE(CG)
    if (auto nativeImage = usability.image->nativeImageForCurrentTime())
        return { PreprocessedForCreatePattern<HTMLVideoElement> { WTFMove(usability), SourceImage { nativeImage.releaseNonNull() } } };
#endif

    // FIXME: It doesn't really make sense to do this here since persistent uses (like CanvasPattern)
    // can be used with any rendering context.
    //
    // Ideally, this would allocate a buffer to be used with any context, to get the allocation
    // failure error case out of the way, and then the buffer would be configured and cached on
    // initial use.

    auto* canvasBuffer = canvasBase.buffer();
    auto renderingMode = canvasBuffer ? canvasBuffer->context().renderingMode() : RenderingMode::Unaccelerated;
    auto colorSpace = canvasBuffer ? canvasBuffer->context().colorSpace() : DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;

    auto size = usability.image->naturalSize();
    auto imageBuffer = usability.image->createBufferForPainting(size, renderingMode, colorSpace, pixelFormat);
    if (!imageBuffer)
        return { ImageUsabilityBad { } };

    usability.image->paintCurrentFrameInContext(imageBuffer->context(), FloatRect { { }, size });

    return { PreprocessedForCreatePattern<HTMLVideoElement> { WTFMove(usability), SourceImage { imageBuffer.releaseNonNull() } } };
}
#endif

#if ENABLE(WEB_CODECS)
ExceptionOr<PreprocessedForCreatePatternOrBad<WebCodecsVideoFrame>> preprocessForCreatePattern(const CanvasBase& canvasBase, ImageUsabilityGood<WebCodecsVideoFrame>&& usability)
{
    // FIXME: The spec is not consistent on when to throw/return "bad". The choices made to throw vs. return "bad" are purely
    // because some existing tests depend on it.

    // FIXME: Should be possible to use the VideoFrame directly without an intermediate buffer.

    // FIXME: It doesn't really make sense to do this here since persistent uses (like CanvasPattern)
    // can be used with any rendering context.
    //
    // Ideally, this would allocate a buffer to be used with any context, to get the allocation
    // failure error case out of the way, and then the buffer would be configured and cached on
    // initial use.

    auto* canvasBuffer = canvasBase.buffer();
    auto renderingMode = canvasBuffer ? canvasBuffer->context().renderingMode() : RenderingMode::Unaccelerated;
    auto colorSpace = canvasBuffer ? canvasBuffer->context().colorSpace() : DestinationColorSpace::SRGB();
    auto pixelFormat = ImageBufferPixelFormat::BGRA8;

    auto size = FloatSize { static_cast<float>(usability.image->displayWidth()), static_cast<float>(usability.image->displayHeight()) };
    auto imageBuffer = ImageBuffer::create(size, RenderingPurpose::MediaPainting, 1, colorSpace, pixelFormat, bufferOptionsForRendingMode(renderingMode));
    if (!imageBuffer)
        return { ImageUsabilityBad { } };

    imageBuffer->context().paintVideoFrame(usability.source.get(), FloatRect { { }, size }, usability.image->shouldDiscardAlpha());

    return { PreprocessedForCreatePattern<WebCodecsVideoFrame> { WTFMove(usability), SourceImage { imageBuffer.releaseNonNull() } } };
}
#endif

} // namespace WebCore
