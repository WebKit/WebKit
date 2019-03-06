/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "DOMMatrix2DInit.h"
#include "Element.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageData.h"
#include "Path2D.h"
#include "TypedOMCSSImageValue.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/Float32Array.h>
#include <JavaScriptCore/Int32Array.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLUniformLocation.h"
#endif

namespace WebCore {

using RecordCanvasActionVariant = Variant<
    CanvasDirection,
    CanvasFillRule,
    CanvasLineCap,
    CanvasLineJoin,
    CanvasTextAlign,
    CanvasTextBaseline,
    DOMMatrix2DInit,
    Element*,
    HTMLImageElement*,
    ImageBitmap*,
    ImageData*,
    ImageSmoothingQuality,
    Path2D*,
#if ENABLE(WEBGL)
    WebGLBuffer*,
    WebGLFramebuffer*,
    WebGLProgram*,
    WebGLRenderbuffer*,
    WebGLShader*,
    WebGLTexture*,
    WebGLUniformLocation*,
#endif
    RefPtr<ArrayBuffer>,
    RefPtr<ArrayBufferView>,
    RefPtr<CanvasGradient>,
    RefPtr<CanvasPattern>,
    RefPtr<Float32Array>,
    RefPtr<HTMLCanvasElement>,
    RefPtr<HTMLImageElement>,
#if ENABLE(VIDEO)
    RefPtr<HTMLVideoElement>,
#endif
    RefPtr<ImageBitmap>,
#if ENABLE(CSS_TYPED_OM)
    RefPtr<TypedOMCSSImageValue>,
#endif
    RefPtr<ImageData>,
    RefPtr<Int32Array>,
    Vector<float>,
    Vector<int>,
    String,
    double,
    float,
    int64_t,
    uint32_t,
    int32_t,
    uint8_t,
    bool
>;

} // namespace WebCore
