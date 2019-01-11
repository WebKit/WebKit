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
#include "GenericCachedHTMLCollection.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HitTestResult.h"
#include "IntSize.h"
#include "NodeRareData.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLMapElement);

using namespace HTMLNames;

HTMLMapElement::HTMLMapElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(mapTag));
}

Ref<HTMLMapElement> HTMLMapElement::create(Document& document)
{
    return adoptRef(*new HTMLMapElement(mapTag, document));
}

Ref<HTMLMapElement> HTMLMapElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLMapElement(tagName, document));
}

HTMLMapElement::~HTMLMapElement() = default;

bool HTMLMapElement::mapMouseEvent(LayoutPoint location, const LayoutSize& size, HitTestResult& result)
{
    RefPtr<HTMLAreaElement> defaultArea;

    for (auto& area : descendantsOfType<HTMLAreaElement>(*this)) {
        if (area.isDefault()) {
            if (!defaultArea)
                defaultArea = &area;
        } else if (area.mapMouseEvent(location, size, result))
            return true;
    }
    
    if (defaultArea) {
        result.setInnerNode(defaultArea.get());
        result.setURLElement(defaultArea.get());
    }
    return defaultArea;
}

HTMLImageElement* HTMLMapElement::imageElement()
{
    if (m_name.isEmpty())
        return nullptr;
    return treeScope().imageElementByUsemap(*m_name.impl());
}

void HTMLMapElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    // FIXME: This logic seems wrong for XML documents.
    // Either the id or name will be used depending on the order the attributes are parsed.

    if (name == HTMLNames::idAttr || name == HTMLNames::nameAttr) {
        if (name == HTMLNames::idAttr) {
            // Call base class so that hasID bit gets set.
            HTMLElement::parseAttribute(name, value);
            if (document().isHTMLDocument())
                return;
        }
        if (isConnected())
            treeScope().removeImageMap(*this);
        String mapName = value;
        if (mapName[0] == '#')
            mapName = mapName.substring(1);
        m_name = mapName;
        if (isConnected())
            treeScope().addImageMap(*this);

        return;
    }

    HTMLElement::parseAttribute(name, value);
}

Ref<HTMLCollection> HTMLMapElement::areas()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<MapAreas>::traversalType>>(*this, MapAreas);
}

Node::InsertedIntoAncestorResult HTMLMapElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    Node::InsertedIntoAncestorResult request = HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.treeScopeChanged)
        treeScope().addImageMap(*this);
    return request;
}

void HTMLMapElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.treeScopeChanged)
        oldParentOfRemovedTree.treeScope().removeImageMap(*this);
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
}

}
