/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "V8CSSValue.h"

#include "V8CSSPrimitiveValue.h"
#include "V8CSSValueList.h"
#include "V8WebKitCSSTransformValue.h"

#if ENABLE(SVG)
#include "V8SVGColor.h"
#include "V8SVGPaint.h"
#endif

namespace WebCore {

v8::Handle<v8::Value> toV8(CSSValue* impl)
{
    if (!impl)
        return v8::Null();
    if (impl->isWebKitCSSTransformValue())
        return toV8(static_cast<WebKitCSSTransformValue*>(impl));
    if (impl->isValueList())
        return toV8(static_cast<CSSValueList*>(impl));
    if (impl->isPrimitiveValue())
        return toV8(static_cast<CSSPrimitiveValue*>(impl));
#if ENABLE(SVG)
    if (impl->isSVGPaint())
        return toV8(static_cast<SVGPaint*>(impl));
    if (impl->isSVGColor())
        return toV8(static_cast<SVGColor*>(impl));
#endif
    return V8CSSValue::wrap(impl);
}

} // namespace WebCore
