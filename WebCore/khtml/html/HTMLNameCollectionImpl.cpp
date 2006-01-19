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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#include "config.h"
#include "HTMLNameCollectionImpl.h"
#include "htmlnames.h"
#include "html_objectimpl.h"
#include "DocumentImpl.h"

namespace WebCore {

using namespace HTMLNames;

HTMLNameCollectionImpl::HTMLNameCollectionImpl(DocumentImpl* _base, int _type, DOMString &name)
    : HTMLCollectionImpl(_base, _type),
      m_name(name)
{
}

NodeImpl *HTMLNameCollectionImpl::traverseNextItem(NodeImpl *current) const
{
    assert(current);

    current = current->traverseNextNode(m_base.get());

    while (current) {
        if (current->isElementNode()) {
            bool found = false;
            ElementImpl *e = static_cast<ElementImpl *>(current);
            switch(type) {
            case WINDOW_NAMED_ITEMS:
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
            case DOCUMENT_NAMED_ITEMS:
                // find images, forms, applets, embeds, objects and iframes by name, 
                // but only applets and object by id (this strange rule matches IE)
                if (e->hasTagName(imgTag) ||
                    e->hasTagName(formTag) ||
                    e->hasTagName(embedTag) ||
                    e->hasTagName(iframeTag))
                    found = e->getAttribute(nameAttr) == m_name;
                else if (e->hasTagName(appletTag))
                    found = e->getAttribute(nameAttr) == m_name ||
                        e->getAttribute(idAttr) == m_name;
                else if (e->hasTagName(objectTag))
                    found = (e->getAttribute(nameAttr) == m_name || e->getAttribute(idAttr) == m_name) &&
                        static_cast<HTMLObjectElementImpl *>(e)->isDocNamedItem();
                break;
            default:
                assert(0);
            }

            if (found)
                return current;
        }
        current = current->traverseNextNode(m_base.get());
    }
    return 0;
}

}
