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

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

class CSSStyleImageValue;
class HTMLCanvasElement;
class HTMLImageElement;
class ImageBitmap;
class SVGImageElement;
#if ENABLE(OFFSCREEN_CANVAS)
class OffscreenCanvas;
#endif
#if ENABLE(VIDEO)
class HTMLVideoElement;
#endif
#if ENABLE(WEB_CODECS)
class WebCodecsVideoFrame;
#endif

// MARK: - Definition
// https://html.spec.whatwg.org/multipage/canvas.html#canvasimagesource
using CanvasImageSource = std::variant<
      RefPtr<HTMLImageElement>
    , RefPtr<SVGImageElement>
    , RefPtr<HTMLCanvasElement>
    , RefPtr<ImageBitmap>
    , RefPtr<CSSStyleImageValue>
#if ENABLE(OFFSCREEN_CANVAS)
    , RefPtr<OffscreenCanvas>
#endif
#if ENABLE(VIDEO)
    , RefPtr<HTMLVideoElement>
#endif
#if ENABLE(WEB_CODECS)
    , RefPtr<WebCodecsVideoFrame>
#endif
    >;

} // namespace WebCore
