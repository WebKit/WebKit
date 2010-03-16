/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "WebGLContextAttributes.h"
#include "V8Binding.h"
#include "V8CanvasRenderingContext2D.h"
#include "V8Node.h"
#include "V8Proxy.h"
#if ENABLE(3D_CANVAS)
#include "V8WebGLRenderingContext.h"
#endif

namespace WebCore {

v8::Handle<v8::Value> V8HTMLCanvasElement::getContextCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.HTMLCanvasElement.context");
    v8::Handle<v8::Object> holder = args.Holder();
    HTMLCanvasElement* imp = V8HTMLCanvasElement::toNative(holder);
    String contextId = toWebCoreString(args[0]);
    RefPtr<CanvasContextAttributes> attrs;
#if ENABLE(3D_CANVAS)
    if (contextId == "experimental-webgl" || contextId == "webkit-3d") {
        attrs = WebGLContextAttributes::create();
        WebGLContextAttributes* webGLAttrs = static_cast<WebGLContextAttributes*>(attrs.get());
        if (args.Length() > 1 && args[1]->IsObject()) {
            v8::Handle<v8::Object> jsAttrs = args[1]->ToObject();
            v8::Handle<v8::String> alpha = v8::String::New("alpha");
            if (jsAttrs->Has(alpha))
                webGLAttrs->setAlpha(jsAttrs->Get(alpha)->BooleanValue());
            v8::Handle<v8::String> depth = v8::String::New("depth");
            if (jsAttrs->Has(depth))
                webGLAttrs->setDepth(jsAttrs->Get(depth)->BooleanValue());
            v8::Handle<v8::String> stencil = v8::String::New("stencil");
            if (jsAttrs->Has(stencil))
                webGLAttrs->setStencil(jsAttrs->Get(stencil)->BooleanValue());
            v8::Handle<v8::String> antialias = v8::String::New("antialias");
            if (jsAttrs->Has(antialias))
                webGLAttrs->setAntialias(jsAttrs->Get(antialias)->BooleanValue());
            v8::Handle<v8::String> premultipliedAlpha = v8::String::New("premultipliedAlpha");
            if (jsAttrs->Has(premultipliedAlpha))
                webGLAttrs->setPremultipliedAlpha(jsAttrs->Get(premultipliedAlpha)->BooleanValue());
        }
    }
#endif
    CanvasRenderingContext* result = imp->getContext(contextId, attrs.get());
    if (!result)
        return v8::Undefined();
    if (result->is2d())
        return toV8(static_cast<CanvasRenderingContext2D*>(result));
#if ENABLE(3D_CANVAS)
    else if (result->is3d())
        return toV8(static_cast<WebGLRenderingContext*>(result));
#endif
    ASSERT_NOT_REACHED();
    return v8::Undefined();
}

} // namespace WebCore
