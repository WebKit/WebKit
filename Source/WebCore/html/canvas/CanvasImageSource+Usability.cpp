/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CanvasBase.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "ImageRequestState.h"
#include "SVGImageElement.h"
#include "WebCodecsVideoFrame.h"

namespace WebCore {

static ExceptionOr<CanvasImageSourceUsability> checkUsabilityOfImageElement(auto& image, ASCIILiteral brokenMessage)
{
    auto requestState = image.currentRequestState();

    // If image's current request's state is broken, then throw an "InvalidStateError" DOMException.
    if (requestState == ImageRequestState::Broken)
        return Exception { ExceptionCode::InvalidStateError, brokenMessage };

    // If image is not fully decodable, then return bad.
    if (requestState != ImageRequestState::CompletelyAvailable)
        return CanvasImageSourceUsability::Bad;

    // If image has a natural width or natural height (or both) equal to zero, then return bad.
    RefPtr sourceImage = image.sourceImage();
    if (!sourceImage)
        return CanvasImageSourceUsability::Bad;
    auto naturalDimensions = sourceImage->naturalDimensions();
    if ((naturalDimensions.width && !*naturalDimensions.width) || (naturalDimensions.height && !*naturalDimensions.height))
        return CanvasImageSourceUsability::Bad;

    return CanvasImageSourceUsability::Good;
}

ExceptionOr<CanvasImageSourceUsability> checkUsability(HTMLImageElement& image)
{
    return checkUsabilityOfImageElement(image, "The HTMLImageElement provided is in the 'broken' state."_s);
}

ExceptionOr<CanvasImageSourceUsability> checkUsability(SVGImageElement& image)
{
    return checkUsabilityOfImageElement(image, "The SVGImageElement provided is in the 'broken' state."_s);
}

#if ENABLE(VIDEO)
// HTMLVideoElement
ExceptionOr<CanvasImageSourceUsability> checkUsability(HTMLVideoElement& image)
{
    // If image's readyState attribute is either HAVE_NOTHING or HAVE_METADATA, then return bad.
    if (image.readyState() == HTMLMediaElement::HAVE_NOTHING || image.readyState() == HTMLMediaElement::HAVE_METADATA)
        return CanvasImageSourceUsability::Bad;

    return CanvasImageSourceUsability::Good;
}
#endif

ExceptionOr<CanvasImageSourceUsability> checkUsability(CanvasBase& image)
{
    // If image has either a horizontal dimension or a vertical dimension equal to zero, then
    // throw an "InvalidStateError" DOMException.
    if (!image.width() || !image.height())
        return Exception { ExceptionCode::InvalidStateError, "The canvas provided has a width or height of zero."_s };

    return CanvasImageSourceUsability::Good;
}

ExceptionOr<CanvasImageSourceUsability> checkUsability(ImageBitmap& image)
{
    // If image's [[Detached]] internal slot value is set to true, then throw an
    // "InvalidStateError" DOMException.
    if (image.isDetached())
        return Exception { ExceptionCode::InvalidStateError, "The ImageBitmap provided is detached."_s };

    return CanvasImageSourceUsability::Good;
}

#if ENABLE(WEB_CODECS)
ExceptionOr<CanvasImageSourceUsability> checkUsability(WebCodecsVideoFrame& image)
{
    // If image's [[Detached]] internal slot value is set to true, then throw an
    // "InvalidStateError" DOMException.
    if (image.isDetached())
        return Exception { ExceptionCode::InvalidStateError, "The VideoFrame provided is detached."_s };

    return CanvasImageSourceUsability::Good;
}
#endif

ExceptionOr<CanvasImageSourceUsability> checkUsability(CSSStyleImageValue&)
{
    // FIXME: CSSImageValue does not define what its check usability algorithm should be.
    return CanvasImageSourceUsability::Good;
}

} // namespace WebCore
