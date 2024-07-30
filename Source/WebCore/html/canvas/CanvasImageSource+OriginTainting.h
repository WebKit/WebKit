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

#include "CanvasImageSource.h"

namespace WebCore {

class SecurityOrigin;

// MARK: - Origin Tainting
// https://html.spec.whatwg.org/multipage/canvas.html#the-image-argument-is-not-origin-clean

// FIXME: According to the definition above, the SecurityOrigin should not be needed
// to implement a `isOriginClean` check, but currently HTMLVideoElement requires it,
// and keeping the signature the same for all is needed for use in generic contexts.
bool taintsOrigin(const SecurityOrigin&, const HTMLImageElement&);
bool taintsOrigin(const SecurityOrigin&, const SVGImageElement&);
bool taintsOrigin(const SecurityOrigin&, const CSSStyleImageValue&);
bool taintsOrigin(const SecurityOrigin&, const ImageBitmap&);
bool taintsOrigin(const SecurityOrigin&, const HTMLCanvasElement&);
#if ENABLE(OFFSCREEN_CANVAS)
bool taintsOrigin(const SecurityOrigin&, const OffscreenCanvas&);
#endif
#if ENABLE(VIDEO)
bool taintsOrigin(const SecurityOrigin&, const HTMLVideoElement&);
#endif
#if ENABLE(WEB_CODECS)
bool taintsOrigin(const SecurityOrigin&, const WebCodecsVideoFrame&);
#endif

} // namespace WebCore
