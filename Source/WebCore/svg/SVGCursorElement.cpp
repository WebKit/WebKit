/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGCursorElement.h"

#include "CSSCursorImageValue.h"
#include "Document.h"
#include "SVGNames.h"
#include "SVGStringList.h"
#include "XLinkNames.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_LENGTH(SVGCursorElement, SVGNames::xAttr, X, x)
DEFINE_ANIMATED_LENGTH(SVGCursorElement, SVGNames::yAttr, Y, y)
DEFINE_ANIMATED_STRING(SVGCursorElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGCursorElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGCursorElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(x)
    REGISTER_LOCAL_ANIMATED_PROPERTY(y)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGTests)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGCursorElement::SVGCursorElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
    , m_x(LengthModeWidth)
    , m_y(LengthModeHeight)
{
    ASSERT(hasTagName(SVGNames::cursorTag));
    registerAnimatedPropertiesForSVGCursorElement();
}

Ref<SVGCursorElement> SVGCursorElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGCursorElement(tagName, document));
}

SVGCursorElement::~SVGCursorElement()
{
    for (auto& client : m_clients)
        client->cursorElementRemoved(*this);
}

bool SVGCursorElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static const auto supportedAttributes = makeNeverDestroyed([] {
        HashSet<QualifiedName> set;
        SVGTests::addSupportedAttributes(set);
        SVGExternalResourcesRequired::addSupportedAttributes(set);
        SVGURIReference::addSupportedAttributes(set);
        set.add({ SVGNames::xAttr, SVGNames::yAttr });
        return set;
    }());
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGCursorElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        setXBaseValue(SVGLengthValue::construct(LengthModeWidth, value, parseError));
    else if (name == SVGNames::yAttr)
        setYBaseValue(SVGLengthValue::construct(LengthModeHeight, value, parseError));

    reportAttributeParsingError(parseError, name, value);

    SVGElement::parseAttribute(name, value);
    SVGTests::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
}

void SVGCursorElement::addClient(CSSCursorImageValue& value)
{
    m_clients.add(&value);
}

void SVGCursorElement::removeClient(CSSCursorImageValue& value)
{
    m_clients.remove(&value);
}

void SVGCursorElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    InstanceInvalidationGuard guard(*this);
    for (auto& client : m_clients)
        client->cursorElementChanged(*this);
}

void SVGCursorElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    SVGElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(href()));
}

Ref<SVGStringList> SVGCursorElement::requiredFeatures()
{
    return SVGTests::requiredFeatures(*this);
}

Ref<SVGStringList> SVGCursorElement::requiredExtensions()
{ 
    return SVGTests::requiredExtensions(*this);
}

Ref<SVGStringList> SVGCursorElement::systemLanguage()
{
    return SVGTests::systemLanguage(*this);
}

}
