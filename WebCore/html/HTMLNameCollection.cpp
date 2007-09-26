/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

namespace WebCore {

using namespace HTMLNames;

HTMLNameCollection::HTMLNameCollection(Document* base, HTMLCollection::Type type, const String& name)
    : HTMLCollection(base, type)
    , m_name(name)
{
    ASSERT(!info);
    info = base->nameCollectionInfo(type, name);
}

Node* HTMLNameCollection::traverseNextItem(Node* current) const
{
    ASSERT(current);

    current = current->traverseNextNode(m_base.get());

    while (current) {
        if (current->isElementNode()) {
            bool found = false;
            Element* e = static_cast<Element*>(current);
            switch(type) {
            case WindowNamedItems:
                // find only images, forms, applets, embeds and objects by name, 
                // but anything by id
                if (e->hasTagName(imgTag) ||
                    e->hasTagName(formTag) ||
                    e->hasTagName(appletTag) ||
                    e->hasTagName(embedTag) ||
                    e->hasTagName(objectTag))
                    found = e->getAttribute(nameAttr) == m_name;
                found |= e->getAttribute(idAttr) == m_name;
                break;
            case DocumentNamedItems:
                // find images, forms, applets, embeds, objects and iframes by name, 
                // applets and object by id, and images by id but only if they have
                // a name attribute (this very strange rule matches IE)
                if (e->hasTagName(formTag) ||
                    e->hasTagName(embedTag) ||
                    e->hasTagName(iframeTag))
                    found = e->getAttribute(nameAttr) == m_name;
                else if (e->hasTagName(appletTag))
                    found = e->getAttribute(nameAttr) == m_name ||
                        e->getAttribute(idAttr) == m_name;
                else if (e->hasTagName(objectTag))
                    found = (e->getAttribute(nameAttr) == m_name || e->getAttribute(idAttr) == m_name) &&
                        static_cast<HTMLObjectElement*>(e)->isDocNamedItem();
                else if (e->hasTagName(imgTag))
                    found = e->getAttribute(nameAttr) == m_name || (e->getAttribute(idAttr) == m_name && e->hasAttribute(nameAttr));
                break;
            default:
                ASSERT(0);
            }

            if (found)
                return current;
        }
        current = current->traverseNextNode(m_base.get());
    }
    return 0;
}

}
