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

#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyParser.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "Element.h"
#include "RenderStyleInlines.h"
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

RefPtr<CSSValue> ComputedStylePropertyMapReadOnly::propertyValue(CSSPropertyID propertyID) const
{
    return ComputedStyleExtractor(m_element.ptr()).propertyValue(propertyID, ComputedStyleExtractor::UpdateLayout::Yes, ComputedStyleExtractor::PropertyValueType::Computed);
}

String ComputedStylePropertyMapReadOnly::shorthandPropertySerialization(CSSPropertyID propertyID) const
{
    auto value = propertyValue(propertyID);
    return value ? value->cssText() : String();
}

RefPtr<CSSValue> ComputedStylePropertyMapReadOnly::customPropertyValue(const AtomString& property) const
{
    return ComputedStyleExtractor(m_element.ptr()).customPropertyValue(property);
}

unsigned ComputedStylePropertyMapReadOnly::size() const
{
    // https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-size

    ComputedStyleExtractor::updateStyleIfNeededForProperty(m_element.get(), CSSPropertyCustom);

    auto* style = m_element->computedStyle();
    if (!style)
        return 0;

    return m_element->document().exposedComputedCSSPropertyIDs().size() + style->inheritedCustomProperties().size() + style->nonInheritedCustomProperties().size();
}

Vector<StylePropertyMapReadOnly::StylePropertyMapEntry> ComputedStylePropertyMapReadOnly::entries(ScriptExecutionContext*) const
{
    // https://drafts.css-houdini.org/css-typed-om-1/#the-stylepropertymap
    Vector<StylePropertyMapReadOnly::StylePropertyMapEntry> values;

    // Ensure custom property counts are correct.
    ComputedStyleExtractor::updateStyleIfNeededForProperty(m_element.get(), CSSPropertyCustom);

    auto* style = m_element->computedStyle();
    if (!style)
        return values;

    Ref document = m_element->protectedDocument();
    const auto& inheritedCustomProperties = style->inheritedCustomProperties();
    const auto& nonInheritedCustomProperties = style->nonInheritedCustomProperties();
    const auto& exposedComputedCSSPropertyIDs = document->exposedComputedCSSPropertyIDs();
    values.reserveInitialCapacity(exposedComputedCSSPropertyIDs.size() + inheritedCustomProperties.size() + nonInheritedCustomProperties.size());

    ComputedStyleExtractor extractor { m_element.ptr() };
    values.appendContainerWithMapping(exposedComputedCSSPropertyIDs, [&](auto propertyID) {
        auto value = extractor.propertyValue(propertyID, ComputedStyleExtractor::UpdateLayout::No, ComputedStyleExtractor::PropertyValueType::Computed);
        return makeKeyValuePair(nameString(propertyID), StylePropertyMapReadOnly::reifyValueToVector(WTFMove(value), propertyID, document));
    });

    for (auto* map : { &nonInheritedCustomProperties, &inheritedCustomProperties }) {
        map->forEach([&](auto& it) {
            values.append(makeKeyValuePair(it.key, StylePropertyMapReadOnly::reifyValueToVector(const_cast<CSSCustomPropertyValue*>(it.value.get()), std::nullopt, document)));
            return IterationStatus::Continue;
        });
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
