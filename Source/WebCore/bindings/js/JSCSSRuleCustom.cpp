/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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
#include "JSCSSRuleCustom.h"

#include "CSSContainerRule.h"
#include "CSSCounterStyleRule.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFeatureValuesRule.h"
#include "CSSFontPaletteValuesRule.h"
#include "CSSFunctionDeclarations.h"
#include "CSSFunctionRule.h"
#include "CSSImportRule.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSLayerBlockRule.h"
#include "CSSLayerStatementRule.h"
#include "CSSMediaRule.h"
#include "CSSNamespaceRule.h"
#include "CSSNestedDeclarations.h"
#include "CSSPageRule.h"
#include "CSSPositionTryRule.h"
#include "CSSPropertyRule.h"
#include "CSSScopeRule.h"
#include "CSSStartingStyleRule.h"
#include "CSSStyleRule.h"
#include "CSSSupportsRule.h"
#include "CSSViewTransitionRule.h"
#include "JSCSSContainerRuleInlines.h"
#include "JSCSSCounterStyleRuleInlines.h"
#include "JSCSSFontFaceRuleInlines.h"
#include "JSCSSFontFeatureValuesRuleInlines.h"
#include "JSCSSFontPaletteValuesRuleInlines.h"
#include "JSCSSFunctionDeclarationsInlines.h"
#include "JSCSSFunctionRuleInlines.h"
#include "JSCSSImportRuleInlines.h"
#include "JSCSSKeyframeRuleInlines.h"
#include "JSCSSKeyframesRuleInlines.h"
#include "JSCSSLayerBlockRuleInlines.h"
#include "JSCSSLayerStatementRuleInlines.h"
#include "JSCSSMediaRuleInlines.h"
#include "JSCSSNamespaceRuleInlines.h"
#include "JSCSSNestedDeclarationsInlines.h"
#include "JSCSSPageRuleInlines.h"
#include "JSCSSPositionTryRuleInlines.h"
#include "JSCSSPropertyRuleInlines.h"
#include "JSCSSRuleInlines.h"
#include "JSCSSScopeRuleInlines.h"
#include "JSCSSStartingStyleRuleInlines.h"
#include "JSCSSStyleRuleInlines.h"
#include "JSCSSSupportsRuleInlines.h"
#include "JSCSSViewTransitionRuleInlines.h"
#include "JSNodeInlines.h"
#include "JSStyleSheetCustom.h"
#include "WebCoreOpaqueRootInlines.h"


namespace WebCore {
using namespace JSC;

template<typename Visitor>
void JSCSSRule::visitAdditionalChildren(Visitor& visitor)
{
    addWebCoreOpaqueRoot(visitor, wrapped());
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSCSSRule);

} // namespace WebCore
