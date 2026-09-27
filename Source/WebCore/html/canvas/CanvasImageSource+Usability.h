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

#pragma once

#include "ExceptionOr.h"

namespace WebCore {

class CSSStyleImageValue;
class CanvasBase;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class SVGImageElement;
class WebCodecsVideoFrame;

enum class CanvasImageSourceUsability : bool { Bad, Good };

// https://html.spec.whatwg.org/multipage/canvas.html#check-the-usability-of-the-image-argument
ExceptionOr<CanvasImageSourceUsability> checkUsability(HTMLImageElement&);
ExceptionOr<CanvasImageSourceUsability> checkUsability(SVGImageElement&);
#if ENABLE(VIDEO)
ExceptionOr<CanvasImageSourceUsability> checkUsability(HTMLVideoElement&);
#endif
ExceptionOr<CanvasImageSourceUsability> checkUsability(CanvasBase&);
ExceptionOr<CanvasImageSourceUsability> checkUsability(ImageBitmap&);
#if ENABLE(WEB_CODECS)
ExceptionOr<CanvasImageSourceUsability> checkUsability(WebCodecsVideoFrame&);
#endif
ExceptionOr<CanvasImageSourceUsability> checkUsability(CSSStyleImageValue&);

// For callers that treat an exception and "bad" alike, as createImageBitmap() and the
// VideoFrame constructor do.
bool isUsable(auto& image)
{
    auto usability = checkUsability(image);
    return !usability.hasException() && usability.returnValue() == CanvasImageSourceUsability::Good;
}

} // namespace WebCore
