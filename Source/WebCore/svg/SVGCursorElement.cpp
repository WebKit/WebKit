/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "SVGNames.h"
#include "SVGStringList.h"
#include "StyleCursorImage.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGCursorElement);

inline SVGCursorElement::SVGCursorElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGTests(this)
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::cursorTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGCursorElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGCursorElement::m_y>();
    });
}

Ref<SVGCursorElement> SVGCursorElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGCursorElement(tagName, document));
}

SVGCursorElement::~SVGCursorElement()
{
    for (auto& client : m_clients)
        client.cursorElementRemoved(*this);
}

void SVGCursorElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;

    if (name == SVGNames::xAttr)
        m_x->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
    else if (name == SVGNames::yAttr)
        m_y->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));

    reportAttributeParsingError(parseError, name, newValue);

    SVGURIReference::parseAttribute(name, newValue);
    SVGTests::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGCursorElement::addClient(StyleCursorImage& value)
{
    m_clients.add(value);
}

void SVGCursorElement::removeClient(StyleCursorImage& value)
{
    m_clients.remove(value);
}

void SVGCursorElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        for (auto& client : m_clients)
            client.cursorElementChanged(*this);
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGCursorElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    SVGElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(href()));
}

}
