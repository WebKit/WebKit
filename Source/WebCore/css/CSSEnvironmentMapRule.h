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

#pragma once

#if ENABLE(SPATIAL_PORTAL)

#include "CSSRule.h"
#include "CSSURL.h"
#include "StyleProperties.h"
#include "StyleRule.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

enum class EnvironmentMapFormat : bool {
    Equirectangular,
};

class StyleRuleEnvironmentMap final : public StyleRuleBase {
public:
    static Ref<StyleRuleEnvironmentMap> create(Ref<StyleProperties>&&);
    ~StyleRuleEnvironmentMap();

    Ref<StyleRuleEnvironmentMap> copy() const;

    bool isUsable() const { return !m_name.isEmpty() && m_format && !m_src.isNone(); }

    const AtomString& name() const { return m_name; }
    std::optional<EnvironmentMapFormat> format() const { return m_format; }
    const CSS::URL& src() const LIFETIME_BOUND { return m_src; }

    const StyleProperties& properties() const LIFETIME_BOUND { return m_properties; }

private:
    explicit StyleRuleEnvironmentMap(Ref<StyleProperties>&&);
    StyleRuleEnvironmentMap(const StyleRuleEnvironmentMap&) = default;

    AtomString m_name;
    std::optional<EnvironmentMapFormat> m_format;
    CSS::URL m_src;
    const Ref<StyleProperties> m_properties;
};

class CSSEnvironmentMapRule final : public CSSRule {
public:
    static Ref<CSSEnvironmentMapRule> create(StyleRuleEnvironmentMap&, CSSStyleSheet*);
    ~CSSEnvironmentMapRule();

    String cssText() const final;
    void NODELETE reattach(StyleRuleBase&) final;
    StyleRuleType styleRuleType() const final { return StyleRuleType::EnvironmentMap; }

    String name() const;
    String format() const;
    String src() const;

private:
    CSSEnvironmentMapRule(StyleRuleEnvironmentMap&, CSSStyleSheet* parent);

    Ref<StyleRuleEnvironmentMap> m_environmentMapRule;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_RULE(CSSEnvironmentMapRule, StyleRuleType::EnvironmentMap)

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleRuleEnvironmentMap)
static bool isType(const WebCore::StyleRuleBase& rule) { return rule.isEnvironmentMapRule(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(SPATIAL_PORTAL)
