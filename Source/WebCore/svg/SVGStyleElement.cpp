/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
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
#include "SVGStyleElement.h"

#include "CSSStyleSheet.h"
#include "CommonAtomStrings.h"
#include "Document.h"
#include "SVGElementInlines.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGStyleElement);

inline SVGStyleElement::SVGStyleElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , m_styleSheetOwner(document, createdByParser)
    , m_loadEventTimer(*this, &SVGElement::loadEventTimerFired)
{
    ASSERT(hasTagName(SVGNames::styleTag));
}

SVGStyleElement::~SVGStyleElement()
{
    m_styleSheetOwner.clearDocumentData(*this);
}

Ref<SVGStyleElement> SVGStyleElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    return adoptRef(*new SVGStyleElement(tagName, document, createdByParser));
}

bool SVGStyleElement::disabled() const
{
    return sheet() && sheet()->disabled();
}

void SVGStyleElement::setDisabled(bool setDisabled)
{
    if (RefPtr styleSheet = sheet())
        styleSheet->setDisabled(setDisabled);
}

const AtomString& SVGStyleElement::type() const
{
    auto& typeValue = getAttribute(SVGNames::typeAttr);
    return typeValue.isNull() ? cssContentTypeAtom() : typeValue;
}

void SVGStyleElement::setType(const AtomString& type)
{
    setAttribute(SVGNames::typeAttr, type);
}

const AtomString& SVGStyleElement::media() const
{
    auto& value = attributeWithoutSynchronization(SVGNames::mediaAttr);
    return value.isNull() ? allAtom() : value;
}

void SVGStyleElement::setMedia(const AtomString& media)
{
    setAttributeWithoutSynchronization(SVGNames::mediaAttr, media);
}

String SVGStyleElement::title() const
{
    return attributeWithoutSynchronization(SVGNames::titleAttr);
}

void SVGStyleElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::titleAttr) {
        if (sheet() && !isInShadowTree())
            sheet()->setTitle(value);
        return;
    }
    if (name == SVGNames::typeAttr) {
        m_styleSheetOwner.setContentType(value);
        return;
    }
    if (name == SVGNames::mediaAttr) {
        m_styleSheetOwner.setMedia(value);
        return;
    }

    SVGElement::parseAttribute(name, value);
}

void SVGStyleElement::finishParsingChildren()
{
    m_styleSheetOwner.finishParsingChildren(*this);
    SVGElement::finishParsingChildren();
}

Node::InsertedIntoAncestorResult SVGStyleElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        m_styleSheetOwner.insertedIntoDocument(*this);
    return result;
}

void SVGStyleElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        m_styleSheetOwner.removedFromDocument(*this);
}

void SVGStyleElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);
    m_styleSheetOwner.childrenChanged(*this);
}

}
