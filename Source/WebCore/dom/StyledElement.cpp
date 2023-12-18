/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "StyledElement.h"

#include "AttributeChangeInvalidation.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSImageValue.h"
#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParser.h"
#include "CSSStyleSheet.h"
#include "CSSUnparsedValue.h"
#include "CSSValuePool.h"
#include "CachedResource.h"
#include "ContentSecurityPolicy.h"
#include "DOMTokenList.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "HTMLElement.h"
#include "HTMLImageElement.h"
#include "HTMLParserIdioms.h"
#include "InlineStylePropertyMap.h"
#include "InspectorInstrumentation.h"
#include "MutableStyleProperties.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "ScriptableDocumentParser.h"
#include "StylePropertyMap.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include <wtf/HashFunctions.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(StyledElement);

static_assert(sizeof(StyledElement) == sizeof(Element), "styledelement should remain same size as element");

using namespace HTMLNames;

void StyledElement::synchronizeStyleAttributeInternalImpl()
{
    ASSERT(elementData());
    ASSERT(elementData()->styleAttributeIsDirty());
    elementData()->setStyleAttributeIsDirty(false);
    if (const StyleProperties* inlineStyle = this->inlineStyle())
        setSynchronizedLazyAttribute(styleAttr, inlineStyle->asTextAtom());
}

StyledElement::~StyledElement() = default;

CSSStyleDeclaration& StyledElement::cssomStyle()
{
    return ensureMutableInlineStyle().ensureInlineCSSStyleDeclaration(*this);
}

StylePropertyMap& StyledElement::ensureAttributeStyleMap()
{
    if (!attributeStyleMap())
        setAttributeStyleMap(InlineStylePropertyMap::create(*this));
    return *attributeStyleMap();
}

MutableStyleProperties& StyledElement::ensureMutableInlineStyle()
{
    RefPtr<StyleProperties>& inlineStyle = ensureUniqueElementData().m_inlineStyle;
    if (!inlineStyle) {
        Ref mutableProperties = MutableStyleProperties::create(strictToCSSParserMode(isHTMLElement() && !document().inQuirksMode()));
        inlineStyle = mutableProperties.copyRef();
        return mutableProperties.get();
    }
    if (RefPtr mutableProperties = dynamicDowncast<MutableStyleProperties>(*inlineStyle))
        return *mutableProperties;
    Ref mutableProperties = inlineStyle->mutableCopy();
    inlineStyle = mutableProperties.copyRef();
    return mutableProperties.get();
}

void StyledElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    Element::attributeChanged(name, oldValue, newValue, reason);
    if (oldValue != newValue) {
        if (name == styleAttr)
            styleAttributeChanged(newValue, reason);
        else if (hasPresentationalHintsForAttribute(name)) {
            elementData()->setPresentationalHintStyleIsDirty(true);
            invalidateStyleInternal();
        }
    }
}

PropertySetCSSStyleDeclaration* StyledElement::inlineStyleCSSOMWrapper()
{
    if (!inlineStyle() || !inlineStyle()->hasCSSOMWrapper())
        return 0;
    PropertySetCSSStyleDeclaration* cssomWrapper = ensureMutableInlineStyle().cssStyleDeclaration();
    ASSERT(cssomWrapper && cssomWrapper->parentElement() == this);
    return cssomWrapper;
}

static bool usesStyleBasedEditability(const StyleProperties& properties)
{
    return properties.getPropertyCSSValue(CSSPropertyWebkitUserModify);
}

void StyledElement::setInlineStyleFromString(const AtomString& newStyleString)
{
    auto& inlineStyle = elementData()->m_inlineStyle;

    // Avoid redundant work if we're using shared attribute data with already parsed inline style.
    if (inlineStyle && !elementData()->isUnique())
        return;

    // We reconstruct the property set instead of mutating if there is no CSSOM wrapper.
    // This makes wrapperless property sets immutable and so cacheable.
    if (RefPtr mutableStyleProperties = dynamicDowncast<MutableStyleProperties>(inlineStyle))
        mutableStyleProperties->parseDeclaration(newStyleString, protectedDocument().get());
    else
        inlineStyle = CSSParser::parseInlineStyleDeclaration(newStyleString, this);

    if (usesStyleBasedEditability(*inlineStyle))
        protectedDocument()->setHasElementUsingStyleBasedEditability();
}

