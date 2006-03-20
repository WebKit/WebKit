/*
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
 *
 */
#ifndef DOM_DocumentTypeImpl_h
#define DOM_DocumentTypeImpl_h

#include "Node.h"

namespace WebCore {

class NamedNodeMap;
class DOMImplementation;

class DocumentType : public Node
{
public:
    DocumentType(DOMImplementation *, Document *, const String &name, const String &publicId, const String &systemId);
    DocumentType(Document *, const String &name, const String &publicId, const String &systemId);
    DocumentType(Document *, const DocumentType &);

    // DOM methods & attributes for DocumentType
    NamedNodeMap *entities() const { return m_entities.get(); }
    NamedNodeMap *notations() const { return m_notations.get(); }

    String name() const { return m_name; }
    String publicId() const { return m_publicId; }
    String systemId() const { return m_systemId; }
    String internalSubset() const { return m_subset; }

    // Other methods (not part of DOM)
    DOMImplementation *implementation() const { return m_implementation.get(); }

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual String toString() const;

private:
    RefPtr<DOMImplementation> m_implementation;
    RefPtr<NamedNodeMap> m_entities;
    RefPtr<NamedNodeMap> m_notations;

    String m_name;
    String m_publicId;
    String m_systemId;
    String m_subset;
};

} //namespace

#endif
