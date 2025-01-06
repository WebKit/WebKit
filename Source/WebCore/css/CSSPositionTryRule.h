/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSRule.h"
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"
#include <wtf/TypeCasts.h>

namespace WebCore {

class CSSStyleDeclaration;
class StyleRuleCSSStyleDeclaration;

class StyleRulePositionTry final : public StyleRuleBase {
public:
    static Ref<StyleRulePositionTry> create(AtomString&& name, Ref<StyleProperties>&&);

    Ref<StyleRulePositionTry> copy() const { return adoptRef(*new StyleRulePositionTry(*this)); }

    AtomString name() const { return m_name; }

    Ref<StyleProperties> protectedProperties() const { return m_properties; }
    Ref<MutableStyleProperties> protectedMutableProperties();

private:
    explicit StyleRulePositionTry(AtomString&& name, Ref<StyleProperties>&&);

    AtomString m_name;
    Ref<StyleProperties> m_properties;
};

class CSSPositionTryRule final : public CSSRule {
public:
    static Ref<CSSPositionTryRule> create(StyleRulePositionTry&, CSSStyleSheet*);
    virtual ~CSSPositionTryRule();

    StyleRuleType styleRuleType() const { return StyleRuleType::PositionTry; }

    String cssText() const;
    void reattach(StyleRuleBase&);

    Ref<StyleRulePositionTry> protectedPositionTryRule() const { return m_positionTryRule; }

    WEBCORE_EXPORT AtomString name() const;
    WEBCORE_EXPORT CSSStyleDeclaration& style();

private:
    CSSPositionTryRule(StyleRulePositionTry&, CSSStyleSheet*);

    Ref<StyleRulePositionTry> m_positionTryRule;
    RefPtr<StyleRuleCSSStyleDeclaration> m_propertiesCSSOMWrapper;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRulePositionTry)
static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isPositionTryRule(); }
SPECIALIZE_TYPE_TRAITS_END()
