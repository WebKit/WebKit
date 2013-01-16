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
#include "V8CSSRule.h"

#include "V8CSSCharsetRule.h"
#include "V8CSSFontFaceRule.h"
#include "V8CSSImportRule.h"
#include "V8CSSMediaRule.h"
#include "V8CSSPageRule.h"
#include "V8CSSStyleRule.h"
#if ENABLE(CSS3_CONDITIONAL_RULES)
#include "V8CSSSupportsRule.h"
#endif
#include "V8WebKitCSSKeyframeRule.h"
#include "V8WebKitCSSKeyframesRule.h"
#include "V8WebKitCSSRegionRule.h"

#if ENABLE(CSS_DEVICE_ADAPTATION)
#include "V8WebKitCSSViewportRule.h"
#endif

namespace WebCore {

v8::Handle<v8::Object> wrap(CSSRule* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    switch (impl->type()) {
    case CSSRule::UNKNOWN_RULE:
        // CSSUnknownRule.idl is explicitly excluded as it doesn't add anything
        // over CSSRule.idl (see WebCore.gyp/WebCore.gyp: 'bindings_idl_files').
        // -> Use the base class wrapper here.
        return V8CSSRule::createWrapper(impl, creationContext, isolate);
    case CSSRule::STYLE_RULE:
        return wrap(static_cast<CSSStyleRule*>(impl), creationContext, isolate);
    case CSSRule::CHARSET_RULE:
        return wrap(static_cast<CSSCharsetRule*>(impl), creationContext, isolate);
    case CSSRule::IMPORT_RULE:
        return wrap(static_cast<CSSImportRule*>(impl), creationContext, isolate);
    case CSSRule::MEDIA_RULE:
        return wrap(static_cast<CSSMediaRule*>(impl), creationContext, isolate);
    case CSSRule::FONT_FACE_RULE:
        return wrap(static_cast<CSSFontFaceRule*>(impl), creationContext, isolate);
    case CSSRule::PAGE_RULE:
        return wrap(static_cast<CSSPageRule*>(impl), creationContext, isolate);
    case CSSRule::WEBKIT_KEYFRAME_RULE:
        return wrap(static_cast<WebKitCSSKeyframeRule*>(impl), creationContext, isolate);
    case CSSRule::WEBKIT_KEYFRAMES_RULE:
        return wrap(static_cast<WebKitCSSKeyframesRule*>(impl), creationContext, isolate);
#if ENABLE(CSS3_CONDITIONAL_RULES)
    case CSSRule::SUPPORTS_RULE:
        return wrap(static_cast<CSSSupportsRule*>(impl), creationContext, isolate);
#endif
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case CSSRule::WEBKIT_VIEWPORT_RULE:
        return wrap(static_cast<WebKitCSSViewportRule*>(impl), creationContext, isolate);
#endif
    case CSSRule::WEBKIT_REGION_RULE:
        return wrap(static_cast<WebKitCSSRegionRule*>(impl), creationContext, isolate);
    }
    return V8CSSRule::createWrapper(impl, creationContext, isolate);
}

} // namespace WebCore
