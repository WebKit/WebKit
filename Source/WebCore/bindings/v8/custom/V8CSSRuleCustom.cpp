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
#include "V8WebKitCSSKeyframeRule.h"
#include "V8WebKitCSSKeyframesRule.h"

namespace WebCore {

v8::Handle<v8::Value> toV8(CSSRule* impl)
{
    if (!impl)
        return v8::Null();
    switch (impl->type()) {
    case CSSRule::STYLE_RULE:
        return toV8(static_cast<CSSStyleRule*>(impl));
    case CSSRule::CHARSET_RULE:
        return toV8(static_cast<CSSCharsetRule*>(impl));
    case CSSRule::IMPORT_RULE:
        return toV8(static_cast<CSSImportRule*>(impl));
    case CSSRule::MEDIA_RULE:
        return toV8(static_cast<CSSMediaRule*>(impl));
    case CSSRule::FONT_FACE_RULE:
        return toV8(static_cast<CSSFontFaceRule*>(impl));
    case CSSRule::PAGE_RULE:
        return toV8(static_cast<CSSPageRule*>(impl));
    case CSSRule::WEBKIT_KEYFRAME_RULE:
        return toV8(static_cast<WebKitCSSKeyframeRule*>(impl));
    case CSSRule::WEBKIT_KEYFRAMES_RULE:
        return toV8(static_cast<WebKitCSSKeyframesRule*>(impl));
    }
    return V8CSSRule::wrap(impl);
}

} // namespace WebCore
