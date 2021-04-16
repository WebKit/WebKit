/*
 * Copyright (C) 2021 Tyler Wilcock <twilco.o@protonmail.com>.
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
#include "CSSCounterStyleRule.h"

namespace WebCore {
    
StyleRuleCounterStyle::StyleRuleCounterStyle(const AtomString& name, Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::CounterStyle)
    , m_name(name)
    , m_properties(WTFMove(properties))
{
}

Ref<StyleRuleCounterStyle> StyleRuleCounterStyle::create(const AtomString& name, Ref<StyleProperties>&& properties)
{
    return adoptRef(*new StyleRuleCounterStyle(name, WTFMove(properties)));
}

StyleRuleCounterStyle::~StyleRuleCounterStyle() = default;

MutableStyleProperties& StyleRuleCounterStyle::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties.get()))
        m_properties = m_properties->mutableCopy();
    return downcast<MutableStyleProperties>(m_properties.get());
}

Ref<CSSCounterStyleRule> CSSCounterStyleRule::create(StyleRuleCounterStyle& rule, CSSStyleSheet* sheet)
{
    return adoptRef(*new CSSCounterStyleRule(rule, sheet));
}

CSSCounterStyleRule::CSSCounterStyleRule(StyleRuleCounterStyle& counterStyleRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_counterStyleRule(counterStyleRule)
{
}

CSSCounterStyleRule::~CSSCounterStyleRule() = default;

String CSSCounterStyleRule::cssText() const
{
    // FIXME: Implement this function when we parse @counter-style descriptors.
    return emptyString();
}

void CSSCounterStyleRule::reattach(StyleRuleBase& rule)
{
    m_counterStyleRule = static_cast<StyleRuleCounterStyle&>(rule);
}

} // namespace WebCore
