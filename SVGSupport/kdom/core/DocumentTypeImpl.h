/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_DocumentTypeImpl_H
#define KDOM_DocumentTypeImpl_H

#include <kdom/core/NodeImpl.h>

namespace KDOM
{
    class NamedNodeMapImpl;
    class DocumentTypeImpl : public NodeImpl
    {
    public:
        DocumentTypeImpl(DocumentPtr *doc, DOMStringImpl *qualifiedName, DOMStringImpl *publicId, DOMStringImpl *systemId);
        virtual ~DocumentTypeImpl();

        virtual DOMStringImpl *nodeName() const;
        virtual unsigned short nodeType() const;

        // 'DocumentTypeImpl' functions
        DOMStringImpl *name() const;
        DOMStringImpl *publicId() const;
        DOMStringImpl *systemId() const;
        DOMStringImpl *internalSubset() const;

        virtual DOMStringImpl *textContent() const; // DOM3

        virtual NodeImpl *cloneNode(bool deep, DocumentPtr *doc) const;

        NamedNodeMapImpl *entities() const;
        NamedNodeMapImpl *notations() const;

        // Other methods (not part of DOM)
        void setName(DOMStringImpl *n);
        void setPublicId(DOMStringImpl *publicId);
        void setSystemId(DOMStringImpl *systemId);

    protected:
        mutable NamedNodeMapImpl *m_entities;
        mutable NamedNodeMapImpl *m_notations;

        DOMStringImpl *m_qualifiedName;
        DOMStringImpl *m_publicId;
        DOMStringImpl *m_systemId;
    };
};

#endif

// vim:ts=4:noet
