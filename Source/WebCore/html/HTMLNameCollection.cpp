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

    Element* current;
    if (!previous)
        current = ElementTraversal::firstWithin(ownerNode());
    else
        current = ElementTraversal::next(previous, ownerNode());

    for (; current; current = ElementTraversal::next(current, ownerNode())) {
        switch (type()) {
        case WindowNamedItems:
            // find only images, forms, applets, embeds and objects by name, 
            // but anything by id
            if (current->hasTagName(imgTag)
                || current->hasTagName(formTag)
                || current->hasTagName(appletTag)
                || current->hasTagName(embedTag)
                || current->hasTagName(objectTag)) {
                if (current->getNameAttribute() == m_name)
                    return current;
            }
            if (current->getIdAttribute() == m_name)
                return current;
            break;
        case DocumentNamedItems:
            // find images, forms, applets, embeds, objects and iframes by name, 
            // applets and object by id, and images by id but only if they have
            // a name attribute (this very strange rule matches IE)
            if (current->hasTagName(formTag) || current->hasTagName(embedTag) || current->hasTagName(iframeTag)) {
                if (current->getNameAttribute() == m_name)
                    return current;
            } else if (current->hasTagName(appletTag)) {
                if (current->getNameAttribute() == m_name || current->getIdAttribute() == m_name)
                    return current;
            } else if (current->hasTagName(objectTag)) {
                if ((current->getNameAttribute() == m_name || current->getIdAttribute() == m_name)
                    && static_cast<HTMLObjectElement*>(current)->isDocNamedItem())
                    return current;
            } else if (current->hasTagName(imgTag)) {
                if (current->getNameAttribute() == m_name || (current->getIdAttribute() == m_name && current->hasName()))
                    return current;
            }
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return 0;
}

}
