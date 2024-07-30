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

#include "CSSStyleImageValue.h"
#include "CanvasImageSource.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
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
#include <wtf/Ref.h>

namespace WebCore {

class SecurityOrigin;

template<typename> class ExceptionOr;

// MARK: - Image Usability Checking
// https://html.spec.whatwg.org/multipage/canvas.html#check-the-usability-of-the-image-argument

struct ImageUsabilityBad { };
template<typename> struct ImageUsabilityGood;
template<typename T> using ImageUsability = std::variant<ImageUsabilityBad, ImageUsabilityGood<T>>;

// Type specific specializations of the `ImageUsabilityGood` struct are used to pass back
// data extracted for usability checking that will also be needed to do anything with the
// the type.

template<> struct ImageUsabilityGood<HTMLImageElement>    { Ref<HTMLImageElement> image; Ref<Image> source; };
ExceptionOr<ImageUsability<HTMLImageElement>> checkUsability(Ref<HTMLImageElement>);

template<> struct ImageUsabilityGood<SVGImageElement>     { Ref<SVGImageElement> image; Ref<Image> source; };
ExceptionOr<ImageUsability<SVGImageElement>> checkUsability(Ref<SVGImageElement>);

template<> struct ImageUsabilityGood<CSSStyleImageValue>  { Ref<CSSStyleImageValue> image; Ref<Image> source; };
ExceptionOr<ImageUsability<CSSStyleImageValue>> checkUsability(Ref<CSSStyleImageValue>);

template<> struct ImageUsabilityGood<ImageBitmap>         { Ref<ImageBitmap> image; Ref<ImageBuffer> source; };
ExceptionOr<ImageUsability<ImageBitmap>> checkUsability(Ref<ImageBitmap>);

template<> struct ImageUsabilityGood<HTMLCanvasElement>   { Ref<HTMLCanvasElement> image; };
ExceptionOr<ImageUsability<HTMLCanvasElement>> checkUsability(Ref<HTMLCanvasElement>);

#if ENABLE(OFFSCREEN_CANVAS)
template<> struct ImageUsabilityGood<OffscreenCanvas>     { Ref<OffscreenCanvas> image; };
ExceptionOr<ImageUsability<OffscreenCanvas>> checkUsability(Ref<OffscreenCanvas>);
#endif
#if ENABLE(VIDEO)
template<> struct ImageUsabilityGood<HTMLVideoElement>    { Ref<HTMLVideoElement> image; };
ExceptionOr<ImageUsability<HTMLVideoElement>> checkUsability(Ref<HTMLVideoElement>);
#endif
#if ENABLE(WEB_CODECS)
template<> struct ImageUsabilityGood<WebCodecsVideoFrame> { Ref<WebCodecsVideoFrame> image; Ref<VideoFrame> source; };
ExceptionOr<ImageUsability<WebCodecsVideoFrame>> checkUsability(Ref<WebCodecsVideoFrame>);
#endif

} // namespace WebCore
