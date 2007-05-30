/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSRule.h"
#include "CSSStyleRule.h"
#include "JSCSSCharsetRule.h"
#include "JSCSSFontFaceRule.h"
#include "JSCSSImportRule.h"
#include "JSCSSMediaRule.h"
#include "JSCSSPageRule.h"
#include "JSCSSStyleRule.h"
#include "kjs_binding.h"

namespace WebCore {

KJS::JSValue* toJS(KJS::ExecState* exec, CSSRule* rule)
{
    if (!rule)
        return KJS::jsNull();

    KJS::ScriptInterpreter* interp = static_cast<KJS::ScriptInterpreter*>(exec->dynamicInterpreter());
    KJS::DOMObject* ret = interp->getDOMObject(rule);

    if (ret)
        return ret;

    switch (rule->type()) {
        case CSSRule::STYLE_RULE:
            ret = new JSCSSStyleRule(exec, static_cast<CSSStyleRule*>(rule));
            break;
        case CSSRule::MEDIA_RULE:
            ret = new JSCSSMediaRule(exec, static_cast<CSSMediaRule*>(rule));
            break;
        case CSSRule::FONT_FACE_RULE:
            ret = new JSCSSFontFaceRule(exec, static_cast<CSSFontFaceRule*>(rule));
            break;
        case CSSRule::PAGE_RULE:
            ret = new JSCSSPageRule(exec, static_cast<CSSPageRule*>(rule));
            break;
        case CSSRule::IMPORT_RULE:
            ret = new JSCSSImportRule(exec, static_cast<CSSImportRule*>(rule));
            break;
        case CSSRule::CHARSET_RULE:
            ret = new JSCSSCharsetRule(exec, static_cast<CSSCharsetRule*>(rule));
            break;
        default:
            ret = new JSCSSRule(exec, rule);
            break;
    }

    interp->putDOMObject(rule, ret);
    return ret;
}

} // namespace WebCore
