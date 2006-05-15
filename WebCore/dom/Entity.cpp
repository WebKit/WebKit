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
#include "Entity.h"

namespace WebCore {

Entity::Entity(Document* doc)
    : ContainerNode(doc)
{
}

Entity::Entity(Document* doc, const String& name)
    : ContainerNode(doc)
    , m_name(name.impl())
{
}

Entity::Entity(Document* doc, const String& publicId, const String& systemId, const String& notationName)
    : ContainerNode(doc)
    , m_publicId(publicId.impl())
    , m_systemId(systemId.impl())
    , m_notationName(notationName.impl())
{
}

String Entity::nodeName() const
{
    return m_name.get();
}

Node::NodeType Entity::nodeType() const
{
    return ENTITY_NODE;
}

PassRefPtr<Node> Entity::cloneNode(bool /*deep*/)
{
    // Spec says cloning Entity nodes is "implementation dependent". We do not support it.
    return 0;
}

// DOM Section 1.1.1
bool Entity::childTypeAllowed(NodeType type)
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

String Entity::toString() const
{
    String result = "<!ENTITY' ";

    if (m_name && m_name->length()) {
        result += " ";
        result += m_name.get();
    }

    if (m_publicId && m_publicId->length()) {
        result += " PUBLIC \"";
        result += m_publicId.get();
        result += "\" \"";
        result += m_systemId.get();
        result += "\"";
    } else if (m_systemId && m_systemId->length()) {
        result += " SYSTEM \"";
        result += m_systemId.get();
        result += "\"";
    }

    if (m_notationName && m_notationName->length()) {
        result += " NDATA ";
        result += m_notationName.get();
    }

    result += ">";

    return result;
}

} // namespace
