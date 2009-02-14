/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "CanvasRenderingContext2D.h"

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasStyle.h"

#include "V8Binding.h"
#include "V8CanvasGradient.h"
#include "V8CanvasPattern.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

namespace WebCore {

static v8::Handle<v8::Value> toV8(CanvasStyle* style)
{
    if (style->canvasGradient())
        return V8Proxy::ToV8Object(V8ClassIndex::CANVASGRADIENT, style->canvasGradient());

    if (style->canvasPattern())
        return V8Proxy::ToV8Object(V8ClassIndex::CANVASPATTERN, style->canvasPattern());

    return v8String(style->color());
}

static PassRefPtr<CanvasStyle> toCanvasStyle(v8::Handle<v8::Value> value)
{
    if (value->IsString())
        return CanvasStyle::create(toWebCoreString(value));

    if (V8CanvasGradient::HasInstance(value))
        return CanvasStyle::create(V8Proxy::DOMWrapperToNative<CanvasGradient>(value));

    if (V8CanvasPattern::HasInstance(value))
        return CanvasStyle::create(V8Proxy::DOMWrapperToNative<CanvasPattern>(value));

    return 0;
}

ACCESSOR_GETTER(CanvasRenderingContext2DStrokeStyle)
{
    CanvasRenderingContext2D* impl = V8Proxy::DOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
    return toV8(impl->strokeStyle());
}

ACCESSOR_SETTER(CanvasRenderingContext2DStrokeStyle)
{
    CanvasRenderingContext2D* impl = V8Proxy::DOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
    impl->setStrokeStyle(toCanvasStyle(value));
}

ACCESSOR_GETTER(CanvasRenderingContext2DFillStyle)
{
    CanvasRenderingContext2D* impl = V8Proxy::DOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
    return toV8(impl->fillStyle());
}

ACCESSOR_SETTER(CanvasRenderingContext2DFillStyle)
{
    CanvasRenderingContext2D* impl = V8Proxy::DOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
    impl->setFillStyle(toCanvasStyle(value));
}

} // namespace WebCore
