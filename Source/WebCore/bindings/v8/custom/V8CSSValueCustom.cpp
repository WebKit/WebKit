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

#if ENABLE(CSS_FILTERS)
#include "V8WebKitCSSFilterValue.h"
#endif

#if ENABLE(CSS_SHADERS)
#include "V8WebKitCSSMixFunctionValue.h"
#include "WebKitCSSMixFunctionValue.h"
#endif

#if ENABLE(SVG)
#include "V8SVGColor.h"
#include "V8SVGPaint.h"
#endif

namespace WebCore {

v8::Handle<v8::Object> wrap(CSSValue* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    if (impl->isWebKitCSSTransformValue())
        return wrap(static_cast<WebKitCSSTransformValue*>(impl), creationContext, isolate);
#if ENABLE(CSS_FILTERS)
    if (impl->isWebKitCSSFilterValue())
        return wrap(static_cast<WebKitCSSFilterValue*>(impl), creationContext, isolate);
#endif
#if ENABLE(CSS_SHADERS)
    if (impl->isWebKitCSSMixFunctionValue())
        return wrap(static_cast<WebKitCSSMixFunctionValue*>(impl), creationContext, isolate);
#endif
    if (impl->isValueList())
        return wrap(static_cast<CSSValueList*>(impl), creationContext, isolate);
    if (impl->isPrimitiveValue())
        return wrap(static_cast<CSSPrimitiveValue*>(impl), creationContext, isolate);
#if ENABLE(SVG)
    if (impl->isSVGPaint())
        return wrap(static_cast<SVGPaint*>(impl), creationContext, isolate);
    if (impl->isSVGColor())
        return wrap(static_cast<SVGColor*>(impl), creationContext, isolate);
#endif
    return V8CSSValue::createWrapper(impl, creationContext, isolate);
}

} // namespace WebCore
