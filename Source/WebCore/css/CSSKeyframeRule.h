/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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
#include "StyleRule.h"

namespace WebCore {

class CSSStyleProperties;
class CSSKeyframesRule;
class StyleProperties;
class StyleRuleCSSStyleProperties;

class StyleRuleKeyframe final : public StyleRuleBase {
public:
    static Ref<StyleRuleKeyframe> create(Ref<StyleProperties>&&);
    static Ref<StyleRuleKeyframe> create(Vector<std::pair<CSSValueID, double>>&& keys, Ref<StyleProperties>&&);
    ~StyleRuleKeyframe();

    Ref<StyleRuleKeyframe> copy() const { RELEASE_ASSERT_NOT_REACHED(); }

    struct Key {
        CSSValueID rangeName;
        double offset;

        void writeToString(StringBuilder&) const;
        bool operator==(const Key&) const = default;
    };

    String keyText() const;
    bool setKeyText(const String&);
    void setKey(Key key)
    {
        ASSERT(m_keys.isEmpty());
        m_keys.clear();
        m_keys.append(key);
    }

    const Vector<Key>& keys() const { return m_keys; };

    const StyleProperties& properties() const { return m_properties; }
    MutableStyleProperties& mutableProperties();

    String cssText() const;

private:
    explicit StyleRuleKeyframe(Ref<StyleProperties>&&);
    StyleRuleKeyframe(Vector<Key>&&, Ref<StyleProperties>&&);

    Ref<StyleProperties> m_properties;
    Vector<Key> m_keys;
};

class CSSKeyframeRule final : public CSSRule {
public:
    virtual ~CSSKeyframeRule();

    String cssText() const final { return m_keyframe->cssText(); }
    void reattach(StyleRuleBase&) final;

    String keyText() const { return m_keyframe->keyText(); }
    void setKeyText(const String& text) { m_keyframe->setKeyText(text); }

    CSSStyleProperties& style();

private:
    CSSKeyframeRule(StyleRuleKeyframe&, CSSKeyframesRule* parent);

    StyleRuleType styleRuleType() const final { return StyleRuleType::Keyframe; }

    const Ref<StyleRuleKeyframe> m_keyframe;
    mutable RefPtr<StyleRuleCSSStyleProperties> m_propertiesCSSOMWrapper;
    
    friend class CSSKeyframesRule;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSKeyframeRule)
    static bool isType(const WebCore::CSSRule& rule) { return rule.styleRuleType() == WebCore::StyleRuleType::Keyframe; }
SPECIALIZE_TYPE_TRAITS_END()
