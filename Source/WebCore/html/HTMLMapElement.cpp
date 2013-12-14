/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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
#include "HTMLMapElement.h"

#include "Attribute.h"
#include "Document.h"
#include "ElementIterator.h"
#include "HTMLAreaElement.h"
#include "HTMLCollection.h"
#include "HTMLImageElement.h"
#include "HitTestResult.h"
#include "IntSize.h"

namespace WebCore {

using namespace HTMLNames;

HTMLMapElement::HTMLMapElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(mapTag));
}

PassRefPtr<HTMLMapElement> HTMLMapElement::create(Document& document)
{
    return adoptRef(new HTMLMapElement(mapTag, document));
}

PassRefPtr<HTMLMapElement> HTMLMapElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLMapElement(tagName, document));
}

HTMLMapElement::~HTMLMapElement()
{
}

bool HTMLMapElement::mapMouseEvent(LayoutPoint location, const LayoutSize& size, HitTestResult& result)
{
    HTMLAreaElement* defaultArea = 0;

    for (auto& area : descendantsOfType<HTMLAreaElement>(*this)) {
        if (area.isDefault()) {
            if (!defaultArea)
                defaultArea = &area;
        } else if (area.mapMouseEvent(location, size, result))
            return true;
    }
    
    if (defaultArea) {
        result.setInnerNode(defaultArea);
        result.setURLElement(defaultArea);
    }
    return defaultArea;
}

HTMLImageElement* HTMLMapElement::imageElement()
{
    if (m_name.isEmpty())
        return 0;
    AtomicString lowercasedName = m_name.lower();
    ASSERT(lowercasedName.impl());
    return document().imageElementByLowercasedUsemap(*lowercasedName.impl());
}

void HTMLMapElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    // FIXME: This logic seems wrong for XML documents.
    // Either the id or name will be used depending on the order the attributes are parsed.

    if (isIdAttributeName(name) || name == nameAttr) {
        if (isIdAttributeName(name)) {
            // Call base class so that hasID bit gets set.
            HTMLElement::parseAttribute(name, value);
            if (document().isHTMLDocument())
                return;
        }
        if (inDocument())
            treeScope().removeImageMap(*this);
        String mapName = value;
        if (mapName[0] == '#')
            mapName = mapName.substring(1);
        m_name = document().isHTMLDocument() ? mapName.lower() : mapName;
        if (inDocument())
            treeScope().addImageMap(*this);

        return;
    }

    HTMLElement::parseAttribute(name, value);
}

PassRefPtr<HTMLCollection> HTMLMapElement::areas()
{
    return ensureCachedHTMLCollection(MapAreas);
}

Node::InsertionNotificationRequest HTMLMapElement::insertedInto(ContainerNode& insertionPoint)
{
    Node::InsertionNotificationRequest request = HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint.inDocument())
        treeScope().addImageMap(*this);
    return request;
}

void HTMLMapElement::removedFrom(ContainerNode& insertionPoint)
{
    if (insertionPoint.inDocument())
        treeScope().removeImageMap(*this);
    HTMLElement::removedFrom(insertionPoint);
}

}
