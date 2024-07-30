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

#include "CanvasImageSource+Usability.h"
#include "SourceImage.h"

namespace WebCore {

// MARK: - Image Preprocessing for "CanvasRenderingContext2D createPattern"

template<typename T> struct PreprocessedForCreatePattern : ImageUsabilityGood<T> {
    SourceImage sourceImage;
};

template<typename T> using PreprocessedForCreatePatternOrBad = std::variant<ImageUsabilityBad, PreprocessedForCreatePattern<T>>;

ExceptionOr<PreprocessedForCreatePatternOrBad<HTMLImageElement>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<HTMLImageElement>&&);
ExceptionOr<PreprocessedForCreatePatternOrBad<SVGImageElement>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<SVGImageElement>&&);
ExceptionOr<PreprocessedForCreatePatternOrBad<CSSStyleImageValue>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<CSSStyleImageValue>&&);
ExceptionOr<PreprocessedForCreatePatternOrBad<ImageBitmap>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<ImageBitmap>&&);
ExceptionOr<PreprocessedForCreatePatternOrBad<HTMLCanvasElement>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<HTMLCanvasElement>&&);

#if ENABLE(OFFSCREEN_CANVAS)
ExceptionOr<PreprocessedForCreatePatternOrBad<OffscreenCanvas>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<OffscreenCanvas>&&);
#endif
#if ENABLE(VIDEO)
ExceptionOr<PreprocessedForCreatePatternOrBad<HTMLVideoElement>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<HTMLVideoElement>&&);
#endif
#if ENABLE(WEB_CODECS)
ExceptionOr<PreprocessedForCreatePatternOrBad<WebCodecsVideoFrame>> preprocessForCreatePattern(const CanvasBase&, ImageUsabilityGood<WebCodecsVideoFrame>&&);
#endif

} // namespace WebCore
