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

#include "CSSParser.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSKeyframeRule.h"
#include "CSSRuleList.h"
#include "StyleSheet.h"

namespace WebCore {

WebKitCSSKeyframesRule::WebKitCSSKeyframesRule(CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_lstCSSRules(CSSRuleList::create())
{
}

WebKitCSSKeyframesRule::~WebKitCSSKeyframesRule()
{
    int length = m_lstCSSRules->length();
    if (length == 0)
        return;
        
    for (int i = 0; i < length; i++)
        m_lstCSSRules->item(i)->setParent(0);
}

String WebKitCSSKeyframesRule::name() const
{
    return m_name;
}

void WebKitCSSKeyframesRule::setName(const String& name, ExceptionCode& /*ec*/)
{
    m_name = name;
}

void WebKitCSSKeyframesRule::setName(String name)
{
    m_name = name;
}

unsigned WebKitCSSKeyframesRule::length() const
{
    return m_lstCSSRules.get()->length();
}

WebKitCSSKeyframeRule* WebKitCSSKeyframesRule::item(unsigned index)
{
    CSSRule* rule = m_lstCSSRules.get()->item(index);
    return (rule && rule->isKeyframeRule()) ? static_cast<WebKitCSSKeyframeRule*>(rule) : 0;
}

const WebKitCSSKeyframeRule* WebKitCSSKeyframesRule::item(unsigned index) const
{
    CSSRule* rule = m_lstCSSRules.get()->item(index);
    return (rule && rule->isKeyframeRule()) ? static_cast<const WebKitCSSKeyframeRule*>(rule) : 0;
}

void WebKitCSSKeyframesRule::insert(WebKitCSSKeyframeRule* rule)
{
    float key = rule->keyAsPercent();

    for (unsigned i = 0; i < length(); ++i) {
        if (item(i)->keyAsPercent() == key) {
            m_lstCSSRules.get()->deleteRule(i);
            m_lstCSSRules.get()->insertRule(rule, i);
            return;
        }
        if (item(i)->keyAsPercent() > key) {
            // insert before
            m_lstCSSRules.get()->insertRule(rule, i);
            return;
        }
    }
    
    // append
    m_lstCSSRules.get()->append(rule);
    stylesheet()->styleSheetChanged();
}

void WebKitCSSKeyframesRule::insertRule(const String& rule)
{
    CSSParser p(useStrictParsing());
    RefPtr<CSSRule> newRule = p.parseKeyframeRule(parentStyleSheet(), rule);
    if (newRule.get() && newRule.get()->isKeyframeRule())
        insert(static_cast<WebKitCSSKeyframeRule*>(newRule.get()));
}

void WebKitCSSKeyframesRule::deleteRule(const String& s)
{
    float key = WebKitCSSKeyframeRule::keyStringToFloat(s);
    int i = findRuleIndex(key);
    if (i >= 0) {
        m_lstCSSRules.get()->deleteRule(i);
        stylesheet()->styleSheetChanged();
    }
}

WebKitCSSKeyframeRule* WebKitCSSKeyframesRule::findRule(const String& s)
{
    float key = WebKitCSSKeyframeRule::keyStringToFloat(s);
    int i = findRuleIndex(key);
    return (i >= 0) ? item(i) : 0;
}

int WebKitCSSKeyframesRule::findRuleIndex(float key) const
{
    for (unsigned i = 0; i < length(); ++i) {
        if (item(i)->keyAsPercent() == key)
            return i;
    }

    return -1;
}

String WebKitCSSKeyframesRule::cssText() const
{
    String result = "@-webkit-keyframes ";
    result += m_name;
    result += " { \n";

    if (m_lstCSSRules) {
        unsigned len = m_lstCSSRules->length();
        for (unsigned i = 0; i < len; i++) {
            result += "  ";
            result += m_lstCSSRules->item(i)->cssText();
            result += "\n";
        }
    }

    result += "}";
    return result;
}

} // namespace WebCore