void StyledElement::styleAttributeChanged(const AtomString& newStyleString, AttributeModificationReason reason)
{
    Ref document = this->document();
    auto startLineNumber = OrdinalNumber::beforeFirst();
    if (document->scriptableDocumentParser() && !document->isInDocumentWrite())
        startLineNumber = document->scriptableDocumentParser()->textPosition().m_line;

    if (newStyleString.isNull())
        ensureMutableInlineStyle().clear();
    else if (reason == AttributeModificationReason::ByCloning || document->checkedContentSecurityPolicy()->allowInlineStyle(document->url().string(), startLineNumber, newStyleString.string(), CheckUnsafeHashes::Yes, *this, nonce(), isInUserAgentShadowTree()))
        setInlineStyleFromString(newStyleString);

    elementData()->setStyleAttributeIsDirty(false);

    invalidateStyleInternal();
    InspectorInstrumentation::didInvalidateStyleAttr(*this);
}

void StyledElement::invalidateStyleAttribute()
{
    if (auto* inlineStyle = this->inlineStyle()) {
        if (usesStyleBasedEditability(*inlineStyle))
            protectedDocument()->setHasElementUsingStyleBasedEditability();
    }

    elementData()->setStyleAttributeIsDirty(true);
    invalidateStyleInternal();

    // In the rare case of selectors like "[style] ~ div" we need to synchronize immediately to invalidate.
    if (styleResolver().ruleSets().hasComplexSelectorsForStyleAttribute()) {
        if (auto* inlineStyle = this->inlineStyle()) {
            elementData()->setStyleAttributeIsDirty(false);
            auto newValue = inlineStyle->asTextAtom();
            Style::AttributeChangeInvalidation styleInvalidation(*this, styleAttr, attributeWithoutSynchronization(styleAttr), newValue);
            setSynchronizedLazyAttribute(styleAttr, newValue);
        }
    }
}

void StyledElement::inlineStyleChanged()
{
    invalidateStyleAttribute();
    InspectorInstrumentation::didInvalidateStyleAttr(*this);
}
    
bool StyledElement::setInlineStyleProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    ensureMutableInlineStyle().setProperty(propertyID, CSSPrimitiveValue::create(identifier), important);
    inlineStyleChanged();
    return true;
}

bool StyledElement::setInlineStyleProperty(CSSPropertyID propertyID, CSSPropertyID identifier, bool important)
{
    ensureMutableInlineStyle().setProperty(propertyID, CSSPrimitiveValue::create(identifier), important);
    inlineStyleChanged();
    return true;
}

bool StyledElement::setInlineStyleProperty(CSSPropertyID propertyID, double value, CSSUnitType unit, bool important)
{
    ensureMutableInlineStyle().setProperty(propertyID, CSSPrimitiveValue::create(value, unit), important);
    inlineStyleChanged();
    return true;
}

bool StyledElement::setInlineStyleProperty(CSSPropertyID propertyID, Ref<CSSValue>&& value, bool important)
{
    ensureMutableInlineStyle().setProperty(propertyID, WTFMove(value), important);
    inlineStyleChanged();
    return true;
}

