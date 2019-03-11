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

#include "config.h"
#include "CSSKeyframeRule.h"

#include "CSSKeyframesRule.h"
#include "CSSParser.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StyleProperties.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

StyleRuleKeyframe::StyleRuleKeyframe(Ref<StyleProperties>&& properties)
    : StyleRuleBase(Keyframe)
    , m_properties(WTFMove(properties))
{
}

StyleRuleKeyframe::StyleRuleKeyframe(std::unique_ptr<Vector<double>> keys, Ref<StyleProperties>&& properties)
    : StyleRuleBase(Keyframe)
    , m_properties(WTFMove(properties))
    , m_keys(*keys)
{
}
    
StyleRuleKeyframe::~StyleRuleKeyframe() = default;

MutableStyleProperties& StyleRuleKeyframe::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties.get()))
        m_properties = m_properties->mutableCopy();
    return downcast<MutableStyleProperties>(m_properties.get());
}

String StyleRuleKeyframe::keyText() const
{
    StringBuilder keyText;

    for (size_t i = 0; i < m_keys.size(); ++i) {
        if (i)
            keyText.append(',');
        keyText.appendFixedPrecisionNumber(m_keys.at(i) * 100);
        keyText.append('%');
    }

    return keyText.toString();
}
    
bool StyleRuleKeyframe::setKeyText(const String& keyText)
{
    ASSERT(!keyText.isNull());
    auto keys = CSSParser::parseKeyframeKeyList(keyText);
    if (!keys || keys->isEmpty())
        return false;
    m_keys = *keys;
    return true;
}

String StyleRuleKeyframe::cssText() const
{
    StringBuilder result;
    result.append(keyText());
    result.appendLiteral(" { ");
    String decls = m_properties->asText();
    result.append(decls);
    if (!decls.isEmpty())
        result.append(' ');
    result.append('}');
    return result.toString();
}

CSSKeyframeRule::CSSKeyframeRule(StyleRuleKeyframe& keyframe, CSSKeyframesRule* parent)
    : CSSRule(0)
    , m_keyframe(keyframe)
{
    setParentRule(parent);
}

CSSKeyframeRule::~CSSKeyframeRule()
{
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->clearParentRule();
}

CSSStyleDeclaration& CSSKeyframeRule::style()
{
    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleDeclaration::create(m_keyframe->mutableProperties(), *this);
    return *m_propertiesCSSOMWrapper;
}

void CSSKeyframeRule::reattach(StyleRuleBase&)
{
    // No need to reattach, the underlying data is shareable on mutation.
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
