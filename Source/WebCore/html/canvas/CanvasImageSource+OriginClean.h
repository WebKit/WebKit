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

namespace WebCore {

class CSSStyleImageValue;
class CachedImage;
class CanvasBase;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class SVGImageElement;
class SecurityOrigin;
class WebCodecsVideoFrame;

// https://html.spec.whatwg.org/multipage/canvas.html#the-image-argument-is-not-origin-clean
bool isOriginClean(const HTMLImageElement&, const SecurityOrigin&);
bool isOriginClean(const SVGImageElement&, const SecurityOrigin&);
bool isOriginClean(const CachedImage&, const SecurityOrigin&);
#if ENABLE(VIDEO)
bool isOriginClean(const HTMLVideoElement&, const SecurityOrigin&);
#endif
bool isOriginClean(const CanvasBase&, const SecurityOrigin&);
bool isOriginClean(const ImageBitmap&, const SecurityOrigin&);
#if ENABLE(WEB_CODECS)
bool isOriginClean(const WebCodecsVideoFrame&, const SecurityOrigin&);
#endif
bool isOriginClean(const CSSStyleImageValue&, const SecurityOrigin&);

} // namespace WebCore
