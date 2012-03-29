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
#include "WebKitCSSKeyframesRule.h"

#include "CSSParser.h"
#include "CSSRuleList.h"
#include "StylePropertySet.h"
#include "StyleSheet.h"
#include "WebKitCSSKeyframeRule.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WebKitCSSKeyframesRule::WebKitCSSKeyframesRule(CSSStyleSheet* parent)
    : CSSRule(parent, CSSRule::WEBKIT_KEYFRAMES_RULE)
{
}

WebKitCSSKeyframesRule::~WebKitCSSKeyframesRule()
{
    if (m_childRuleCSSOMWrappers) {
        ASSERT(m_childRuleCSSOMWrappers->size() == m_keyframes.size());
        for (unsigned i = 0; i < m_childRuleCSSOMWrappers->size(); ++i) {
            if (m_childRuleCSSOMWrappers->at(i))
                m_childRuleCSSOMWrappers->at(i)->setParentRule(0);
        }
    }
}

void WebKitCSSKeyframesRule::setName(const String& name)
{
    m_name = name;

    // Since the name is used in the keyframe map list in CSSStyleSelector, we need
    // to recompute the style sheet to get the updated name.
    if (CSSStyleSheet* styleSheet = parentStyleSheet())
        styleSheet->styleSheetChanged();
}

void WebKitCSSKeyframesRule::parserAppendKeyframe(PassRefPtr<StyleKeyframe> keyframe)
{
    ASSERT(!m_childRuleCSSOMWrappers);
    if (!keyframe)
        return;
    m_keyframes.append(keyframe);
}

void WebKitCSSKeyframesRule::insertRule(const String& ruleText)
{
    CSSParser p(useStrictParsing());
    RefPtr<StyleKeyframe> keyframe = p.parseKeyframeRule(parentStyleSheet(), ruleText);
    if (!keyframe)
        return;

    m_keyframes.append(keyframe);

    if (m_childRuleCSSOMWrappers)
        m_childRuleCSSOMWrappers->grow(m_keyframes.size());
}

void WebKitCSSKeyframesRule::deleteRule(const String& s)
{
    int i = findKeyframeIndex(s);
    if (i < 0)
        return;

    m_keyframes.remove(i);

    if (m_childRuleCSSOMWrappers) {
        if (m_childRuleCSSOMWrappers->at(i))
            m_childRuleCSSOMWrappers->at(i)->setParentRule(0);
        m_childRuleCSSOMWrappers->remove(i);
    }
}

WebKitCSSKeyframeRule* WebKitCSSKeyframesRule::findRule(const String& s)
{
    int i = findKeyframeIndex(s);
    return (i >= 0) ? item(i) : 0;
}

int WebKitCSSKeyframesRule::findKeyframeIndex(const String& key) const
{
    String percentageString;
    if (equalIgnoringCase(key, "from"))
        percentageString = "0%";
    else if (equalIgnoringCase(key, "to"))
        percentageString = "100%";
    else
        percentageString = key;

    for (unsigned i = 0; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i]->keyText() == percentageString)
            return i;
    }

    return -1;
}

String WebKitCSSKeyframesRule::cssText() const
{
    StringBuilder result;
    result.append("@-webkit-keyframes ");
    result.append(m_name);
    result.append(" { \n");

    for (unsigned i = 0; i < m_keyframes.size(); ++i) {
        result.append("  ");
        result.append(m_keyframes[i]->cssText());
        result.append("\n");
    }

    result.append("}");
    return result.toString();
}

WebKitCSSKeyframeRule* WebKitCSSKeyframesRule::item(unsigned index) const
{ 
    if (index >= m_keyframes.size())
        return 0;
    if (!m_childRuleCSSOMWrappers)
        m_childRuleCSSOMWrappers = adoptPtr(new Vector<RefPtr<WebKitCSSKeyframeRule> >(m_keyframes.size()));

    ASSERT(m_childRuleCSSOMWrappers->size() == m_keyframes.size());
    RefPtr<WebKitCSSKeyframeRule>& rule = m_childRuleCSSOMWrappers->at(index);
    if (!rule)
        rule = adoptRef(new WebKitCSSKeyframeRule(m_keyframes[index].get(), const_cast<WebKitCSSKeyframesRule*>(this)));

    return rule.get(); 
}

CSSRuleList* WebKitCSSKeyframesRule::cssRules()
{
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = adoptPtr(new LiveCSSRuleList<WebKitCSSKeyframesRule>(this));
    return m_ruleListCSSOMWrapper.get();
}

} // namespace WebCore
