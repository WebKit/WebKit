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
#include "CanvasImageSource+OriginTainting.h"

#include "CSSStyleImageValue.h"
#include "CachedImage.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "SVGImageElement.h"
#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif
#if ENABLE(VIDEO)
#include "HTMLVideoElement.h"
#endif

namespace WebCore {

// MARK: - Origin Tainting
// https://html.spec.whatwg.org/multipage/canvas.html#the-image-argument-is-not-origin-clean

static bool taintsOrigin(const CachedImage* cachedImage)
{
    if (!cachedImage)
        return false;

    RefPtr image = cachedImage->image();
    if (!image)
        return false;

    if (image->sourceURL().protocolIsData())
        return false;

    if (image->renderingTaintsOrigin())
        return true;

    if (cachedImage->isCORSCrossOrigin())
        return true;

    ASSERT(cachedImage->origin());
    return false;
}

bool taintsOrigin(const SecurityOrigin&, const HTMLImageElement& element)
{
    return taintsOrigin(element.cachedImage());
}

bool taintsOrigin(const SecurityOrigin&, const SVGImageElement& element)
{
    return taintsOrigin(element.cachedImage());
}

bool taintsOrigin(const SecurityOrigin&, const HTMLCanvasElement& element)
{
    return !element.originClean();
}

bool taintsOrigin(const SecurityOrigin&, const ImageBitmap& imageBitmap)
{
    return !imageBitmap.originClean();
}

bool taintsOrigin(const SecurityOrigin&, const CSSStyleImageValue& imageValue)
{
    return taintsOrigin(imageValue.image());
}

#if ENABLE(OFFSCREEN_CANVAS)
bool taintsOrigin(const SecurityOrigin&, const OffscreenCanvas& offscreenCanvas)
{
    return !offscreenCanvas.originClean();
}
#endif

#if ENABLE(VIDEO)
bool taintsOrigin(const SecurityOrigin& origin, const HTMLVideoElement& video)
{
    // FIXME: This should not require passing a SecurityOrigin in. The spec stipulates for HTMLVideoElement:
    //
    //   "image's media data is CORS-cross-origin." (https://html.spec.whatwg.org/multipage/canvas.html#the-image-argument-is-not-origin-clean)
    //
    // where "CORS-cross-origin" is defined as
    //
    //   "A response whose type is "opaque" or "opaqueredirect" is CORS-cross-origin." (https://html.spec.whatwg.org/multipage/urls-and-fetching.html#cors-cross-origin)

    return video.taintsOrigin(origin);
}
#endif

#if ENABLE(WEB_CODECS)
bool taintsOrigin(const SecurityOrigin&, const WebCodecsVideoFrame&)
{
    // FIXME: This is currently undefined in the standard, but it does not appear a WebCodecsVideoFrame can ever be constructed in a way that is not origin clean. See https://github.com/whatwg/html/issues/10489
    return false;
}
#endif

} // namespace WebCore
