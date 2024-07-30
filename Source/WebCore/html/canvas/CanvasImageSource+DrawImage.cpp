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
#include "CanvasImageSource+DrawImage.h"

#include "BitmapImage.h"
#include "CSSStyleImageValue.h"
#include "CanvasRenderingContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBitmap.h"
#include "ObjectSizeNegotiation.h"
#include "SVGImageElement.h"
#include "SVGImageForContainer.h"
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

// MARK: - Image Preprocessing for "CanvasRenderingContext2D drawImage"

// Helper used by sources that have `Ref<Image>` values (HTMLImageElement, SVGImageElement, CSSStyleImageValue).
template<typename T> static ExceptionOr<PreprocessedForDrawImageOrBad<T>> preprocessImageForDrawImage(const CanvasBase& canvasBase, ImageUsabilityGood<T>&& usability)
{
    // Additional steps based on step 4 of https://html.spec.whatwg.org/multipage/canvas.html#dom-context-2d-drawimage.

    auto naturalDimensions = usability.source->naturalDimensions();

    FloatSize size;
    if (naturalDimensions.width && naturalDimensions.height)
        size = { naturalDimensions.width->toFloat(), naturalDimensions.height->toFloat() };
    else {
        auto specifiedSize = ObjectSizeNegotiation::SpecifiedSize { std::nullopt, std::nullopt };
        auto defaultObjectSize = LayoutSize { canvasBase.size() };
        size = ObjectSizeNegotiation::defaultSizingAlgorithm(naturalDimensions, specifiedSize, defaultObjectSize);
    }

    if (RefPtr bitmapImage = dynamicDowncast<BitmapImage>(usability.source)) {
        // Drawing an animated image to a canvas should draw the first frame.
        if (bitmapImage->isAnimated() && !canvasBase.scriptExecutionContext()->settingsValues().animatedImageDebugCanvasDrawingEnabled) {
            bitmapImage = BitmapImage::create(bitmapImage->nativeImage());
            if (!bitmapImage)
                return { ImageUsabilityBad { } };
            usability.source = bitmapImage.releaseNonNull();
        }
    } else if (RefPtr svgImage = dynamicDowncast<SVGImage>(usability.source))
        usability.source = SVGImageForContainer::create(svgImage.get(), size, 1, svgImage->sourceURL());

    if constexpr (std::is_same_v<T, HTMLImageElement>) {
        // FIXME: HTML doesn't specified this behavior. A discussion on it is ongoing: https://github.com/whatwg/html/issues/10492.
        auto orientation = ImageOrientation::Orientation::FromImage;
        if (usability.image->allowsOrientationOverride()) {
            if (auto* renderer = usability.image->renderer())
                orientation = renderer->style().imageOrientation().orientation();
            else if (auto* computedStyle = usability.image->computedStyle())
                orientation = computedStyle->imageOrientation().orientation();
        }
        return { PreprocessedForDrawImage<T> { WTFMove(usability), size, orientation } };
    } else
        return { PreprocessedForDrawImage<T> { WTFMove(usability), size } };
}

// Helper used by sources that are canvases (HTMLCanvasElement, OffscreenCanvas).
template<typename T> static ExceptionOr<PreprocessedForDrawImageOrBad<T>> preprocessCanvasForDrawImage(ImageUsabilityGood<T>&& usability)
{
    RefPtr buffer = usability.image->makeRenderingResultsAvailable(ShouldApplyPostProcessingToDirtyRect::No);
    if (!buffer)
        return { ImageUsabilityBad { } };

    auto size = usability.image->size();
    return { PreprocessedForDrawImage<T> { WTFMove(usability), size, buffer.releaseNonNull() } };
}

ExceptionOr<PreprocessedForDrawImageOrBad<HTMLImageElement>> preprocessForDrawImage(const CanvasBase& canvasBase, ImageUsabilityGood<HTMLImageElement>&& usability)
{
    return preprocessImageForDrawImage(canvasBase, WTFMove(usability));
}

ExceptionOr<PreprocessedForDrawImageOrBad<SVGImageElement>> preprocessForDrawImage(const CanvasBase& canvasBase, ImageUsabilityGood<SVGImageElement>&& usability)
{
    return preprocessImageForDrawImage(canvasBase, WTFMove(usability));
}

ExceptionOr<PreprocessedForDrawImageOrBad<CSSStyleImageValue>> preprocessForDrawImage(const CanvasBase& canvasBase, ImageUsabilityGood<CSSStyleImageValue>&& usability)
{
    return preprocessImageForDrawImage(canvasBase, WTFMove(usability));
}

ExceptionOr<PreprocessedForDrawImageOrBad<ImageBitmap>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<ImageBitmap>&& usability)
{
    auto size = usability.source->logicalSize();
    return { PreprocessedForDrawImage<ImageBitmap> { WTFMove(usability), size } };
}

ExceptionOr<PreprocessedForDrawImageOrBad<HTMLCanvasElement>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<HTMLCanvasElement>&& usability)
{
    return preprocessCanvasForDrawImage(WTFMove(usability));
}

