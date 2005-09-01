/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef KDOM_NamedAttrMapImpl_H
#define KDOM_NamedAttrMapImpl_H

#include <kdom/impl/AttrImpl.h>
#include <kdom/impl/NamedNodeMapImpl.h>

namespace KDOM
{
    class NamedAttrMapImpl : public NamedNodeMapImpl
    {
    public:
        NamedAttrMapImpl(ElementImpl *);
        virtual ~NamedAttrMapImpl();

        // 'NamedAttrMapImpl' functions
        virtual NodeImpl *getNamedItem(DOMStringImpl *name);
        virtual NodeImpl *setNamedItem(NodeImpl *arg);
        virtual NodeImpl *removeNamedItem(DOMStringImpl *name);
        virtual NodeImpl *getNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName);
        virtual NodeImpl *setNamedItemNS(NodeImpl *arg);
        virtual NodeImpl *removeNamedItemNS(DOMStringImpl *namespaceURI, DOMStringImpl *localName);

        virtual NodeImpl *item(unsigned long index) const;
        virtual unsigned long length() const;

        virtual bool isReadOnly() const;

        DOMStringImpl *getValue(NodeImpl::Id id, bool nsAware = false, DOMStringImpl *qName = 0) const;
        void setValue(NodeImpl::Id id, DOMStringImpl *value, DOMStringImpl *qName = 0, DOMStringImpl *prefix = 0, bool nsAware = false, bool hasNS = false);

        AttrImpl *removeAttr(AttrImpl *attr);

        virtual void clone(NamedNodeMapImpl *other);

        NodeImpl *getNamedItem(NodeImpl::Id id, bool nsAware = false, DOMStringImpl *qName = 0);
        NodeImpl *removeNamedItem(NodeImpl::Id id, bool nsAware, DOMStringImpl *qName);
        NodeImpl *setNamedItem(NodeImpl *arg, bool nsAware, DOMStringImpl *qName);

        NodeImpl::Id mapId(DOMStringImpl *namespaceURI, DOMStringImpl *localName, bool readonly);

        AttrImpl *lookupAttribute(DOMStringImpl *name) const;

        int index(NodeImpl *item) const;

        // Event dispatching helpers
        void dispatchAttrMutationEvent(NodeImpl *node, DOMStringImpl *prevValue, DOMStringImpl *newValue, DOMStringImpl *name, unsigned short attrChange);
        void dispatchSubtreeModifiedEvent();

        // Helpers
        NodeImpl::Id idAt(unsigned long index) const;
        DOMStringImpl *valueAt(unsigned long index) const;

    protected:
        friend class ElementImpl;

        void detachFromElement();
        void setElement(ElementImpl *element);

        ElementImpl *m_ownerElement;
        AttributeImpl *m_attrs;
        unsigned long m_attrCount;
    };
};

#endif

// vim:ts=4:noet
