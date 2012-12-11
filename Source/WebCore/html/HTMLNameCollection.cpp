/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2011, 2012 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "HTMLNameCollection.h"

#include "Element.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "NodeRareData.h"
#include "NodeTraversal.h"

namespace WebCore {

using namespace HTMLNames;

HTMLNameCollection::HTMLNameCollection(Node* document, CollectionType type, const AtomicString& name)
    : HTMLCollection(document, type, OverridesItemAfter)
    , m_name(name)
{
}

HTMLNameCollection::~HTMLNameCollection()
{
    ASSERT(ownerNode());
    ASSERT(ownerNode()->isDocumentNode());
    ASSERT(type() == WindowNamedItems || type() == DocumentNamedItems);

    ownerNode()->nodeLists()->removeCacheWithAtomicName(this, type(), m_name);
}

Element* HTMLNameCollection::virtualItemAfter(unsigned& offsetInArray, Element* previous) const
{
    ASSERT_UNUSED(offsetInArray, !offsetInArray);
    ASSERT(previous != ownerNode());

    Node* current;
    if (!previous)
        current = ownerNode()->firstChild();
    else
        current = NodeTraversal::next(previous, ownerNode());

    for (; current; current = NodeTraversal::next(current, ownerNode())) {
        if (!current->isElementNode())
            continue;
        Element* e = static_cast<Element*>(current);
        switch (type()) {
            case WindowNamedItems:
                // find only images, forms, applets, embeds and objects by name, 
                // but anything by id
                if (e->hasTagName(imgTag) ||
                    e->hasTagName(formTag) ||
                    e->hasTagName(appletTag) ||
                    e->hasTagName(embedTag) ||
                    e->hasTagName(objectTag))
                    if (e->getNameAttribute() == m_name)
                        return e;
                if (e->getIdAttribute() == m_name)
                    return e;
                break;
            case DocumentNamedItems:
                // find images, forms, applets, embeds, objects and iframes by name, 
                // applets and object by id, and images by id but only if they have
                // a name attribute (this very strange rule matches IE)
                if (e->hasTagName(formTag) || e->hasTagName(embedTag) || e->hasTagName(iframeTag)) {
                    if (e->getNameAttribute() == m_name)
                        return e;
                } else if (e->hasTagName(appletTag)) {
                    if (e->getNameAttribute() == m_name || e->getIdAttribute() == m_name)
                        return e;
                } else if (e->hasTagName(objectTag)) {
                    if ((e->getNameAttribute() == m_name || e->getIdAttribute() == m_name)
                            && static_cast<HTMLObjectElement*>(e)->isDocNamedItem())
                        return e;
                } else if (e->hasTagName(imgTag)) {
                    if (e->getNameAttribute() == m_name || (e->getIdAttribute() == m_name && e->hasName()))
                        return e;
                }
                break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return 0;
}

}
