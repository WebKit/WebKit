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
#include "CSSPropertyParserConsumer+Animations.h"
#include "CSSSerializationContext.h"
#include "CSSStyleProperties.h"
#include "MutableStyleProperties.h"
#include "StyleProperties.h"
#include "StylePropertiesInlines.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

void StyleRuleKeyframe::Key::writeToString(StringBuilder& str) const
{
    if (rangeName == CSSValueContain)
        str.append("contain "_s);
    else if (rangeName == CSSValueCover)
        str.append("cover "_s);
    else if (rangeName == CSSValueEntry)
        str.append("entry "_s);
    else if (rangeName == CSSValueEntryCrossing)
        str.append("entry-crossing "_s);
    else if (rangeName == CSSValueExit)
        str.append("exit "_s);
    else if (rangeName == CSSValueExitCrossing)
        str.append("exit-crossing "_s);
    else if (rangeName == CSSValueScroll)
        str.append("scroll "_s);
    str.append(offset * 100, '%');
}

StyleRuleKeyframe::StyleRuleKeyframe(Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::Keyframe)
    , m_properties(WTF::move(properties))
{
}

StyleRuleKeyframe::StyleRuleKeyframe(Vector<Key>&& keys, Ref<StyleProperties>&& properties)
    : StyleRuleBase(StyleRuleType::Keyframe)
    , m_properties(WTF::move(properties))
    , m_keys(WTF::move(keys))
{
}

Ref<StyleRuleKeyframe> StyleRuleKeyframe::create(Ref<StyleProperties>&& properties)
{
    return adoptRef(*new StyleRuleKeyframe(WTF::move(properties)));
}

Ref<StyleRuleKeyframe> StyleRuleKeyframe::create(Vector<std::pair<CSSValueID, double>>&& keys, Ref<StyleProperties>&& properties)
{
    auto keyStructs = keys.map([](auto& pair) -> Key {
        return { pair.first, pair.second };
    });
    return adoptRef(*new StyleRuleKeyframe(WTF::move(keyStructs), WTF::move(properties)));
}

StyleRuleKeyframe::~StyleRuleKeyframe() = default;

MutableStyleProperties& StyleRuleKeyframe::mutableProperties()
{
    if (!is<MutableStyleProperties>(m_properties))
        m_properties = m_properties->mutableCopy();
    return uncheckedDowncast<MutableStyleProperties>(m_properties.get());
}

String StyleRuleKeyframe::keyText() const
{
    StringBuilder keyText;
    for (size_t i = 0; i < m_keys.size(); ++i) {
        if (i)
            keyText.append(',');
        m_keys[i].writeToString(keyText);
    }
    return keyText.toString();
}
    
bool StyleRuleKeyframe::setKeyText(const String& keyText)
{
    ASSERT(!keyText.isNull());
    auto keys = CSSPropertyParserHelpers::parseKeyframeKeyList(keyText, strictCSSParserContext());
    if (keys.isEmpty())
        return false;
    m_keys = keys.map([](auto& pair) -> Key {
        return { pair.first, pair.second };
    });
    return true;
}

String StyleRuleKeyframe::cssText() const
{
    if (auto declarations = m_properties->asText(CSS::defaultSerializationContext()); !declarations.isEmpty())
        return makeString(keyText(), " { "_s, declarations, " }"_s);
    return makeString(keyText(), " { }"_s);
}

CSSKeyframeRule::CSSKeyframeRule(StyleRuleKeyframe& keyframe, CSSKeyframesRule* parent)
    : CSSRule(nullptr)
    , m_keyframe(keyframe)
{
    setParentRule(parent);
}

CSSKeyframeRule::~CSSKeyframeRule()
{
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->clearParentRule();
}

CSSStyleProperties& CSSKeyframeRule::style()
{
    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleProperties::create(m_keyframe->mutableProperties(), *this);
    return *m_propertiesCSSOMWrapper;
}

void CSSKeyframeRule::reattach(StyleRuleBase&)
{
    // No need to reattach, the underlying data is shareable on mutation.
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
