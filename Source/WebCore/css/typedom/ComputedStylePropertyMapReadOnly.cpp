/*
 * Copyright (C) 2022 Igalia S.L.
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
#include "ComputedStylePropertyMapReadOnly.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyParser.h"
#include "Document.h"
#include "Element.h"
#include "RenderStyle.h"
#include "StylePropertyShorthand.h"
#include "StyleScope.h"
#include <wtf/KeyValuePair.h>

namespace WebCore {

Ref<ComputedStylePropertyMapReadOnly> ComputedStylePropertyMapReadOnly::create(Element& element)
{
    return adoptRef(*new ComputedStylePropertyMapReadOnly(element));
}

ComputedStylePropertyMapReadOnly::ComputedStylePropertyMapReadOnly(Element& element)
    : m_element(element)
{
}

ExceptionOr<RefPtr<CSSStyleValue>> ComputedStylePropertyMapReadOnly::get(const AtomString& property) const
{
    // https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-get
    if (isCustomPropertyName(property))
        return StylePropertyMapReadOnly::reifyValue(ComputedStyleExtractor(m_element.ptr()).customPropertyValue(property).get(), m_element->document(), m_element.ptr());

    auto propertyID = cssPropertyID(property);
    if (!propertyID || !isCSSPropertyExposed(propertyID, &m_element->document().settings()))
        return Exception { TypeError, makeString("Invalid property ", property) };

    if (isShorthandCSSProperty(propertyID)) {
        auto value = ComputedStyleExtractor(m_element.ptr()).propertyValue(propertyID, EUpdateLayout::UpdateLayout, ComputedStyleExtractor::PropertyValueType::Computed);
        return CSSStyleValue::create(WTFMove(value), String(property)).ptr();
    }

    return StylePropertyMapReadOnly::reifyValue(ComputedStyleExtractor(m_element.ptr()).propertyValue(propertyID, EUpdateLayout::UpdateLayout, ComputedStyleExtractor::PropertyValueType::Computed).get(), m_element->document(), m_element.ptr());
}

ExceptionOr<Vector<RefPtr<CSSStyleValue>>> ComputedStylePropertyMapReadOnly::getAll(const AtomString& property) const
{
    // https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-getall
    if (isCustomPropertyName(property))
        return StylePropertyMapReadOnly::reifyValueToVector(ComputedStyleExtractor(m_element.ptr()).customPropertyValue(property).get(), m_element->document(), m_element.ptr());

    auto propertyID = cssPropertyID(property);
    if (!propertyID || !isCSSPropertyExposed(propertyID, &m_element->document().settings()))
        return Exception { TypeError, makeString("Invalid property ", property) };

    if (isShorthandCSSProperty(propertyID)) {
        auto value = ComputedStyleExtractor(m_element.ptr()).propertyValue(propertyID, EUpdateLayout::UpdateLayout, ComputedStyleExtractor::PropertyValueType::Computed);
        return Vector<RefPtr<CSSStyleValue>> { CSSStyleValue::create(WTFMove(value), String(property)) };
    }

    return StylePropertyMapReadOnly::reifyValueToVector(ComputedStyleExtractor(m_element.ptr()).propertyValue(propertyID, EUpdateLayout::UpdateLayout, ComputedStyleExtractor::PropertyValueType::Computed).get(), m_element->document(), m_element.ptr());
}

ExceptionOr<bool> ComputedStylePropertyMapReadOnly::has(const AtomString& property) const
{
    // https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-has
    auto result = get(property);
    if (result.hasException())
        return result.releaseException();

    return !!result.releaseReturnValue();
}

unsigned ComputedStylePropertyMapReadOnly::size() const
{
    // https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-size
    auto* style = m_element->computedStyle();
    if (!style)
        return 0;

    return m_element->document().exposedComputedCSSPropertyIDs().size() + style->inheritedCustomProperties().size() + style->nonInheritedCustomProperties().size();
}

Vector<StylePropertyMapReadOnly::StylePropertyMapEntry> ComputedStylePropertyMapReadOnly::entries() const
{
    // https://drafts.css-houdini.org/css-typed-om-1/#the-stylepropertymap
    Vector<StylePropertyMapReadOnly::StylePropertyMapEntry> values;
    auto* style = m_element->computedStyle();
    if (!style)
        return values;

    const auto& inheritedCustomProperties = style->inheritedCustomProperties();
    const auto& nonInheritedCustomProperties = style->nonInheritedCustomProperties();
    const auto& exposedComputedCSSPropertyIDs = m_element->document().exposedComputedCSSPropertyIDs();
    values.reserveInitialCapacity(exposedComputedCSSPropertyIDs.size() + inheritedCustomProperties.size() + nonInheritedCustomProperties.size());
    for (auto propertyID : exposedComputedCSSPropertyIDs) {
        auto key = getPropertyNameString(propertyID);
        auto value = ComputedStyleExtractor(m_element.ptr()).propertyValue(propertyID, EUpdateLayout::DoNotUpdateLayout, ComputedStyleExtractor::PropertyValueType::Computed);
        values.uncheckedAppend(makeKeyValuePair(WTFMove(key), StylePropertyMapReadOnly::reifyValueToVector(value.get(), m_element->document(), m_element.ptr())));
    }

    for (auto* map : { &nonInheritedCustomProperties, &inheritedCustomProperties }) {
        for (const auto& it : *map)
            values.uncheckedAppend(makeKeyValuePair(it.key, StylePropertyMapReadOnly::reifyValueToVector(it.value.get(), m_element->document(), m_element.ptr())));
    }

    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
        const auto& nameA = a.key;
        const auto& nameB = b.key;
        if (nameA.startsWith("--"_s))
            return nameB.startsWith("--"_s) && codePointCompareLessThan(nameA, nameB);

        if (nameA.startsWith('-'))
            return nameB.startsWith("--"_s) || (nameB.startsWith('-') && codePointCompareLessThan(nameA, nameB));

        return nameB.startsWith('-') || codePointCompareLessThan(nameA, nameB);
    });

    return values;
}

} // namespace WebCore

#endif // ENABLE(CSS_TYPED_OM)
