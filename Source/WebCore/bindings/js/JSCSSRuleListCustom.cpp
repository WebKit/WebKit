/*
 * Copyright (C) 2009-2021 Apple Inc. All right reserved.
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
#include "JSCSSRuleList.h"

#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleSheet.h"
#include "JSCSSRuleCustom.h"
#include "JSStyleSheetCustom.h"


namespace WebCore {
using namespace JSC;

bool JSCSSRuleListOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, const char** reason)
{
    JSCSSRuleList* jsCSSRuleList = jsCast<JSCSSRuleList*>(handle.slot()->asCell());
    if (!jsCSSRuleList->hasCustomProperties(jsCSSRuleList->vm()))
        return false;

    if (CSSStyleSheet* styleSheet = jsCSSRuleList->wrapped().styleSheet()) {
        if (UNLIKELY(reason))
            *reason = "CSSStyleSheet is opaque root";

        return visitor.containsOpaqueRoot(root(styleSheet));
    }
    
    if (CSSRule* cssRule = jsCSSRuleList->wrapped().item(0)) {
        if (UNLIKELY(reason))
            *reason = "CSSRule is opaque root";

        return visitor.containsOpaqueRoot(root(cssRule));
    }
    return false;
}

}
