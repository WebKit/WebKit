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

#ifndef CSSKeyframeRule_h
#define CSSKeyframeRule_h

#include "CSSParser.h"
#include "CSSRule.h"
#include "StyleProperties.h"

namespace WebCore {

class CSSStyleDeclaration;
class StyleRuleCSSStyleDeclaration;
class CSSKeyframesRule;

class StyleKeyframe : public RefCounted<StyleKeyframe> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<StyleKeyframe> create(Ref<StyleProperties>&& properties)
    {
        return adoptRef(*new StyleKeyframe(WTFMove(properties)));
    }
    ~StyleKeyframe();

    String keyText() const;
    void setKeyText(const String& text) { m_keys = CSSParser::parseKeyframeSelector(text); }

    const Vector<double>& keys() const { return m_keys; };

    const StyleProperties& properties() const { return m_properties; }
    MutableStyleProperties& mutableProperties();

    String cssText() const;

private:
    explicit StyleKeyframe(Ref<StyleProperties>&&);

    Ref<StyleProperties> m_properties;
    Vector<double> m_keys;
};

class CSSKeyframeRule final : public CSSRule {
public:
    virtual ~CSSKeyframeRule();

    virtual String cssText() const override { return m_keyframe->cssText(); }
    virtual void reattach(StyleRuleBase&) override;

    String keyText() const { return m_keyframe->keyText(); }
    void setKeyText(const String& text) { m_keyframe->setKeyText(text); }

    CSSStyleDeclaration& style();

private:
    CSSKeyframeRule(StyleKeyframe&, CSSKeyframesRule* parent);

    virtual CSSRule::Type type() const override { return KEYFRAME_RULE; }

    Ref<StyleKeyframe> m_keyframe;
    mutable RefPtr<StyleRuleCSSStyleDeclaration> m_propertiesCSSOMWrapper;
    
    friend class CSSKeyframesRule;
};

} // namespace WebCore

#endif // CSSKeyframeRule_h
