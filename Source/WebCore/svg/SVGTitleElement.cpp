/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "SVGNames.h"
#include "Text.h"

namespace WebCore {

inline SVGTitleElement::SVGTitleElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::titleTag));
}

Ref<SVGTitleElement> SVGTitleElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGTitleElement(tagName, document));
}

Node::InsertionNotificationRequest SVGTitleElement::insertedInto(ContainerNode& rootParent)
{
    SVGElement::insertedInto(rootParent);
    if (!rootParent.inDocument())
        return InsertionDone;

    if (firstChild() && document().isSVGDocument())
        document().titleElementAdded(*this);
    return InsertionDone;
}

void SVGTitleElement::removedFrom(ContainerNode& rootParent)
{
    SVGElement::removedFrom(rootParent);
    if (rootParent.inDocument() && document().isSVGDocument())
        document().titleElementRemoved(*this);
}

void SVGTitleElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);
    document().titleElementTextChanged(*this);
}

}
