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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "DocumentTypeImpl.h"
#include "DOMImplementationImpl.h"
#include "DocumentImpl.h"
#include "dom_node.h"

namespace DOM {

DocumentTypeImpl::DocumentTypeImpl(DOMImplementationImpl *i, DocumentImpl *doc, const DOMString &n, const DOMString &p, const DOMString &s)
    : NodeImpl(doc), m_implementation(i), m_name(n), m_publicId(p), m_systemId(s)
{
}

DocumentTypeImpl::DocumentTypeImpl(DocumentImpl *doc, const DOMString &n, const DOMString &p, const DOMString &s)
    : NodeImpl(doc), m_name(n), m_publicId(p), m_systemId(s)
{
}

DocumentTypeImpl::DocumentTypeImpl(DocumentImpl *doc, const DocumentTypeImpl &t)
    : NodeImpl(doc), m_implementation(t.m_implementation)
    , m_name(t.m_name), m_publicId(t.m_publicId), m_systemId(t.m_systemId), m_subset(t.m_subset)
{
}

DOMString DocumentTypeImpl::toString() const
{
    if (m_name.isEmpty())
        return "";

    DOMString result = "<!DOCTYPE ";
    result += m_name;
    if (!m_publicId.isEmpty()) {
	result += " PUBLIC \"";
	result += m_publicId;
	result += "\" \"";
	result += m_systemId;
	result += "\"";
    } else if (!m_systemId.isEmpty()) {
	result += " SYSTEM \"";
	result += m_systemId;
	result += "\"";
    }
    if (!m_subset.isEmpty()) {
	result += " [";
	result += m_subset;
	result += "]";
    }
    result += ">";
    return result;
}

DOMString DocumentTypeImpl::nodeName() const
{
    return name();
}

unsigned short DocumentTypeImpl::nodeType() const
{
    return Node::DOCUMENT_TYPE_NODE;
}

NodeImpl *DocumentTypeImpl::cloneNode(bool /*deep*/)
{
    // The DOM Level 2 specification says cloning DocumentType nodes is "implementation dependent" so for now we do not support it.
    return 0;
}

};
