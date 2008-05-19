/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "DocumentFragment.h"

namespace WebCore {

DocumentFragment::DocumentFragment(Document *doc) : ContainerNode(doc)
{
}

String DocumentFragment::nodeName() const
{
  return "#document-fragment";
}

Node::NodeType DocumentFragment::nodeType() const
{
    return DOCUMENT_FRAGMENT_NODE;
}

// DOM Section 1.1.1
bool DocumentFragment::childTypeAllowed(NodeType type)
{
    switch (type) {
        case ELEMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        case COMMENT_NODE:
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case ENTITY_REFERENCE_NODE:
            return true;
        default:
            return false;
    }
}

PassRefPtr<Node> DocumentFragment::cloneNode(bool deep)
{
    RefPtr<DocumentFragment> clone = new DocumentFragment(document());
    if (deep)
        cloneChildNodes(clone.get());
    return clone.release();
}

}
