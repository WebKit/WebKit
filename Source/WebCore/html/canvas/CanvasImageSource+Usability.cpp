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
#include "CanvasImageSource+Usability.h"

#include "CSSStyleImageValue.h"
#include "CachedImage.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBitmap.h"
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

// MARK: - Image Usability Checking
// https://html.spec.whatwg.org/multipage/canvas.html#check-the-usability-of-the-image-argument

ExceptionOr<ImageUsability<HTMLImageElement>> checkUsability(Ref<HTMLImageElement> image)
{
    // For both HTMLImageElement and SVGImageElement:
    //
    // 1. If `image`'s current request's state is broken, then throw an "InvalidStateError" DOMException.
    // 2. If `image` is not fully decodable, then return bad.

    // FIXME: Expose better interface on HTMLImageElement using these spec terms.

    if (!image->complete())
        return { ImageUsabilityBad { } };

    auto* cachedImage = image->cachedImage();
    if (!cachedImage)
        return { ImageUsabilityBad { } };

    if (cachedImage->status() == CachedImage::Status::DecodeError)
        return Exception { ExceptionCode::InvalidStateError, "The HTMLImageElement provided is in the 'broken' state."_s };

    RefPtr rawImage = cachedImage->rawImage();
    if (!rawImage)
        return { ImageUsabilityBad { } };

    // 3. If `image` has a natural width or natural height (or both) equal to zero, then return bad.
    auto naturalDimensions = rawImage->naturalDimensions();
    if ((naturalDimensions.width && !*naturalDimensions.width) || (naturalDimensions.height && !*naturalDimensions.height))
        return { ImageUsabilityBad { } };

    return { ImageUsabilityGood<HTMLImageElement> { WTFMove(image), rawImage.releaseNonNull() } };
}

ExceptionOr<ImageUsability<SVGImageElement>> checkUsability(Ref<SVGImageElement> image)
{
    // For both HTMLImageElement and SVGImageElement:
    //
    // 1. If `image`'s current request's state is broken, then throw an "InvalidStateError" DOMException.
    // 2. If `image` is not fully decodable, then return bad.

    // FIXME: Expose better interface on HTMLImageElement using these spec terms.
    // FIXME: Unlike, HTMLImageElement, this one does not check the "complete()" function.

    auto* cachedImage = image->cachedImage();
    if (!cachedImage)
        return { ImageUsabilityBad { } };

    if (cachedImage->status() == CachedImage::Status::DecodeError)
        return Exception { ExceptionCode::InvalidStateError, "The HTMLImageElement provided is in the 'broken' state."_s };

    RefPtr rawImage = cachedImage->rawImage();
    if (!rawImage)
        return { ImageUsabilityBad { } };

    // 3. If `image` has a natural width or natural height (or both) equal to zero, then return bad.
    auto naturalDimensions = rawImage->naturalDimensions();
    if ((naturalDimensions.width && !*naturalDimensions.width) || (naturalDimensions.height && !*naturalDimensions.height))
        return { ImageUsabilityBad { } };

    return { ImageUsabilityGood<SVGImageElement> { WTFMove(image), rawImage.releaseNonNull() } };
}

ExceptionOr<ImageUsability<CSSStyleImageValue>> checkUsability(Ref<CSSStyleImageValue> image)
{
    // https://drafts.css-houdini.org/css-paint-api/#drawing-a-cssimagevalue
    //
    // FIXME: Its not clear what rules to use for this.
    //
    // All the spec currently says is:
    //
    //   "When a CanvasImageSource object represents an CSSImageValue, the result of invoking
    //    the value’s underlying image algorithm must be used as the source image for the
    //    purposes of drawImage."
    //
    // Using rules similar to HTMLImageElement/SVGImageElement for now.

    // 1. If `image`'s current request's state is broken, then throw an "InvalidStateError" DOMException.
    // 2. If `image` is not fully decodable, then return bad.

    auto* cachedImage = image->image();
    if (!cachedImage)
        return { ImageUsabilityBad { } };

    if (cachedImage->status() == CachedImage::Status::DecodeError)
        return Exception { ExceptionCode::InvalidStateError };

    RefPtr rawImage = cachedImage->rawImage();
    if (!rawImage)
        return { ImageUsabilityBad { } };

    // 3. If `image` has a natural width or natural height (or both) equal to zero, then return bad.
    auto naturalDimensions = rawImage->naturalDimensions();
    if ((naturalDimensions.width && !*naturalDimensions.width) || (naturalDimensions.height && !*naturalDimensions.height))
        return { ImageUsabilityBad { } };

    return { ImageUsabilityGood<CSSStyleImageValue> { WTFMove(image), rawImage.releaseNonNull() } };
}

ExceptionOr<ImageUsability<ImageBitmap>> checkUsability(Ref<ImageBitmap> image)
{
    // 1. If `image`'s [[Detached]] internal slot value is set to true, then throw an "InvalidStateError" DOMException.
    auto buffer = image->bufferIfNotDetached();
    if (!buffer)
        return Exception { ExceptionCode::InvalidStateError };

    return { ImageUsabilityGood<ImageBitmap> { WTFMove(image), *buffer } };
}

ExceptionOr<ImageUsability<HTMLCanvasElement>> checkUsability(Ref<HTMLCanvasElement> image)
{
    // 1. If `image` has either a horizontal dimension or a vertical dimension equal to zero, then throw an "InvalidStateError" DOMException.
    if (!image->width() || !image->height())
        return Exception { ExceptionCode::InvalidStateError };

    return { ImageUsabilityGood<HTMLCanvasElement> { WTFMove(image) } };
}

#if ENABLE(OFFSCREEN_CANVAS)
ExceptionOr<ImageUsability<OffscreenCanvas>> checkUsability(Ref<OffscreenCanvas> image)
{
    // If `image` has either a horizontal dimension or a vertical dimension equal to zero, then throw an "InvalidStateError" DOMException.
    if (!image->width() || !image->height())
        return Exception { ExceptionCode::InvalidStateError };

    return { ImageUsabilityGood<OffscreenCanvas> { WTFMove(image) } };
}
#endif

#if ENABLE(VIDEO)
ExceptionOr<ImageUsability<HTMLVideoElement>> checkUsability(Ref<HTMLVideoElement> image)
{
    // 1. If `image`'s readyState attribute is either HAVE_NOTHING or HAVE_METADATA, then return bad.
    if (image->readyState() == HTMLMediaElement::HAVE_NOTHING || image->readyState() == HTMLMediaElement::HAVE_METADATA)
        return { ImageUsabilityBad { } };

    return { ImageUsabilityGood<HTMLVideoElement> { WTFMove(image) } };
}
#endif

#if ENABLE(WEB_CODECS)
ExceptionOr<ImageUsability<WebCodecsVideoFrame>> checkUsability(Ref<WebCodecsVideoFrame> image)
{
    // 1. If `image`'s [[Detached]] internal slot value is set to true, then throw an "InvalidStateError" DOMException.
    RefPtr internalFrame = image->internalFrameIfNotDetached();
    if (!internalFrame)
        return Exception { ExceptionCode::InvalidStateError, "frame is detached"_s };

    return { ImageUsabilityGood<WebCodecsVideoFrame> { WTFMove(image), internalFrame.releaseNonNull() } };
}
#endif

} // namespace WebCore
