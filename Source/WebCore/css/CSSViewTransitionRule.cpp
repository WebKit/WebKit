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

#include "config.h"
#include "CSSViewTransitionRule.h"

#include "CSSPropertyParser.h"
#include "CSSStyleSheet.h"
#include "CSSTokenizer.h"
#include "CSSValuePair.h"
#include "MutableStyleProperties.h"
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static std::optional<ViewTransitionNavigation> toViewTransitionNavigationEnum(RefPtr<CSSValue> navigation)
{
    if (!navigation || !navigation->isPrimitiveValue())
        return std::nullopt;

    auto& primitiveNavigationValue = downcast<CSSPrimitiveValue>(*navigation);
    ASSERT(primitiveNavigationValue.isValueID());

    if (primitiveNavigationValue.valueID() == CSSValueAuto)
        return ViewTransitionNavigation::Auto;
    return ViewTransitionNavigation::None;
}

StyleRuleViewTransition::StyleRuleViewTransition(Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::ViewTransition)
{
    m_navigation = toViewTransitionNavigationEnum(properties->getPropertyCSSValue(CSSPropertyNavigation));

    if (auto value = properties->getPropertyCSSValue(CSSPropertyTypes)) {
        auto processSingleValue = [&](const CSSValue& currentValue) {
            if (currentValue.isCustomIdent())
                m_types.append(currentValue.customIdent());
        };
        if (auto* list = dynamicDowncast<CSSValueList>(*value)) {
            for (auto& currentValue : *list)
                processSingleValue(currentValue);
        } else
            processSingleValue(*value);
    }
}

Ref<StyleRuleViewTransition> StyleRuleViewTransition::create(Ref<StyleProperties>&& properties)
{
    return adoptRef(*new StyleRuleViewTransition(WTFMove(properties)));
}

StyleRuleViewTransition::~StyleRuleViewTransition() = default;

Ref<CSSViewTransitionRule> CSSViewTransitionRule::create(StyleRuleViewTransition& rule, CSSStyleSheet* sheet)
{
    return adoptRef(*new CSSViewTransitionRule(rule, sheet));
}

CSSViewTransitionRule::CSSViewTransitionRule(StyleRuleViewTransition& viewTransitionRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_viewTransitionRule(viewTransitionRule)
{
}

CSSViewTransitionRule::~CSSViewTransitionRule() = default;

String CSSViewTransitionRule::cssText() const
{
    StringBuilder builder;
    builder.append("@view-transition { "_s);

    if (m_viewTransitionRule->navigation()) {
        builder.append("navigation: "_s);
        if (*m_viewTransitionRule->navigation() == ViewTransitionNavigation::Auto)
            builder.append("auto"_s);
        else
            builder.append("none"_s);
        builder.append("; "_s);
    }

    if (!types().isEmpty())
        builder.append("types:"_s);
    for (auto& type : types()) {
        builder.append(' ');
        builder.append(type);
    }
    if (!types().isEmpty())
        builder.append('}');

    builder.append('}');
    return builder.toString();
}

void CSSViewTransitionRule::reattach(StyleRuleBase& rule)
{
    m_viewTransitionRule = downcast<StyleRuleViewTransition>(rule);
}

} // namespace WebCore
