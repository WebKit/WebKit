/*
 * Copyright 2011 Adobe Systems Incorporated. All Rights Reserved.
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

namespace WebCore {
WebKitCSSRegionRule::WebKitCSSRegionRule(CSSStyleSheet* parent, Vector<OwnPtr<CSSParserSelector> >* selectors, PassRefPtr<CSSRuleList> rules)
    : CSSRule(parent, CSSRule::WEBKIT_REGION_RULE)
    , m_ruleList(rules)
{
    for (unsigned index = 0; index < m_ruleList->length(); ++index)
        m_ruleList->item(index)->setParentRule(this);

    m_selectorList.adoptSelectorVector(*selectors);
}

WebKitCSSRegionRule::~WebKitCSSRegionRule()
{
    for (unsigned index = 0; index < m_ruleList->length(); ++index)
        m_ruleList->item(index)->setParentRule(0);
}

String WebKitCSSRegionRule::cssText() const
{
    String result = "@-webkit-region ";

    // First add the selectors.
    result += m_selectorList.selectorsText();

    // Then add the rules.
    result += " { \n";

    if (m_ruleList)
        result += m_ruleList->rulesText();

    result += "}";
    return result;
}

} // namespace WebCore
