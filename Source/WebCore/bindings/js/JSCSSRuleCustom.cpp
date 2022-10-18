/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
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

#include "CSSContainerRule.h"
#include "CSSCounterStyleRule.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFeatureValuesRule.h"
#include "CSSFontPaletteValuesRule.h"
#include "CSSImportRule.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSLayerBlockRule.h"
#include "CSSLayerStatementRule.h"
#include "CSSMediaRule.h"
#include "CSSNamespaceRule.h"
#include "CSSPageRule.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "JSCSSContainerRule.h"
#include "JSCSSCounterStyleRule.h"
#include "JSCSSFontFaceRule.h"
#include "JSCSSFontFeatureValuesRule.h"
#include "JSCSSFontPaletteValuesRule.h"
#include "JSCSSImportRule.h"
#include "JSCSSKeyframeRule.h"
#include "JSCSSKeyframesRule.h"
#include "JSCSSLayerBlockRule.h"
#include "JSCSSLayerStatementRule.h"
#include "JSCSSMediaRule.h"
#include "JSCSSNamespaceRule.h"
#include "JSCSSPageRule.h"
#include "JSCSSStyleRule.h"
#include "JSCSSSupportsRule.h"
#include "JSNode.h"
#include "JSStyleSheetCustom.h"
#include "WebCoreOpaqueRoot.h"


namespace WebCore {
using namespace JSC;

template<typename Visitor>
void JSCSSRule::visitAdditionalChildren(Visitor& visitor)
{
    addWebCoreOpaqueRoot(visitor, wrapped());
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSCSSRule);

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<CSSRule>&& rule)
{
    switch (rule->styleRuleType()) {
    case StyleRuleType::Style:
        return createWrapper<CSSStyleRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Media:
        return createWrapper<CSSMediaRule>(globalObject, WTFMove(rule));
    case StyleRuleType::FontFace:
        return createWrapper<CSSFontFaceRule>(globalObject, WTFMove(rule));
    case StyleRuleType::FontPaletteValues:
        return createWrapper<CSSFontPaletteValuesRule>(globalObject, WTFMove(rule));
    case StyleRuleType::FontFeatureValues:
        return createWrapper<CSSFontFeatureValuesRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Page:
        return createWrapper<CSSPageRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Import:
        return createWrapper<CSSImportRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Namespace:
        return createWrapper<CSSNamespaceRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Keyframe:
        return createWrapper<CSSKeyframeRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Keyframes:
        return createWrapper<CSSKeyframesRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Supports:
        return createWrapper<CSSSupportsRule>(globalObject, WTFMove(rule));
    case StyleRuleType::CounterStyle:
        return createWrapper<CSSCounterStyleRule>(globalObject, WTFMove(rule));
    case StyleRuleType::LayerBlock:
        return createWrapper<CSSLayerBlockRule>(globalObject, WTFMove(rule));
    case StyleRuleType::LayerStatement:
        return createWrapper<CSSLayerStatementRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Container:
        return createWrapper<CSSContainerRule>(globalObject, WTFMove(rule));
    case StyleRuleType::Unknown:
    case StyleRuleType::Charset:
    case StyleRuleType::Margin:
    case StyleRuleType::FontFeatureValuesBlock:
        return createWrapper<CSSRule>(globalObject, WTFMove(rule));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, CSSRule& object)
{
    return wrap(lexicalGlobalObject, globalObject, object);
}

} // namespace WebCore
