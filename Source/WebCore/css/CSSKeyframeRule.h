/*
 * Copyright (C) 2007, 2008, 2012, 2014 Apple Inc. All rights reserved.
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

namespace WebCore {

class CSSStyleDeclaration;
class StyleRuleCSSStyleDeclaration;
class CSSKeyframesRule;

class StyleRuleKeyframe final : public StyleRuleBase {
public:
    static Ref<StyleRuleKeyframe> create(Ref<StyleProperties>&& properties)
    {
        return adoptRef(*new StyleRuleKeyframe(WTFMove(properties)));
    }

    static Ref<StyleRuleKeyframe> create(std::unique_ptr<Vector<double>> keys, Ref<StyleProperties>&& properties)
    {
        return adoptRef(*new StyleRuleKeyframe(WTFMove(keys), WTFMove(properties)));
    }
    ~StyleRuleKeyframe();

    String keyText() const;
    bool setKeyText(const String&);
    void setKey(double key)
    {
        ASSERT(m_keys.isEmpty());
        m_keys.clear();
        m_keys.append(key);
    }

    const Vector<double>& keys() const { return m_keys; };

    const StyleProperties& properties() const { return m_properties; }
    MutableStyleProperties& mutableProperties();

    String cssText() const;

private:
    explicit StyleRuleKeyframe(Ref<StyleProperties>&&);
    StyleRuleKeyframe(std::unique_ptr<Vector<double>>, Ref<StyleProperties>&&);

    Ref<StyleProperties> m_properties;
    Vector<double> m_keys;
};

class CSSKeyframeRule final : public CSSRule {
public:
    virtual ~CSSKeyframeRule();

    String cssText() const final { return m_keyframe->cssText(); }
    void reattach(StyleRuleBase&) final;

    String keyText() const { return m_keyframe->keyText(); }
    void setKeyText(const String& text) { m_keyframe->setKeyText(text); }

    CSSStyleDeclaration& style();

private:
    CSSKeyframeRule(StyleRuleKeyframe&, CSSKeyframesRule* parent);

    CSSRule::Type type() const final { return KEYFRAME_RULE; }

    Ref<StyleRuleKeyframe> m_keyframe;
    mutable RefPtr<StyleRuleCSSStyleDeclaration> m_propertiesCSSOMWrapper;
    
    friend class CSSKeyframesRule;
};

} // namespace WebCore
