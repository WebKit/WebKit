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
#include "PropertySetCSSStyleDeclaration.h"
#include "ScriptableDocumentParser.h"
#include "StyleProperties.h"
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

StyledElement::~StyledElement()
{
    if (PropertySetCSSStyleDeclaration* cssomWrapper = inlineStyleCSSOMWrapper())
        cssomWrapper->clearParentElement();
}

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
    if (!inlineStyle)
        inlineStyle = MutableStyleProperties::create(strictToCSSParserMode(isHTMLElement() && !document().inQuirksMode()));
    else if (!is<MutableStyleProperties>(*inlineStyle))
        inlineStyle = inlineStyle->mutableCopy();
    return downcast<MutableStyleProperties>(*inlineStyle);
}

void StyledElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    if (oldValue != newValue) {
        if (name == styleAttr)
            styleAttributeChanged(newValue, reason);
        else if (hasPresentationalHintsForAttribute(name)) {
            elementData()->setPresentationalHintStyleIsDirty(true);
            invalidateStyle();
        }
    }

    Element::attributeChanged(name, oldValue, newValue, reason);
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
    if (inlineStyle && !is<MutableStyleProperties>(*inlineStyle))
        inlineStyle = nullptr;

    if (!inlineStyle)
        inlineStyle = CSSParser::parseInlineStyleDeclaration(newStyleString, this);
    else
        downcast<MutableStyleProperties>(*inlineStyle).parseDeclaration(newStyleString, document());

    if (usesStyleBasedEditability(*inlineStyle))
        document().setHasElementUsingStyleBasedEditability();
}

void StyledElement::styleAttributeChanged(const AtomString& newStyleString, AttributeModificationReason reason)
{
    auto startLineNumber = OrdinalNumber::beforeFirst();
    if (document().scriptableDocumentParser() && !document().isInDocumentWrite())
        startLineNumber = document().scriptableDocumentParser()->textPosition().m_line;

    if (newStyleString.isNull())
        ensureMutableInlineStyle().clear();
    else if (reason == ModifiedByCloning || document().contentSecurityPolicy()->allowInlineStyle(document().url().string(), startLineNumber, newStyleString.string(), CheckUnsafeHashes::Yes, *this, nonce(), isInUserAgentShadowTree()))
        setInlineStyleFromString(newStyleString);

    elementData()->setStyleAttributeIsDirty(false);

    invalidateStyle();
    InspectorInstrumentation::didInvalidateStyleAttr(*this);
}

void StyledElement::invalidateStyleAttribute()
{
    if (auto* inlineStyle = this->inlineStyle()) {
        if (usesStyleBasedEditability(*inlineStyle))
            document().setHasElementUsingStyleBasedEditability();
    }

    elementData()->setStyleAttributeIsDirty(true);
    invalidateStyle();

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
    ensureMutableInlineStyle().setProperty(propertyID, CSSValuePool::singleton().createIdentifierValue(identifier), important);
    inlineStyleChanged();
    return true;
}

bool StyledElement::setInlineStyleProperty(CSSPropertyID propertyID, CSSPropertyID identifier, bool important)
{
    ensureMutableInlineStyle().setProperty(propertyID, CSSValuePool::singleton().createIdentifierValue(identifier), important);
    inlineStyleChanged();
    return true;
}

bool StyledElement::setInlineStyleProperty(CSSPropertyID propertyID, double value, CSSUnitType unit, bool important)
{
    ensureMutableInlineStyle().setProperty(propertyID, CSSValuePool::singleton().createValue(value, unit), important);
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
    auto* inlineStyle = this->inlineStyle();
    if (!inlineStyle)
        return;
    inlineStyle->traverseSubresources([&] (auto& resource) {
        urls.add(resource.url());
        return false;
    });
}

void StyledElement::rebuildPresentationalHintStyle()
{
    auto style = MutableStyleProperties::create(isSVGElement() ? SVGAttributeMode : HTMLQuirksMode);
    for (const Attribute& attribute : attributesIterator())
        collectPresentationalHintsForAttribute(attribute.name(), attribute.value(), style);

    if (is<HTMLImageElement>(*this))
        collectExtraStyleForPresentationalHints(style);

    // ShareableElementData doesn't store presentation attribute style, so make sure we have a UniqueElementData.
    UniqueElementData& elementData = ensureUniqueElementData();

    elementData.setPresentationalHintStyleIsDirty(false);
    if (style->isEmpty())
        elementData.m_presentationalHintStyle = nullptr;
    else
        elementData.m_presentationalHintStyle = WTFMove(style);
}

void StyledElement::addPropertyToPresentationalHintStyle(MutableStyleProperties& style, CSSPropertyID propertyID, CSSValueID identifier)
{
    style.setProperty(propertyID, CSSValuePool::singleton().createIdentifierValue(identifier));
}

void StyledElement::addPropertyToPresentationalHintStyle(MutableStyleProperties& style, CSSPropertyID propertyID, double value, CSSUnitType unit)
{
    style.setProperty(propertyID, CSSValuePool::singleton().createValue(value, unit));
}
    
void StyledElement::addPropertyToPresentationalHintStyle(MutableStyleProperties& style, CSSPropertyID propertyID, const String& value)
{
    style.setProperty(propertyID, value, false, CSSParserContext(document()));
}

}
