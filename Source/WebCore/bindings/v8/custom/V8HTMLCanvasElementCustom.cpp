/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8HTMLCanvasElement.h"

#include "CanvasContextAttributes.h"
#include "CanvasRenderingContext.h"
#include "HTMLCanvasElement.h"
#include "InspectorCanvasInstrumentation.h"
#include "WebGLContextAttributes.h"
#include "V8Binding.h"
#include "V8CanvasRenderingContext2D.h"
#include "V8Node.h"
#if ENABLE(WEBGL)
#include "V8WebGLRenderingContext.h"
#endif
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

v8::Handle<v8::Value> V8HTMLCanvasElement::getContextCallback(const v8::Arguments& args)
{
    v8::Handle<v8::Object> holder = args.Holder();
    HTMLCanvasElement* imp = V8HTMLCanvasElement::toNative(holder);
    String contextId = toWebCoreString(args[0]);
    RefPtr<CanvasContextAttributes> attrs;
#if ENABLE(WEBGL)
    if (contextId == "experimental-webgl" || contextId == "webkit-3d") {
        attrs = WebGLContextAttributes::create();
        WebGLContextAttributes* webGLAttrs = static_cast<WebGLContextAttributes*>(attrs.get());
        if (args.Length() > 1 && args[1]->IsObject()) {
            v8::Handle<v8::Object> jsAttrs = args[1]->ToObject();
            v8::Handle<v8::String> alpha = v8::String::NewSymbol("alpha");
            if (jsAttrs->Has(alpha))
                webGLAttrs->setAlpha(jsAttrs->Get(alpha)->BooleanValue());
            v8::Handle<v8::String> depth = v8::String::NewSymbol("depth");
            if (jsAttrs->Has(depth))
                webGLAttrs->setDepth(jsAttrs->Get(depth)->BooleanValue());
            v8::Handle<v8::String> stencil = v8::String::NewSymbol("stencil");
            if (jsAttrs->Has(stencil))
                webGLAttrs->setStencil(jsAttrs->Get(stencil)->BooleanValue());
            v8::Handle<v8::String> antialias = v8::String::NewSymbol("antialias");
            if (jsAttrs->Has(antialias))
                webGLAttrs->setAntialias(jsAttrs->Get(antialias)->BooleanValue());
            v8::Handle<v8::String> premultipliedAlpha = v8::String::NewSymbol("premultipliedAlpha");
            if (jsAttrs->Has(premultipliedAlpha))
                webGLAttrs->setPremultipliedAlpha(jsAttrs->Get(premultipliedAlpha)->BooleanValue());
            v8::Handle<v8::String> preserveDrawingBuffer = v8::String::NewSymbol("preserveDrawingBuffer");
            if (jsAttrs->Has(preserveDrawingBuffer))
                webGLAttrs->setPreserveDrawingBuffer(jsAttrs->Get(preserveDrawingBuffer)->BooleanValue());
        }
    }
#endif
    CanvasRenderingContext* result = imp->getContext(contextId, attrs.get());
    if (!result)
        return v8Null(args.GetIsolate());
    else if (result->is2d()) {
        v8::Handle<v8::Value> v8Result = toV8(static_cast<CanvasRenderingContext2D*>(result), args.Holder(), args.GetIsolate());
        if (InspectorInstrumentation::canvasAgentEnabled(imp->document())) {
            ScriptState* scriptState = ScriptState::forContext(v8::Context::GetCurrent());
            ScriptObject context(scriptState, v8::Handle<v8::Object>::Cast(v8Result));
            ScriptObject wrapped = InspectorInstrumentation::wrapCanvas2DRenderingContextForInstrumentation(imp->document(), context);
            if (!wrapped.hasNoValue())
                return wrapped.v8Value();
        }
        return v8Result;
    }
#if ENABLE(WEBGL)
    else if (result->is3d()) {
        // 3D canvas contexts can hold on to lots of GPU resources, and we want to take an
        // opportunity to get rid of them as soon as possible when we navigate away from pages using
        // them.
        V8PerIsolateData* perIsolateData = V8PerIsolateData::from(args.GetIsolate());
        perIsolateData->setShouldCollectGarbageSoon();

        v8::Handle<v8::Value> v8Result = toV8(static_cast<WebGLRenderingContext*>(result), args.Holder(), args.GetIsolate());
        if (InspectorInstrumentation::canvasAgentEnabled(imp->document())) {
            ScriptState* scriptState = ScriptState::forContext(v8::Context::GetCurrent());
            ScriptObject glContext(scriptState, v8::Handle<v8::Object>::Cast(v8Result));
            ScriptObject wrapped = InspectorInstrumentation::wrapWebGLRenderingContextForInstrumentation(imp->document(), glContext);
            if (!wrapped.hasNoValue())
                return wrapped.v8Value();
        }
        return v8Result;
    }
#endif
    ASSERT_NOT_REACHED();
    return v8Null(args.GetIsolate());
}

v8::Handle<v8::Value> V8HTMLCanvasElement::toDataURLCallback(const v8::Arguments& args)
{
    v8::Handle<v8::Object> holder = args.Holder();
    HTMLCanvasElement* canvas = V8HTMLCanvasElement::toNative(holder);
    ExceptionCode ec = 0;

    String type = toWebCoreString(args[0]);
    double quality;
    double* qualityPtr = 0;
    if (args.Length() > 1 && args[1]->IsNumber()) {
        quality = args[1]->NumberValue();
        qualityPtr = &quality;
    }

    String result = canvas->toDataURL(type, qualityPtr, ec);
    setDOMException(ec, args.GetIsolate());
    return v8StringOrUndefined(result, args.GetIsolate());
}

} // namespace WebCore
