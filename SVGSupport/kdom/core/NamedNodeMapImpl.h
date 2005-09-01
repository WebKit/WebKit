/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
              (C) 2003 Apple Computer, Inc.

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

#ifndef KDOM_NamedNodeMapImpl_H
#define KDOM_NamedNodeMapImpl_H

#include <qptrlist.h>

#include <kdom/Shared.h>
#include <kdom/core/NodeImpl.h>

namespace KDOM
{
    class NamedNodeMapImpl : public Shared
    {
    public:
        NamedNodeMapImpl();
        virtual ~NamedNodeMapImpl();

        // 'NamedNodeMapImpl' functions
        virtual NodeImpl *getNamedItem(DOMStringImpl *name) = 0;
        virtual NodeImpl *setNamedItem(NodeImpl *arg) = 0;

        virtual NodeImpl *removeNamedItem(DOMStringImpl *name) = 0;
        virtual NodeImpl *getNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) = 0;
        virtual NodeImpl *setNamedItemNS(NodeImpl *arg) = 0;
        virtual NodeImpl *removeNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName) = 0;

        virtual NodeImpl *item(unsigned long index) const = 0;
        virtual unsigned long length() const = 0;

        virtual bool isReadOnly() const = 0;

        virtual void clone(NamedNodeMapImpl *other) = 0;
    };

    class RONamedNodeMapImpl : public NamedNodeMapImpl
    {
    public:
        RONamedNodeMapImpl(DocumentPtr *doc);
        virtual ~RONamedNodeMapImpl();

        // 'NamedNodeMapImpl' functions
        virtual NodeImpl *getNamedItem(DOMStringImpl *name);
        virtual NodeImpl *setNamedItem(NodeImpl *arg);
        virtual NodeImpl *removeNamedItem(DOMStringImpl *name);
        virtual NodeImpl *getNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName);
        virtual NodeImpl *setNamedItemNS(NodeImpl *arg);
        virtual NodeImpl *removeNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName);

        virtual NodeImpl *item(unsigned long index) const;
        virtual unsigned long length() const;

        virtual bool isReadOnly() const;

        virtual void clone(NamedNodeMapImpl *other);

        void addNode(NodeImpl *n);

    protected:
        QPtrList<NodeImpl> *m_map;
        DocumentPtr *m_doc;
    };
};

#endif

// vim:ts=4:noet
