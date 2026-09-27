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
#include "CanvasImageSource+OriginClean.h"

#include "CachedImage.h"
#include "CanvasBase.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "SVGImageElement.h"
#include "SecurityOrigin.h"
#include "WebCodecsVideoFrame.h"

namespace WebCore {

bool isOriginClean(const CachedImage& cachedImage, const SecurityOrigin&)
{
    RefPtr image = cachedImage.image();
    if (!image)
        return true;

    if (image->sourceURL().protocolIsData())
        return true;

    // An SVG with a <foreignObject> can leak cross-origin data, such as visited links.
    if (image->renderingTaintsOrigin())
        return false;

    return !cachedImage.isCORSCrossOrigin();
}

bool isOriginClean(const HTMLImageElement& image, const SecurityOrigin& origin)
{
    RefPtr cachedImage = image.cachedImage();
    return !cachedImage || isOriginClean(*cachedImage, origin);
}

bool isOriginClean(const SVGImageElement& image, const SecurityOrigin& origin)
{
    RefPtr cachedImage = image.cachedImage();
    return !cachedImage || isOriginClean(*cachedImage, origin);
}

#if ENABLE(VIDEO)
bool isOriginClean(const HTMLVideoElement& image, const SecurityOrigin& origin)
{
    return !image.taintsOrigin(origin);
}
#endif

bool isOriginClean(const CanvasBase& image, const SecurityOrigin&)
{
    return image.originClean();
}

bool isOriginClean(const ImageBitmap& image, const SecurityOrigin&)
{
    return image.originClean();
}

#if ENABLE(WEB_CODECS)
bool isOriginClean(const WebCodecsVideoFrame&, const SecurityOrigin&)
{
    return true;
}
#endif

// FIXME: CSSStyleImageValue is not supported as an image source, and is treated as never
// origin-clean.
bool isOriginClean(const CSSStyleImageValue&, const SecurityOrigin&)
{
    return false;
}

} // namespace WebCore
