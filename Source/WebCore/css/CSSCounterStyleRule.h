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

#pragma once

#include "CSSRule.h"
#include "StyleProperties.h"
#include "StyleRule.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

class StyleRuleCounterStyle final : public StyleRuleBase {
public:
    static Ref<StyleRuleCounterStyle> create(const AtomString& name, Ref<StyleProperties>&&);
    ~StyleRuleCounterStyle();

    const StyleProperties& properties() const { return m_properties; }
    MutableStyleProperties& mutableProperties();

    const AtomString& name() const { return m_name; }

private:
    explicit StyleRuleCounterStyle(const AtomString&, Ref<StyleProperties>&&);

    AtomString m_name;
    Ref<StyleProperties> m_properties;
};

class CSSCounterStyleRule final : public CSSRule {
public:
    static Ref<CSSCounterStyleRule> create(StyleRuleCounterStyle&, CSSStyleSheet*);
    virtual ~CSSCounterStyleRule();

    String cssText() const final;
    void reattach(StyleRuleBase&) final;
    CSSRule::Type type() const final { return COUNTER_STYLE_RULE; }

    String name() const { return m_counterStyleRule->name(); }
    // FIXME: Implement after we parse @counter-style descriptors.
    String system() const { return emptyString(); }
    String negative() const { return emptyString(); }
    String prefix() const { return emptyString(); }
    String suffix() const { return emptyString(); }
    String range() const { return emptyString(); }
    String pad() const { return emptyString(); }
    String fallback() const { return emptyString(); }
    String symbols() const { return emptyString(); }
    String additiveSymbols() const { return emptyString(); }
    String speakAs() const { return emptyString(); }

    // FIXME: Implement after we parse @counter-style descriptors.
    void setName(const String&) { }
    void setSystem(const String&) { }
    void setNegative(const String&) { }
    void setPrefix(const String&) { }
    void setSuffix(const String&) { }
    void setRange(const String&) { }
    void setPad(const String&) { }
    void setFallback(const String&) { }
    void setSymbols(const String&) { }
    void setAdditiveSymbols(const String&) { }
    void setSpeakAs(const String&) { }

private:
    CSSCounterStyleRule(StyleRuleCounterStyle&, CSSStyleSheet* parent);

    Ref<StyleRuleCounterStyle> m_counterStyleRule;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSCounterStyleRule, CSSRule::COUNTER_STYLE_RULE)

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleCounterStyle)
static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isCounterStyleRule(); }
SPECIALIZE_TYPE_TRAITS_END()

