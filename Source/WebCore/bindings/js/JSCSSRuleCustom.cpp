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

#include "CSSCharsetRule.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "JSCSSCharsetRule.h"
#include "JSCSSFontFaceRule.h"
#include "JSCSSImportRule.h"
#include "JSCSSKeyframeRule.h"
#include "JSCSSKeyframesRule.h"
#include "JSCSSMediaRule.h"
#include "JSCSSPageRule.h"
#include "JSCSSStyleRule.h"
#include "JSCSSSupportsRule.h"
#include "JSNode.h"
#include "JSStyleSheetCustom.h"
#include "JSWebKitCSSRegionRule.h"
#include "JSWebKitCSSViewportRule.h"
#include "WebKitCSSRegionRule.h"
#include "WebKitCSSViewportRule.h"

using namespace JSC;

namespace WebCore {

void JSCSSRule::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(root(&wrapped()));
}

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<CSSRule>&& rule)
{
    switch (rule->type()) {
    case CSSRule::STYLE_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSStyleRule, WTFMove(rule));
    case CSSRule::MEDIA_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSMediaRule, WTFMove(rule));
    case CSSRule::FONT_FACE_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSFontFaceRule, WTFMove(rule));
    case CSSRule::PAGE_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSPageRule, WTFMove(rule));
    case CSSRule::IMPORT_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSImportRule, WTFMove(rule));
    case CSSRule::CHARSET_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSCharsetRule, WTFMove(rule));
    case CSSRule::KEYFRAME_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSKeyframeRule, WTFMove(rule));
    case CSSRule::KEYFRAMES_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSKeyframesRule, WTFMove(rule));
    case CSSRule::SUPPORTS_RULE:
        return CREATE_DOM_WRAPPER(globalObject, CSSSupportsRule, WTFMove(rule));
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case CSSRule::WEBKIT_VIEWPORT_RULE:
        return CREATE_DOM_WRAPPER(globalObject, WebKitCSSViewportRule, WTFMove(rule));
#endif
#if ENABLE(CSS_REGIONS)
    case CSSRule::WEBKIT_REGION_RULE:
        return CREATE_DOM_WRAPPER(globalObject, WebKitCSSRegionRule, WTFMove(rule));
#endif
    default:
        return CREATE_DOM_WRAPPER(globalObject, CSSRule, WTFMove(rule));
    }
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, CSSRule& object)
{
    return wrap(state, globalObject, object);
}

} // namespace WebCore
