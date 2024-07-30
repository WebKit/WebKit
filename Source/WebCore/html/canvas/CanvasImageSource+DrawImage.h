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
#include "FloatSize.h"
#include "ImagePaintingOptions.h"

namespace WebCore {

// MARK: - Image Preprocessing for "CanvasRenderingContext2D drawImage"

// Default implementation of `PreprocessedForDrawImage`.
template<typename T> struct PreprocessedForDrawImage : ImageUsabilityGood<T> {
    FloatSize size;
};

// Specialization of `PreprocessedForDrawImage` for `HTMLImageElement`. Includes orientation override.
template<> struct PreprocessedForDrawImage<HTMLImageElement> : ImageUsabilityGood<HTMLImageElement> {
    FloatSize size;
    ImageOrientation::Orientation orientation;
};

// Specialization of `PreprocessedForDrawImage` for `HTMLCanvasElement`. Ensures allocation of ImageBuffer succeeds.
template<> struct PreprocessedForDrawImage<HTMLCanvasElement> : ImageUsabilityGood<HTMLCanvasElement> {
    FloatSize size;
    Ref<ImageBuffer> source;
};

#if ENABLE(OFFSCREEN_CANVAS)
// Specialization of `PreprocessedForDrawImage` for `OffscreenCanvas`. Ensures allocation of ImageBuffer succeeds.
template<> struct PreprocessedForDrawImage<OffscreenCanvas> : ImageUsabilityGood<OffscreenCanvas> {
    FloatSize size;
    Ref<ImageBuffer> source;
};
#endif

template<typename T> using PreprocessedForDrawImageOrBad = std::variant<ImageUsabilityBad, PreprocessedForDrawImage<T>>;

ExceptionOr<PreprocessedForDrawImageOrBad<HTMLImageElement>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<HTMLImageElement>&&);
ExceptionOr<PreprocessedForDrawImageOrBad<SVGImageElement>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<SVGImageElement>&&);
ExceptionOr<PreprocessedForDrawImageOrBad<CSSStyleImageValue>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<CSSStyleImageValue>&&);
ExceptionOr<PreprocessedForDrawImageOrBad<ImageBitmap>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<ImageBitmap>&&);
ExceptionOr<PreprocessedForDrawImageOrBad<HTMLCanvasElement>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<HTMLCanvasElement>&&);
#if ENABLE(OFFSCREEN_CANVAS)
ExceptionOr<PreprocessedForDrawImageOrBad<OffscreenCanvas>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<OffscreenCanvas>&&);
#endif
#if ENABLE(VIDEO)
ExceptionOr<PreprocessedForDrawImageOrBad<HTMLVideoElement>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<HTMLVideoElement>&&);
#endif
#if ENABLE(WEB_CODECS)
ExceptionOr<PreprocessedForDrawImageOrBad<WebCodecsVideoFrame>> preprocessForDrawImage(const CanvasBase&, ImageUsabilityGood<WebCodecsVideoFrame>&&);
#endif

// MARK: - DrawImage Painting Options

ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<HTMLImageElement>&, CompositeOperator, BlendMode);
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<SVGImageElement>&, CompositeOperator, BlendMode);
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<CSSStyleImageValue>&, CompositeOperator, BlendMode);
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<ImageBitmap>&, CompositeOperator, BlendMode);
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<HTMLCanvasElement>&, CompositeOperator, BlendMode);
#if ENABLE(OFFSCREEN_CANVAS)
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<OffscreenCanvas>&, CompositeOperator, BlendMode);
#endif
#if ENABLE(VIDEO)
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<HTMLVideoElement>&, CompositeOperator, BlendMode);
#endif
#if ENABLE(WEB_CODECS)
ImagePaintingOptions drawImageOptions(PreprocessedForDrawImage<WebCodecsVideoFrame>&, CompositeOperator, BlendMode);
#endif

// MARK: - DrawImage Post Processing Requirements

bool shouldPostProcess(PreprocessedForDrawImage<HTMLImageElement>&);
bool shouldPostProcess(PreprocessedForDrawImage<SVGImageElement>&);
bool shouldPostProcess(PreprocessedForDrawImage<CSSStyleImageValue>&);
bool shouldPostProcess(PreprocessedForDrawImage<HTMLCanvasElement>&);
#if ENABLE(OFFSCREEN_CANVAS)
bool shouldPostProcess(PreprocessedForDrawImage<OffscreenCanvas>&);
#endif
template<typename T> constexpr bool shouldPostProcess(PreprocessedForDrawImage<T>&) { return false; }

} // namespace WebCore
