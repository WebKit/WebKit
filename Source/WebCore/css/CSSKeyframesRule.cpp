/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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
#include "CSSKeyframesRule.h"

#include "CSSKeyframeRule.h"
#include "CSSParser.h"
#include "CSSRuleList.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

StyleRuleKeyframes::StyleRuleKeyframes(const AtomString& name)
    : StyleRuleBase(StyleRuleType::Keyframes)
    , m_name(name)
{
}

StyleRuleKeyframes::StyleRuleKeyframes(const StyleRuleKeyframes& o)
    : StyleRuleBase(o)
    , m_keyframes(o.keyframes())
    , m_name(o.m_name)
{
}

Ref<StyleRuleKeyframes> StyleRuleKeyframes::create(const AtomString& name)
{
    return adoptRef(*new StyleRuleKeyframes(name));
}

StyleRuleKeyframes::~StyleRuleKeyframes() = default;

const Vector<Ref<StyleRuleKeyframe>>& StyleRuleKeyframes::keyframes() const
{
    return m_keyframes;
}

void StyleRuleKeyframes::parserAppendKeyframe(RefPtr<StyleRuleKeyframe>&& keyframe)
{
    if (!keyframe)
        return;
    m_keyframes.append(keyframe.releaseNonNull());
}

void StyleRuleKeyframes::wrapperAppendKeyframe(Ref<StyleRuleKeyframe>&& keyframe)
{
    m_keyframes.append(WTFMove(keyframe));
}

void StyleRuleKeyframes::wrapperRemoveKeyframe(unsigned index)
{
    m_keyframes.remove(index);
}

std::optional<size_t> StyleRuleKeyframes::findKeyframeIndex(const String& key) const
{
    auto keys = CSSParser::parseKeyframeKeyList(key);
    if (keys.isEmpty())
        return std::nullopt;
    for (auto i = m_keyframes.size(); i--; ) {
        if (m_keyframes[i]->keys() == keys)
            return i;
    }
    return std::nullopt;
}

void StyleRuleKeyframes::shrinkToFit()
{
    m_keyframes.shrinkToFit();
}

CSSKeyframesRule::CSSKeyframesRule(StyleRuleKeyframes& keyframesRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_keyframesRule(keyframesRule)
    , m_childRuleCSSOMWrappers(keyframesRule.keyframes().size())
{
}

CSSKeyframesRule::~CSSKeyframesRule()
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_keyframesRule->keyframes().size());

    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->setParentRule(0);
    }
}

void CSSKeyframesRule::setName(const AtomString& name)
{
    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_keyframesRule->setName(name);
}

void CSSKeyframesRule::appendRule(const String& ruleText)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_keyframesRule->keyframes().size());

    CSSParser parser(parserContext());
    RefPtr<StyleRuleKeyframe> keyframe = parser.parseKeyframeRule(ruleText);
    if (!keyframe)
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_keyframesRule->wrapperAppendKeyframe(keyframe.releaseNonNull());

    m_childRuleCSSOMWrappers.grow(length());
}

void CSSKeyframesRule::insertRule(const String& ruleText)
{
    if (CSSStyleSheet* parent = parentStyleSheet()) {
        if (Document* ownerDocument = parent->ownerDocument())
            ownerDocument->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "CSSKeyframesRule 'insertRule' function is deprecated.  Use 'appendRule' instead."_s);
    }
    appendRule(ruleText);
}

void CSSKeyframesRule::deleteRule(const String& s)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_keyframesRule->keyframes().size());

    auto i = m_keyframesRule->findKeyframeIndex(s);
    if (!i)
        return;

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_keyframesRule->wrapperRemoveKeyframe(*i);

    if (m_childRuleCSSOMWrappers[*i])
        m_childRuleCSSOMWrappers[*i]->setParentRule(nullptr);
    m_childRuleCSSOMWrappers.remove(*i);
}

CSSKeyframeRule* CSSKeyframesRule::findRule(const String& s)
{
    auto i = m_keyframesRule->findKeyframeIndex(s);
    return i ? item(*i) : nullptr;
}

String CSSKeyframesRule::cssText() const
{
    StringBuilder result;
    result.append("@keyframes ", name(), " { \n");
    for (unsigned i = 0, size = length(); i < size; ++i)
        result.append("  ", m_keyframesRule->keyframes()[i]->cssText(), '\n');
    result.append('}');
    return result.toString();
}

unsigned CSSKeyframesRule::length() const
{ 
    return m_keyframesRule->keyframes().size(); 
}

CSSKeyframeRule* CSSKeyframesRule::item(unsigned index) const
{ 
    if (index >= length())
        return nullptr;
    ASSERT(m_childRuleCSSOMWrappers.size() == m_keyframesRule->keyframes().size());
    auto& rule = m_childRuleCSSOMWrappers[index];
    if (!rule)
        rule = adoptRef(*new CSSKeyframeRule(m_keyframesRule->keyframes()[index], const_cast<CSSKeyframesRule*>(this)));
    return rule.get(); 
}

CSSRuleList& CSSKeyframesRule::cssRules()
{
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = makeUnique<LiveCSSRuleList<CSSKeyframesRule>>(*this);
    return *m_ruleListCSSOMWrapper;
}

void CSSKeyframesRule::reattach(StyleRuleBase& rule)
{
    m_keyframesRule = downcast<StyleRuleKeyframes>(rule);
}

} // namespace WebCore
