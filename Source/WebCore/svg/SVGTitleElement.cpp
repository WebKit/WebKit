/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "SVGTitleElement.h"

#include "Document.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGTitleElement);

inline SVGTitleElement::SVGTitleElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::titleTag));
}

Ref<SVGTitleElement> SVGTitleElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGTitleElement(tagName, document));
}

Node::InsertedIntoAncestorResult SVGTitleElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument && parentNode() == document().documentElement())
        document().titleElementAdded(*this);
    return result;
}

static bool isTitleElementRemovedFromSVGSVGElement(SVGTitleElement& title, ContainerNode& oldParentOfRemovedTree)
{
    if (!title.parentNode() && is<SVGSVGElement>(oldParentOfRemovedTree) && title.document().documentElement() == &oldParentOfRemovedTree)
        return true;
    if (title.parentNode() && is<SVGSVGElement>(*title.parentNode()) && !title.parentNode()->parentNode() && is<Document>(oldParentOfRemovedTree))
        return true;
    return false;
}

void SVGTitleElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    SVGElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument && isTitleElementRemovedFromSVGSVGElement(*this, oldParentOfRemovedTree))
        document().titleElementRemoved(*this);
}

void SVGTitleElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);
    if (isConnected() && parentNode() == document().documentElement())
        document().titleElementTextChanged(*this);
}

}