#if ENABLE(OFFSCREEN_CANVAS)
ExceptionOr<PreprocessedForDrawImageOrBad<OffscreenCanvas>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<OffscreenCanvas>&& usability)
{
    return preprocessCanvasForDrawImage(WTFMove(usability));
}
#endif

#if ENABLE(VIDEO)
ExceptionOr<PreprocessedForDrawImageOrBad<HTMLVideoElement>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<HTMLVideoElement>&& usability)
{
    auto size = usability.image->naturalSize();
    return { PreprocessedForDrawImage<HTMLVideoElement> { WTFMove(usability), size } };
}
#endif

#if ENABLE(WEB_CODECS)
ExceptionOr<PreprocessedForDrawImageOrBad<WebCodecsVideoFrame>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<WebCodecsVideoFrame>&& usability)
{
    auto size = FloatSize { static_cast<float>(usability.image->displayWidth()), static_cast<float>(usability.image->displayHeight()) };
    return { PreprocessedForDrawImage<WebCodecsVideoFrame> { WTFMove(usability), size } };
}

#endif

// MARK: - DrawImage Painting Options

ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<HTMLImageElement>& preprocessed, CompositeOperator op, BlendMode blendMode)
{
    return {
        op,
        blendMode,
        preprocessed.orientation,
        preprocessed.image->document().settings().imageSubsamplingEnabled() ? AllowImageSubsampling::Yes : AllowImageSubsampling::No,
        preprocessed.image->document().settings().showDebugBorders() ? ShowDebugBackground::Yes : ShowDebugBackground::No
    };
}

ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<SVGImageElement>& preprocessed, CompositeOperator op, BlendMode blendMode)
{
    return {
        op,
        blendMode,
        ImageOrientation::Orientation::FromImage,
        preprocessed.image->document().settings().imageSubsamplingEnabled() ? AllowImageSubsampling::Yes : AllowImageSubsampling::No,
        preprocessed.image->document().settings().showDebugBorders() ? ShowDebugBackground::Yes : ShowDebugBackground::No
    };
}

ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<CSSStyleImageValue>& preprocessed, CompositeOperator op, BlendMode blendMode)
{
    auto document = preprocessed.image->document();
    return {
        op,
        blendMode,
        ImageOrientation::Orientation::FromImage,
        document && document->settings().imageSubsamplingEnabled() ? AllowImageSubsampling::Yes : AllowImageSubsampling::No,
        document && document->settings().showDebugBorders() ? ShowDebugBackground::Yes : ShowDebugBackground::No
    };
}

ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<ImageBitmap>&, CompositeOperator op, BlendMode blendMode)
{
    return { op, blendMode };
}

ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<HTMLCanvasElement>& preprocessed, CompositeOperator op, BlendMode blendMode)
{
    return {
        op,
        blendMode,
        ImageOrientation::Orientation::FromImage,
        preprocessed.image->document().settings().imageSubsamplingEnabled() ? AllowImageSubsampling::Yes : AllowImageSubsampling::No,
        preprocessed.image->document().settings().showDebugBorders() ? ShowDebugBackground::Yes : ShowDebugBackground::No
    };
}

#if ENABLE(OFFSCREEN_CANVAS)
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<OffscreenCanvas>&, CompositeOperator op, BlendMode blendMode)
{
    return { op, blendMode };
}
#endif
#if ENABLE(VIDEO)
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<HTMLVideoElement>& preprocessed, CompositeOperator op, BlendMode blendMode)
{
    return {
        op,
        blendMode,
        ImageOrientation::Orientation::FromImage,
        preprocessed.image->document().settings().imageSubsamplingEnabled() ? AllowImageSubsampling::Yes : AllowImageSubsampling::No,
        preprocessed.image->document().settings().showDebugBorders() ? ShowDebugBackground::Yes : ShowDebugBackground::No
    };
}
#endif
#if ENABLE(WEB_CODECS)
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<WebCodecsVideoFrame>&, CompositeOperator op, BlendMode blendMode)
{
    return { op, blendMode };
}
#endif

// MARK: - DrawImage Post Processing Requirements

bool shouldPostProcess(PreprocessedForDrawImage<HTMLImageElement>& preprocessed)
{
    return !is<BitmapImage>(preprocessed.source);
}

bool shouldPostProcess(PreprocessedForDrawImage<SVGImageElement>& preprocessed)
{
    return !is<BitmapImage>(preprocessed.source);
}

bool shouldPostProcess(PreprocessedForDrawImage<CSSStyleImageValue>& preprocessed)
{
    return !is<BitmapImage>(preprocessed.source);
}

bool shouldPostProcess(PreprocessedForDrawImage<HTMLCanvasElement>& preprocessed)
{
    return !preprocessed.image->renderingContext() || !preprocessed.image->renderingContext()->is2d() || preprocessed.image->havePendingCanvasNoiseInjection();
}

#if ENABLE(OFFSCREEN_CANVAS)
bool shouldPostProcess(PreprocessedForDrawImage<OffscreenCanvas>& preprocessed)
{
    return !preprocessed.image->renderingContext() || !preprocessed.image->renderingContext()->is2d() || preprocessed.image->havePendingCanvasNoiseInjection();
}
#endif

} // namespace WebCore
