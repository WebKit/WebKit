/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSCSSRule.h"

#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSMediaRule.h"
#include "CSSNamespaceRule.h"
#include "CSSPageRule.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "JSCSSFontFaceRule.h"
#include "JSCSSImportRule.h"
#include "JSCSSKeyframeRule.h"
#include "JSCSSKeyframesRule.h"
#include "JSCSSMediaRule.h"
#include "JSCSSNamespaceRule.h"
#include "JSCSSPageRule.h"
#include "JSCSSStyleRule.h"
#include "JSCSSSupportsRule.h"
#include "JSNode.h"
#include "JSStyleSheetCustom.h"
#include "JSWebKitCSSViewportRule.h"
#include "WebKitCSSViewportRule.h"


namespace WebCore {
using namespace JSC;

void JSCSSRule::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(root(&wrapped()));
}

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<CSSRule>&& rule)
{
    switch (rule->type()) {
    case CSSRule::STYLE_RULE:
        return createWrapper<CSSStyleRule>(globalObject, WTFMove(rule));
    case CSSRule::MEDIA_RULE:
        return createWrapper<CSSMediaRule>(globalObject, WTFMove(rule));
    case CSSRule::FONT_FACE_RULE:
        return createWrapper<CSSFontFaceRule>(globalObject, WTFMove(rule));
    case CSSRule::PAGE_RULE:
        return createWrapper<CSSPageRule>(globalObject, WTFMove(rule));
    case CSSRule::IMPORT_RULE:
        return createWrapper<CSSImportRule>(globalObject, WTFMove(rule));
    case CSSRule::NAMESPACE_RULE:
        return createWrapper<CSSNamespaceRule>(globalObject, WTFMove(rule));
    case CSSRule::KEYFRAME_RULE:
        return createWrapper<CSSKeyframeRule>(globalObject, WTFMove(rule));
    case CSSRule::KEYFRAMES_RULE:
        return createWrapper<CSSKeyframesRule>(globalObject, WTFMove(rule));
    case CSSRule::SUPPORTS_RULE:
        return createWrapper<CSSSupportsRule>(globalObject, WTFMove(rule));
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case CSSRule::WEBKIT_VIEWPORT_RULE:
        return createWrapper<WebKitCSSViewportRule>(globalObject, WTFMove(rule));
#endif
    default:
        return createWrapper<CSSRule>(globalObject, WTFMove(rule));
    }
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, CSSRule& object)
{
    return wrap(state, globalObject, object);
}

} // namespace WebCore
