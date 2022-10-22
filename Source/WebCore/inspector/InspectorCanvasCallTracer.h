/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "CanvasRenderingContext2DBase.h"
#include "WebGL2RenderingContext.h"
#include "WebGLRenderingContextBase.h"
#include <JavaScriptCore/TypedArrays.h>
#include <initializer_list>
#include <wtf/JSONValues.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class ArrayBuffer;
class ArrayBufferView;

} // namespace JSC

namespace WebCore {

class CanvasGradient;
class CanvasPattern;
class CanvasRenderingContext;
class Element;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class ImageData;
class OffscreenCanvas;
class Path2D;
class CSSStyleImageValue;
class WebGLBuffer;
class WebGLFramebuffer;
class WebGLProgram;
class WebGLQuery;
class WebGLRenderbuffer;
class WebGLSampler;
class WebGLShader;
class WebGLSync;
class WebGLTexture;
class WebGLTransformFeedback;
class WebGLUniformLocation;
class WebGLVertexArrayObject;
struct DOMMatrix2DInit;
struct DOMPointInit;
struct ImageDataSettings;
enum class RecordingSwizzleType : int;
enum class CanvasDirection;
enum class CanvasFillRule;
enum class CanvasLineCap;
enum class CanvasLineJoin;
enum class CanvasTextAlign;
enum class CanvasTextBaseline;
enum ImageSmoothingQuality;

#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_CSS_TYPED_OM_ARGUMENT(macro) \
    macro(RefPtr<CSSStyleImageValue>&) \
// end of FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_CSS_TYPED_OM_ARGUMENT

#if ENABLE(OFFSCREEN_CANVAS)
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_OFFSCREEN_CANVAS_ARGUMENT(macro) \
    macro(RefPtr<OffscreenCanvas>&) \
// end of FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_OFFSCREEN_CANVAS_ARGUMENT
#else
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_OFFSCREEN_CANVAS_ARGUMENT(macro)
#endif // ENABLE(OFFSCREEN_CANVAS)

#if ENABLE(VIDEO)
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_VIDEO_ARGUMENT(macro) \
    macro(RefPtr<HTMLVideoElement>&) \
// end of FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_VIDEO_ARGUMENT
#else
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_VIDEO_ARGUMENT(macro)
#endif // ENABLE(VIDEO)

#if ENABLE(WEB_CODECS)
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_VIDEOFRAME_ARGUMENT(macro) \
    macro(RefPtr<WebCodecsVideoFrame>&) \
// end of FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_VIDEOFRAME_ARGUMENT
#else
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_VIDEOFRAME_ARGUMENT(macro)
#endif // ENABLE(WEB_CODECS)

#if ENABLE(WEBGL)
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_WEBGL_ARGUMENT(macro) \
    macro(std::optional<WebGLRenderingContextBase::BufferDataSource>&) \
    macro(std::optional<WebGLRenderingContextBase::TexImageSource>&) \
    macro(WebGLBuffer*) \
    macro(WebGLFramebuffer*) \
    macro(WebGLProgram*) \
    macro(WebGLQuery*) \
    macro(WebGLRenderbuffer*) \
    macro(WebGLRenderingContextBase::BufferDataSource&) \
    macro(WebGLRenderingContextBase::Float32List::VariantType&) \
    macro(WebGLRenderingContextBase::Int32List::VariantType&) \
    macro(WebGLRenderingContextBase::TexImageSource&) \
    macro(WebGLSampler*) \
    macro(WebGLShader*) \
    macro(WebGLSync*) \
    macro(WebGLTexture*) \
    macro(WebGLUniformLocation*) \
// end of FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_WEBGL_ARGUMENT
#else
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_WEBGL_ARGUMENT(macro)
#endif // ENABLE(WEBGL)

#if ENABLE(WEBGL2)
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_WEBGL2_ARGUMENT(macro) \
    macro(WebGLTransformFeedback*) \
    macro(WebGL2RenderingContext::Uint32List::VariantType&) \
    macro(WebGLVertexArrayObject*) \
// end of FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_WEBGL2_ARGUMENT
#else
#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_WEBGL2_ARGUMENT(macro)
#endif // ENABLE(WEBGL2)

#define FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_ARGUMENT(macro) \
    macro(CanvasDirection) \
    macro(CanvasFillRule) \
    macro(CanvasImageSource&) \
    macro(CanvasLineCap) \
    macro(CanvasLineJoin) \
    macro(CanvasPath::RadiusVariant&) \
    macro(CanvasRenderingContext2DBase::StyleVariant&) \
    macro(CanvasTextAlign) \
    macro(CanvasTextBaseline) \
    macro(DOMMatrix2DInit&) \
    macro(Element*) \
    macro(HTMLImageElement*) \
    macro(ImageBitmap*) \
    macro(ImageData*) \
    macro(ImageDataSettings&) \
    macro(ImageSmoothingQuality) \
    macro(std::optional<float>&) \
    macro(std::optional<double>&) \
    macro(Path2D*) \
    macro(RefPtr<CanvasGradient>&) \
    macro(RefPtr<CanvasPattern>&) \
    macro(RefPtr<HTMLCanvasElement>&) \
    macro(RefPtr<HTMLImageElement>&) \
    macro(RefPtr<ImageBitmap>&) \
    macro(RefPtr<ImageData>&) \
    macro(RefPtr<JSC::ArrayBuffer>&) \
    macro(RefPtr<JSC::ArrayBufferView>&) \
    macro(RefPtr<JSC::Float32Array>&) \
    macro(RefPtr<JSC::Int32Array>&) \
    macro(RefPtr<JSC::Uint32Array>&) \
    macro(String&) \
    macro(Vector<String>&) \
    macro(Vector<float>&) \
    macro(Vector<double>&) \
    macro(Vector<uint32_t>&) \
    macro(Vector<int32_t>&) \
    macro(Vector<CanvasPath::RadiusVariant>&) \
    macro(double) \
    macro(float) \
    macro(uint64_t) \
    macro(int64_t) \
    macro(uint32_t) \
    macro(int32_t) \
    macro(uint8_t) \
    macro(bool) \
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_CSS_TYPED_OM_ARGUMENT(macro) \
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_OFFSCREEN_CANVAS_ARGUMENT(macro) \
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_VIDEO_ARGUMENT(macro) \
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_WEBGL_ARGUMENT(macro) \
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_WEBGL2_ARGUMENT(macro) \
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_VIDEOFRAME_ARGUMENT(macro) \
// end of FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_ARGUMENT

class InspectorCanvasCallTracer {
public:
    struct ProcessedArgument {
        Ref<JSON::Value> value;
        RecordingSwizzleType swizzleType;
    };

    using ProcessedArguments = std::initializer_list<std::optional<ProcessedArgument>>;

#define PROCESS_ARGUMENT_DECLARATION(ArgumentType) \
    static std::optional<ProcessedArgument> processArgument(CanvasRenderingContext&, ArgumentType); \
// end of PROCESS_ARGUMENT_DECLARATION
    FOR_EACH_INSPECTOR_CANVAS_CALL_TRACER_ARGUMENT(PROCESS_ARGUMENT_DECLARATION)
#undef PROCESS_ARGUMENT_DECLARATION

    static void recordAction(CanvasRenderingContext&, String&&, ProcessedArguments&& = { });

    static std::optional<ProcessedArgument> processArgument(const HTMLCanvasElement&, uint32_t);
    static void recordAction(const HTMLCanvasElement&, String&&, ProcessedArguments&& = { });
};

} // namespace WebCore
