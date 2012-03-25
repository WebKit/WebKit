/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include "WebKitCSSRegionRule.h"

#include "CSSParser.h"
#include "CSSParserValues.h"
#include "CSSRuleList.h"
#include "Document.h"
#include "ExceptionCode.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
WebKitCSSRegionRule::WebKitCSSRegionRule(CSSStyleSheet* parent, Vector<OwnPtr<CSSParserSelector> >* selectors, Vector<RefPtr<CSSRule> >& rules)
    : CSSRule(parent, CSSRule::WEBKIT_REGION_RULE)
{
    m_selectorList.adoptSelectorVector(*selectors);
    m_childRules.swap(rules);
    
    for (unsigned i = 0; i < m_childRules.size(); ++i)
        m_childRules[i]->setParentRule(this);
}

WebKitCSSRegionRule::~WebKitCSSRegionRule()
{
    for (unsigned i = 0; i < m_childRules.size(); ++i)
        m_childRules[i]->setParentRule(0);
}

String WebKitCSSRegionRule::cssText() const
{
    StringBuilder result;
    result.append("@-webkit-region ");

    // First add the selectors.
    result.append(m_selectorList.selectorsText());

    // Then add the rules.
    result.append(" { \n");

    for (unsigned i = 0; i < m_childRules.size(); ++i) {
        result.append("  ");
        result.append(m_childRules[i]->cssText());
        result.append("\n");
    }
    result.append("}");
    return result.toString();
}

CSSRuleList* WebKitCSSRegionRule::cssRules()
{
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = adoptPtr(new LiveCSSRuleList<WebKitCSSRegionRule>(this));
    return m_ruleListCSSOMWrapper.get();
}

} // namespace WebCore
