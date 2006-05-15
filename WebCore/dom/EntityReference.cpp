/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Computer, Inc.
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
 */
#include "config.h"
#include "EntityReference.h"

namespace WebCore {

EntityReference::EntityReference(Document* doc)
    : ContainerNode(doc)
{
}

EntityReference::EntityReference(Document* doc, StringImpl* entityName)
    : ContainerNode(doc)
    , m_entityName(entityName)
{
}

String EntityReference::nodeName() const
{
    return m_entityName.get();
}

Node::NodeType EntityReference::nodeType() const
{
    return ENTITY_REFERENCE_NODE;
}

PassRefPtr<Node> EntityReference::cloneNode(bool deep)
{
    RefPtr<EntityReference> clone = new EntityReference(document(), m_entityName.get());
    // ### make sure children are readonly
    // ### since we are a reference, should we clone children anyway (even if not deep?)
    if (deep)
        cloneChildNodes(clone.get());
    return clone.release();
}

// DOM Section 1.1.1
bool EntityReference::childTypeAllowed(NodeType type)
{
    switch (type) {
        case ELEMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        case COMMENT_NODE:
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case ENTITY_REFERENCE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

String EntityReference::toString() const
{
    String result = "&";
    result += m_entityName.get();
    result += ";";

    return result;
}

} // namespace
