/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "StyleRule.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

enum class ViewTransitionNavigation : bool {
    Auto,
    None,
};

class StyleRuleViewTransition final : public StyleRuleBase {
public:
    static Ref<StyleRuleViewTransition> create(Ref<StyleProperties>&&);
    ~StyleRuleViewTransition();

    Ref<StyleRuleViewTransition> copy() const { return adoptRef(*new StyleRuleViewTransition(*this)); }

    std::optional<ViewTransitionNavigation> navigation() const { return m_navigation; }
    ViewTransitionNavigation computedNavigation() const { return navigation().value_or(ViewTransitionNavigation::None); }
    Vector<AtomString> types() const { return m_types; }

private:
    explicit StyleRuleViewTransition(Ref<StyleProperties>&&);
    StyleRuleViewTransition(const StyleRuleViewTransition&) = default;

    std::optional<ViewTransitionNavigation> m_navigation;
    Vector<AtomString> m_types;
};

class CSSViewTransitionRule final : public CSSRule {
public:
    using ViewTransitionNavigation = WebCore::ViewTransitionNavigation;

    static Ref<CSSViewTransitionRule> create(StyleRuleViewTransition&, CSSStyleSheet*);
    virtual ~CSSViewTransitionRule();

    String cssText() const final;
    void reattach(StyleRuleBase&) final;
    StyleRuleType styleRuleType() const final { return StyleRuleType::ViewTransition; }

    ViewTransitionNavigation navigation() const { return m_viewTransitionRule->computedNavigation(); }
    Vector<AtomString> types() const { return m_viewTransitionRule->types(); }

private:
    CSSViewTransitionRule(StyleRuleViewTransition&, CSSStyleSheet* parent);

    Ref<StyleRuleViewTransition> m_viewTransitionRule;
};


} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSViewTransitionRule, StyleRuleType::ViewTransition)

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleViewTransition)
static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isViewTransitionRule(); }
SPECIALIZE_TYPE_TRAITS_END()