bool StyledElement::setInlineStyleProperty(CSSPropertyID propertyID, const String& value, bool important, bool* didFailParsing)
{
    bool changes = ensureMutableInlineStyle().setProperty(propertyID, value, important, CSSParserContext(document()), didFailParsing);
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool StyledElement::setInlineStyleCustomProperty(const AtomString& property, const String& value, bool important)
{
    bool changes = ensureMutableInlineStyle().setCustomProperty(property.string(), value, important, CSSParserContext(document()));
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool StyledElement::setInlineStyleCustomProperty(Ref<CSSValue>&& customPropertyValue, bool important)
{
    ensureMutableInlineStyle().addParsedProperty(CSSProperty(CSSPropertyCustom, WTFMove(customPropertyValue), important));
    inlineStyleChanged();
    return true;
}

bool StyledElement::removeInlineStyleProperty(CSSPropertyID propertyID)
{
    if (!inlineStyle())
        return false;
    bool changes = ensureMutableInlineStyle().removeProperty(propertyID);
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool StyledElement::removeInlineStyleCustomProperty(const AtomString& property)
{
    if (!inlineStyle())
        return false;
    bool changes = ensureMutableInlineStyle().removeCustomProperty(property);
    if (changes)
        inlineStyleChanged();
    return changes;
}

void StyledElement::removeAllInlineStyleProperties()
{
    if (!inlineStyle() || inlineStyle()->isEmpty())
        return;
    ensureMutableInlineStyle().clear();
    inlineStyleChanged();
}

void StyledElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    RefPtr inlineStyle = this->inlineStyle();
    if (!inlineStyle)
        return;
    inlineStyle->traverseSubresources([&] (auto& resource) {
        urls.add(resource.url());
        return false;
    });
}

Attribute StyledElement::replaceURLsInAttributeValue(const Attribute& attribute, const HashMap<String, String>& replacementURLStrings) const
{
    if (replacementURLStrings.isEmpty())
        return attribute;

    if (attribute.name() != styleAttr)
        return attribute;

    RefPtr properties = this->inlineStyle();
    if (!properties)
        return attribute;

    auto mutableProperties = properties->mutableCopy();
    mutableProperties->setReplacementURLForSubresources(replacementURLStrings);
    auto inlineStyleString = mutableProperties->asText();
    mutableProperties->clearReplacementURLForSubresources();
    return Attribute { styleAttr, AtomString { inlineStyleString } };
}

const ImmutableStyleProperties* StyledElement::presentationalHintStyle() const
{
    if (!elementData())
        return nullptr;
    if (elementData()->presentationalHintStyleIsDirty())
        const_cast<StyledElement&>(*this).rebuildPresentationalHintStyle();
    return elementData()->presentationalHintStyle();
}

void StyledElement::rebuildPresentationalHintStyle()
{
    auto style = MutableStyleProperties::create(isSVGElement() ? SVGAttributeMode : HTMLQuirksMode);
    for (auto& attribute : attributesIterator())
        collectPresentationalHintsForAttribute(attribute.name(), attribute.value(), style);

    if (auto* imageElement = dynamicDowncast<HTMLImageElement>(*this))
        imageElement->collectExtraStyleForPresentationalHints(style);

    // ShareableElementData doesn't store presentation attribute style, so make sure we have a UniqueElementData.
    auto& elementData = ensureUniqueElementData();

    elementData.setPresentationalHintStyleIsDirty(false);
    if (style->isEmpty())
        elementData.m_presentationalHintStyle = nullptr;
    else
        elementData.m_presentationalHintStyle = style->immutableCopy();
}

void StyledElement::addPropertyToPresentationalHintStyle(MutableStyleProperties& style, CSSPropertyID propertyID, CSSValueID identifier)
{
    style.setProperty(propertyID, CSSPrimitiveValue::create(identifier));
}

void StyledElement::addPropertyToPresentationalHintStyle(MutableStyleProperties& style, CSSPropertyID propertyID, double value, CSSUnitType unit)
{
    style.setProperty(propertyID, CSSPrimitiveValue::create(value, unit));
}
    
void StyledElement::addPropertyToPresentationalHintStyle(MutableStyleProperties& style, CSSPropertyID propertyID, const String& value)
{
    style.setProperty(propertyID, value, false, CSSParserContext(document()));
}

}
