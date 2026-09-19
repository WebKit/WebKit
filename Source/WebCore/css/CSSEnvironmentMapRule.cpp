/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CSSEnvironmentMapRule.h"

#if ENABLE(SPATIAL_PORTAL)

#include "CSSKeywordValue.h"
#include "CSSStringValue.h"
#include "CSSStyleSheet.h"
#include "CSSURLValue.h"
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

static std::optional<EnvironmentMapFormat> toEnvironmentMapFormat(const CSSValue* format)
{
    RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(format);
    if (!keywordValue)
        return std::nullopt;

    if (keywordValue->valueID() == CSSValueEquirectangular)
        return EnvironmentMapFormat::Equirectangular;
    return std::nullopt;
}

StyleRuleEnvironmentMap::StyleRuleEnvironmentMap(Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::EnvironmentMap)
    , m_properties(WTF::move(properties))
{
    if (RefPtr name = dynamicDowncast<CSSStringValue>(m_properties->getPropertyCSSValue(CSSPropertyName).get()))
        m_name = AtomString { name->string().value };

    m_format = toEnvironmentMapFormat(m_properties->getPropertyCSSValue(CSSPropertyFormat).get());

    if (RefPtr src = dynamicDowncast<CSSURLValue>(m_properties->getPropertyCSSValue(CSSPropertySrc).get()))
        m_src = src->url();
}

Ref<StyleRuleEnvironmentMap> StyleRuleEnvironmentMap::create(Ref<StyleProperties>&& properties)
{
    return adoptRef(*new StyleRuleEnvironmentMap(WTF::move(properties)));
}

Ref<StyleRuleEnvironmentMap> StyleRuleEnvironmentMap::copy() const
{
    return adoptRef(*new StyleRuleEnvironmentMap(*this));
}

StyleRuleEnvironmentMap::~StyleRuleEnvironmentMap() = default;

Ref<CSSEnvironmentMapRule> CSSEnvironmentMapRule::create(StyleRuleEnvironmentMap& rule, CSSStyleSheet* sheet)
{
    return adoptRef(*new CSSEnvironmentMapRule(rule, sheet));
}

CSSEnvironmentMapRule::CSSEnvironmentMapRule(StyleRuleEnvironmentMap& environmentMapRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_environmentMapRule(environmentMapRule)
{
}

CSSEnvironmentMapRule::~CSSEnvironmentMapRule() = default;

String CSSEnvironmentMapRule::name() const
{
    return m_environmentMapRule->name();
}

String CSSEnvironmentMapRule::format() const
{
    auto format = m_environmentMapRule->format();
    if (!format)
        return { };

    switch (*format) {
    case EnvironmentMapFormat::Equirectangular:
        return nameString(CSSValueEquirectangular);
    }

    RELEASE_ASSERT_NOT_REACHED();
}

String CSSEnvironmentMapRule::src() const
{
    return m_environmentMapRule->src().specified;
}

String CSSEnvironmentMapRule::cssText() const
{
    auto declarations = m_environmentMapRule->properties().asText(CSS::defaultSerializationContext());
    if (declarations.isEmpty())
        return "@environment-map { }"_s;

    return makeString("@environment-map { "_s, declarations, " }"_s);
}

void CSSEnvironmentMapRule::reattach(StyleRuleBase& rule)
{
    m_environmentMapRule = downcast<StyleRuleEnvironmentMap>(rule);
}

} // namespace WebCore

#endif // ENABLE(SPATIAL_PORTAL)
