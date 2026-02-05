/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"
#include "CSSComputedStyleDeclaration.h"

#include "CSSProperty.h"
#include "CSSPropertyParser.h"
#include "CSSSelectorParser.h"
#include "CSSSerializationContext.h"
#include "CSSValuePool.h"
#include "ComposedTreeAncestorIterator.h"
#include "DeprecatedCSSOMValue.h"
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "RenderBox.h"
#include "RenderBoxModelObject.h"
#include "RenderStyle+GettersInlines.h"
#include "Settings.h"
#include "ShorthandSerializer.h"
#include "StylePropertiesInlines.h"
#include "StylePropertyShorthand.h"
#include "StyleScope.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSComputedStyleDeclaration);

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(Element& element, AllowVisited allowVisited)
    : m_element(element)
    , m_allowVisitedStyle(allowVisited == AllowVisited::Yes)
{
}

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(Element& element, IsEmpty isEmpty)
    : m_element(element)
    , m_isEmpty(isEmpty == IsEmpty::Yes)
{
}

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(Element& element, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
    : m_element(element)
    , m_pseudoElementIdentifier(pseudoElementIdentifier)
{
}

CSSComputedStyleDeclaration::~CSSComputedStyleDeclaration() = default;

Ref<CSSComputedStyleDeclaration> CSSComputedStyleDeclaration::create(Element& element, AllowVisited allowVisited)
{
    return adoptRef(*new CSSComputedStyleDeclaration(element, allowVisited));
}

Ref<CSSComputedStyleDeclaration> CSSComputedStyleDeclaration::create(Element& element, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    return adoptRef(*new CSSComputedStyleDeclaration(element, pseudoElementIdentifier));
}

Ref<CSSComputedStyleDeclaration> CSSComputedStyleDeclaration::createEmpty(Element& element)
{
    return adoptRef(*new CSSComputedStyleDeclaration(element, IsEmpty::Yes));
}

Style::Extractor CSSComputedStyleDeclaration::extractor() const
{
    return Style::Extractor(m_element.ptr(), m_allowVisitedStyle, m_pseudoElementIdentifier);
}

String CSSComputedStyleDeclaration::cssText() const
{
    return emptyString();
}

ExceptionOr<void> CSSComputedStyleDeclaration::setCssText(const String&)
{
    return Exception { ExceptionCode::NoModificationAllowedError };
}

Ref<MutableStyleProperties> CSSComputedStyleDeclaration::copyProperties() const
{
    if (m_isEmpty)
        return MutableStyleProperties::create();
    return extractor().copyProperties();
}

const Settings* CSSComputedStyleDeclaration::settings() const
{
    return &m_element->document().settings();
}

const FixedVector<CSSPropertyID>& CSSComputedStyleDeclaration::exposedComputedCSSPropertyIDs() const
{
    return protect(protectedElement()->document())->exposedComputedCSSPropertyIDs();
}

String CSSComputedStyleDeclaration::getPropertyValue(CSSPropertyID propertyID) const
{
    if (m_isEmpty)
        return emptyString(); // FIXME: Should this be null instead, as it is in StyleProperties::getPropertyValue?

    return extractor().propertyValueSerialization(propertyID, CSS::defaultSerializationContext());
}

unsigned CSSComputedStyleDeclaration::length() const
{
    if (m_isEmpty)
        return 0;

    Style::Extractor::updateStyleIfNeededForProperty(m_element.get(), CSSPropertyCustom);

    CheckedPtr style = protectedElement()->computedStyle(m_pseudoElementIdentifier);
    if (!style)
        return 0;

    return exposedComputedCSSPropertyIDs().size() + style->inheritedCustomProperties().size() + style->nonInheritedCustomProperties().size();
}

String CSSComputedStyleDeclaration::item(unsigned i) const
{
    if (m_isEmpty)
        return String();

    if (i >= length())
        return String();

    if (i < exposedComputedCSSPropertyIDs().size())
        return nameString(exposedComputedCSSPropertyIDs().at(i));

    CheckedPtr style = protectedElement()->computedStyle(m_pseudoElementIdentifier);
    if (!style)
        return String();

    Ref inheritedCustomProperties = style->inheritedCustomProperties();

    // FIXME: findKeyAtIndex does a linear search for the property name, so if
    // we are called in a loop over all item indexes, we'll spend quadratic time
    // searching for keys.

    if (i < exposedComputedCSSPropertyIDs().size() + inheritedCustomProperties->size())
        return inheritedCustomProperties->findKeyAtIndex(i - exposedComputedCSSPropertyIDs().size());

    Ref nonInheritedCustomProperties = style->nonInheritedCustomProperties();
    return nonInheritedCustomProperties->findKeyAtIndex(i - inheritedCustomProperties->size() - exposedComputedCSSPropertyIDs().size());
}

CSSRule* CSSComputedStyleDeclaration::parentRule() const
{
    return nullptr;
}

CSSRuleList* CSSComputedStyleDeclaration::cssRules() const
{
    return nullptr;
}

RefPtr<DeprecatedCSSOMValue> CSSComputedStyleDeclaration::getPropertyCSSValue(const String& propertyName)
{
    if (m_isEmpty)
        return nullptr;

    if (isCustomPropertyName(propertyName)) {
        auto value = extractor().customPropertyValue(AtomString { propertyName });
        if (!value)
            return nullptr;
        return value->createDeprecatedCSSOMWrapper(*this);
    }

    auto propertyID = cssPropertyID(propertyName);
    if (!propertyID)
        return nullptr;

    auto value = extractor().propertyValue(propertyID);
    if (!value)
        return nullptr;
    return value->createDeprecatedCSSOMWrapper(*this);
}

String CSSComputedStyleDeclaration::getPropertyValue(const String& propertyName)
{
    if (m_isEmpty)
        return String();

    if (isCustomPropertyName(propertyName))
        return extractor().customPropertyValueSerialization(AtomString { propertyName }, CSS::defaultSerializationContext());

    auto propertyID = cssPropertyID(propertyName);
    if (!propertyID)
        return String();

    return extractor().propertyValueSerialization(propertyID, CSS::defaultSerializationContext());
}

String CSSComputedStyleDeclaration::getPropertyPriority(const String&)
{
    // All computed styles have a priority of not "important".
    return emptyString();
}

String CSSComputedStyleDeclaration::getPropertyShorthand(const String&)
{
    return emptyString(); // FIXME: Should this sometimes be null instead of empty, to match a normal style declaration?
}

bool CSSComputedStyleDeclaration::isPropertyImplicit(const String&)
{
    return false;
}

ExceptionOr<void> CSSComputedStyleDeclaration::setProperty(const String&, const String&, const String&)
{
    return Exception { ExceptionCode::NoModificationAllowedError };
}

ExceptionOr<String> CSSComputedStyleDeclaration::removeProperty(const String&)
{
    return Exception { ExceptionCode::NoModificationAllowedError };
}
    
String CSSComputedStyleDeclaration::getPropertyValueInternal(CSSPropertyID propertyID)
{
    return getPropertyValue(propertyID);
}

ExceptionOr<void> CSSComputedStyleDeclaration::setPropertyInternal(CSSPropertyID, const String&, IsImportant)
{
    return Exception { ExceptionCode::NoModificationAllowedError };
}

} // namespace WebCore
