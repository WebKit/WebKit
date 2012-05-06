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
#include "CSSRuleList.h"
#include "StyleRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
WebKitCSSRegionRule::WebKitCSSRegionRule(StyleRuleRegion* regionRule, CSSStyleSheet* parent)
    : CSSRule(parent, CSSRule::WEBKIT_REGION_RULE)
    , m_regionRule(regionRule)
    , m_childRuleCSSOMWrappers(regionRule->childRules().size())
{
}

WebKitCSSRegionRule::~WebKitCSSRegionRule()
{
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->setParentRule(0);
    }
}

String WebKitCSSRegionRule::cssText() const
{
    StringBuilder result;
    result.append("@-webkit-region ");

    // First add the selectors.
    result.append(m_regionRule->selectorList().selectorsText());

    // Then add the rules.
    result.append(" { \n");

    unsigned size = length();
    for (unsigned i = 0; i < size; ++i) {
        result.append("  ");
        result.append(item(i)->cssText());
        result.append("\n");
    }
    result.append("}");
    return result.toString();
}

unsigned WebKitCSSRegionRule::length() const
{ 
    return m_regionRule->childRules().size(); 
}

CSSRule* WebKitCSSRegionRule::item(unsigned index) const
{ 
    if (index >= length())
        return 0;
    ASSERT(m_childRuleCSSOMWrappers.size() == m_regionRule->childRules().size());
    RefPtr<CSSRule>& rule = m_childRuleCSSOMWrappers[index];
    if (!rule)
        rule = m_regionRule->childRules()[index]->createCSSOMWrapper(const_cast<WebKitCSSRegionRule*>(this));
    return rule.get();
}

CSSRuleList* WebKitCSSRegionRule::cssRules() const
{
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = adoptPtr(new LiveCSSRuleList<WebKitCSSRegionRule>(const_cast<WebKitCSSRegionRule*>(this)));
    return m_ruleListCSSOMWrapper.get();
}

void WebKitCSSRegionRule::reattach(StyleRuleRegion* rule)
{
    ASSERT(rule);
    m_regionRule = rule;
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->reattach(m_regionRule->childRules()[i].get());
    }
}

} // namespace WebCore
