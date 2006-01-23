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

#include "NodeImpl.h"

namespace DOM {

class NamedNodeMapImpl;
class DOMImplementationImpl;

class DocumentTypeImpl : public NodeImpl
{
public:
    DocumentTypeImpl(DOMImplementationImpl *, DocumentImpl *, const DOMString &name, const DOMString &publicId, const DOMString &systemId);
    DocumentTypeImpl(DocumentImpl *, const DOMString &name, const DOMString &publicId, const DOMString &systemId);
    DocumentTypeImpl(DocumentImpl *, const DocumentTypeImpl &);

    // DOM methods & attributes for DocumentType
    NamedNodeMapImpl *entities() const { return m_entities.get(); }
    NamedNodeMapImpl *notations() const { return m_notations.get(); }

    DOMString name() const { return m_name; }
    DOMString publicId() const { return m_publicId; }
    DOMString systemId() const { return m_systemId; }
    DOMString internalSubset() const { return m_subset; }

    // Other methods (not part of DOM)
    DOMImplementationImpl *implementation() const { return m_implementation.get(); }

    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);
    virtual DOMString toString() const;

private:
    RefPtr<DOMImplementationImpl> m_implementation;
    RefPtr<NamedNodeMapImpl> m_entities;
    RefPtr<NamedNodeMapImpl> m_notations;

    DOMString m_name;
    DOMString m_publicId;
    DOMString m_systemId;
    DOMString m_subset;
};

} //namespace

#endif
